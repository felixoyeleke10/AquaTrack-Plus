#pragma once

#include "data/BrackishMotSequence.h"
#include "data/media/FrameDecoder.h"
#include "data/media/MediaSource.h"
#include "domain/ScientificTypes.h"

#include <QElapsedTimer>
#include <QImage>
#include <QObject>

class QTimer;

namespace aquatrack::application {

// Opens media for review and drives the playhead. Metadata (frame count,
// rate, ground truth) loads synchronously; pixels arrive asynchronously from
// a FrameDecoder, so neither scrubbing nor playback blocks the UI.
class ReviewController final : public QObject {
  Q_OBJECT

public:
  explicit ReviewController(QObject *parent = nullptr);
  ~ReviewController() override;

  // Opens an image sequence, a video file or a video sequence. On failure
  // the media already open stays open, errorOccurred is emitted and false
  // is returned.
  bool loadSequence(const QString &path);
  // Asks for a frame. Returns false (without emitting) when out of range;
  // frameReady or errorOccurred follows otherwise.
  bool setFrame(int frame);
  void togglePlayback();
  void stopPlayback();
  // Closes the media: nothing is loaded afterwards.
  void close();
  [[nodiscard]] bool isPlaying() const { return m_playing; }
  [[nodiscard]] bool isLoaded() const { return m_source.isOpen(); }
  // Last frame actually shown.
  [[nodiscard]] int currentFrame() const { return m_frame; }
  [[nodiscard]] const media::MediaSource &source() const { return m_source; }
  // Ground truth as observations (image sequences carry gt.txt). Rows that
  // cannot form a valid observation (zero-size boxes) are skipped.
  [[nodiscard]] std::vector<domain::Observation> observations() const;
  void setDecoderCacheSize(int frames);

signals:
  void sequenceLoaded(QString name, int frames, int frameRate, int width,
                      int height);
  void frameReady(QImage image, QVector<aquatrack::GroundTruth> annotations,
                  int frame, int total);
  void playbackChanged(bool playing);
  void errorOccurred(QString message);

private:
  void onDecoded(quint64 generation, int frame, const QImage &image);
  void onFailed(quint64 generation, int frame, const QString &error);
  void requestFrame(int frame);
  void scheduleNext();

  media::MediaSource m_source;
  media::FrameDecoder *m_decoder = nullptr;
  QTimer *m_pacer = nullptr;
  QElapsedTimer m_lastShown;
  int m_frame = 0;
  int m_inFlight = 0; // frame requested and not yet answered, 0 for none
  bool m_playing = false;
};

} // namespace aquatrack::application
