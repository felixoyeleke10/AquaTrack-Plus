#include "data/media/ThumbnailCache.h"

#include "data/media/FrameDecoder.h"
#include "data/media/MediaSource.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>

#include <algorithm>
#include <functional>

namespace aquatrack::media {
namespace {

QString cacheFolder()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty())
        base = QDir::tempPath() + QStringLiteral("/AquaTrack");
    return base + QStringLiteral("/thumbnails");
}

// Changes whenever the media does: a re-encoded video, or frames added to a
// sequence folder, gets a new key rather than a stale picture.
QString cacheFile(const QString& path, int frame, int width)
{
    const QFileInfo info(path);
    // "v2": thumbnails since black borders are trimmed.
    const QString key = QStringLiteral("v2|%1|%2|%3|%4")
                            .arg(QDir::cleanPath(info.absoluteFilePath()).toLower())
                            .arg(info.lastModified().toMSecsSinceEpoch())
                            .arg(frame)
                            .arg(width);
    const QByteArray hash =
        QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex();
    return cacheFolder() + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".jpg");
}

// Footage is often letterboxed or pillarboxed: black bands baked into the
// frame. Cropping them lets a poster fill its card instead of showing bars.
// Only near-uniform black edges go, and never more than a third per side.
QImage trimBlackBorders(const QImage& source)
{
    if (source.isNull())
        return source;
    const QImage image = source.convertToFormat(QImage::Format_RGB32);
    const auto dark = [&](bool column, int index) {
        const int length = column ? image.height() : image.width();
        int bright = 0;
        for (int i = 0; i < length; i += 2) {
            const QRgb pixel = column ? image.pixel(index, i) : image.pixel(i, index);
            if (qGray(pixel) > 28)
                ++bright;
        }
        return bright * 2 < length / 50 + 1;  // under ~2% non-black
    };
    int left = 0, right = image.width() - 1, top = 0, bottom = image.height() - 1;
    while (left < image.width() / 3 && dark(true, left))
        ++left;
    while (right > image.width() * 2 / 3 && dark(true, right))
        --right;
    while (top < image.height() / 3 && dark(false, top))
        ++top;
    while (bottom > image.height() * 2 / 3 && dark(false, bottom))
        --bottom;
    if (left == 0 && top == 0 && right == image.width() - 1 && bottom == image.height() - 1)
        return source;
    return source.copy(QRect(QPoint(left, top), QPoint(right, bottom)));
}

QImage cached(const QString& file, const std::function<QImage()>& make)
{
    if (QFileInfo::exists(file)) {
        QImage image(file);
        if (!image.isNull())
            return image;
    }
    QImage image = trimBlackBorders(make());
    if (!image.isNull() && QDir().mkpath(QFileInfo(file).absolutePath()))
        (void)image.save(file, "JPG", 85);
    return image;
}

QImage readScaled(const QString& file, int width)
{
    QImageReader reader(file);
    const QSize full = reader.size();
    if (full.isValid() && full.width() > width)
        reader.setScaledSize(full.scaled(width, width * full.height() / full.width(),
                                         Qt::KeepAspectRatio));
    return reader.read();
}

}  // namespace

QImage cachedMediaThumbnail(const QString& path, int width)
{
    if (path.isEmpty() || !QFileInfo::exists(path))
        return {};
    return cached(cacheFile(path, 0, width), [&] { return mediaThumbnail(path, width); });
}

QImage cachedFrameThumbnail(const QString& path, int frame, int width)
{
    if (path.isEmpty() || frame < 1 || !QFileInfo::exists(path))
        return {};
    return cached(cacheFile(path, frame, width), [&]() -> QImage {
        MediaSource source;
        if (!source.open(path) || frame > source.frameCount())
            return {};
        if (source.kind() == MediaKind::ImageSequence)
            return readScaled(source.imagePath(frame), width);
        const int clipIndex = source.clipIndexForFrame(frame);
        if (clipIndex < 0)
            return {};
        const Clip& clip = source.clips().at(clipIndex);
        const double seconds = (source.timestampMs(frame) - clip.startMs) / 1000.0;
        return grabVideoFrame(clip.path, std::max(0.0, seconds), width);
    });
}

}  // namespace aquatrack::media
