#pragma once

#include <QFrame>
#include <QVector>

class QComboBox;
class QLabel;
class QListWidget;
class QSlider;

namespace aquatrack::ui {
class ToggleSwitch;
}

// Left rail: what is loaded, what it adds up to, and what is drawn on it.
class SurveyPanel final : public QFrame {
    Q_OBJECT

public:
    // Which overlays the review canvas should draw, and which observations
    // pass the reviewer's filter.
    struct Filters {
        bool showDetections = true;
        bool showTracks = true;
        bool showIds = true;
        int speciesClassId = -1;  // -1 = every species; 0 = Unknown
        double minConfidence = 0.0;
    };

    // One row of the sequence's contents: a clip, or a file of an image
    // sequence. `frame` is where it starts (0: not a place on the timeline).
    struct Entry {
        QString label;
        QString trailing;  // right-aligned, e.g. the clip's duration
        int frame = 0;
    };

    // What the left rail's summary shows.
    struct Totals {
        int annotations = 0;
        int tracks = 0;
        int peakInFrame = 0;
        int species = 0;
        int events = 0;
        QString dominantSpecies;
    };

    explicit SurveyPanel(QWidget* parent = nullptr);

    // `detail` is one line under the name, e.g. "4 clips · 2,264 frames".
    void setSequence(const QString& name, const QString& location, const QString& detail,
                     const QVector<Entry>& entries);
    void clearSequence();
    // Highlights the file (or clip) the playhead is in.
    void setCurrentFrame(int frame);
    void setTotals(const Totals& totals);
    void setSpecies(const QVector<QPair<int, QString>>& species);

    [[nodiscard]] Filters filters() const;

signals:
    void filtersChanged();
    void sequenceFileRequested(int frame);

private:
    QLabel* m_sequenceName = nullptr;
    QLabel* m_sequenceDetail = nullptr;
    QListWidget* m_sequenceFiles = nullptr;
    // (first frame, row) for rows that map to frames, sorted by frame.
    QVector<QPair<int, int>> m_rowsByFrame;
    int m_highlightedRow = -1;
    QLabel* m_annotationCount = nullptr;
    QLabel* m_trackCount = nullptr;
    QLabel* m_peakCount = nullptr;
    QLabel* m_speciesCount = nullptr;
    QLabel* m_eventCount = nullptr;

    QComboBox* m_species = nullptr;
    QSlider* m_confidence = nullptr;
    QLabel* m_confidenceValue = nullptr;
    aquatrack::ui::ToggleSwitch* m_showDetections = nullptr;
    aquatrack::ui::ToggleSwitch* m_showTracks = nullptr;
    aquatrack::ui::ToggleSwitch* m_showIds = nullptr;
};
