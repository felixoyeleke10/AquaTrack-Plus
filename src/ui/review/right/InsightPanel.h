#pragma once

#include "analytics/SurveyAnalytics.h"
#include "domain/ScientificTypes.h"

#include <QFrame>
#include <QVector>

#include <vector>

class QLabel;
class QListWidget;
class QPushButton;

namespace aquatrack::ui {
class Badge;
class InspectorTabs;
class LineChart;
}

// Right rail, an inspector of the moment under review and the tools to fix
// it, one tab per concern: what is annotated in this frame (and its edits),
// likely identity switches, and detection confidence. Whole-sequence lists
// (tracks, events, charts) live in the tabs under the timeline instead.
class InsightPanel final : public QFrame {
    Q_OBJECT

public:
    explicit InsightPanel(QWidget* parent = nullptr);

    void setIdSwitches(const std::vector<aquatrack::analytics::IdSwitchCandidate>& candidates);
    void setConfidenceSeries(const QVector<double>& values, double latest);
    // Figures under the confidence chart; `observations` 0 shows none.
    void setQuality(double meanConfidence, double lowConfidenceShare, double verifiedShare,
                    int observations);
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

private:
    QWidget* buildFramePage();
    QWidget* buildSwitchPage();
    QWidget* buildConfidencePage();
    void showSwitch(int index);
    [[nodiscard]] QString selectedObservationId() const;

    aquatrack::ui::InspectorTabs* m_tabs = nullptr;
    int m_framePage = -1;
    int m_switchPage = -1;
    int m_confidencePage = -1;

    // ---- In this frame
    QListWidget* m_observations = nullptr;
    QLabel* m_observationsEmpty = nullptr;
    QLabel* m_editHint = nullptr;
    QPushButton* m_relabel = nullptr;
    QPushButton* m_delete = nullptr;
    QPushButton* m_undo = nullptr;
    QPushButton* m_redo = nullptr;
    QPushButton* m_save = nullptr;

    // ---- ID switches
    std::vector<aquatrack::analytics::IdSwitchCandidate> m_candidates;
    int m_selectedSwitch = -1;
    QWidget* m_switchContent = nullptr;
    QLabel* m_switchEmpty = nullptr;
    QLabel* m_switchSummary = nullptr;
    QLabel* m_switchAt = nullptr;
    QLabel* m_switchGap = nullptr;
    QLabel* m_switchSimilarity = nullptr;
    aquatrack::ui::Badge* m_switchBadge = nullptr;
    QPushButton* m_merge = nullptr;
    QPushButton* m_reviewClip = nullptr;
    QLabel* m_switchListTitle = nullptr;
    QListWidget* m_switchList = nullptr;

    // ---- Confidence
    aquatrack::ui::LineChart* m_confidenceChart = nullptr;
    QLabel* m_meanConfidence = nullptr;
    QLabel* m_lowConfidence = nullptr;
    QLabel* m_verified = nullptr;

    bool m_editingEnabled = false;
};
