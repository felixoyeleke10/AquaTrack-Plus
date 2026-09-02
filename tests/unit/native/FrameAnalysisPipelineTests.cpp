#include "TestSupport.h"
#include "pipeline/BatchAnalysisRunner.h"
#include "pipeline/FrameAnalysisPipeline.h"

#include <QImage>

class FakeDetector final : public aquatrack::IDetector {
public:
  QString name() const override { return QStringLiteral("fake"); }
  bool loadModel(const QString &, QString *) override { return true; }
  std::vector<aquatrack::Detection> detect(const QImage &) override {
    return {{{10, 10, 40, 40}, 0.9F, 1, "Fish"},
            {{0, 0, 5, 5}, 0.2F, 1, "Fish"},
            {{20, 20, 20, 30}, 0.99F, 1, "Invalid"}};
  }
};

class FakeFrameSource final : public aquatrack::pipeline::IFrameSource {
public:
  QString name() const override { return QStringLiteral("frames"); }
  std::int64_t frameCount() const override { return 4; }
  bool read(std::int64_t index, aquatrack::pipeline::DecodedFrame &frame,
            QString *) override {
    QImage image(64, 64, QImage::Format_RGB32);
    image.fill(Qt::black);
    frame = {std::move(image), index, index * 40};
    return true;
  }
};

int main() {
  FakeDetector detector;
  aquatrack::pipeline::FrameAnalysisPipeline pipeline(
      detector, {.confidenceThreshold = 0.5F,
                 .matchIou = 0.3F,
                 .maximumMissedFrames = 2,
                 .minimumHits = 2});
  QImage frame(64, 64, QImage::Format_RGB32);
  frame.fill(Qt::black);
  const auto first = pipeline.process(frame, {"video-1", 0, 0});
  requireTest(first.detections.size() == 1,
              "filter confidence and invalid boxes");
  requireTest(first.visibleTracks.size() == 1, "create visible track");
  requireTest(first.observations.empty(), "respect minimum track hits");
  const auto second = pipeline.process(frame, {"video-1", 1, 40});
  requireTest(second.observations.size() == 1, "emit confirmed observation");
  const auto &observation = second.observations.front();
  requireTest(observation.videoId == "video-1" && observation.frameIndex == 1,
              "observation provenance");
  requireTest(observation.localTrackId == first.visibleTracks.front().id,
              "stable track identity");
  requireTest(observation.className == "Fish" && observation.confidence == 0.9F,
              "detection metadata");
  bool rejected = false;
  try {
    (void)pipeline.process(QImage(), {"video-1", 2, 80});
  } catch (const std::invalid_argument &) {
    rejected = true;
  }
  requireTest(rejected, "reject null frames");

  FakeDetector batchDetector;
  aquatrack::pipeline::FrameAnalysisPipeline batchPipeline(
      batchDetector, {.confidenceThreshold = 0.5F, .minimumHits = 1});
  aquatrack::pipeline::BatchAnalysisRunner runner(batchPipeline);
  FakeFrameSource source;
  std::stop_source cancellation;
  int progressCalls = 0;
  const auto batch =
      runner.run(source, "batch-video", cancellation.get_token(),
                 [&](std::int64_t processed, std::int64_t total) {
                   ++progressCalls;
                   if (processed == 2 && total == 4)
                     cancellation.request_stop();
                 });
  requireTest(batch.cancelled && batch.processedFrames == 2,
              "cooperative batch cancellation");
  requireTest(batch.observations.size() == 2 && progressCalls == 2,
              "batch observations and progress");
  return 0;
}
