#pragma once

#include "data/BrackishMotSequence.h"

#include <QHash>
#include <QImage>
#include <QPointF>
#include <QVector>
#include <QWidget>

// The review canvas: the frame, its boxes, recent track trails, and the
// read-outs that belong on top of the video rather than beside it.
class SequenceView final : public QWidget {
    Q_OBJECT

public:
    struct Overlays {
        bool showDetections = true;
        bool showTracks = true;
        bool showIds = true;
        int speciesClassId = 0;
        double minConfidence = 0.0;
    };

    explicit SequenceView(QWidget* parent = nullptr);

    void setFrame(QImage image, QVector<aquatrack::GroundTruth> annotations);
    void setOverlays(const Overlays& overlays);
    void setFrameInfo(int frame, int total, qint64 timeMs);
    // Recent centre points per track, in image coordinates, oldest first.
    void setTrails(QHash<int, QVector<QPointF>> trails);
    void clear();

    [[nodiscard]] QVector<aquatrack::GroundTruth> visibleAnnotations() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    [[nodiscard]] bool passes(const aquatrack::GroundTruth& annotation) const;

    QImage m_image;
    QVector<aquatrack::GroundTruth> m_annotations;
    QHash<int, QVector<QPointF>> m_trails;
    Overlays m_overlays;
    int m_frame = 0;
    int m_total = 0;
    qint64 m_timeMs = 0;
};
