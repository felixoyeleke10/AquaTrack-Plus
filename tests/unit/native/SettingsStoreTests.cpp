#include "TestSupport.h"
#include "data/SettingsStore.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <string>
int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QTemporaryDir d;
  aquatrack::system::settings::AquaTrackSettings expected;
  expected.detection.modelId = "fish-v2";
  expected.detection.confidenceThreshold = 0.42;
  expected.pipeline.heavyJobLimit = 2;
  expected.storage.defaultProjectDirectory = "D:/surveys";
  const auto path = d.filePath("settings.json");
  aquatrack::data::SettingsStore store;
  QString error;
  requireTest(store.save(path, expected, &error), "save settings");
  aquatrack::system::settings::AquaTrackSettings actual;
  requireTest(store.load(path, actual, &error), "load settings");
  requireTest(actual.detection.modelId == "fish-v2", "model id");
  requireTest(actual.detection.confidenceThreshold == 0.42, "confidence");
  actual.pipeline.heavyJobLimit = 0;
  requireTest(!store.save(path, actual, &error), "invalid settings rejected");

  // Recent projects round-trip and stay capped.
  aquatrack::system::settings::AquaTrackSettings recents;
  for (std::size_t index = 0;
       index < aquatrack::system::settings::MaxRecentProjects; ++index)
    recents.storage.recentProjects.push_back("D:/surveys/p" +
                                             std::to_string(index) + ".aqp");
  requireTest(store.save(path, recents, &error), "save recent projects");
  aquatrack::system::settings::AquaTrackSettings reloaded;
  requireTest(store.load(path, reloaded, &error), "load recent projects");
  requireTest(reloaded.storage.recentProjects.size() ==
                  aquatrack::system::settings::MaxRecentProjects,
              "recent projects round-trip");
  requireTest(reloaded.storage.recentProjects.front() == "D:/surveys/p0.aqp",
              "recent project order preserved");

  recents.storage.recentProjects.push_back("D:/surveys/overflow.aqp");
  requireTest(!aquatrack::system::settings::isValid(recents),
              "an over-long recent list is rejected");
  return 0;
}
