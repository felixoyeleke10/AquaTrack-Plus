#pragma once

#include "data/BrackishMotSequence.h"
#include "data/media/MediaTools.h"

#include <QSize>
#include <QString>
#include <QVector>

namespace aquatrack::media {

// One video file's place on a source's timeline.
struct Clip {
    QString path;
    int firstFrame = 1;  // global, 1-based
    int frameCount = 0;
    double frameRate = 0.0;
    QSize size;
    qint64 startMs = 0;  // global time of the clip's first frame
    qint64 durationMs = 0;

    [[nodiscard]] int lastFrame() const { return firstFrame + frameCount - 1; }
};

// Anything Review can open, behind one frame-numbered timeline: a BrackishMOT
// image sequence, a single video, or several videos played as one. Frames
// are 1-based and continuous across clips; times come from each clip's own
// frame rate, so mixing 25 and 29.97 fps footage keeps correct clocks.
//
// Opening reads metadata only (seqinfo.ini, ffprobe, sequence.json); frames
// are decoded by FrameDecoder.
class MediaSource {
public:
    [[nodiscard]] bool open(const QString& path, QString* error = nullptr);

    [[nodiscard]] bool isOpen() const { return m_frameCount > 0; }
    [[nodiscard]] MediaKind kind() const { return m_kind; }
    [[nodiscard]] bool isVideo() const
    {
        return m_kind == MediaKind::VideoFile || m_kind == MediaKind::VideoSequence;
    }
    // The path that was opened (file or folder), cleaned and absolute.
    [[nodiscard]] QString path() const { return m_path; }
    // Display name, and the key annotations are stored under.
    [[nodiscard]] QString name() const { return m_name; }
    [[nodiscard]] int frameCount() const { return m_frameCount; }
    // Nominal rate (the first clip's) for displays that need a single value.
    [[nodiscard]] double frameRate() const { return m_frameRate; }
    // Size of the first frame; clips of a video sequence may differ.
    [[nodiscard]] QSize frameSize() const { return m_size; }
    [[nodiscard]] qint64 durationMs() const;

    [[nodiscard]] const QVector<Clip>& clips() const { return m_clips; }
    // Index into clips() for a global frame, or -1 for image sequences and
    // out-of-range frames.
    [[nodiscard]] int clipIndexForFrame(int frame) const;
    // Pixel size the frame's annotations are expressed in.
    [[nodiscard]] QSize frameSizeAt(int frame) const;

    [[nodiscard]] qint64 timestampMs(int frame) const;
    [[nodiscard]] int frameAtTimestamp(qint64 milliseconds) const;
    // Playback interval for the frame's clip.
    [[nodiscard]] int frameIntervalMs(int frame) const;

    // Image sequences only.
    [[nodiscard]] const BrackishMotSequence& imageSequence() const { return m_images; }
    [[nodiscard]] QString imagePath(int frame) const;
    [[nodiscard]] QVector<GroundTruth> groundTruth(int frame) const;

    // Name the annotations of `path` are stored under, without decoding
    // anything (used to detect two media items that would share one record).
    [[nodiscard]] static QString reviewIdFor(const QString& path);

private:
    MediaKind m_kind = MediaKind::Missing;
    QString m_path;
    QString m_name;
    int m_frameCount = 0;
    double m_frameRate = 0.0;
    QSize m_size;
    QVector<Clip> m_clips;
    BrackishMotSequence m_images;
};

}  // namespace aquatrack::media
