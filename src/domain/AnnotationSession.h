#pragma once

#include "domain/ScientificTypes.h"

#include <string>
#include <vector>

namespace aquatrack::domain {
class AnnotationSession final {
public:
  explicit AnnotationSession(std::vector<Observation> observations = {});
  [[nodiscard]] const std::vector<Observation> &observations() const {
    return m_observations;
  }
  [[nodiscard]] bool relabel(const ObservationId &id, std::string className,
                             float confidence);
  [[nodiscard]] bool updateBox(const ObservationId &id, BoundingBox box);
  [[nodiscard]] bool erase(const ObservationId &id);
  [[nodiscard]] bool mergeTracks(const VideoId &videoId, LocalTrackId source,
                                 LocalTrackId target, std::int64_t fromFrame);
  [[nodiscard]] bool splitTrack(const VideoId &videoId, LocalTrackId source,
                                LocalTrackId replacement,
                                std::int64_t fromFrame);
  [[nodiscard]] bool canUndo() const { return !m_undo.empty(); }
  [[nodiscard]] bool canRedo() const { return !m_redo.empty(); }
  bool undo();
  bool redo();

private:
  void checkpoint();
  Observation *find(const ObservationId &id);
  std::vector<Observation> m_observations;
  std::vector<std::vector<Observation>> m_undo;
  std::vector<std::vector<Observation>> m_redo;
};
} // namespace aquatrack::domain
