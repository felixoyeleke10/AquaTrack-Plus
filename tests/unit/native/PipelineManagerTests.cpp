#include "TestSupport.h"
#include "system/PipelineManager.h"
#include <QCoreApplication>
int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  aquatrack::system::PipelineManager manager;
  requireTest(manager.status() == aquatrack::system::PipelineStatus::Stopped,
              "initial status");
  requireTest(manager.start(), "start");
  requireTest(!manager.start(), "duplicate start rejected");
  aquatrack::system::PipelineMetrics metrics;
  metrics.totalFps = 24.5;
  metrics.processedFrames = 100;
  requireTest(manager.reportMetrics(metrics), "running metrics accepted");
  metrics.totalFps = -1;
  requireTest(!manager.reportMetrics(metrics), "negative metrics rejected");
  manager.reportError({"detector", "model unavailable", true});
  requireTest(manager.status() == aquatrack::system::PipelineStatus::Faulted,
              "fault state");
  requireTest(manager.restart(), "restart from fault");
  requireTest(manager.status() == aquatrack::system::PipelineStatus::Running,
              "running after restart");
  requireTest(manager.stop(), "stop");
  return 0;
}
