#pragma once

#include "analytics/SurveyAnalytics.h"
#include "domain/ScientificTypes.h"

#include <QFrame>
#include <QVector>

#include <optional>
#include <vector>

class QLabel;
class QListWidget;
class QPushButton;
class QVBoxLayout;

namespace aquatrack::ui {
class Badge;
class LineChart;
}

// Right rail: what looks wrong in the sequence, and the controls for
// correcting the frame under review. Totals live in the left rail.
class InsightPanel final : public QFrame {
    Q_OBJECT

public:
    explicit InsightPanel(QWidget* parent = nullptr);

    void setEvents(const std::vector<aquatrack::analytics::SurveyEvent>& events);
    void setIdSwitch(const std::optional<aquatrack::analytics::IdSwitchCandidate>& candidate,
                     int remaining);
    void setConfidenceSeries(const QVector<double>& values, double latest);
    void setCurrentObservations(const std::vector<aquatrack::domain::Observation>& observations);
    void setEditingEnabled(bool enabled);
    void setHistoryState(bool canUndo, bool canRedo, bool dirty);

signals:
    void relabelRequested(QString observationId);
    void deleteRequested(QString observationId);
    void undoRequested();
    void redoRequested();
    void saveRequested();
    void mergeTracksRequested(qint64 source, qint64 target);
    // A moment in the sequence, as a timestamp in milliseconds.
    void frameRequested(int timestampMs);
    // "All N events": show the full list.
    void allEventsRequested();

private:
    [[nodiscard]] QString selectedObservationId() const;

    QVBoxLayout* m_eventList = nullptr;
    QLabel* m_eventEmpty = nullptr;
    QPushButton* m_allEvents = nullptr;
    QLabel* m_eventCount = nullptr;

    QFrame* m_switchCard = nullptr;
    QLabel* m_switchSummary = nullptr;
    QLabel* m_switchAt = nullptr;
    QLabel* m_switchGap = nullptr;
    QLabel* m_switchSimilarity = nullptr;
    QLabel* m_switchRemaining = nullptr;
    aquatrack::ui::Badge* m_switchBadge = nullptr;
    QPushButton* m_merge = nullptr;
    QPushButton* m_reviewClip = nullptr;

    aquatrack::ui::LineChart* m_confidenceChart = nullptr;

    QLabel* m_frameCount = nullptr;
    QListWidget* m_observations = nullptr;
    QLabel* m_observationsEmpty = nullptr;
    QLabel* m_editHint = nullptr;
    QPushButton* m_relabel = nullptr;
    QPushButton* m_delete = nullptr;
    QPushButton* m_undo = nullptr;
    QPushButton* m_redo = nullptr;
    QPushButton* m_save = nullptr;

    std::optional<aquatrack::analytics::IdSwitchCandidate> m_candidate;
    bool m_editingEnabled = false;
};
