#pragma once

#include "pipeline/FrameAnalysisPipeline.h"
#include "pipeline/IFrameSource.h"

#include <functional>
#include <stop_token>

namespace aquatrack::pipeline {
struct BatchAnalysisResult {
  std::vector<domain::Observation> observations;
  std::int64_t processedFrames = 0;
  bool cancelled = false;
};
using ProgressCallback =
    std::function<void(std::int64_t processed, std::int64_t total)>;
class BatchAnalysisRunner final {
public:
  explicit BatchAnalysisRunner(FrameAnalysisPipeline &pipeline)
      : m_pipeline(pipeline) {}
  [[nodiscard]] BatchAnalysisResult run(IFrameSource &source,
                                        const domain::VideoId &videoId,
                                        std::stop_token stopToken = {},
                                        ProgressCallback progress = {});

private:
  FrameAnalysisPipeline &m_pipeline;
};
} // namespace aquatrack::pipeline
