#pragma once

#include "data/catalog/DatasetCatalog.h"

#include <QHash>
#include <QPixmap>
#include <QSet>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTimer;

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
    // Which sequences the open project already references, so rows can say
    // "In project" and Add does not offer them twice.
    void setProjectMedia(bool projectOpen, const QStringList& mediaPaths);
    // Root paths of the selected sequences that can be reviewed.
    [[nodiscard]] QStringList selectedSequencePaths() const;

    // List (sortable columns) or grid (cards with thumbnails).
    void setGridView(bool grid);
    [[nodiscard]] bool isGridView() const;
    // Opens the image inspector on the current dataset.
    void inspectCurrent();

signals:
    void scanRequested();
    void refreshRequested();
    void reviewRequested(QString sequencePath);
    // Sequences to add to the project; the window creates a project first
    // when none is open.
    void addToProjectRequested(QStringList sequencePaths);

private:
    void applyFilter();
    void showDetail();
    void updateProjectAction();
    void requestAddSelected();
    void rebuildGrid();
    void syncGridFromTable();
    void syncTableFromGrid();
    void queueThumbnails();
    void loadSomeThumbnails();
    void showContextMenu(const QPoint& globalPosition);
    void activateCurrent();
    // Dataset index shown by a table row (rows are sortable, so this is
    // stored on the row rather than derived from its position).
    [[nodiscard]] int datasetIndexAtRow(int row) const;
    [[nodiscard]] int rowForDatasetIndex(int index) const;
    [[nodiscard]] const aquatrack::data::DatasetDescriptor* selectedDataset() const;
    [[nodiscard]] QVector<const aquatrack::data::DatasetDescriptor*> selectedDatasets() const;
    [[nodiscard]] bool inProject(const QString& path) const;

    QVector<aquatrack::data::DatasetDescriptor> m_datasets;
    QVector<int> m_visible;
    // Selection captured before the backing list is replaced. m_visible maps
    // rows into the *old* list, so it must not be read once the list changes.
    QString m_pendingSelection;

    QLabel* m_rootLabel = nullptr;
    QLabel* m_summary = nullptr;
    QPushButton* m_refresh = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_formatFilter = nullptr;
    QTableWidget* m_table = nullptr;
    QListWidget* m_grid = nullptr;
    QStackedWidget* m_views = nullptr;
    QPushButton* m_listViewButton = nullptr;
    QPushButton* m_gridViewButton = nullptr;
    bool m_syncingSelection = false;
    // Thumbnails by dataset id, decoded a few at a time.
    QHash<QString, QPixmap> m_thumbnails;
    QStringList m_thumbnailQueue;
    QTimer* m_thumbnailTimer = nullptr;

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
    QPushButton* m_addToProject = nullptr;
    QPushButton* m_inspect = nullptr;
    QPushButton* m_reveal = nullptr;
    QLabel* m_selectionNote = nullptr;
    aquatrack::ui::Badge* m_detailOrigin = nullptr;

    bool m_projectOpen = false;
    QSet<QString> m_projectMedia;  // normalised, case-folded paths

    QWidget* m_warningCard = nullptr;
    QListWidget* m_warningList = nullptr;
};
