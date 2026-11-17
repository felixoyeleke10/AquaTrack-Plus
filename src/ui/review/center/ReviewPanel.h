#pragma once

#include "analytics/SurveyAnalytics.h"
#include "data/BrackishMotSequence.h"
#include "ui/review/center/ActivityPanel.h"
#include "ui/review/center/Timeline.h"

#include <QWidget>

class QActionGroup;
class QLabel;
class QProgressBar;
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
    // A detector Detect & track can run.
    struct DetectorOption {
        QString id;
        QString name;
    };

    explicit ReviewPanel(QWidget* parent = nullptr);

    [[nodiscard]] SequenceView* sequenceView() const { return m_view; }
    [[nodiscard]] aquatrack::ui::TrackLanes* timeline() const { return m_timeline; }
    [[nodiscard]] ActivityPanel* activity() const { return m_activity; }
    [[nodiscard]] aquatrack::ui::Filmstrip* filmstrip() const { return m_filmstrip; }

    void configureSequence(int frames, int frameRate, qint64 durationMs = -1);
    // `sourceSize`: pixel space of `annotations` (see SequenceView::setFrame).
    void showFrame(const QImage& image, const QVector<aquatrack::GroundTruth>& annotations,
                   int frame, int total, qint64 timeMs, QSize sourceSize = {});

    // Header above the canvas: what is open, and where its boxes come from.
    void setMediaTitle(const QString& title, const QString& detail);
    // `details`: tooltip, for example which model produced the boxes.
    void setAnnotationSource(const QString& text, const QColor& accent,
                             const QString& details = {});
    // The picker beside Detect & track. `forProject`: the open project
    // remembers the choice (otherwise it is the app's active detector).
    void setDetectorChoices(const QVector<DetectorOption>& options, const QString& current,
                            bool forProject);
    void setMediaActionsEnabled(bool enabled);
    // Detection/tracking run state. `total` <= 0 shows a busy indicator.
    void setAnalysisRunning(bool running);
    void setAnalysisProgress(int done, int total, const QString& text);
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
    // A moment chosen by time (an event row), in milliseconds.
    void timestampRequested(qint64 milliseconds);
    void stepRequested(int delta);
    void snapshotRequested();
    void fullscreenRequested();
    void importAnnotationsRequested();
    void analyzeRequested();
    void analysisCancelRequested();
    void detectorChosen(QString modelId);
    void manageModelsRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    // Narrow windows: icon-only Import, a shorter Detect, no source pill,
    // so the buttons keep their labels readable and the title shortens.
    void setCompactHeader(bool compact);
    QWidget* buildHeader();
    QWidget* buildTransport();
    QWidget* buildAnalyticsRow(QWidget* parent);

    SequenceView* m_view = nullptr;
    ActivityPanel* m_activity = nullptr;
    aquatrack::ui::TrackLanes* m_timeline = nullptr;  // inside m_activity
    aquatrack::ui::Filmstrip* m_filmstrip = nullptr;

    QLabel* m_title = nullptr;
    QLabel* m_detail = nullptr;
    QLabel* m_source = nullptr;
    QPushButton* m_import = nullptr;
    QPushButton* m_analyze = nullptr;
    QPushButton* m_detector = nullptr;
    QActionGroup* m_detectorGroup = nullptr;
    QString m_detectorCurrent;
    QString m_detectorName;
    void updateDetectorButton();
    QWidget* m_progressRow = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_progressText = nullptr;
    bool m_analysisRunning = false;
    bool m_compactHeader = false;
    QString m_sourceText;
    // Full header texts; the labels show them shortened to fit.
    QString m_titleText;
    QString m_detailText;
    void fitHeaderText();

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
    qint64 m_durationMs = 0;
};
