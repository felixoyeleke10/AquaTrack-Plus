#include "pipeline/FrameAnalysisPipeline.h"

#include <QElapsedTimer>

#include <algorithm>
#include <stdexcept>

namespace aquatrack::pipeline {
FrameAnalysisPipeline::FrameAnalysisPipeline(IDetector &detector,
                                             FrameAnalysisConfig config)
    : m_detector(detector), m_config(config),
      m_tracker(config.matchIou, config.maximumMissedFrames,
                config.minimumHits) {
  if (config.confidenceThreshold < 0.0F || config.confidenceThreshold > 1.0F ||
      config.matchIou < 0.0F || config.matchIou > 1.0F ||
      config.maximumMissedFrames < 0 || config.minimumHits < 1)
    throw std::invalid_argument("invalid frame analysis configuration");
}

FrameAnalysisResult
FrameAnalysisPipeline::process(const QImage &frame,
                               const FrameContext &context) {
  if (frame.isNull() || context.videoId.empty() || context.frameIndex < 0 ||
      context.timestampMs < 0)
    throw std::invalid_argument("invalid frame analysis input");
  QElapsedTimer timer;
  timer.start();
  auto detections = m_detector.detect(frame);
  const double inferenceMs = timer.nsecsElapsed() / 1'000'000.0;
  std::erase_if(detections, [this](const Detection &detection) {
    return detection.confidence < m_config.confidenceThreshold ||
           detection.box.area() <= 0.0F;
  });
  auto tracks = m_tracker.update(detections);
  FrameAnalysisResult result{
      context, std::move(detections), tracks, {}, inferenceMs};
  result.observations.reserve(tracks.size());
  for (const auto &track : tracks) {
    if (!track.confirmed)
      continue;
    domain::Observation observation;
    observation.id = context.videoId + ":" +
                     std::to_string(context.frameIndex) + ":" +
                     std::to_string(track.id);
    observation.videoId = context.videoId;
    observation.timestampMs = context.timestampMs;
    observation.frameIndex = context.frameIndex;
    observation.localTrackId = track.id;
    observation.box = track.detection.box;
    observation.className = track.detection.className;
    observation.confidence = track.detection.confidence;
    result.observations.push_back(std::move(observation));
  }
  return result;
}
} // namespace aquatrack::pipeline
