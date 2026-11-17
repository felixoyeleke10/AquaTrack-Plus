#pragma once
#include <cstddef>
#include <string>
#include <vector>
namespace aquatrack::system::settings {
struct VideoSettings {
  int decodeThreads = 2;
  int frameBufferSize = 8;
  bool hardwareDecode = true;
};
struct DetectionSettings {
  std::string modelId;
  double confidenceThreshold = 0.35;
  double nmsThreshold = 0.70;
  int inputSize = 960;
};
struct TrackingSettings {
  double matchIou = 0.30;
  int minimumHits = 3;
  int maximumMissedFrames = 20;
};
struct PipelineSettings {
  int heavyJobLimit = 2;
  bool processVisibleFirst = true;
};
struct StorageSettings {
  std::string defaultProjectDirectory;
  // What adding media to a project does: "ask" each time, "copy" it into
  // the project's media folder, or "link" to it where it is.
  std::string mediaImport = "ask";
  // Most recent first. Capped so the settings file cannot grow without bound.
  std::vector<std::string> recentProjects;
};
inline constexpr std::size_t MaxRecentProjects = 10;
struct ExportSettings {
  bool includeConfidence = true;
  bool includeUnverified = true;
};
// How "Detect & track" runs. An empty interpreter means auto-detect (the
// checkout's .venv, then python on PATH).
struct AnalysisSettings {
  std::string pythonExecutable;
  std::string tracker = "bytetrack"; // or "iou"
};
struct AppearanceSettings {
  std::string theme = "dark";
  double interfaceScale = 1.0;
};
// Window layout the user chose by dragging, restored next session.
struct LayoutSettings {
  // The Review page's panels under the picture, in pixels; 0 = collapsed.
  int reviewActivityHeight = 200;
  int reviewLogHeight = 200;
};
inline constexpr int MaxReviewPanelHeight = 4000;
struct AquaTrackSettings {
  VideoSettings video;
  DetectionSettings detection;
  TrackingSettings tracking;
  PipelineSettings pipeline;
  StorageSettings storage;
  ExportSettings exportSettings;
  AnalysisSettings analysis;
  AppearanceSettings appearance;
  LayoutSettings layout;
};
[[nodiscard]] bool isValid(const AquaTrackSettings &,
                           std::string *reason = nullptr);
} // namespace aquatrack::system::settings
