#pragma once

#include "domain/ScientificTypes.h"

#include <QWidget>

#include <vector>

class QCheckBox;
class QLabel;
class QPushButton;

namespace aquatrack::ui {
class MetricTile;
}

// The last step of the workflow: what is about to leave the application,
// in which format, and where it went. Exports the observations of the
// sequence open in Review.
class ExportPage final : public QWidget {
    Q_OBJECT

public:
    explicit ExportPage(QWidget* parent = nullptr);

    void setObservations(std::vector<aquatrack::domain::Observation> observations,
                         const QString& sourceName);
    void setIncludeUnverified(bool include);
    // Where the save dialogs start: the project's exports/ (CSV) and
    // annotations/ (MOTChallenge gt.txt). Empty when no project is open.
    void setDefaultFolders(const QString& exports, const QString& annotations);

    // The observations an export would write with the current options.
    [[nodiscard]] std::vector<aquatrack::domain::Observation> exportable() const;

signals:
    // The reviewer changed the option; the window persists it to settings.
    void includeUnverifiedChanged(bool include);
    void reviewRequested();

private:
    enum class Format { Csv, MotChallenge };
    void refresh();
    void exportAs(Format format);

    std::vector<aquatrack::domain::Observation> m_observations;
    QString m_sourceName;

    QLabel* m_source = nullptr;
    QWidget* m_emptyState = nullptr;
    QWidget* m_content = nullptr;
    aquatrack::ui::MetricTile* m_rows = nullptr;
    aquatrack::ui::MetricTile* m_tracks = nullptr;
    aquatrack::ui::MetricTile* m_verified = nullptr;
    aquatrack::ui::MetricTile* m_frames = nullptr;
    QCheckBox* m_includeUnverified = nullptr;
    QPushButton* m_csv = nullptr;
    QPushButton* m_mot = nullptr;
    QLabel* m_result = nullptr;
    QPushButton* m_reveal = nullptr;
    QString m_lastExport;
    QString m_exportsFolder;
    QString m_annotationsFolder;
};
