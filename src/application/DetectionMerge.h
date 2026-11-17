#pragma once

#include "domain/ScientificTypes.h"

#include <vector>

namespace aquatrack::application {

// The outcome of running a detector again over media that has been reviewed.
struct DetectionMerge {
    std::vector<domain::Observation> observations;  // by frame, then track
    int keptVerified = 0;     // reviewer-verified boxes carried over unchanged
    int replaced = 0;         // unverified boxes from the earlier run, dropped
    int added = 0;            // new detections kept
    int duplicates = 0;       // new detections a verified box already covers
    int continuedTracks = 0;  // new tracks that took over a verified track's ID
};

// Keeps every verified box in `existing` and replaces the unverified rest
// with `detections`:
//  - a detection overlapping a verified box on the same frame (IoU >=
//    `overlap`) is the same fish already confirmed, so it is dropped;
//  - a new track whose dropped boxes all matched one verified track carries
//    that track's ID on, so a reviewed fish keeps its identity in the frames
//    the reviewer has not reached yet (unless the two would share a frame);
//  - every other new track gets an ID no kept track uses.
// Ids are rebuilt as "<video>:<frame>:<track>", made unique if needed.
[[nodiscard]] DetectionMerge mergeDetections(const std::vector<domain::Observation>& existing,
                                             std::vector<domain::Observation> detections,
                                             float overlap = 0.5F);

}  // namespace aquatrack::application
