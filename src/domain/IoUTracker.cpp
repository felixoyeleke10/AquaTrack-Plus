#include "domain/IoUTracker.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <tuple>

namespace aquatrack {

float intersectionOverUnion(const BoundingBox& a, const BoundingBox& b)
{
    const float left = std::max(a.x1, b.x1);
    const float top = std::max(a.y1, b.y1);
    const float right = std::min(a.x2, b.x2);
    const float bottom = std::min(a.y2, b.y2);
    const float intersection = std::max(0.0F, right - left) * std::max(0.0F, bottom - top);
    const float unionArea = a.area() + b.area() - intersection;
    return unionArea > 0.0F ? intersection / unionArea : 0.0F;
}

IoUTracker::IoUTracker(float matchThreshold, int maxMissed, int minimumHits)
    : m_matchThreshold(matchThreshold),
      m_maxMissed(maxMissed),
      m_minimumHits(minimumHits)
{
}

std::vector<Track> IoUTracker::update(const std::vector<Detection>& detections)
{
    using Candidate = std::tuple<float, std::size_t, std::size_t>;
    std::vector<Candidate> candidates;
    for (std::size_t trackIndex = 0; trackIndex < m_tracks.size(); ++trackIndex) {
        for (std::size_t detectionIndex = 0; detectionIndex < detections.size(); ++detectionIndex) {
            if (m_tracks[trackIndex].detection.classId != detections[detectionIndex].classId)
                continue;
            const float score = intersectionOverUnion(
                m_tracks[trackIndex].detection.box, detections[detectionIndex].box);
            if (score >= m_matchThreshold)
                candidates.emplace_back(score, trackIndex, detectionIndex);
        }
    }
    std::sort(candidates.begin(), candidates.end(), std::greater<>());
    std::vector<bool> usedTracks(m_tracks.size(), false);
    std::vector<bool> usedDetections(detections.size(), false);
    for (const auto& [score, trackIndex, detectionIndex] : candidates) {
        (void)score;
        if (usedTracks[trackIndex] || usedDetections[detectionIndex])
            continue;
        Track& track = m_tracks[trackIndex];
        track.detection = detections[detectionIndex];
        track.hits += 1;
        track.missed = 0;
        track.confirmed = track.hits >= m_minimumHits;
        usedTracks[trackIndex] = true;
        usedDetections[detectionIndex] = true;
    }
    for (std::size_t index = 0; index < m_tracks.size(); ++index) {
        if (!usedTracks[index])
            m_tracks[index].missed += 1;
    }
    std::erase_if(m_tracks, [this](const Track& track) { return track.missed > m_maxMissed; });
    for (std::size_t index = 0; index < detections.size(); ++index) {
        if (usedDetections[index])
            continue;
        m_tracks.push_back(Track{
            .id = m_nextId++,
            .detection = detections[index],
            .hits = 1,
            .missed = 0,
            .confirmed = m_minimumHits <= 1,
        });
    }
    std::vector<Track> visible;
    std::copy_if(m_tracks.begin(), m_tracks.end(), std::back_inserter(visible),
                 [](const Track& track) { return track.missed == 0; });
    return visible;
}

}  // namespace aquatrack
