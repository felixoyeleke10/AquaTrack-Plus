#pragma once

#include <QDateTime>
#include <QString>

namespace aquatrack::media {

// What ffprobe reports about the first video stream of a file.
struct VideoInfo {
    QString path;
    int frameCount = 0;
    // Exact average rate (29.97, not 30): frame times are derived from it.
    double frameRate = 0.0;
    int width = 0;
    int height = 0;
    qint64 durationMs = 0;
    qint64 bytes = 0;
    QDateTime modified;
    QString codec;

    [[nodiscard]] bool isValid() const
    {
        return frameCount > 0 && frameRate > 0.0 && width > 0 && height > 0;
    }
};

// Runs ffprobe. Blocking (typically 50-150 ms): call it from a worker
// thread when probing many files.
[[nodiscard]] bool probeVideo(const QString& path, VideoInfo& info, QString* error = nullptr);

// "00:35" or "1:02:07".
[[nodiscard]] QString formatDuration(qint64 milliseconds);

}  // namespace aquatrack::media
