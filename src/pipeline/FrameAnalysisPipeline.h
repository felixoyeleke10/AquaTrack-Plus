#pragma once

#include "domain/IoUTracker.h"
#include "domain/ScientificTypes.h"
#include "inference/IDetector.h"

#include <cstdint>

namespace aquatrack::pipeline {

struct FrameContext {
  domain::VideoId videoId;
  std::int64_t frameIndex = 0;
  domain::TimestampMs timestampMs = 0;
};

struct FrameAnalysisResult {
  FrameContext context;
  std::vector<Detection> detections;
  std::vector<Track> visibleTracks;
  std::vector<domain::Observation> observations;
  double inferenceMs = 0.0;
};

struct FrameAnalysisConfig {
  float confidenceThreshold = 0.35F;
  float matchIou = 0.30F;
  int maximumMissedFrames = 20;
  int minimumHits = 3;
};

class FrameAnalysisPipeline final {
public:
  FrameAnalysisPipeline(IDetector &detector, FrameAnalysisConfig config = {});
  [[nodiscard]] FrameAnalysisResult process(const QImage &frame,
                                            const FrameContext &context);

private:
  IDetector &m_detector;
  FrameAnalysisConfig m_config;
  IoUTracker m_tracker;
};

} // namespace aquatrack::pipeline
