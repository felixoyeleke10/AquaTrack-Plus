#pragma once

#include "data/catalog/DatasetCatalog.h"

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

// Dataset catalog browser: what is on disk, whether its imagery is actually
// there, and a route from a sequence straight into review.
class DatasetsPage final : public QWidget {
    Q_OBJECT

public:
    explicit DatasetsPage(QWidget* parent = nullptr);

    void setCatalog(const QString& root, const aquatrack::data::CatalogScanResult& result);

signals:
    void scanRequested();
    void refreshRequested();
    void reviewRequested(QString sequencePath);

private:
    void applyFilter();
    void showDetail();
    [[nodiscard]] const aquatrack::data::DatasetDescriptor* selectedDataset() const;

    QVector<aquatrack::data::DatasetDescriptor> m_datasets;
    QVector<int> m_visible;

    QLabel* m_rootLabel = nullptr;
    QLabel* m_summary = nullptr;
    QPushButton* m_refresh = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_formatFilter = nullptr;
    QTableWidget* m_table = nullptr;

    QWidget* m_detailBody = nullptr;
    QLabel* m_detailEmpty = nullptr;
    QLabel* m_detailName = nullptr;
    QLabel* m_detailPath = nullptr;
    aquatrack::ui::Badge* m_detailFormat = nullptr;
    aquatrack::ui::Badge* m_detailState = nullptr;
    QLabel* m_detailFrames = nullptr;
    QLabel* m_detailImages = nullptr;
    QLabel* m_detailAnnotations = nullptr;
    QLabel* m_detailClasses = nullptr;
    QLabel* m_splitsCaption = nullptr;
    aquatrack::ui::BarList* m_detailSplits = nullptr;
    QPushButton* m_review = nullptr;
    QPushButton* m_reveal = nullptr;

    QWidget* m_warningCard = nullptr;
    QListWidget* m_warningList = nullptr;
};
