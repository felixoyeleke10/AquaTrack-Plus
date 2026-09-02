#include "domain/AnnotationSession.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace aquatrack::domain {
AnnotationSession::AnnotationSession(std::vector<Observation> observations)
    : m_observations(std::move(observations)) {
  if (std::any_of(m_observations.cbegin(), m_observations.cend(),
                  [](const auto &value) { return !isValid(value); }))
    throw std::invalid_argument(
        "annotation session contains invalid observations");
}
void AnnotationSession::checkpoint() {
  m_undo.push_back(m_observations);
  m_redo.clear();
}
Observation *AnnotationSession::find(const ObservationId &id) {
  const auto found =
      std::find_if(m_observations.begin(), m_observations.end(),
                   [&id](const auto &value) { return value.id == id; });
  return found == m_observations.end() ? nullptr : &*found;
}
bool AnnotationSession::relabel(const ObservationId &id, std::string className,
                                float confidence) {
  auto *value = find(id);
  if (!value || className.empty() || !std::isfinite(confidence) ||
      confidence < 0 || confidence > 1)
    return false;
  checkpoint();
  value = find(id);
  value->className = std::move(className);
  value->confidence = confidence;
  value->humanVerified = true;
  return true;
}
bool AnnotationSession::updateBox(const ObservationId &id, BoundingBox box) {
  auto *value = find(id);
  if (!value || box.area() <= 0)
    return false;
  checkpoint();
  value = find(id);
  value->box = box;
  value->humanVerified = true;
  return true;
}
bool AnnotationSession::erase(const ObservationId &id) {
  if (!find(id))
    return false;
  checkpoint();
  std::erase_if(m_observations,
                [&id](const auto &value) { return value.id == id; });
  return true;
}
bool AnnotationSession::mergeTracks(const VideoId &videoId, LocalTrackId source,
                                    LocalTrackId target,
                                    std::int64_t fromFrame) {
  if (videoId.empty() || source <= 0 || target <= 0 || source == target ||
      fromFrame < 0)
    return false;
  const bool exists = std::any_of(
      m_observations.cbegin(), m_observations.cend(), [&](const auto &value) {
        return value.videoId == videoId && value.localTrackId == source &&
               value.frameIndex >= fromFrame;
      });
  if (!exists)
    return false;
  checkpoint();
  for (auto &value : m_observations)
    if (value.videoId == videoId && value.localTrackId == source &&
        value.frameIndex >= fromFrame) {
      value.localTrackId = target;
      value.humanVerified = true;
    }
  return true;
}
bool AnnotationSession::splitTrack(const VideoId &videoId, LocalTrackId source,
                                   LocalTrackId replacement,
                                   std::int64_t fromFrame) {
  return mergeTracks(videoId, source, replacement, fromFrame);
}
bool AnnotationSession::undo() {
  if (m_undo.empty())
    return false;
  m_redo.push_back(std::move(m_observations));
  m_observations = std::move(m_undo.back());
  m_undo.pop_back();
  return true;
}
bool AnnotationSession::redo() {
  if (m_redo.empty())
    return false;
  m_undo.push_back(std::move(m_observations));
  m_observations = std::move(m_redo.back());
  m_redo.pop_back();
  return true;
}
} // namespace aquatrack::domain
