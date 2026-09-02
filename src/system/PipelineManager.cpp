#include "system/PipelineManager.h"
#include <cmath>
namespace {
bool finiteNonNegative(double value) {
  return std::isfinite(value) && value >= 0.0;
}
} // namespace
namespace aquatrack::system {
PipelineManager::PipelineManager(QObject *parent) : QObject(parent) {}
void PipelineManager::setStatus(PipelineStatus status) {
  if (m_status == status)
    return;
  m_status = status;
  emit stateChanged(status);
}
bool PipelineManager::start() {
  if (m_status != PipelineStatus::Stopped &&
      m_status != PipelineStatus::Faulted)
    return false;
  setStatus(PipelineStatus::Starting);
  m_metrics = {};
  setStatus(PipelineStatus::Running);
  return true;
}
bool PipelineManager::stop() {
  if (m_status != PipelineStatus::Running &&
      m_status != PipelineStatus::Faulted)
    return false;
  setStatus(PipelineStatus::Stopping);
  setStatus(PipelineStatus::Stopped);
  return true;
}
bool PipelineManager::restart() {
  if (m_status == PipelineStatus::Running ||
      m_status == PipelineStatus::Faulted) {
    if (!stop())
      return false;
  }
  return start();
}
bool PipelineManager::reportMetrics(const PipelineMetrics &m) {
  if (m_status != PipelineStatus::Running)
    return false;
  if (!finiteNonNegative(m.decodeFps) || !finiteNonNegative(m.detectionFps) ||
      !finiteNonNegative(m.trackingFps) || !finiteNonNegative(m.totalFps) ||
      !finiteNonNegative(m.inferenceLatencyMs) ||
      !finiteNonNegative(m.totalLatencyMs))
    return false;
  m_metrics = m;
  emit metricsUpdated(m_metrics);
  return true;
}
void PipelineManager::reportError(PipelineError error) {
  setStatus(PipelineStatus::Faulted);
  emit pipelineError(std::move(error));
}
} // namespace aquatrack::system
