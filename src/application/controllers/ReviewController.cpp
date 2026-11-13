#include "application/controllers/ReviewController.h"

#include <QTimer>

#include <algorithm>
#include <cmath>
#include <utility>

namespace aquatrack::application {

ReviewController::ReviewController(QObject *parent)
    : QObject(parent), m_decoder(new media::FrameDecoder(this)),
      m_pacer(new QTimer(this)) {
  m_pacer->setSingleShot(true);
  m_pacer->setTimerType(Qt::PreciseTimer);
  connect(m_pacer, &QTimer::timeout, this, [this] {
    if (m_playing)
      requestFrame(m_frame + 1);
  });
  connect(m_decoder, &media::FrameDecoder::frameDecoded, this,
          &ReviewController::onDecoded);
  connect(m_decoder, &media::FrameDecoder::frameFailed, this,
          &ReviewController::onFailed);
}

ReviewController::~ReviewController() = default;

bool ReviewController::loadSequence(const QString &path) {
  media::MediaSource opened;
  QString error;
  if (!opened.open(path, &error)) {
    emit errorOccurred(error);
    return false;
  }
  stopPlayback();
  m_source = std::move(opened);
  m_decoder->setSource(m_source);
  m_frame = 0;
  m_inFlight = 0;
  emit sequenceLoaded(m_source.name(), m_source.frameCount(),
                      std::max(1, int(std::lround(m_source.frameRate()))),
                      m_source.frameSize().width(),
                      m_source.frameSize().height());
  setFrame(1);
  return true;
}

bool ReviewController::setFrame(int frame) {
  if (!isLoaded() || frame < 1 || frame > m_source.frameCount())
    return false;
  requestFrame(frame);
  return true;
}

void ReviewController::requestFrame(int frame) {
  m_inFlight = frame;
  m_decoder->request(frame);
}

void ReviewController::onDecoded(quint64 generation, int frame,
                                 const QImage &image) {
  if (generation != m_decoder->generation())
    return;
  if (frame == m_inFlight)
    m_inFlight = 0;
  m_frame = frame;
  m_lastShown.restart();
  emit frameReady(image, m_source.groundTruth(frame), frame,
                  m_source.frameCount());
  if (m_playing && m_inFlight == 0)
    scheduleNext();
}

void ReviewController::onFailed(quint64 generation, int frame,
                                const QString &error) {
  if (generation != m_decoder->generation())
    return;
  if (frame == m_inFlight)
    m_inFlight = 0;
  // Stop before reporting: the error is shown in a modal dialog, and
  // playback must not keep asking for the same unreadable frame meanwhile.
  stopPlayback();
  emit errorOccurred(error);
}

void ReviewController::scheduleNext() {
  if (m_frame >= m_source.frameCount()) {
    stopPlayback();
    return;
  }
  // Pace by the clip's own rate, counting the time the frame took to arrive
  // and be shown, so slow decoding lowers the frame rate instead of piling
  // up requests.
  const int interval = m_source.frameIntervalMs(m_frame + 1);
  const qint64 spent = m_lastShown.isValid() ? m_lastShown.elapsed() : 0;
  m_pacer->start(int(std::max<qint64>(0, interval - spent)));
}

void ReviewController::togglePlayback() {
  if (!isLoaded())
    return;
  if (m_playing) {
    stopPlayback();
    return;
  }
  m_playing = true;
  emit playbackChanged(true);
  if (m_inFlight != 0)
    return; // the frame on its way starts the clock
  if (m_frame >= m_source.frameCount())
    requestFrame(1);
  else
    requestFrame(m_frame + 1);
}

void ReviewController::stopPlayback() {
  m_pacer->stop();
  if (!m_playing)
    return;
  m_playing = false;
  emit playbackChanged(false);
}

void ReviewController::close() {
  stopPlayback();
  m_source = media::MediaSource();
  m_decoder->clear();
  m_frame = 0;
  m_inFlight = 0;
}

void ReviewController::setDecoderCacheSize(int frames) {
  m_decoder->setCacheSize(frames);
}

std::vector<domain::Observation> ReviewController::observations() const {
  std::vector<domain::Observation> result;
  if (m_source.kind() != media::MediaKind::ImageSequence)
    return result;
  const auto &sequence = m_source.imageSequence();
  const auto videoId = sequence.name().toStdString();
  for (int frame = 1; frame <= sequence.frameCount(); ++frame) {
    for (const auto &truth : sequence.annotations(frame)) {
      domain::Observation observation;
      observation.id = videoId + ":" + std::to_string(frame) + ":" +
                       std::to_string(truth.trackId);
      observation.videoId = videoId;
      observation.frameIndex = frame;
      observation.timestampMs = m_source.timestampMs(frame);
      observation.localTrackId = truth.trackId;
      observation.box = {static_cast<float>(truth.box.left()),
                         static_cast<float>(truth.box.top()),
                         static_cast<float>(truth.box.right()),
                         static_cast<float>(truth.box.bottom())};
      observation.className =
          BrackishMotSequence::className(truth.classId).toStdString();
      observation.confidence = truth.confidence;
      observation.humanVerified = true;
      if (!domain::isValid(observation))
        continue;
      result.push_back(std::move(observation));
    }
  }
  return result;
}

} // namespace aquatrack::application
