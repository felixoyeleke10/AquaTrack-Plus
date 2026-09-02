#include "TestSupport.h"
#include "domain/AnnotationSession.h"

namespace {
aquatrack::domain::Observation observation(std::string id, std::int64_t frame,
                                           std::int64_t track) {
  return {std::move(id), "video-1",      frame * 40, frame, track,
          std::nullopt,  {1, 2, 20, 30}, "Fish",     0.8F,  false};
}
} // namespace

int main() {
  aquatrack::domain::AnnotationSession session(
      {observation("a", 0, 1), observation("b", 1, 1), observation("c", 2, 1)});
  requireTest(session.relabel("a", "Pollock", 0.95F), "relabel observation");
  requireTest(session.observations()[0].humanVerified, "mark relabel verified");
  requireTest(session.updateBox("a", {2, 3, 22, 33}), "update bounding box");
  requireTest(session.splitTrack("video-1", 1, 8, 2), "split track from frame");
  requireTest(session.observations()[1].localTrackId == 1 &&
                  session.observations()[2].localTrackId == 8,
              "split boundary");
  requireTest(session.mergeTracks("video-1", 8, 1, 2), "merge track");
  requireTest(session.erase("b") && session.observations().size() == 2,
              "delete observation");
  requireTest(session.canUndo() && session.undo(), "undo delete");
  requireTest(session.observations().size() == 3 && session.canRedo(),
              "undo restores state");
  requireTest(session.redo() && session.observations().size() == 2,
              "redo delete");
  requireTest(!session.relabel("missing", "Fish", 1.0F),
              "reject missing observation");
  requireTest(!session.updateBox("a", {1, 1, 1, 2}), "reject invalid box");
  requireTest(!session.mergeTracks("video-1", 1, 1, 0), "reject self merge");
  return 0;
}
