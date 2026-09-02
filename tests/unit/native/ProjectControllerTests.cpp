#include "TestSupport.h"
#include "application/controllers/ProjectController.h"

#include <QCoreApplication>
#include <QTemporaryDir>

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QTemporaryDir directory;
  requireTest(directory.isValid(), "temporary controller directory");
  aquatrack::application::ProjectController controller;
  QString error;
  requireTest(!controller.saveProject(&error), "cannot save absent project");
  const auto projectPath = directory.filePath(QStringLiteral("survey.aqp"));
  requireTest(controller.createProject(projectPath,
                                       QStringLiteral("Survey 2026"), &error),
              "create project");
  requireTest(controller.observationStorePath() ==
                  directory.filePath(QStringLiteral("observations.json")),
              "project-relative observation store");
  requireTest(controller.addMedia(
                  directory.filePath(QStringLiteral("video.mp4")), &error),
              "add media");
  requireTest(controller.addMedia(
                  directory.filePath(QStringLiteral("video.mp4")), &error),
              "duplicate media is idempotent");
  requireTest(controller.project()->mediaPaths.size() == 1,
              "media de-duplicated");
  requireTest(controller.saveProject(&error), "save changed project");
  aquatrack::application::ProjectController reopened;
  requireTest(reopened.openProject(projectPath, &error), "reopen project");
  requireTest(reopened.project()->name == "Survey 2026",
              "project name persisted");
  requireTest(reopened.project()->mediaPaths.size() == 1,
              "project media persisted");
  return 0;
}
