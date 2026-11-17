#include "application/DetectionMerge.h"

#include "domain/IoUTracker.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <unordered_set>

namespace aquatrack::application {

DetectionMerge mergeDetections(const std::vector<domain::Observation>& existing,
                               std::vector<domain::Observation> detections, float overlap)
{
    DetectionMerge merge;

    // What the reviewer confirmed stays exactly as it is.
    std::map<std::int64_t, std::vector<const domain::Observation*>> verifiedByFrame;
    std::map<domain::LocalTrackId, std::set<std::int64_t>> verifiedFrames;  // track -> frames
    domain::LocalTrackId highestKept = 0;
    for (const auto& observation : existing) {
        if (!observation.humanVerified) {
            ++merge.replaced;
            continue;
        }
        merge.observations.push_back(observation);
        ++merge.keptVerified;
        verifiedByFrame[observation.frameIndex].push_back(&observation);
        verifiedFrames[observation.localTrackId].insert(observation.frameIndex);
        highestKept = std::max(highestKept, observation.localTrackId);
    }

    // New boxes the verified ones already cover; for each new track, which
    // verified tracks those boxes matched.
    std::map<domain::LocalTrackId, std::set<domain::LocalTrackId>> matchedTracks;
    std::vector<domain::Observation> fresh;
    for (auto& detection : detections) {
        const domain::Observation* best = nullptr;
        float bestOverlap = overlap;
        if (const auto found = verifiedByFrame.find(detection.frameIndex);
            found != verifiedByFrame.end()) {
            for (const auto* verified : found->second) {
                const float value = aquatrack::intersectionOverUnion(verified->box, detection.box);
                if (value >= bestOverlap) {
                    bestOverlap = value;
                    best = verified;
                }
            }
        }
        if (best) {
            ++merge.duplicates;
            matchedTracks[detection.localTrackId].insert(best->localTrackId);
            continue;
        }
        fresh.push_back(std::move(detection));
    }

    // Frames each new track still has boxes on.
    std::map<domain::LocalTrackId, std::set<std::int64_t>> freshFrames;
    for (const auto& detection : fresh)
        freshFrames[detection.localTrackId].insert(detection.frameIndex);

    // A new track that followed one verified fish continues its ID, unless
    // that would put two boxes of one track on a frame. Two new tracks may
    // not both take over the same verified track.
    std::map<domain::LocalTrackId, domain::LocalTrackId> renumber;
    std::set<domain::LocalTrackId> continued;
    for (const auto& [track, frames] : freshFrames) {
        const auto matched = matchedTracks.find(track);
        if (matched == matchedTracks.end() || matched->second.size() != 1)
            continue;
        const domain::LocalTrackId target = *matched->second.begin();
        if (continued.contains(target))
            continue;
        const auto& taken = verifiedFrames[target];
        const bool clash = std::any_of(frames.begin(), frames.end(),
                                       [&taken](std::int64_t frame) { return taken.contains(frame); });
        if (clash)
            continue;
        renumber[track] = target;
        continued.insert(target);
        ++merge.continuedTracks;
    }
    // Everything else moves above the kept IDs, in the detector's order.
    domain::LocalTrackId next = highestKept;
    for (const auto& [track, frames] : freshFrames)
        if (!renumber.contains(track))
            renumber[track] = highestKept > 0 ? ++next : track;

    std::unordered_set<std::string> ids;
    for (const auto& observation : merge.observations)
        ids.insert(observation.id);
    for (auto& detection : fresh) {
        detection.localTrackId = renumber[detection.localTrackId];
        const std::string base = detection.videoId + ":" + std::to_string(detection.frameIndex) +
                                 ":" + std::to_string(detection.localTrackId);
        std::string id = base;
        for (int suffix = 2; ids.contains(id); ++suffix)
            id = base + "#" + std::to_string(suffix);
        ids.insert(id);
        detection.id = std::move(id);
        merge.observations.push_back(std::move(detection));
        ++merge.added;
    }

    std::sort(merge.observations.begin(), merge.observations.end(),
              [](const domain::Observation& left, const domain::Observation& right) {
                  return left.frameIndex != right.frameIndex ? left.frameIndex < right.frameIndex
                                                             : left.localTrackId < right.localTrackId;
              });
    return merge;
}

}  // namespace aquatrack::application
