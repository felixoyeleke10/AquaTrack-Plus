#pragma once
#include <QObject>
#include <QString>
#include <cstdint>
namespace aquatrack::system {
enum class PipelineStatus { Stopped, Starting, Running, Stopping, Faulted };
struct PipelineMetrics {
  double decodeFps = 0;
  double detectionFps = 0;
  double trackingFps = 0;
  double totalFps = 0;
  double inferenceLatencyMs = 0;
  double totalLatencyMs = 0;
  std::uint64_t processedFrames = 0;
  std::uint64_t droppedFrames = 0;
};
struct PipelineError {
  QString component;
  QString message;
  bool recoverable = false;
};
class PipelineManager final : public QObject {
  Q_OBJECT
public:
  explicit PipelineManager(QObject *parent = nullptr);
  [[nodiscard]] PipelineStatus status() const { return m_status; }
  [[nodiscard]] PipelineMetrics metrics() const { return m_metrics; }
  bool start();
  bool stop();
  bool restart();
  bool reportMetrics(const PipelineMetrics &metrics);
  void reportError(PipelineError error);
signals:
  void stateChanged(PipelineStatus status);
  void metricsUpdated(PipelineMetrics metrics);
  void pipelineError(PipelineError error);

private:
  void setStatus(PipelineStatus status);
  PipelineStatus m_status = PipelineStatus::Stopped;
  PipelineMetrics m_metrics;
};
} // namespace aquatrack::system
Q_DECLARE_METATYPE(aquatrack::system::PipelineStatus)
Q_DECLARE_METATYPE(aquatrack::system::PipelineMetrics)
Q_DECLARE_METATYPE(aquatrack::system::PipelineError)
