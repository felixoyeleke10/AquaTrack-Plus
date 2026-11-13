#include "application/ProjectSummary.h"

#include "data/BrackishMotSequence.h"
#include "data/media/MediaSource.h"
#include "data/observations/JsonObservationRepository.h"

#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <set>
#include <string>
#include <utility>

namespace aquatrack::application {
namespace {

// What one observation contributes, whichever source it came from.
struct Box {
    qint64 frame = 0;
    qint64 track = 0;
    QString species;
    bool verified = false;
};

}  // namespace

ProjectSummary summarizeProject(const QStringList& mediaPaths, const QString& observationStore)
{
    ProjectSummary summary;
    data::JsonObservationRepository repository(observationStore);
    std::string error;
    const bool haveStore = !observationStore.isEmpty() && repository.open(error);

    // Individuals per species across the project: (media, track) pairs.
    QHash<QString, QSet<QString>> individuals;
    QHash<QString, int> boxesPerSpecies;

    for (const QString& path : mediaPaths) {
        MediaProgress item;
        item.path = path;
        item.name = media::MediaSource::reviewIdFor(path);
        item.kind = media::classifyMedia(path);

        media::MediaSource source;
        if (item.kind != media::MediaKind::Missing && source.open(path)) {
            item.available = true;
            item.name = source.name();
            item.frames = source.frameCount();
            item.durationMs = source.durationMs();
            item.clips = std::max(1, int(source.clips().size()));
        } else if (item.name.isEmpty()) {
            item.name = QFileInfo(path).completeBaseName();
        }

        QVector<Box> boxes;
        const std::string videoId = item.name.toStdString();
        if (haveStore && repository.containsVideo(videoId)) {
            for (const auto& observation : repository.observationsForVideo(videoId))
                boxes.push_back({observation.frameIndex, observation.localTrackId,
                                 QString::fromStdString(observation.className),
                                 observation.humanVerified});
        } else if (item.available && source.kind() == media::MediaKind::ImageSequence) {
            const auto& sequence = source.imageSequence();
            for (int frame = 1; frame <= sequence.frameCount(); ++frame)
                for (const auto& truth : sequence.annotations(frame))
                    boxes.push_back({frame, truth.trackId,
                                     BrackishMotSequence::className(truth.classId), false});
        }

        std::set<qint64> frames;
        std::set<qint64> tracks;
        for (const Box& box : boxes) {
            frames.insert(box.frame);
            tracks.insert(box.track);
            if (box.verified)
                ++item.verified;
            const QString species = box.species.isEmpty() ? QStringLiteral("Unlabelled") : box.species;
            individuals[species].insert(item.name + QLatin1Char('#') + QString::number(box.track));
            ++boxesPerSpecies[species];
        }
        item.observations = int(boxes.size());
        item.annotatedFrames = int(frames.size());
        item.tracks = int(tracks.size());

        summary.frames += item.frames;
        summary.annotatedFrames += item.annotatedFrames;
        summary.observations += item.observations;
        summary.verified += item.verified;
        summary.tracks += item.tracks;
        summary.media.push_back(std::move(item));
    }

    for (auto it = individuals.cbegin(); it != individuals.cend(); ++it)
        summary.species.push_back({it.key(), int(it.value().size()), boxesPerSpecies.value(it.key())});
    std::sort(summary.species.begin(), summary.species.end(),
              [](const SpeciesCount& left, const SpeciesCount& right) {
                  return left.tracks != right.tracks ? left.tracks > right.tracks
                                                     : left.name < right.name;
              });
    return summary;
}

}  // namespace aquatrack::application
