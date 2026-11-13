#pragma once

#include "ui/review/center/Timeline.h"

#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QStackedWidget;

class ActivityStrip;

// The sequence timeline under the frame: how busy the water is, second by
// second, as a heat strip from quiet (violet) to busy (red), with the
// playhead on it. A mode menu switches the measure (tracks per second,
// detections per second) or shows the individual track lanes instead, which
// suit sequences with few tracks. Clicking or dragging seeks.
class ActivityPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ActivityPanel(QWidget* parent = nullptr);

    // Per-second series over the whole sequence (index = second).
    void setActivity(const QVector<int>& tracksPerSecond, const QVector<int>& detectionsPerSecond,
                     int totalTracks);
    void setDuration(qint64 milliseconds);
    void setPlayhead(qint64 milliseconds);
    void clear();

    [[nodiscard]] aquatrack::ui::TrackLanes* lanes() const { return m_lanes; }

signals:
    void timestampRequested(qint64 milliseconds);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyMode();
    // Narrow panels drop the subtitle, tile captions and bucket list so the
    // figures and the strip stay readable.
    void setCompact(bool compact);

    QLabel* m_subtitle = nullptr;
    QLabel* m_totalValue = nullptr;
    QLabel* m_peakValue = nullptr;
    QLabel* m_averageValue = nullptr;
    QComboBox* m_mode = nullptr;
    QStackedWidget* m_views = nullptr;
    ActivityStrip* m_strip = nullptr;
    aquatrack::ui::TrackLanes* m_lanes = nullptr;
    QWidget* m_legend = nullptr;
    QLabel* m_buckets = nullptr;
    QList<QLabel*> m_captions;
    QWidget* m_lowLabel = nullptr;
    QWidget* m_highLabel = nullptr;
    bool m_compact = false;

    QVector<int> m_tracks;
    QVector<int> m_detections;
    int m_totalTracks = 0;
};
