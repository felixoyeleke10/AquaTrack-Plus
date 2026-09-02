#pragma once

#include "domain/ScientificTypes.h"

#include <optional>
#include <span>

namespace aquatrack::domain {

class IObservationRepository {
public:
  virtual ~IObservationRepository() = default;
  virtual bool saveSurvey(const Survey &survey, std::string &error) = 0;
  virtual bool saveObservations(std::span<const Observation> observations,
                                std::string &error) = 0;
  virtual bool
  replaceObservationsForVideo(const VideoId &id,
                              std::span<const Observation> observations,
                              std::string &error) = 0;
  [[nodiscard]] virtual std::optional<Survey>
  survey(const SurveyId &id) const = 0;
  [[nodiscard]] virtual std::vector<Observation>
  observationsForVideo(const VideoId &id) const = 0;
  [[nodiscard]] virtual bool containsVideo(const VideoId &id) const = 0;
};

} // namespace aquatrack::domain
