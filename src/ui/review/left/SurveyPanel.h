#pragma once

#include <QFrame>
#include <QVector>

class QComboBox;
class QLabel;
class QPushButton;
class QSlider;

namespace aquatrack::ui {
class Badge;
class ToggleSwitch;
}

// Left rail: what sequence is loaded, what is in it, and what is drawn on it.
class SurveyPanel final : public QFrame {
    Q_OBJECT

public:
    // Which overlays the review canvas should draw, and which observations
    // pass the reviewer's filter.
    struct Filters {
        bool showDetections = true;
        bool showTracks = true;
        bool showIds = true;
        int speciesClassId = 0;  // 0 = every species
        double minConfidence = 0.0;
    };

    explicit SurveyPanel(QWidget* parent = nullptr);

    void setSequenceInfo(const QString& name, int frames, int frameRate, int width, int height);
    void setCounts(int annotations, int tracks, int species, int events);
    void setSpecies(const QVector<QPair<int, QString>>& species);
    void setExportEnabled(bool enabled);

    [[nodiscard]] Filters filters() const;

signals:
    void openSequenceRequested();
    void exportCsvRequested();
    void exportMotRequested();
    void filtersChanged();

private:
    QLabel* m_sequenceName = nullptr;
    aquatrack::ui::Badge* m_stateBadge = nullptr;
    QLabel* m_recorded = nullptr;
    QLabel* m_duration = nullptr;
    QLabel* m_resolution = nullptr;
    QLabel* m_frameRate = nullptr;
    QLabel* m_source = nullptr;

    QLabel* m_annotationCount = nullptr;
    QLabel* m_trackCount = nullptr;
    QLabel* m_speciesCount = nullptr;
    QLabel* m_eventCount = nullptr;

    QComboBox* m_species = nullptr;
    QSlider* m_confidence = nullptr;
    QLabel* m_confidenceValue = nullptr;
    aquatrack::ui::ToggleSwitch* m_showDetections = nullptr;
    aquatrack::ui::ToggleSwitch* m_showTracks = nullptr;
    aquatrack::ui::ToggleSwitch* m_showIds = nullptr;

    QPushButton* m_open = nullptr;
    QPushButton* m_csv = nullptr;
    QPushButton* m_mot = nullptr;
};
