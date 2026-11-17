#include "TestSupport.h"
#include "data/SettingsStore.h"
#include <QCoreApplication>
#include <QFile>
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

  // Review's panel heights round-trip, collapsed included.
  aquatrack::system::settings::AquaTrackSettings layout;
  requireTest(layout.layout.reviewLogHeight == 200 && layout.layout.reviewActivityHeight == 200,
              "the panels start at 200 px");
  for (int height : {340, 0}) {
    layout.layout.reviewLogHeight = height;
    layout.layout.reviewActivityHeight = height + 20;
    requireTest(store.save(path, layout, &error), "save the panel heights");
    aquatrack::system::settings::AquaTrackSettings back;
    requireTest(store.load(path, back, &error) && back.layout.reviewLogHeight == height &&
                    back.layout.reviewActivityHeight == height + 20,
                "the panel heights round-trip");
  }
  // Folded sections round-trip in order.
  layout.layout.foldedSections = {"summary", "log"};
  requireTest(store.save(path, layout, &error), "save folded sections");
  {
    aquatrack::system::settings::AquaTrackSettings back;
    requireTest(store.load(path, back, &error) && back.layout.foldedSections == layout.layout.foldedSections,
                "folded sections round-trip");
  }
  // A hand-edited, out-of-range height is brought into range; it does not
  // make the whole settings file unreadable.
  {
    QFile file(path);
    requireTest(file.open(QIODevice::ReadOnly), "read settings back");
    QByteArray json = file.readAll();
    file.close();
    json.replace("\"reviewLogHeight\": 0", "\"reviewLogHeight\": -50");
    requireTest(json.contains("-50"), "the height was edited");
    requireTest(file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                    file.write(json) == json.size(),
                "write an out-of-range height");
    file.close();
    aquatrack::system::settings::AquaTrackSettings clamped;
    requireTest(store.load(path, clamped, &error) && clamped.layout.reviewLogHeight == 0,
                "an out-of-range height is clamped");
  }
  return 0;
}
