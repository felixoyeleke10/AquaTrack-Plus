#pragma once

#include "domain/AnnotationSession.h"
#include "domain/IObservationRepository.h"

#include <QObject>
#include <memory>

namespace aquatrack::application {
class AnnotationController final : public QObject {
  Q_OBJECT
public:
  explicit AnnotationController(domain::IObservationRepository &repository,
                                QObject *parent = nullptr);
  void open(const domain::VideoId &videoId);
  void begin(const domain::VideoId &videoId,
             std::vector<domain::Observation> observations);
  [[nodiscard]] const std::vector<domain::Observation> &observations() const;
  [[nodiscard]] bool relabel(const domain::ObservationId &id,
                             std::string className, float confidence);
  [[nodiscard]] bool erase(const domain::ObservationId &id);
  [[nodiscard]] bool undo();
  [[nodiscard]] bool redo();
  [[nodiscard]] bool save(QString *error = nullptr);
  [[nodiscard]] bool isDirty() const { return m_dirty; }
  [[nodiscard]] bool canUndo() const {
    return m_session && m_session->canUndo();
  }
  [[nodiscard]] bool canRedo() const {
    return m_session && m_session->canRedo();
  }
signals:
  void annotationsChanged();
  void dirtyChanged(bool dirty);

private:
  void changed();
  domain::IObservationRepository &m_repository;
  domain::VideoId m_videoId;
  std::unique_ptr<domain::AnnotationSession> m_session;
  bool m_dirty = false;
};
} // namespace aquatrack::application
