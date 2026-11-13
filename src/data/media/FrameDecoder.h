#pragma once

#include "data/media/MediaSource.h"

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QThread>

#include <atomic>

namespace aquatrack::media {

// Decodes frames of a MediaSource on a worker thread, so the UI never waits
// on disk or FFmpeg.
//
// Video is read from one long-lived ffmpeg process per clip that streams raw
// BGRA frames: playing forward costs a pipe read (1080p sustains >100 fps),
// a short skip forward discards a few frames, and anything else re-seeks
// (frame-accurate, ~0.3 s). Recently decoded frames are cached so stepping
// back is instant.
//
// Requests coalesce: while one frame decodes, newer requests replace older
// pending ones, so dragging a slider asks for the frame under the cursor
// rather than queueing every frame it passed.
class FrameDecoder final : public QObject {
    Q_OBJECT

public:
    explicit FrameDecoder(QObject* parent = nullptr);
    ~FrameDecoder() override;

    // Replaces the source. Frames of the previous source still being decoded
    // are reported with the old generation and should be ignored.
    void setSource(const MediaSource& source);
    void clear();
    void request(int frame);
    [[nodiscard]] quint64 generation() const;
    // Frames kept for instant re-display (at 1080p, about 8 MB each).
    void setCacheSize(int frames);
    // Videos wider than this are scaled down on decode; annotations are
    // unaffected because they are drawn in source coordinates.
    void setMaximumWidth(int pixels);

signals:
    void frameDecoded(quint64 generation, int frame, QImage image);
    void frameFailed(quint64 generation, int frame, QString error);

private:
    class Worker;
    friend class Worker;

    void scheduleDrain();

    QThread m_thread;
    Worker* m_worker = nullptr;

    mutable QMutex m_mutex;
    MediaSource m_source;
    quint64 m_generation = 0;
    int m_pending = -1;
    bool m_drainScheduled = false;
    int m_cacheSize = 16;
    int m_maximumWidth = 1920;
    std::atomic_bool m_abort{false};
};

// One frame of a video, scaled to `width` (aspect kept). Blocking: for
// thumbnails, run it on a pool thread.
[[nodiscard]] QImage grabVideoFrame(const QString& path, double seconds, int width,
                                    QString* error = nullptr);
// A thumbnail for any reviewable media path (first meaningful frame).
[[nodiscard]] QImage mediaThumbnail(const QString& path, int width);

}  // namespace aquatrack::media
