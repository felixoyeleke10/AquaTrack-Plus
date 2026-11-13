#pragma once

#include "domain/IObservationRepository.h"

#include <QString>
#include <map>
#include <set>

namespace aquatrack::data {
class JsonObservationRepository final : public domain::IObservationRepository {
public:
  explicit JsonObservationRepository(QString path);
  [[nodiscard]] bool open(std::string &error);
  bool saveSurvey(const domain::Survey &survey, std::string &error) override;
  bool saveObservations(std::span<const domain::Observation> observations,
                        std::string &error) override;
  bool
  replaceObservationsForVideo(const domain::VideoId &id,
                              std::span<const domain::Observation> observations,
                              std::string &error) override;
  [[nodiscard]] std::optional<domain::Survey>
  survey(const domain::SurveyId &id) const override;
  [[nodiscard]] std::vector<domain::Observation>
  observationsForVideo(const domain::VideoId &id) const override;
  [[nodiscard]] bool containsVideo(const domain::VideoId &id) const override;
  // Forgets a video entirely: its observations and the record that it was
  // reviewed (unlike replacing with nothing, which keeps "reviewed, none
  // found"). Used when media is renamed and its annotations move.
  bool removeVideo(const domain::VideoId &id, std::string &error);

private:
  [[nodiscard]] bool persist(std::string &error) const;
  QString m_path;
  std::map<domain::SurveyId, domain::Survey> m_surveys;
  std::map<domain::ObservationId, domain::Observation> m_observations;
  std::set<domain::VideoId> m_videos;
};
} // namespace aquatrack::data
