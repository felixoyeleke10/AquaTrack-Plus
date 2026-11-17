#pragma once

#include "data/media/MediaTools.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <array>

namespace aquatrack::application {

// Review progress of one media item, as Review would show it: reviewed edits
// from the project's observation store, or, for an image sequence never
// opened in Review, the ground truth that ships with it.
struct MediaProgress {
    QString path;
    QString name;
    media::MediaKind kind = media::MediaKind::Missing;
    bool available = false;  // exists and its metadata could be read
    int frames = 0;
    qint64 durationMs = 0;
    int clips = 0;
    int observations = 0;
    int verified = 0;
    int annotatedFrames = 0;
    int tracks = 0;
    QString detector;       // model behind its detections, if recorded
    qint64 detectedAtMs = 0;
};

struct SpeciesCount {
    QString name;
    int tracks = 0;        // individuals: distinct tracks labelled this species
    int observations = 0;  // boxes
};

// Where a piece of footage is in the survey workflow.
enum class FootageState {
    NotAnalysed,  // never run through detection or annotated
    Detected,     // analysed (possibly finding nothing), nothing verified yet
    InReview,     // some boxes verified
    Verified,     // every box verified
};
constexpr int kFootageStates = 4;

// Project totals count footage once. The same video can be in a project
// several times (on its own, and as a clip of one or more sequences); each
// video file - or image sequence - is one piece of footage, and its figures
// come from whichever media item has reviewed it best (most verified boxes,
// then most boxes). Per-item figures are in `media`, unchanged.
struct ProjectSummary {
    QVector<MediaProgress> media;   // in project order
    QVector<SpeciesCount> species;  // most individuals first, footage counted once
    int footage = 0;                // distinct videos and image sequences
    qint64 durationMs = 0;
    int frames = 0;
    int annotatedFrames = 0;
    int observations = 0;
    int verified = 0;
    int tracks = 0;
    // Footage duration in each FootageState, indexed by the enum.
    std::array<qint64, kFootageStates> stateMs{};
    // True when some footage is in more than one media item.
    bool overlapping = false;

    [[nodiscard]] double verifiedRatio() const
    {
        return observations > 0 ? double(verified) / observations : 0.0;
    }
    [[nodiscard]] qint64 msIn(FootageState state) const { return stateMs[std::size_t(state)]; }
};

// Reads each media item's metadata (ffprobe for video) and the observation
// store. Blocking: run it off the UI thread.
[[nodiscard]] ProjectSummary summarizeProject(const QStringList& mediaPaths,
                                              const QString& observationStore);

}  // namespace aquatrack::application
