#pragma once

#include <QMainWindow>
#include <QStringList>

namespace aquatrack::application {
class AppState;
class ProjectController;
class ModelController;
class SettingsController;
class DatasetController;
} // namespace aquatrack::application
class AppHeader;
class DashboardPage;
class QStackedWidget;
class VideosPage;
class ModelsPage;
class SettingsPage;
class ReviewPage;
class AnalyticsPage;
class DatasetsPage;

class MainWindow final : public QMainWindow {
public:
  explicit MainWindow(QWidget *parent = nullptr);

  // Opens a BrackishMOT sequence directory and shows the review workspace.
  void openSequence(const QString &directory);

  // Scans a model registry directory and shows the models page.
  void openRegistry(const QString &directory);

  // Scans a dataset root and shows the datasets page.
  void openDatasets(const QString &directory);

  // Opens an existing .aqp project.
  void openProject(const QString &path);

private:
  void refreshAnalytics();
  void refreshDashboard();
  void addProjectMedia(const QStringList &paths);
  [[nodiscard]] QStringList recentProjects() const;

  aquatrack::application::AppState *m_appState = nullptr;
  aquatrack::application::ProjectController *m_projectController = nullptr;
  aquatrack::application::ModelController *m_modelController = nullptr;
  aquatrack::application::SettingsController *m_settingsController = nullptr;
  aquatrack::application::DatasetController *m_datasetController = nullptr;
  AppHeader *m_header = nullptr;
  QStackedWidget *m_pages = nullptr;
  DashboardPage *m_dashboard = nullptr;
  VideosPage *m_videos = nullptr;
  ReviewPage *m_review = nullptr;
  AnalyticsPage *m_analytics = nullptr;
  DatasetsPage *m_datasets = nullptr;
  ModelsPage *m_models = nullptr;
  SettingsPage *m_settings = nullptr;
};
