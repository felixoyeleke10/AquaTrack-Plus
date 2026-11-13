#pragma once

#include "data/media/VideoProbe.h"
#include "domain/ScientificTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <vector>

namespace aquatrack::application {

// Annotations of a video sequence are stored against global frame numbers
// (1-based, continuous across clips). Editing the sequence - reordering,
// adding or removing clips - moves where each clip starts, so every
// observation must be moved with the clip it was drawn on.
struct SequenceRemap {
    std::vector<domain::Observation> kept;  // moved onto the new layout
    int dropped = 0;                        // on clips that were removed
    QStringList droppedClips;               // file names of those clips
};

// Clips are matched by file path (case-insensitive, cleaned). An observation
// keeps its position within its clip; its frame number and timestamp are
// recomputed exactly as MediaSource numbers the new sequence. Observations
// on removed clips, or on frames past a clip's end, are dropped. `videoId`
// is written to every kept observation (it changes when the sequence is
// renamed).
[[nodiscard]] SequenceRemap remapSequenceObservations(
    const std::vector<domain::Observation>& observations, const QVector<media::VideoInfo>& before,
    const QVector<media::VideoInfo>& after, const std::string& videoId);

// The same mapping for a single frame (e.g. where review last stopped);
// 0 when the frame's clip was removed.
[[nodiscard]] int remapSequenceFrame(int frame, const QVector<media::VideoInfo>& before,
                                     const QVector<media::VideoInfo>& after);

}  // namespace aquatrack::application
