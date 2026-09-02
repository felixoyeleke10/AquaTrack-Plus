#pragma once

#include "domain/ScientificTypes.h"

#include <QWidget>

#include <vector>

class QLabel;
class QPushButton;
class QTableWidget;
class QVBoxLayout;

namespace aquatrack::ui {
class BarList;
class DonutChart;
class LineChart;
class MetricTile;
}

// Survey analytics for whatever observation set is currently loaded: how much
// was seen, of what, when, and how well the tracker held identities.
class AnalyticsPage final : public QWidget {
    Q_OBJECT

public:
    explicit AnalyticsPage(QWidget* parent = nullptr);

    void setObservations(const std::vector<aquatrack::domain::Observation>& observations,
                         const QString& sourceName = {});

signals:
    void exportSummaryRequested();

private:
    void clearAll();

    std::vector<aquatrack::domain::Observation> m_observations;

    QLabel* m_source = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_export = nullptr;

    aquatrack::ui::MetricTile* m_totalObservations = nullptr;
    aquatrack::ui::MetricTile* m_uniqueTracks = nullptr;
    aquatrack::ui::MetricTile* m_speciesCount = nullptr;
    aquatrack::ui::MetricTile* m_peakInFrame = nullptr;
    aquatrack::ui::MetricTile* m_meanConfidence = nullptr;
    aquatrack::ui::MetricTile* m_duration = nullptr;

    aquatrack::ui::LineChart* m_abundance = nullptr;
    aquatrack::ui::DonutChart* m_composition = nullptr;
    aquatrack::ui::BarList* m_durations = nullptr;
    aquatrack::ui::LineChart* m_confidence = nullptr;

    QTableWidget* m_speciesTable = nullptr;
    QVBoxLayout* m_eventList = nullptr;
    QLabel* m_eventEmpty = nullptr;
};
