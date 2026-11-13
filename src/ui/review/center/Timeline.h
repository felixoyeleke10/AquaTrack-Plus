#pragma once

#include <QColor>
#include <QImage>
#include <QPair>
#include <QString>
#include <QVector>
#include <QWidget>

namespace aquatrack::ui {

// One lane per track across the whole sequence: a bar wherever the track is
// visible and a gap wherever it was lost. It answers the reviewer's track
// questions at a glance -- how many animals, for how long, which tracks
// fragment, and where an identity probably switched (drawn as a dashed link
// from the track that ends to the one that starts). Event pins sit above the
// lanes, a time axis below, and the playhead runs through all of them.
// Clicking or dragging anywhere seeks.
class TrackLanes final : public QWidget {
    Q_OBJECT

public:
    struct Lane {
        int trackId = 0;
        QString species;
        QColor color;
        // Inclusive [first, last] frame runs in which the track was observed.
        QVector<QPair<int, int>> segments;
        int observations = 0;
    };

    // A point event (startFrame == endFrame) or an interval to shade.
    struct Marker {
        int startFrame = 0;
        int endFrame = 0;
        QColor color;
        QString label;
    };

    // A likely identity switch: `fromTrack` ends at `fromFrame` and
    // `toTrack` starts at `toFrame`, probably the same animal.
    struct Link {
        int fromTrack = 0;
        int toTrack = 0;
        int fromFrame = 0;
        int toFrame = 0;
    };

    explicit TrackLanes(QWidget* parent = nullptr);

    void setSequenceLength(int frames, int frameRate);
    void setLanes(QVector<Lane> lanes);
    void setMarkers(QVector<Marker> markers);
    void setLinks(QVector<Link> links);
    void setCurrentFrame(int frame);
    void clear();

    [[nodiscard]] int laneCount() const { return m_lanes.size(); }
    // Asks for full-size lanes but accepts less: in a short window the lanes
    // compress (labels drop out below a readable size) instead of
    // overlapping whatever sits beneath them.
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

signals:
    void frameRequested(int frame);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    [[nodiscard]] qreal idealPitch() const;
    [[nodiscard]] QRectF plot() const;
    [[nodiscard]] qreal lanePitch() const;
    [[nodiscard]] QRectF laneRect(int index) const;
    [[nodiscard]] double xForFrame(int frame) const;
    [[nodiscard]] int frameAt(double x) const;
    [[nodiscard]] int laneAt(const QPoint& point) const;
    [[nodiscard]] int markerAt(const QPoint& point) const;
    [[nodiscard]] int laneIndexOf(int trackId) const;
    [[nodiscard]] bool visibleAt(const Lane& lane, int frame) const;
    [[nodiscard]] QString clock(int frame) const;

    QVector<Lane> m_lanes;
    // Row each lane is drawn in. With many tracks, tracks that never
    // overlap in time share a row, so every track stays visible instead of
    // lanes shrinking below a pixel and the later ones falling off the chart.
    QVector<int> m_rowOfLane;
    int m_rows = 0;
    bool m_packed = false;
    QVector<Marker> m_markers;
    QVector<Link> m_links;
    int m_frames = 0;
    int m_frameRate = 15;
    int m_currentFrame = 0;
    int m_hoveredLane = -1;
    int m_hoveredMarker = -1;
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
    [[nodiscard]] QSize sizeHint() const override;

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
