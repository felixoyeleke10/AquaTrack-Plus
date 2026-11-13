#include "data/media/FrameDecoder.h"

#include "data/media/MediaTools.h"
#include "data/media/VideoProbe.h"
#include "data/media/VideoSequenceManifest.h"

#include <QCache>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QImageReader>
#include <QMutexLocker>
#include <QProcess>

#include <algorithm>
#include <cmath>
#include <memory>

namespace aquatrack::media {
namespace {

constexpr int kReadTimeoutMs = 20000;

QSize outputSizeFor(QSize source, int maximumWidth)
{
    if (source.width() <= maximumWidth || maximumWidth <= 0)
        return source;
    const int height = int(std::lround(double(source.height()) * maximumWidth / source.width() / 2.0)) * 2;
    return {maximumWidth, std::max(2, height)};
}

}  // namespace

// Runs on the decoder thread. Everything here except the shared fields of
// FrameDecoder (guarded by its mutex) is touched by this thread only.
class FrameDecoder::Worker final : public QObject {
public:
    explicit Worker(FrameDecoder* owner) : m_owner(owner) {}

    void drain()
    {
        for (;;) {
            int frame = -1;
            quint64 generation = 0;
            {
                QMutexLocker lock(&m_owner->m_mutex);
                if (m_owner->m_pending < 0 || m_owner->m_abort) {
                    m_owner->m_drainScheduled = false;
                    return;
                }
                frame = std::exchange(m_owner->m_pending, -1);
                generation = m_owner->m_generation;
                if (generation != m_generation) {
                    resetPipe();
                    m_cache.clear();
                    m_lastImage = QImage();
                    m_source = m_owner->m_source;
                    m_generation = generation;
                }
                m_cache.setMaxCost(std::max(1, m_owner->m_cacheSize));
                m_maximumWidth = m_owner->m_maximumWidth;
            }

            QString error;
            QImage image = decode(frame, &error);
            if (m_owner->m_abort)
                return;
            if (image.isNull())
                emit m_owner->frameFailed(generation, frame,
                                          error.isEmpty()
                                              ? QStringLiteral("Could not read frame %1").arg(frame)
                                              : error);
            else
                emit m_owner->frameDecoded(generation, frame, std::move(image));
        }
    }

    void shutdown() { resetPipe(); }

private:
    QImage decode(int frame, QString* error)
    {
        if (!m_source.isOpen() || frame < 1 || frame > m_source.frameCount()) {
            *error = QStringLiteral("Frame %1 is outside the sequence").arg(frame);
            return {};
        }
        if (const QImage* cached = m_cache.object(frame))
            return *cached;

        QImage image = m_source.isVideo() ? decodeVideo(frame, error) : decodeImage(frame, error);
        if (!image.isNull())
            m_cache.insert(frame, new QImage(image));
        return image;
    }

    QImage decodeImage(int frame, QString* error)
    {
        const QString path = m_source.imagePath(frame);
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QImage image = reader.read();
        if (image.isNull())
            *error = QStringLiteral("Could not read frame %1 (%2): %3")
                         .arg(frame)
                         .arg(QFileInfo(path).fileName(), reader.errorString());
        return image;
    }

    QImage decodeVideo(int frame, QString* error)
    {
        const int index = m_source.clipIndexForFrame(frame);
        const Clip& clip = m_source.clips().at(index);
        const int local = frame - clip.firstFrame + 1;

        // Reading forward is far cheaper than seeking for short gaps.
        const int skipLimit = std::max(8, int(std::lround(clip.frameRate / 2.0)));
        const bool reuse = m_pipe && m_pipeClip == index && local >= m_pipeNext &&
                           local - m_pipeNext <= skipLimit;
        if (!reuse && !startPipe(index, local, error))
            return {};

        QImage image;
        while (m_pipeNext <= local) {
            const int reading = m_pipeNext;
            QImage next;
            const ReadResult result = readFrame(next, error);
            if (result == ReadResult::EndOfStream) {
                // Containers sometimes claim a frame or two more than the
                // stream holds; show the last real frame for those.
                if (!m_lastImage.isNull() && m_lastClip == index) {
                    error->clear();
                    return m_lastImage;
                }
                if (error->isEmpty())
                    *error = QStringLiteral("Frame %1 is past the end of %2")
                                 .arg(frame)
                                 .arg(QFileInfo(clip.path).fileName());
                resetPipe();
                return {};
            }
            if (result == ReadResult::Failed) {
                resetPipe();
                return {};
            }
            m_lastImage = next;
            m_lastClip = index;
            if (reading == local)
                image = std::move(next);
            else
                m_cache.insert(clip.firstFrame + reading - 1, new QImage(next));
        }
        return image;
    }

