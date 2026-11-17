#pragma once

#include "data/ModelRegistry.h"

#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QTableWidget;

namespace aquatrack::ui {
class Badge;
class BarList;
}

// Model registry browser: what is on disk, whether it is actually usable, and
// which detector the pipeline is configured to use.
class ModelsPage final : public QWidget {
    Q_OBJECT

public:
    explicit ModelsPage(QWidget* parent = nullptr);

    void setRegistry(const QString& root, const QVector<aquatrack::data::ModelEntry>& entries,
                     const QStringList& warnings);
    // The detector id currently stored in settings, so the page can show which
    // entry is live and stop offering to re-select it.
    void setActiveDetector(const QString& modelId);

signals:
    void scanRequested();
    // Bring trained models into the project's models folder.
    void importRunsRequested();
    void importWeightsRequested();
    void refreshRequested();
    void activeDetectorRequested(QString modelId);
    // The Datasets page, its sibling in "Models & data".
    void datasetsRequested();

private:
    void applyFilter();
    void showDetail(int entryIndex);
    [[nodiscard]] const aquatrack::data::ModelEntry* selectedEntry() const;

    QVector<aquatrack::data::ModelEntry> m_entries;
    QVector<int> m_visible;  // row -> index into m_entries
    // Selection captured before the backing list is replaced. m_visible maps
    // rows into the *old* list, so it must not be read once the list changes.
    QString m_pendingSelection;
    QString m_activeDetector;

    QLabel* m_rootLabel = nullptr;
    QLabel* m_summary = nullptr;
    QPushButton* m_refresh = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_typeFilter = nullptr;
    QTableWidget* m_table = nullptr;

    QWidget* m_detailBody = nullptr;
    QLabel* m_detailEmpty = nullptr;
    QLabel* m_detailName = nullptr;
    QLabel* m_detailId = nullptr;
    aquatrack::ui::Badge* m_detailType = nullptr;
    aquatrack::ui::Badge* m_detailState = nullptr;
    QLabel* m_detailBackend = nullptr;
    QLabel* m_detailPrecision = nullptr;
    QLabel* m_detailInput = nullptr;
    QLabel* m_detailArtifact = nullptr;
    QLabel* m_detailClasses = nullptr;
    aquatrack::ui::BarList* m_detailMetrics = nullptr;
    QPushButton* m_activate = nullptr;
    QPushButton* m_reveal = nullptr;

    QWidget* m_warningCard = nullptr;
    QListWidget* m_warningList = nullptr;
};
