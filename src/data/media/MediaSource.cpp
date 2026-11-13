#include "data/media/MediaSource.h"

#include "data/media/VideoProbe.h"
#include "data/media/VideoSequenceManifest.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <algorithm>
#include <cmath>

namespace aquatrack::media {
namespace {

void setError(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

Clip clipFrom(const VideoInfo& info, int firstFrame, qint64 startMs)
{
    Clip clip;
    clip.path = info.path;
    clip.firstFrame = firstFrame;
    clip.frameCount = info.frameCount;
    clip.frameRate = info.frameRate;
    clip.size = QSize(info.width, info.height);
    clip.startMs = startMs;
    clip.durationMs = info.durationMs > 0
                          ? info.durationMs
                          : qint64(std::llround(info.frameCount * 1000.0 / info.frameRate));
    return clip;
}

}  // namespace

bool MediaSource::open(const QString& path, QString* error)
{
    const QString cleaned = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const MediaKind kind = classifyMedia(cleaned);
    MediaSource opened;
    opened.m_kind = kind;
    opened.m_path = cleaned;

    switch (kind) {
    case MediaKind::ImageSequence: {
        if (!opened.m_images.load(cleaned, error))
            return false;
        opened.m_name = opened.m_images.name();
        opened.m_frameCount = opened.m_images.frameCount();
        opened.m_frameRate = opened.m_images.frameRate();
        opened.m_size = QSize(opened.m_images.imageWidth(), opened.m_images.imageHeight());
        break;
    }
    case MediaKind::VideoFile: {
        VideoInfo info;
        if (!probeVideo(cleaned, info, error))
            return false;
        opened.m_name = QFileInfo(cleaned).completeBaseName();
        opened.m_clips = {clipFrom(info, 1, 0)};
        break;
    }
    case MediaKind::VideoSequence: {
        VideoSequenceManifest manifest;
        if (!manifest.load(cleaned, error))
            return false;
        bool changed = false;
        if (!manifest.refresh(error, &changed))
            return false;
        if (changed)
            (void)manifest.save(cleaned);  // a stale cache only costs time
        opened.m_name = manifest.name;
        int first = 1;
        qint64 start = 0;
        for (const auto& info : manifest.clips) {
            opened.m_clips.push_back(clipFrom(info, first, start));
            first += info.frameCount;
            start += opened.m_clips.last().durationMs;
        }
        break;
    }
    case MediaKind::Folder:
        setError(error, QStringLiteral("%1 is a folder but not a sequence. Choose a BrackishMOT "
                                       "sequence folder (with seqinfo.ini) or a video file.")
                            .arg(QDir::toNativeSeparators(cleaned)));
        return false;
    case MediaKind::Unsupported:
        setError(error, QStringLiteral("%1 is not a video file AquaTrack can open.")
                            .arg(QFileInfo(cleaned).fileName()));
        return false;
    case MediaKind::Missing:
        setError(error, QStringLiteral("Nothing exists at %1. It may have been moved or its drive "
                                       "disconnected.")
                            .arg(QDir::toNativeSeparators(cleaned)));
        return false;
    }

    if (opened.isVideo()) {
        const Clip& last = opened.m_clips.last();
        opened.m_frameCount = last.lastFrame();
        opened.m_frameRate = opened.m_clips.first().frameRate;
        opened.m_size = opened.m_clips.first().size;
    }
    if (opened.m_frameCount <= 0) {
        setError(error, QStringLiteral("%1 has no frames").arg(QFileInfo(cleaned).fileName()));
        return false;
    }
    *this = std::move(opened);
    return true;
}

qint64 MediaSource::durationMs() const
{
    if (isVideo())
        return m_clips.isEmpty() ? 0 : m_clips.last().startMs + m_clips.last().durationMs;
    return m_frameRate > 0.0 ? qint64(std::llround(m_frameCount * 1000.0 / m_frameRate)) : 0;
}

int MediaSource::clipIndexForFrame(int frame) const
{
    if (!isVideo() || frame < 1 || frame > m_frameCount)
        return -1;
    const auto found = std::upper_bound(m_clips.cbegin(), m_clips.cend(), frame,
                                        [](int value, const Clip& clip) {
                                            return value < clip.firstFrame;
                                        });
    return int(std::distance(m_clips.cbegin(), found)) - 1;
}

QSize MediaSource::frameSizeAt(int frame) const
{
    const int index = clipIndexForFrame(frame);
    return index >= 0 ? m_clips.at(index).size : m_size;
}

qint64 MediaSource::timestampMs(int frame) const
{
    if (frame < 1 || m_frameRate <= 0.0)
        return 0;
    if (!isVideo())
        return qint64(std::llround((frame - 1) * 1000.0 / m_frameRate));
    const int index = clipIndexForFrame(std::min(frame, m_frameCount));
    if (index < 0)
        return 0;
    const Clip& clip = m_clips.at(index);
    return clip.startMs + qint64(std::llround((frame - clip.firstFrame) * 1000.0 / clip.frameRate));
}

int MediaSource::frameAtTimestamp(qint64 milliseconds) const
{
    if (m_frameCount <= 0)
        return 0;
    milliseconds = std::max<qint64>(0, milliseconds);
    if (!isVideo() || m_clips.isEmpty()) {
        // Nearest frame: timestamps are rounded to whole milliseconds, so
        // flooring could land one frame early.
        const int frame = int(std::lround(milliseconds * m_frameRate / 1000.0)) + 1;
        return std::clamp(frame, 1, m_frameCount);
    }
    auto clip = std::upper_bound(m_clips.cbegin(), m_clips.cend(), milliseconds,
                                 [](qint64 value, const Clip& item) { return value < item.startMs; });
    if (clip != m_clips.cbegin())
        --clip;
    const int offset = int(std::lround((milliseconds - clip->startMs) * clip->frameRate / 1000.0));
    return std::clamp(clip->firstFrame + std::min(offset, clip->frameCount - 1), 1, m_frameCount);
}

int MediaSource::frameIntervalMs(int frame) const
{
    const int index = clipIndexForFrame(frame);
    const double rate = index >= 0 ? m_clips.at(index).frameRate : m_frameRate;
    return rate > 0.0 ? std::max(1, int(std::lround(1000.0 / rate))) : 66;
}

QString MediaSource::imagePath(int frame) const
{
    return m_kind == MediaKind::ImageSequence ? m_images.imagePath(frame) : QString();
}

QVector<GroundTruth> MediaSource::groundTruth(int frame) const
{
    return m_kind == MediaKind::ImageSequence ? m_images.annotations(frame) : QVector<GroundTruth>{};
}

QString MediaSource::reviewIdFor(const QString& path)
{
    switch (classifyMedia(path)) {
    case MediaKind::ImageSequence: {
        QSettings info(QDir(path).filePath(QStringLiteral("seqinfo.ini")), QSettings::IniFormat);
        return info.value(QStringLiteral("Sequence/name"), QDir(path).dirName()).toString();
    }
    case MediaKind::VideoSequence: {
        VideoSequenceManifest manifest;
        return manifest.load(path) ? manifest.name : QDir(path).dirName();
    }
    case MediaKind::VideoFile:
        return QFileInfo(path).completeBaseName();
    case MediaKind::Folder:
    case MediaKind::Unsupported:
    case MediaKind::Missing:
        break;
    }
    return {};
}

}  // namespace aquatrack::media
