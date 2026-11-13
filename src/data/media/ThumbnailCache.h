#pragma once

#include <QImage>
#include <QString>

namespace aquatrack::media {

// Thumbnails kept on disk in the application's cache folder, keyed by the
// media path, its modification time, the frame and the width. The first
// request decodes (ffmpeg for video), later ones read a small JPEG, so the
// dashboard can show posters as soon as it opens. Blocking: call from a pool
// thread. A null image means the media could not be read.

// The media's representative frame (as mediaThumbnail chooses it).
[[nodiscard]] QImage cachedMediaThumbnail(const QString& path, int width);

// A specific 1-based frame, e.g. where review last stopped.
[[nodiscard]] QImage cachedFrameThumbnail(const QString& path, int frame, int width);

}  // namespace aquatrack::media
