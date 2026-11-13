#include "system/settings/AquaTrackSettings.h"
#include <cmath>
namespace aquatrack::system::settings {
bool isValid(const AquaTrackSettings &s, std::string *reason) {
  auto fail = [reason](const char *m) {
    if (reason)
      *reason = m;
    return false;
  };
  if (s.video.decodeThreads < 1 || s.video.decodeThreads > 32)
    return fail("decode threads must be between 1 and 32");
  if (s.video.frameBufferSize < 2 || s.video.frameBufferSize > 512)
    return fail("frame buffer size must be between 2 and 512");
  if (!std::isfinite(s.detection.confidenceThreshold) ||
      s.detection.confidenceThreshold < 0 ||
      s.detection.confidenceThreshold > 1)
    return fail("confidence threshold must be between 0 and 1");
  if (!std::isfinite(s.detection.nmsThreshold) ||
      s.detection.nmsThreshold < 0 || s.detection.nmsThreshold > 1)
    return fail("NMS threshold must be between 0 and 1");
  if (s.detection.inputSize < 256 || s.detection.inputSize > 4096)
    return fail("detector input size must be between 256 and 4096");
  if (s.tracking.matchIou < 0 || s.tracking.matchIou > 1 ||
      s.tracking.minimumHits < 1 || s.tracking.maximumMissedFrames < 0)
    return fail("tracking settings are invalid");
  if (s.pipeline.heavyJobLimit < 1 || s.pipeline.heavyJobLimit > 8)
    return fail("heavy job limit must be between 1 and 8");
  if (s.appearance.theme != "dark" && s.appearance.theme != "light" &&
      s.appearance.theme != "system")
    return fail("theme must be dark, light, or system");
  if (s.appearance.interfaceScale < 0.75 || s.appearance.interfaceScale > 2.0)
    return fail("interface scale must be between 0.75 and 2.0");
  if (s.analysis.tracker != "bytetrack" && s.analysis.tracker != "iou")
    return fail("tracker must be bytetrack or iou");
  if (s.storage.mediaImport != "ask" && s.storage.mediaImport != "copy" &&
      s.storage.mediaImport != "link")
    return fail("media import must be ask, copy, or link");
  if (s.storage.recentProjects.size() > MaxRecentProjects)
    return fail("too many recent projects");
  return true;
}
} // namespace aquatrack::system::settings