    bool startPipe(int index, int local, QString* error)
    {
        resetPipe();
        const QString ffmpeg = ffmpegTool(QStringLiteral("ffmpeg"));
        if (ffmpeg.isEmpty()) {
            *error = ffmpegMissingMessage();
            return false;
        }
        const Clip& clip = m_source.clips().at(index);
        m_outputSize = outputSizeFor(clip.size, m_maximumWidth);

        QStringList arguments{QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-nostdin")};
        if (local > 1) {
            // Input seeking decodes from the previous keyframe and drops
            // frames before the target time, so the first frame out is the
            // one whose timestamp is at or after it. Aim a quarter frame
            // early so rounding cannot skip the wanted frame.
            const double seconds = (local - 1.25) / clip.frameRate;
            arguments << QStringLiteral("-ss") << QString::number(std::max(0.0, seconds), 'f', 6);
        }
        arguments << QStringLiteral("-i") << clip.path << QStringLiteral("-map")
                  << QStringLiteral("0:v:0") << QStringLiteral("-an") << QStringLiteral("-sn")
                  << QStringLiteral("-dn");
        if (m_outputSize != clip.size)
            arguments << QStringLiteral("-vf")
                      << QStringLiteral("scale=%1:%2:flags=bicubic")
                             .arg(m_outputSize.width())
                             .arg(m_outputSize.height());
        arguments << QStringLiteral("-f") << QStringLiteral("rawvideo") << QStringLiteral("-pix_fmt")
                  << QStringLiteral("bgra") << QStringLiteral("pipe:1");

        m_pipe = std::make_unique<QProcess>();
        m_pipe->setReadChannel(QProcess::StandardOutput);
        m_pipe->start(ffmpeg, arguments, QIODevice::ReadOnly);
        if (!m_pipe->waitForStarted(5000)) {
            *error = QStringLiteral("Could not start FFmpeg: %1").arg(m_pipe->errorString());
            m_pipe.reset();
            return false;
        }
        m_pipeClip = index;
        m_pipeNext = local;
        return true;
    }

    enum class ReadResult { Frame, EndOfStream, Failed };

    ReadResult readFrame(QImage& image, QString* error)
    {
        image = QImage(m_outputSize, QImage::Format_RGB32);
        if (image.isNull()) {
            *error = QStringLiteral("Not enough memory for a %1x%2 frame")
                         .arg(m_outputSize.width())
                         .arg(m_outputSize.height());
            return ReadResult::Failed;
        }
        char* target = reinterpret_cast<char*>(image.bits());
        const qint64 needed = image.sizeInBytes();
        qint64 received = 0;
        QElapsedTimer waiting;
        waiting.start();
        while (received < needed) {
            if (m_owner->m_abort)
                return ReadResult::Failed;
            if (m_pipe->bytesAvailable() > 0) {
                const qint64 read = m_pipe->read(target + received, needed - received);
                if (read < 0) {
                    *error = m_pipe->errorString();
                    return ReadResult::Failed;
                }
                received += read;
                waiting.restart();
                continue;
            }
            if (m_pipe->state() == QProcess::NotRunning) {
                const QString detail = QString::fromUtf8(m_pipe->readAllStandardError()).trimmed();
                if (!detail.isEmpty())
                    *error = QStringLiteral("FFmpeg: %1").arg(detail.section(QLatin1Char('\n'), -1));
                return received == 0 && detail.isEmpty() ? ReadResult::EndOfStream : ReadResult::Failed;
            }
            // Short waits keep shutdown and source changes responsive.
            m_pipe->waitForReadyRead(50);
            if (waiting.elapsed() > kReadTimeoutMs) {
                *error = QStringLiteral("FFmpeg stopped producing frames");
                return ReadResult::Failed;
            }
        }
        ++m_pipeNext;
        return ReadResult::Frame;
    }

    void resetPipe()
    {
        if (!m_pipe)
            return;
        m_pipe->kill();
        m_pipe->waitForFinished(2000);
        m_pipe.reset();
        m_pipeClip = -1;
        m_pipeNext = 0;
    }

    FrameDecoder* m_owner;
    MediaSource m_source;
    quint64 m_generation = 0;
    QCache<int, QImage> m_cache{16};
    int m_maximumWidth = 1920;

    std::unique_ptr<QProcess> m_pipe;
    int m_pipeClip = -1;
    int m_pipeNext = 0;  // clip-local frame the next read returns
    QSize m_outputSize;
    QImage m_lastImage;
    int m_lastClip = -1;
};

FrameDecoder::FrameDecoder(QObject* parent) : QObject(parent)
{
    m_thread.setObjectName(QStringLiteral("AquaTrack frame decoder"));
    m_worker = new Worker(this);
    m_worker->moveToThread(&m_thread);
    m_thread.start();
}

FrameDecoder::~FrameDecoder()
{
    m_abort = true;
    {
        QMutexLocker lock(&m_mutex);
        m_pending = -1;
    }
    QMetaObject::invokeMethod(m_worker, [worker = m_worker] { worker->shutdown(); },
                              Qt::BlockingQueuedConnection);
    m_thread.quit();
    m_thread.wait();
    delete m_worker;
}

void FrameDecoder::setSource(const MediaSource& source)
{
    QMutexLocker lock(&m_mutex);
    m_source = source;
    ++m_generation;
    m_pending = -1;
}

void FrameDecoder::clear()
{
    setSource(MediaSource());
}

quint64 FrameDecoder::generation() const
{
    QMutexLocker lock(&m_mutex);
    return m_generation;
}

void FrameDecoder::setCacheSize(int frames)
{
    QMutexLocker lock(&m_mutex);
    m_cacheSize = std::clamp(frames, 1, 256);
}

void FrameDecoder::setMaximumWidth(int pixels)
{
    QMutexLocker lock(&m_mutex);
    m_maximumWidth = std::max(0, pixels);
}

void FrameDecoder::request(int frame)
{
    QMutexLocker lock(&m_mutex);
    m_pending = frame;
    if (m_drainScheduled)
        return;
    m_drainScheduled = true;
    lock.unlock();
    scheduleDrain();
}

void FrameDecoder::scheduleDrain()
{
    QMetaObject::invokeMethod(m_worker, [worker = m_worker] { worker->drain(); }, Qt::QueuedConnection);
}

QImage grabVideoFrame(const QString& path, double seconds, int width, QString* error)
{
    const QString ffmpeg = ffmpegTool(QStringLiteral("ffmpeg"));
    if (ffmpeg.isEmpty()) {
        if (error)
            *error = ffmpegMissingMessage();
        return {};
    }
    QProcess process;
    process.start(ffmpeg,
                  {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-nostdin"),
                   QStringLiteral("-ss"), QString::number(std::max(0.0, seconds), 'f', 3),
                   QStringLiteral("-i"), path, QStringLiteral("-frames:v"), QStringLiteral("1"),
                   QStringLiteral("-vf"), QStringLiteral("scale=%1:-2").arg(std::max(16, width)),
                   QStringLiteral("-f"), QStringLiteral("image2pipe"), QStringLiteral("-vcodec"),
                   QStringLiteral("bmp"), QStringLiteral("pipe:1")});
    if (!process.waitForFinished(30000) || process.exitCode() != 0) {
        process.kill();
        if (error)
            *error = QStringLiteral("Could not grab a frame from %1").arg(QFileInfo(path).fileName());
        return {};
    }
    QImage image;
    image.loadFromData(process.readAllStandardOutput(), "BMP");
    return image;
}

QImage mediaThumbnail(const QString& path, int width)
{
    switch (classifyMedia(path)) {
    case MediaKind::ImageSequence: {
        BrackishMotSequence sequence;
        if (!sequence.load(path))
            return {};
        QImageReader reader(sequence.imagePath(std::min(sequence.frameCount(), 1)));
        const QSize full = reader.size();
        if (full.isValid() && full.width() > width)
            reader.setScaledSize(full.scaled(width, width * full.height() / full.width(),
                                             Qt::KeepAspectRatio));
        return reader.read();
    }
    case MediaKind::VideoFile: {
        // A little way in: first frames of field footage are often a lens
        // cap or the boat deck.
        VideoInfo info;
        const double seconds = probeVideo(path, info) ? std::min(2.0, info.durationMs / 4000.0) : 0.0;
        return grabVideoFrame(path, seconds, width);
    }
    case MediaKind::VideoSequence: {
        VideoSequenceManifest manifest;
        if (!manifest.load(path) || manifest.clips.isEmpty())
            return {};
        return mediaThumbnail(manifest.clips.first().path, width);
    }
    case MediaKind::Folder:
    case MediaKind::Unsupported:
    case MediaKind::Missing:
        break;
    }
    return {};
}

}  // namespace aquatrack::media
