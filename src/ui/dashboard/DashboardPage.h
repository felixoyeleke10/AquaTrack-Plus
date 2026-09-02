#pragma once

#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

namespace aquatrack::ui {
class Badge;
class MetricTile;
}

// Home screen. Shows the state of the workspace and the next useful action,
// rather than describing the software to the person already running it.
class DashboardPage final : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(QWidget* parent = nullptr);

    void setProject(const QString& name, const QString& path, int mediaCount, bool unsaved);
    void setRecentProjects(const QStringList& paths);
    // Counts from the catalog, registry and open observation store.
    void setWorkspaceTotals(int datasets, int models, int observations, int tracks);

signals:
    void newProjectRequested();
    void openProjectRequested();
    void saveProjectRequested();
    void reviewRequested();
    void openSequenceRequested();
    void scanDatasetsRequested();
    void scanModelsRequested();
    void recentProjectActivated(QString path);

private:
    QLabel* m_projectName = nullptr;
    QLabel* m_projectPath = nullptr;
    QLabel* m_projectMedia = nullptr;
    aquatrack::ui::Badge* m_projectState = nullptr;
    QPushButton* m_save = nullptr;
    QPushButton* m_review = nullptr;

    QWidget* m_gettingStarted = nullptr;
    QWidget* m_projectDetail = nullptr;

    QListWidget* m_recent = nullptr;
    QLabel* m_recentEmpty = nullptr;

    aquatrack::ui::MetricTile* m_datasets = nullptr;
    aquatrack::ui::MetricTile* m_models = nullptr;
    aquatrack::ui::MetricTile* m_observations = nullptr;
    aquatrack::ui::MetricTile* m_tracks = nullptr;
};
