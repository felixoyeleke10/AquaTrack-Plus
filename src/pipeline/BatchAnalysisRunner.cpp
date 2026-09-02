#include "pipeline/BatchAnalysisRunner.h"

#include <iterator>
#include <stdexcept>

namespace aquatrack::pipeline {
BatchAnalysisResult BatchAnalysisRunner::run(IFrameSource &source,
                                             const domain::VideoId &videoId,
                                             std::stop_token stopToken,
                                             ProgressCallback progress) {
  if (videoId.empty())
    throw std::invalid_argument("video id is required");
  BatchAnalysisResult result;
  for (std::int64_t index = 0; index < source.frameCount(); ++index) {
    if (stopToken.stop_requested()) {
      result.cancelled = true;
      break;
    }
    DecodedFrame frame;
    QString error;
    if (!source.read(index, frame, &error))
      throw std::runtime_error(error.toStdString());
    auto analysis = m_pipeline.process(
        frame.image, {videoId, frame.index, frame.timestampMs});
    result.observations.insert(
        result.observations.end(),
        std::make_move_iterator(analysis.observations.begin()),
        std::make_move_iterator(analysis.observations.end()));
    ++result.processedFrames;
    if (progress)
      progress(result.processedFrames, source.frameCount());
  }
  return result;
}
} // namespace aquatrack::pipeline
