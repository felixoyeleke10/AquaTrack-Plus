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
  bool useExternalMediaReferences = true;
  // Most recent first. Capped so the settings file cannot grow without bound.
  std::vector<std::string> recentProjects;
};
inline constexpr std::size_t MaxRecentProjects = 10;
struct ExportSettings {
  bool includeConfidence = true;
  bool includeUnverified = true;
};
struct AppearanceSettings {
  std::string theme = "dark";
  double interfaceScale = 1.0;
};
struct AquaTrackSettings {
  VideoSettings video;
  DetectionSettings detection;
  TrackingSettings tracking;
  PipelineSettings pipeline;
  StorageSettings storage;
  ExportSettings exportSettings;
  AppearanceSettings appearance;
};
[[nodiscard]] bool isValid(const AquaTrackSettings &,
                           std::string *reason = nullptr);
} // namespace aquatrack::system::settings
