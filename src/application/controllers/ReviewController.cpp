#include "application/controllers/ReviewController.h"

#include <QTimer>

#include <utility>

namespace aquatrack::application {

ReviewController::ReviewController(QObject *parent)
    : QObject(parent), m_timer(new QTimer(this)) {
  connect(m_timer, &QTimer::timeout, this, [this] {
    if (m_frame >= m_sequence.frameCount()) {
      m_timer->stop();
      emit playbackChanged(false);
      return;
    }
    setFrame(m_frame + 1);
  });
}

void ReviewController::loadSequence(const QString &directory) {
  QString error;
  if (!m_sequence.load(directory, &error)) {
    emit errorOccurred(error);
    return;
  }
  m_timer->stop();
  m_timer->setInterval(qMax(1, 1000 / m_sequence.frameRate()));
  emit playbackChanged(false);
  emit sequenceLoaded(m_sequence.name(), m_sequence.frameCount(),
                      m_sequence.frameRate(), m_sequence.imageWidth(),
                      m_sequence.imageHeight());
  setFrame(1);
}

void ReviewController::setFrame(int frame) {
  if (!m_sequence.isLoaded() || frame < 1 || frame > m_sequence.frameCount())
    return;
  QImage image(m_sequence.imagePath(frame));
  if (image.isNull()) {
    emit errorOccurred(QStringLiteral("Could not read frame %1").arg(frame));
    return;
  }
  m_frame = frame;
  emit frameReady(std::move(image), m_sequence.annotations(frame), frame,
                  m_sequence.frameCount());
}

void ReviewController::togglePlayback() {
  if (!m_sequence.isLoaded())
    return;
  if (m_timer->isActive()) {
    m_timer->stop();
    emit playbackChanged(false);
    return;
  }
  if (m_frame >= m_sequence.frameCount())
    setFrame(1);
  m_timer->start();
  emit playbackChanged(true);
}

std::vector<domain::Observation> ReviewController::observations() const {
  std::vector<domain::Observation> result;
  if (!m_sequence.isLoaded())
    return result;
  const auto videoId = m_sequence.name().toStdString();
  for (int frame = 1; frame <= m_sequence.frameCount(); ++frame) {
    for (const auto &truth : m_sequence.annotations(frame)) {
      domain::Observation observation;
      observation.id = videoId + ":" + std::to_string(frame) + ":" +
                       std::to_string(truth.trackId);
      observation.videoId = videoId;
      observation.frameIndex = frame;
      observation.timestampMs = static_cast<domain::TimestampMs>(frame - 1) *
                                1000 / m_sequence.frameRate();
      observation.localTrackId = truth.trackId;
      observation.box = {static_cast<float>(truth.box.left()),
                         static_cast<float>(truth.box.top()),
                         static_cast<float>(truth.box.right()),
                         static_cast<float>(truth.box.bottom())};
      observation.className =
          BrackishMotSequence::className(truth.classId).toStdString();
      observation.confidence = truth.confidence;
      observation.humanVerified = true;
      result.push_back(std::move(observation));
    }
  }
  return result;
}

} // namespace aquatrack::application
