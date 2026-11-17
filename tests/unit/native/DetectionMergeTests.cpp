#include "TestSupport.h"

#include "application/DetectionMerge.h"
#include "application/controllers/ProjectController.h"
#include "data/ProjectFileStore.h"
#include "data/observations/JsonObservationRepository.h"

#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

using aquatrack::application::mergeDetections;
using aquatrack::domain::Observation;

namespace {

Observation box(qint64 frame, qint64 track, float x, bool verified)
{
    Observation item;
    item.id = "clip:" + std::to_string(frame) + ":" + std::to_string(track);
    item.videoId = "clip";
    item.frameIndex = frame;
    item.timestampMs = (frame - 1) * 100;
    item.localTrackId = track;
    item.box = {x, 10, x + 20, 30};
    item.className = "Fish";
    item.confidence = verified ? 1.0F : 0.7F;
    item.humanVerified = verified;
    return item;
}

const Observation* find(const std::vector<Observation>& items, qint64 frame, float x)
{
    for (const auto& item : items)
        if (item.frameIndex == frame && std::abs(item.box.x1 - x) < 0.5F)
            return &item;
    return nullptr;
}

void mergeKeepsReviewWork()
{
    // Reviewed: fish 7 verified on frames 1-2 (at x=10), plus an unverified
    // model box on frame 3 that the new run supersedes.
    const std::vector<Observation> existing{box(1, 7, 10, true), box(2, 7, 10, true),
                                            box(3, 7, 10, false), box(3, 9, 200, false)};
    // New run: track 1 follows fish 7 through frames 1-4; track 2 is a new
    // fish at x=100 on frames 1-2.
    const std::vector<Observation> detections{box(1, 1, 11, false), box(2, 1, 11, false),
                                              box(3, 1, 11, false), box(4, 1, 11, false),
                                              box(1, 2, 100, false), box(2, 2, 100, false)};
    const auto merge = mergeDetections(existing, detections);

    requireTest(merge.keptVerified == 2 && merge.replaced == 2, "verified kept, unverified replaced");
    requireTest(merge.duplicates == 2, "detections on verified boxes are dropped");
    requireTest(merge.added == 4 && merge.observations.size() == 6, "the rest are added");
    requireTest(find(merge.observations, 3, 200) == nullptr, "old unverified boxes are gone");

    const auto* verified = find(merge.observations, 1, 10);
    requireTest(verified && verified->humanVerified && verified->localTrackId == 7 &&
                    verified->confidence == 1.0F,
                "verified boxes are unchanged");
    const auto* continued = find(merge.observations, 4, 11);
    requireTest(continued && continued->localTrackId == 7 && !continued->humanVerified,
                "a track that followed a verified fish keeps its ID");
    requireTest(merge.continuedTracks == 1, "one track continued");
    const auto* fresh = find(merge.observations, 1, 100);
    requireTest(fresh && fresh->localTrackId > 7, "new fish get IDs above the kept ones");

    // One box per track per frame, unique ids.
    std::set<std::pair<qint64, qint64>> occupied;
    std::set<std::string> ids;
    for (const auto& item : merge.observations) {
        requireTest(occupied.insert({item.frameIndex, item.localTrackId}).second,
                    "no track has two boxes on a frame");
        requireTest(ids.insert(item.id).second, "observation ids are unique");
    }
    requireTest(std::is_sorted(merge.observations.begin(), merge.observations.end(),
                               [](const Observation& a, const Observation& b) {
                                   return a.frameIndex < b.frameIndex;
                               }),
                "sorted by frame");
}

void mergeDoesNotDoubleUpAVerifiedTrack()
{
    // Fish 7 verified on frames 1 and 3. The new track matches it on frame 1
    // but sits elsewhere on frame 3: continuing ID 7 would put two boxes of
    // track 7 on frame 3, so it gets a fresh ID instead.
    const std::vector<Observation> existing{box(1, 7, 10, true), box(3, 7, 10, true)};
    const std::vector<Observation> detections{box(1, 4, 10, false), box(3, 4, 300, false)};
    const auto merge = mergeDetections(existing, detections);
    const auto* moved = find(merge.observations, 3, 300);
    requireTest(moved && moved->localTrackId != 7 && merge.continuedTracks == 0,
                "no clash with a verified track's frames");
}

void mergeWithoutReviewKeepsDetectorIds()
{
    const std::vector<Observation> existing{box(1, 3, 10, false)};
    const std::vector<Observation> detections{box(1, 5, 50, false), box(2, 5, 50, false)};
    const auto merge = mergeDetections(existing, detections);
    requireTest(merge.keptVerified == 0 && merge.observations.size() == 2 &&
                    merge.observations.front().localTrackId == 5,
                "with nothing verified the run is taken as it is");
}

void storeRecordsTheDetector(const QDir& root)
{
    const QString path = root.filePath(QStringLiteral("observations.json"));
    std::string error;
    {
        aquatrack::data::JsonObservationRepository repository(path);
        requireTest(repository.open(error), "open store");
        const std::vector<Observation> items{box(1, 1, 10, false)};
        requireTest(repository.replaceObservationsForVideo("clip", items, error), "store boxes");
        requireTest(!repository.detectorFor("clip"), "no detector recorded yet");
        requireTest(repository.setDetector("clip", aquatrack::data::DetectorRun{"yolo-v8", "YOLOv8 fish", 1234}, error),
                    "record the detector");
    }
    aquatrack::data::JsonObservationRepository reopened(path);
    requireTest(reopened.open(error), "reopen store");
    const auto run = reopened.detectorFor("clip");
    requireTest(run && run->modelId == "yolo-v8" && run->modelName == "YOLOv8 fish" && run->atMs == 1234,
                "the detector survives a reload");
    // Editing the boxes keeps the record; forgetting the video drops it.
    requireTest(reopened.replaceObservationsForVideo("clip", {}, error) && reopened.detectorFor("clip"),
                "edits keep the record");
    requireTest(reopened.setDetector("clip", std::nullopt, error) && !reopened.detectorFor("clip"),
                "the record can be cleared");
    requireTest(reopened.setDetector("clip", aquatrack::data::DetectorRun{"a", "A", 1}, error) &&
                    reopened.removeVideo("clip", error) && !reopened.detectorFor("clip"),
                "removing the video drops the record");
}

void projectRemembersItsDetector(const QDir& root)
{
    const QString path = root.filePath(QStringLiteral("survey.aquatrack"));
    QString error;
    {
        aquatrack::application::ProjectController controller;
        requireTest(controller.createProject(path, QStringLiteral("Survey"), &error), "create project");
        requireTest(controller.addMedia(root.filePath(QStringLiteral("clip.mp4")), &error),
                    "an unsaved media edit");
        requireTest(controller.setDetector(QStringLiteral("yolo-v8"), &error), "choose a detector");
        requireTest(controller.project()->detectorModelId == "yolo-v8", "in memory");
        requireTest(controller.isDirty(), "the media edit is still unsaved");
    }
    aquatrack::data::ProjectFileStore store;
    aquatrack::domain::ProjectManifest manifest;
    requireTest(store.load(path, manifest, &error), "load project");
    requireTest(manifest.detectorModelId == "yolo-v8", "the choice is saved at once");
    requireTest(manifest.mediaPaths.empty(), "without saving unrelated unsaved edits");
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    requireTest(temporary.isValid(), "temporary directory");
    const QDir root(temporary.path());
    mergeKeepsReviewWork();
    mergeDoesNotDoubleUpAVerifiedTrack();
    mergeWithoutReviewKeepsDetectorIds();
    storeRecordsTheDetector(root);
    projectRemembersItsDetector(root);
    return 0;
}
