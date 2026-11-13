#pragma once

#include "data/BrackishMotSequence.h"
#include "data/catalog/DatasetCatalog.h"

#include <QDialog>
#include <QImage>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace aquatrack::ui {
class InspectorCanvas;
}

// A window for looking inside any dataset, whatever its format: step through
// its images with the annotation boxes drawn and labelled by class. YOLO
// detection sets are browsed split by split; BrackishMOT sequences frame by
// frame, with track IDs.
class DatasetInspector final : public QDialog {
    Q_OBJECT

public:
    explicit DatasetInspector(aquatrack::data::DatasetDescriptor dataset,
                              QWidget* parent = nullptr);

    // For tests and scripting.
    [[nodiscard]] int itemCount() const { return int(m_items.size()); }
    [[nodiscard]] int currentIndex() const { return m_current; }
    [[nodiscard]] int boxCount() const;
    [[nodiscard]] QStringList sources() const;
    void selectSource(int index);
    void showIndex(int index);

signals:
    // "Open in review" for a BrackishMOT sequence.
    void reviewRequested(QString sequencePath);

private:
    struct Item {
        QString image;
        int frame = 0;  // BrackishMOT frame number; 0 for YOLO images
    };

    void loadSource(int index);
    void loadCurrent();
    void applyListFilter();
    void step(int delta);

    aquatrack::data::DatasetDescriptor m_dataset;
    aquatrack::BrackishMotSequence m_sequence;
    bool m_isSequence = false;
    QVector<Item> m_items;
    int m_current = -1;
    QString m_labelFolder;  // YOLO: labels folder for the current split

    QComboBox* m_source = nullptr;
    QLineEdit* m_filter = nullptr;
    QListWidget* m_list = nullptr;
    aquatrack::ui::InspectorCanvas* m_canvas = nullptr;
    QLabel* m_position = nullptr;
    QLabel* m_fileName = nullptr;
    QLabel* m_imageInfo = nullptr;
    QListWidget* m_legend = nullptr;
    QCheckBox* m_showLabels = nullptr;
};
