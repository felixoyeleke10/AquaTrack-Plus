#include "data/media/VideoProbe.h"

#include "data/media/MediaTools.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <cmath>

namespace aquatrack::media {
namespace {

void setError(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

double rational(const QString& value)
{
    const auto parts = value.split(QLatin1Char('/'));
    if (parts.size() != 2)
        return value.toDouble();
    const double denominator = parts.at(1).toDouble();
    return denominator == 0.0 ? 0.0 : parts.at(0).toDouble() / denominator;
}

bool runProbe(const QStringList& arguments, QJsonObject& result, QString* error)
{
    const QString ffprobe = ffmpegTool(QStringLiteral("ffprobe"));
    if (ffprobe.isEmpty()) {
        setError(error, ffmpegMissingMessage());
        return false;
    }
    QProcess process;
    process.start(ffprobe, arguments);
    if (!process.waitForStarted(5000)) {
        setError(error, QStringLiteral("Could not start ffprobe: %1").arg(process.errorString()));
        return false;
    }
    // Counting packets reads the whole container; allow for large files.
    if (!process.waitForFinished(120000)) {
        process.kill();
        process.waitForFinished(2000);
        setError(error, QStringLiteral("ffprobe timed out"));
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString detail = QString::fromUtf8(process.readAllStandardError()).trimmed();
        setError(error, detail.isEmpty() ? QStringLiteral("ffprobe could not read the file")
                                         : detail.section(QLatin1Char('\n'), -1));
        return false;
    }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(process.readAllStandardOutput(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("ffprobe returned unreadable output"));
        return false;
    }
    result = document.object();
    return true;
}

}  // namespace

bool probeVideo(const QString& path, VideoInfo& info, QString* error)
{
    const QFileInfo file(path);
    if (!file.isFile()) {
        setError(error, QStringLiteral("%1 does not exist").arg(path));
        return false;
    }

    QJsonObject result;
    if (!runProbe({QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-select_streams"),
                   QStringLiteral("v:0"), QStringLiteral("-show_entries"),
                   QStringLiteral("stream=codec_name,width,height,r_frame_rate,avg_frame_rate,"
                                  "nb_frames,duration:format=duration"),
                   QStringLiteral("-of"), QStringLiteral("json"), file.absoluteFilePath()},
                  result, error))
        return false;

    const auto streams = result.value(QStringLiteral("streams")).toArray();
    if (streams.isEmpty()) {
        setError(error, QStringLiteral("%1 has no video stream").arg(file.fileName()));
        return false;
    }
    const auto stream = streams.first().toObject();

    VideoInfo probed;
    probed.path = file.absoluteFilePath();
    probed.bytes = file.size();
    probed.modified = file.lastModified();
    probed.codec = stream.value(QStringLiteral("codec_name")).toString();
    probed.width = stream.value(QStringLiteral("width")).toInt();
    probed.height = stream.value(QStringLiteral("height")).toInt();

    // The average rate is the one frame timestamps follow; r_frame_rate is
    // the container's guess and can be a multiple of it.
    double fps = rational(stream.value(QStringLiteral("avg_frame_rate")).toString());
    if (!(fps > 0.0) || !std::isfinite(fps))
        fps = rational(stream.value(QStringLiteral("r_frame_rate")).toString());
    probed.frameRate = (fps > 0.0 && std::isfinite(fps)) ? fps : 0.0;

    double seconds = stream.value(QStringLiteral("duration")).toString().toDouble();
    if (!(seconds > 0.0))
        seconds = result.value(QStringLiteral("format"))
                      .toObject()
                      .value(QStringLiteral("duration"))
                      .toString()
                      .toDouble();
    probed.durationMs = qint64(std::llround(std::max(0.0, seconds) * 1000.0));

    probed.frameCount = stream.value(QStringLiteral("nb_frames")).toString().toInt();
    if (probed.frameCount <= 0) {
        // MKV/WebM do not record a frame count; counting packets is exact.
        QJsonObject counted;
        if (runProbe({QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-select_streams"),
                      QStringLiteral("v:0"), QStringLiteral("-count_packets"),
                      QStringLiteral("-show_entries"), QStringLiteral("stream=nb_read_packets"),
                      QStringLiteral("-of"), QStringLiteral("json"), file.absoluteFilePath()},
                     counted, nullptr)) {
            const auto countedStreams = counted.value(QStringLiteral("streams")).toArray();
            if (!countedStreams.isEmpty())
                probed.frameCount = countedStreams.first()
                                        .toObject()
                                        .value(QStringLiteral("nb_read_packets"))
                                        .toString()
                                        .toInt();
        }
        if (probed.frameCount <= 0 && probed.frameRate > 0.0)
            probed.frameCount = int(std::llround(seconds * probed.frameRate));
    }
    if (probed.frameRate <= 0.0 && probed.frameCount > 0 && seconds > 0.0)
        probed.frameRate = probed.frameCount / seconds;
    if (probed.durationMs <= 0 && probed.frameRate > 0.0)
        probed.durationMs = qint64(std::llround(probed.frameCount * 1000.0 / probed.frameRate));

    if (!probed.isValid()) {
        setError(error, QStringLiteral("%1 has no usable frame size, rate or count")
                            .arg(file.fileName()));
        return false;
    }
    info = std::move(probed);
    return true;
}

QString formatDuration(qint64 milliseconds)
{
    const qint64 seconds = std::max<qint64>(0, milliseconds) / 1000;
    if (seconds >= 3600)
        return QStringLiteral("%1:%2:%3")
            .arg(seconds / 3600)
            .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
            .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace aquatrack::media
