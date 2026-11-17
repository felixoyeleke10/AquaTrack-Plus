#include "application/ProjectSummary.h"

#include "data/BrackishMotSequence.h"
#include "data/media/MediaSource.h"
#include "data/observations/JsonObservationRepository.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <set>
#include <string>
#include <tuple>
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

// One piece of footage inside a media item: a video file (a clip of a
// sequence, or the item itself) or a whole image sequence, with the frames
// it occupies on the item's timeline.
struct Segment {
    QString key;  // identifies the footage across media items
    int firstFrame = 1;
    int frameCount = 0;
    qint64 durationMs = 0;
};

// What one media item recorded about one piece of footage.
struct Coverage {
    int mediaIndex = 0;
    Segment segment;
    // The item has been through detection or annotation: footage with no
    // boxes was looked at and found empty, not skipped.
    bool analysed = false;
    int observations = 0;
    int verified = 0;
    int annotatedFrames = 0;
    QSet<QString> tracks;                     // "<media>#<track>"
    QHash<QString, QSet<QString>> individuals;  // species -> tracks
    QHash<QString, int> boxesPerSpecies;
};

QString footageKey(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toLower();
}

QVector<Segment> segmentsOf(const media::MediaSource& source)
{
    QVector<Segment> segments;
    for (const auto& clip : source.clips())
        segments.push_back({footageKey(clip.path), clip.firstFrame, clip.frameCount, clip.durationMs});
    if (segments.isEmpty() && source.frameCount() > 0)
        segments.push_back({footageKey(source.path()), 1, source.frameCount(), source.durationMs()});
    return segments;
}

FootageState stateOf(const Coverage& coverage)
{
    if (coverage.observations == 0)
        return coverage.analysed ? FootageState::Detected : FootageState::NotAnalysed;
    if (coverage.verified == 0)
        return FootageState::Detected;
    return coverage.verified < coverage.observations ? FootageState::InReview
                                                     : FootageState::Verified;
}

}  // namespace

ProjectSummary summarizeProject(const QStringList& mediaPaths, const QString& observationStore)
{
    ProjectSummary summary;
    data::JsonObservationRepository repository(observationStore);
    std::string error;
    const bool haveStore = !observationStore.isEmpty() && repository.open(error);

    // Every (media item, piece of footage) pair, grouped by footage.
    QHash<QString, QVector<Coverage>> coverageByFootage;
    QStringList footageOrder;

    for (int mediaIndex = 0; mediaIndex < mediaPaths.size(); ++mediaIndex) {
        const QString& path = mediaPaths.at(mediaIndex);
        MediaProgress item;
        item.path = path;
        item.name = media::MediaSource::reviewIdFor(path);
        item.kind = media::classifyMedia(path);

        media::MediaSource source;
        QVector<Segment> segments;
        if (item.kind != media::MediaKind::Missing && source.open(path)) {
            item.available = true;
            item.name = source.name();
            item.frames = source.frameCount();
            item.durationMs = source.durationMs();
            item.clips = std::max(1, int(source.clips().size()));
            segments = segmentsOf(source);
        } else if (item.name.isEmpty()) {
            item.name = QFileInfo(path).completeBaseName();
        }

        QVector<Box> boxes;
        const std::string videoId = item.name.toStdString();
        // Recorded in the store (detected, imported or edited), or an image
        // sequence with its own ground truth.
        bool analysed = false;
        if (haveStore && repository.containsVideo(videoId)) {
            analysed = true;
            if (const auto run = repository.detectorFor(videoId)) {
                item.detector = QString::fromStdString(
                    run->modelName.empty() ? run->modelId : run->modelName);
                item.detectedAtMs = run->atMs;
            }
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
            analysed = !boxes.isEmpty();
        }

        // The item's own figures, for its card.
        std::set<qint64> frames;
        std::set<qint64> tracks;
        for (const Box& box : boxes) {
            frames.insert(box.frame);
            tracks.insert(box.track);
            if (box.verified)
                ++item.verified;
        }
        item.observations = int(boxes.size());
        item.annotatedFrames = int(frames.size());
        item.tracks = int(tracks.size());

        // The same figures split by the footage they fall on.
        QVector<Coverage> covered;
        for (const Segment& segment : segments)
            covered.push_back({mediaIndex, segment, analysed});
        QVector<std::set<qint64>> framesPerSegment(covered.size());
        for (const Box& box : boxes) {
            int index = -1;
            for (int i = 0; i < covered.size(); ++i) {
                const Segment& segment = covered[i].segment;
                if (box.frame >= segment.firstFrame && box.frame < segment.firstFrame + segment.frameCount) {
                    index = i;
                    break;
                }
            }
            if (index < 0)
                continue;
            Coverage& coverage = covered[index];
            ++coverage.observations;
            if (box.verified)
                ++coverage.verified;
            framesPerSegment[index].insert(box.frame);
            const QString track = item.name + QLatin1Char('#') + QString::number(box.track);
            coverage.tracks.insert(track);
            const QString species = box.species.isEmpty() ? QStringLiteral("Unlabelled") : box.species;
            coverage.individuals[species].insert(track);
            ++coverage.boxesPerSpecies[species];
        }
        for (int i = 0; i < covered.size(); ++i) {
            covered[i].annotatedFrames = int(framesPerSegment[i].size());
            const QString& key = covered[i].segment.key;
            if (!coverageByFootage.contains(key))
                footageOrder << key;
            coverageByFootage[key].push_back(std::move(covered[i]));
        }
        summary.media.push_back(std::move(item));
    }

    // Each piece of footage once, from the media item that reviewed it best.
    QSet<QString> tracks;
    QHash<QString, QSet<QString>> individuals;
    QHash<QString, int> boxesPerSpecies;
    for (const QString& key : footageOrder) {
        const auto& candidates = coverageByFootage[key];
        if (candidates.size() > 1)
            summary.overlapping = true;
        const Coverage& best = *std::max_element(
            candidates.cbegin(), candidates.cend(), [](const Coverage& left, const Coverage& right) {
                // Analysed beats untouched when neither has boxes; remaining
                // ties go to the item listed first in the project.
                return std::make_tuple(left.verified, left.observations, left.analysed, -left.mediaIndex) <
                       std::make_tuple(right.verified, right.observations, right.analysed,
                                       -right.mediaIndex);
            });
        ++summary.footage;
        summary.frames += best.segment.frameCount;
        summary.durationMs += best.segment.durationMs;
        summary.observations += best.observations;
        summary.verified += best.verified;
        summary.annotatedFrames += best.annotatedFrames;
        summary.stateMs[std::size_t(stateOf(best))] += best.segment.durationMs;
        tracks.unite(best.tracks);
        for (auto it = best.individuals.cbegin(); it != best.individuals.cend(); ++it)
            individuals[it.key()].unite(it.value());
        for (auto it = best.boxesPerSpecies.cbegin(); it != best.boxesPerSpecies.cend(); ++it)
            boxesPerSpecies[it.key()] += it.value();
    }
    summary.tracks = int(tracks.size());

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
