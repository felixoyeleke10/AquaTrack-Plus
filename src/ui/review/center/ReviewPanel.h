#pragma once

#include "analytics/SurveyAnalytics.h"
#include "data/BrackishMotSequence.h"
#include "ui/review/center/ActivityPanel.h"
#include "ui/review/center/Timeline.h"

#include <QWidget>

#include <optional>

class QActionGroup;
class ElidedLabel;
class QLabel;
class QProgressBar;
class QPushButton;
class QSlider;
class QSplitter;
class QTableWidget;
class QStackedWidget;
class QTabBar;
class LogView;
class SequenceView;

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
    [[nodiscard]] LogView* activityLog() const { return m_log; }

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
    void setEventsTable(const std::vector<aquatrack::analytics::SurveyEvent>& events);
    void clearAnalytics();
    // Heights of the panels under the picture (0 = collapsed). Applied once
    // the panel has been laid out, so they are absolute, not shares.
    struct PanelHeights {
        int activity = 200;
        int log = 200;
    };
    [[nodiscard]] PanelHeights panelHeights() const;
    void setPanelHeights(PanelHeights heights);

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
    // The reviewer dragged a panel's edge.
    void panelHeightsChanged();
    void detectorChosen(QString modelId);
    void manageModelsRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    // Narrow windows: icon-only Import, a shorter Detect, no source pill,
    // so the buttons keep their labels readable and the title shortens.
    void setCompactHeader(bool compact);
    QWidget* buildHeader();
    QWidget* buildTransport();

    SequenceView* m_view = nullptr;
    ActivityPanel* m_activity = nullptr;
    aquatrack::ui::TrackLanes* m_timeline = nullptr;  // inside m_activity
    aquatrack::ui::Filmstrip* m_filmstrip = nullptr;

    QWidget* m_titleRow = nullptr;  // the title with its status badge beside it
    ElidedLabel* m_title = nullptr;
    ElidedLabel* m_detail = nullptr;
    QLabel* m_source = nullptr;
    QPushButton* m_import = nullptr;
    QPushButton* m_analyze = nullptr;
    QWidget* m_split = nullptr;  // Detect & track and its detector, one capsule
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

    QPushButton* m_play = nullptr;
    QVector<QPushButton*> m_transport;  // the controls around Play
    QSlider* m_frameSlider = nullptr;
    QLabel* m_clock = nullptr;       // timecode of the frame on screen
    QLabel* m_clockTotal = nullptr;  // "/ duration"
    QLabel* m_frameCounter = nullptr;
    void showClock(int frame, int total, qint64 timeMs);

    QSplitter* m_logSplit = nullptr;      // picture | activity | log
    std::optional<PanelHeights> m_pendingHeights;  // to apply once laid out
    void applyPanelHeights();
    QTabBar* m_tabs = nullptr;            // Log | Events
    QStackedWidget* m_tabPages = nullptr;  // their pages
    LogView* m_log = nullptr;
    QTableWidget* m_eventTable = nullptr;

    int m_frameRate = 15;
    int m_totalFrames = 0;
    qint64 m_durationMs = 0;
};
