#pragma once

#include "data/media/VideoProbe.h"

#include <QString>
#include <QVector>

namespace aquatrack::media {

// Several video files reviewed as one continuous sequence, in play order.
//
// Stored as <folder>/sequence.json. The videos are referenced, never copied
// or converted: a path inside the sequence folder's project is stored
// relative to the folder, anything else absolute. Probe results are cached
// with each clip so opening a long sequence does not re-run ffprobe on every
// file; a clip whose size or date changed is probed again.
//
//   {
//     "format": "aquatrack-video-sequence", "version": 1,
//     "name": "Transect A",
//     "clips": [ { "path": "../video_045.mp4", "frames": 1067,
//                  "frameRate": 29.97, "width": 1920, "height": 1080,
//                  "durationMs": 35602, "bytes": 72646371,
//                  "modified": "2025-09-01T15:12:00Z" }, ... ]
//   }
//
// Saving also writes README.txt beside it: the same clips for people, and
// how to open the folder (unless a README.txt not written by AquaTrack+ is
// already there).
struct VideoSequenceManifest {
    QString name;
    // Absolute paths after load(); probe data may be stale until refresh().
    QVector<VideoInfo> clips;

    [[nodiscard]] static QString manifestPath(const QString& folder);
    [[nodiscard]] bool load(const QString& folder, QString* error = nullptr);
    [[nodiscard]] bool save(const QString& folder, QString* error = nullptr) const;
    // Re-probes clips whose file changed or was never probed. Returns false
    // (with the first problem) when a clip is missing or undecodable.
    // `changed` reports whether any cached probe data was updated.
    [[nodiscard]] bool refresh(QString* error = nullptr, bool* changed = nullptr);

    [[nodiscard]] int totalFrames() const;
    [[nodiscard]] qint64 totalDurationMs() const;
};

}  // namespace aquatrack::media
