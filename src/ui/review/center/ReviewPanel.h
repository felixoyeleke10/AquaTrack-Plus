#pragma once

#include "analytics/SurveyAnalytics.h"
#include "data/BrackishMotSequence.h"
#include "ui/review/center/Timeline.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QSlider;
class QTableWidget;
class QTabWidget;
class SequenceView;

namespace aquatrack::ui {
class BarList;
class DonutChart;
class LineChart;
}

// Centre column: the frame under review, transport, the sequence timeline,
// and the summary charts for whatever is currently loaded.
class ReviewPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ReviewPanel(QWidget* parent = nullptr);

    [[nodiscard]] SequenceView* sequenceView() const { return m_view; }
    [[nodiscard]] aquatrack::ui::EventTimeline* timeline() const { return m_timeline; }
    [[nodiscard]] aquatrack::ui::Filmstrip* filmstrip() const { return m_filmstrip; }

    void configureSequence(int frames, int frameRate);
    void showFrame(const QImage& image, const QVector<aquatrack::GroundTruth>& annotations,
                   int frame, int total, qint64 timeMs);
    void setPlaying(bool playing);
    void setAnalytics(const std::vector<aquatrack::analytics::TimeBin>& bins,
                      const aquatrack::analytics::SurveySummary& summary,
                      const std::vector<aquatrack::analytics::TrackSpan>& spans, int frameRate);
    void setTracksTable(const std::vector<aquatrack::analytics::TrackSpan>& spans);
    void setEventsTable(const std::vector<aquatrack::analytics::SurveyEvent>& events);
    void clearAnalytics();

signals:
    void playToggled();
    void frameRequested(int frame);
    void stepRequested(int delta);

private:
    QWidget* buildTransport();
    QWidget* buildAnalyticsRow(QWidget* parent);

    SequenceView* m_view = nullptr;
    aquatrack::ui::EventTimeline* m_timeline = nullptr;
    aquatrack::ui::Filmstrip* m_filmstrip = nullptr;

    QPushButton* m_play = nullptr;
    QSlider* m_frameSlider = nullptr;
    QLabel* m_clock = nullptr;

    QTabWidget* m_tabs = nullptr;
    QTableWidget* m_trackTable = nullptr;
    QTableWidget* m_eventTable = nullptr;

    aquatrack::ui::LineChart* m_countChart = nullptr;
    aquatrack::ui::DonutChart* m_speciesChart = nullptr;
    aquatrack::ui::LineChart* m_activityChart = nullptr;
    aquatrack::ui::BarList* m_topTracks = nullptr;
    QLabel* m_countTotal = nullptr;

    int m_frameRate = 15;
    int m_totalFrames = 0;
};
