#include "TestSupport.h"
#include "application/controllers/AnnotationController.h"
#include "data/observations/JsonObservationRepository.h"

#include <QCoreApplication>
#include <QTemporaryDir>

namespace {
aquatrack::domain::Observation observation(std::string id, std::int64_t frame) {
  return {std::move(id), "video-1",      frame * 40, frame, 3,
          std::nullopt,  {1, 2, 20, 30}, "Fish",     0.8F,  false};
}
} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QTemporaryDir temporary;
  requireTest(temporary.isValid(), "temporary directory");
  std::string repositoryError;
  aquatrack::data::JsonObservationRepository repository(
      temporary.filePath(QStringLiteral("observations.json")));
  requireTest(repository.open(repositoryError), "open repository");
  aquatrack::application::AnnotationController controller(repository);
  controller.begin("video-1", {observation("a", 0), observation("b", 1)});
  requireTest(!controller.isDirty() && controller.observations().size() == 2,
              "clean initial session");
  requireTest(controller.relabel("a", "Pollock", 0.95F), "controller relabel");
  requireTest(controller.isDirty(), "edit marks session dirty");
  requireTest(controller.erase("b") && controller.observations().size() == 1,
              "controller delete");
  requireTest(controller.undo() && controller.observations().size() == 2,
              "controller undo");
  requireTest(controller.redo() && controller.observations().size() == 1,
              "controller redo");
  QString error;
  requireTest(controller.save(&error), "save annotation session");
  requireTest(!controller.isDirty(), "save clears dirty state");
  aquatrack::application::AnnotationController reopened(repository);
  reopened.open("video-1");
  requireTest(reopened.observations().size() == 1,
              "deleted observation stays deleted");
  requireTest(reopened.observations()[0].className == "Pollock" &&
                  reopened.observations()[0].humanVerified,
              "relabel persists on reopen");
  return 0;
}
