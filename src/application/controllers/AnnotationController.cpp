#include "application/controllers/AnnotationController.h"

namespace aquatrack::application {
namespace {
const std::vector<domain::Observation> EmptyObservations;
}
AnnotationController::AnnotationController(
    domain::IObservationRepository &repository, QObject *parent)
    : QObject(parent), m_repository(repository) {}
void AnnotationController::open(const domain::VideoId &videoId) {
  begin(videoId, m_repository.observationsForVideo(videoId));
}
void AnnotationController::begin(
    const domain::VideoId &videoId,
    std::vector<domain::Observation> observations) {
  m_videoId = videoId;
  m_session =
      std::make_unique<domain::AnnotationSession>(std::move(observations));
  m_dirty = false;
  emit annotationsChanged();
  emit dirtyChanged(false);
}
const std::vector<domain::Observation> &
AnnotationController::observations() const {
  return m_session ? m_session->observations() : EmptyObservations;
}
bool AnnotationController::relabel(const domain::ObservationId &id,
                                   std::string className, float confidence) {
  if (!m_session || !m_session->relabel(id, std::move(className), confidence))
    return false;
  changed();
  return true;
}
bool AnnotationController::erase(const domain::ObservationId &id) {
  if (!m_session || !m_session->erase(id))
    return false;
  changed();
  return true;
}
bool AnnotationController::mergeTracks(domain::LocalTrackId source,
                                       domain::LocalTrackId target,
                                       std::int64_t fromFrame) {
  if (!m_session ||
      !m_session->mergeTracks(m_videoId, source, target, fromFrame))
    return false;
  changed();
  return true;
}
bool AnnotationController::undo() {
  if (!m_session || !m_session->undo())
    return false;
  changed();
  return true;
}
bool AnnotationController::redo() {
  if (!m_session || !m_session->redo())
    return false;
  changed();
  return true;
}
bool AnnotationController::save(QString *error) {
  if (!m_session || m_videoId.empty()) {
    if (error)
      *error = QStringLiteral("No annotation session is open");
    return false;
  }
  std::string message;
  if (!m_repository.replaceObservationsForVideo(
          m_videoId, m_session->observations(), message)) {
    if (error)
      *error = QString::fromStdString(message);
    return false;
  }
  m_dirty = false;
  emit dirtyChanged(false);
  return true;
}
void AnnotationController::changed() {
  if (!m_dirty) {
    m_dirty = true;
    emit dirtyChanged(true);
  }
  emit annotationsChanged();
}
} // namespace aquatrack::application
