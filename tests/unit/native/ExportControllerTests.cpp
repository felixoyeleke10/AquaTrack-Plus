#include "TestSupport.h"
#include "application/controllers/ExportController.h"

#include <QFile>
#include <QTemporaryDir>

int main() {
  QTemporaryDir temporary;
  requireTest(temporary.isValid(), "temporary directory");
  const aquatrack::domain::Observation observation{
      "obs-1",      "video-1",      40,        1,    7,
      std::nullopt, {1, 2, 20, 30}, "Pollock", 0.9F, true};
  aquatrack::application::ExportController controller;
  QString error;
  const QString csvPath =
      temporary.filePath(QStringLiteral("observations.csv"));
  requireTest(controller.exportCsv(csvPath, {observation}, &error),
              "atomic CSV export");
  QFile csv(csvPath);
  requireTest(csv.open(QIODevice::ReadOnly | QIODevice::Text),
              "open CSV export");
  const QByteArray csvBytes = csv.readAll();
  requireTest(csvBytes.contains("observation_id") &&
                  csvBytes.contains("Pollock"),
              "CSV content");
  const QString motPath = temporary.filePath(QStringLiteral("gt.txt"));
  requireTest(controller.exportMot(motPath, {observation}, &error),
              "atomic MOT export");
  QFile mot(motPath);
  requireTest(mot.open(QIODevice::ReadOnly | QIODevice::Text),
              "open MOT export");
  requireTest(mot.readAll().startsWith("2,7,1.000"), "MOT content");
  requireTest(!controller.exportCsv(QString(), {observation}, &error),
              "reject empty export path");
  return 0;
}
