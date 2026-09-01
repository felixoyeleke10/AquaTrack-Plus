#pragma once

#include "domain/DetectionTypes.h"

#include <vector>

namespace aquatrack {

[[nodiscard]] float intersectionOverUnion(const BoundingBox& a, const BoundingBox& b);

class IoUTracker {
public:
    IoUTracker(float matchThreshold = 0.3F, int maxMissed = 20, int minimumHits = 3);
    [[nodiscard]] std::vector<Track> update(const std::vector<Detection>& detections);
    [[nodiscard]] const std::vector<Track>& tracks() const { return m_tracks; }

private:
    float m_matchThreshold;
    int m_maxMissed;
    int m_minimumHits;
    int m_nextId = 1;
    std::vector<Track> m_tracks;
};

}  // namespace aquatrack
