#pragma once

#include "data/media/MediaTools.h"

#include <QString>
#include <QStringList>
#include <QVector>

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
};

struct SpeciesCount {
    QString name;
    int tracks = 0;        // individuals: distinct tracks labelled this species
    int observations = 0;  // boxes
};

struct ProjectSummary {
    QVector<MediaProgress> media;  // in project order
    QVector<SpeciesCount> species;  // most individuals first
    int frames = 0;
    int annotatedFrames = 0;
    int observations = 0;
    int verified = 0;
    int tracks = 0;

    [[nodiscard]] double verifiedRatio() const
    {
        return observations > 0 ? double(verified) / observations : 0.0;
    }
};

// Reads each media item's metadata (ffprobe for video) and the observation
// store. Blocking: run it off the UI thread.
[[nodiscard]] ProjectSummary summarizeProject(const QStringList& mediaPaths,
                                              const QString& observationStore);

}  // namespace aquatrack::application
