#include "TestSupport.h"
#include "data/observations/JsonObservationRepository.h"

#include <QTemporaryDir>

int main() {
  QTemporaryDir temporary;
  requireTest(temporary.isValid(), "temporary directory");
  const QString path = temporary.filePath(QStringLiteral("observations.json"));
  std::string error;
  aquatrack::data::JsonObservationRepository repository(path);
  requireTest(repository.open(error), "open empty repository");
  aquatrack::domain::Survey survey{
      "survey-1", "Harbour survey", 100, "Holyrood", {"video-1"}};
  requireTest(repository.saveSurvey(survey, error), "save survey");
  aquatrack::domain::Observation observation{
      "obs-1",      "video-1",      40,     1,     7,
      std::nullopt, {1, 2, 20, 30}, "Fish", 0.85F, false};
  requireTest(repository.saveObservations({&observation, 1}, error),
              "save observation");
  observation.confidence = 0.9F;
  requireTest(repository.saveObservations({&observation, 1}, error),
              "idempotent observation upsert");
  aquatrack::data::JsonObservationRepository reopened(path);
  requireTest(reopened.open(error), "reopen repository");
  requireTest(reopened.survey("survey-1").has_value(), "survey round trip");
  const auto observations = reopened.observationsForVideo("video-1");
  requireTest(observations.size() == 1 && observations[0].confidence == 0.9F,
              "observation upsert round trip");
  requireTest(reopened.replaceObservationsForVideo("video-1", {}, error),
              "replace video observations with empty set");
  requireTest(reopened.observationsForVideo("video-1").empty(),
              "persist observation deletion");
  aquatrack::data::JsonObservationRepository emptyReopened(path);
  requireTest(emptyReopened.open(error), "reopen empty video record");
  requireTest(emptyReopened.containsVideo("video-1") &&
                  emptyReopened.observationsForVideo("video-1").empty(),
              "distinguish saved empty video from absent video");
  requireTest(emptyReopened.removeVideo("video-1", error) &&
                  !emptyReopened.containsVideo("video-1"),
              "removing a video forgets that it was reviewed");
  aquatrack::data::JsonObservationRepository removedReopened(path);
  requireTest(removedReopened.open(error) && !removedReopened.containsVideo("video-1"),
              "video removal is persisted");
  observation.confidence = 2.0F;
  requireTest(!reopened.saveObservations({&observation, 1}, error),
              "reject invalid observation");
  return 0;
}
