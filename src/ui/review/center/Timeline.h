#pragma once

#include <QColor>
#include <QImage>
#include <QString>
#include <QVector>
#include <QWidget>

namespace aquatrack::ui {

// A single thin time axis marked with where things happen: one tick per
// track entry or exit, coloured by track, plus pins for derived events.
// Deliberately not one lane per track and not a density ribbon -- lanes turn
// into a video editor's clip view and a ribbon buries the individual moments,
// when the question a reviewer actually has is "where should I look next".
class EventTimeline final : public QWidget {
    Q_OBJECT

public:
    // A point event (startFrame == endFrame) or an interval to shade.
    struct Marker {
        int startFrame = 0;
        int endFrame = 0;
        QColor color;
        QString label;
    };

    // Where one track enters or leaves. Entries and exits are the moments a
    // reviewer checks for missed or duplicated identities.
    struct Tick {
        int frame = 0;
        int trackId = 0;
        bool entering = true;
    };

    explicit EventTimeline(QWidget* parent = nullptr);

    void setSequenceLength(int frames, int frameRate);
    void setMarkers(QVector<Marker> markers);
    void setTicks(QVector<Tick> ticks);
    void setCurrentFrame(int frame);
    void clear();

signals:
    void frameRequested(int frame);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    [[nodiscard]] int frameAt(int x) const;
    [[nodiscard]] double xForFrame(int frame) const;
    [[nodiscard]] int markerAt(const QPoint& point) const;
    [[nodiscard]] int tickAt(const QPoint& point) const;
    [[nodiscard]] QRectF lane() const;

    QVector<Marker> m_markers;
    QVector<Tick> m_ticks;
    int m_frames = 0;
    int m_frameRate = 15;
    int m_currentFrame = 0;
    int m_hoveredMarker = -1;
    int m_hoveredTick = -1;
};

// A strip of evenly spaced preview frames. Clicking one seeks to it.
class Filmstrip final : public QWidget {
    Q_OBJECT

public:
    struct Thumbnail {
        int frame = 0;
        QImage image;
        QString caption;
    };

    explicit Filmstrip(QWidget* parent = nullptr);

    void setThumbnails(QVector<Thumbnail> thumbnails);
    void setCurrentFrame(int frame);
    void clear();

signals:
    void frameRequested(int frame);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] int indexAt(const QPoint& point) const;

    QVector<Thumbnail> m_thumbnails;
    int m_currentFrame = 0;
};

}  // namespace aquatrack::ui
