#pragma once

#include "application/ModelImporter.h"

#include <QDialog>
#include <QVector>

class QLabel;
class QPushButton;
class QTableWidget;

// Choose which trained models to bring into the project. Benchmarked,
// non-pilot runs are ticked to start with; pilots and smoke tests are listed
// but left for the reviewer to opt into.
class ModelImportDialog final : public QDialog {
    Q_OBJECT

public:
    ModelImportDialog(QVector<aquatrack::application::ModelCandidate> candidates,
                      const QString& destination, QWidget* parent = nullptr);

    [[nodiscard]] QVector<aquatrack::application::ModelCandidate> selected() const;
    void setChecked(int row, bool checked);

private:
    void updateSummary();

    QVector<aquatrack::application::ModelCandidate> m_candidates;
    QTableWidget* m_table = nullptr;
    QLabel* m_summary = nullptr;
    QPushButton* m_import = nullptr;
};
