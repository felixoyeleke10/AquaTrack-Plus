#include "ui/MainWindow.h"

#include "application/AppState.h"
#include "application/controllers/DatasetController.h"
#include "application/controllers/ExportController.h"
#include "application/controllers/ModelController.h"
#include "application/controllers/ProjectController.h"
#include "application/controllers/SettingsController.h"
#include "ui/AppTheme.h"
#include "ui/analytics/AnalyticsPage.h"
#include "ui/dashboard/DashboardPage.h"
#include "ui/datasets/DatasetsPage.h"
#include "ui/models/ModelsPage.h"
#include "ui/navigation/AppHeader.h"
#include "ui/review/ReviewPage.h"
#include "ui/settings/SettingsPage.h"
#include "ui/videos/VideosPage.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStandardPaths>

#include <set>
#include <QStatusBar>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_appState(new aquatrack::application::AppState(this)),
      m_projectController(new aquatrack::application::ProjectController(this)),
      m_modelController(new aquatrack::application::ModelController(this)),
      m_datasetController(new aquatrack::application::DatasetController(this)),
      m_settingsController(new aquatrack::application::SettingsController(
          QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
              QStringLiteral("/settings.json"),
          this)) {
  setWindowTitle(QStringLiteral("AquaTrack+"));
  resize(1500, 920);
  setMinimumSize(1120, 720);
  auto *central = new QWidget(this);
  auto *layout = new QVBoxLayout(central);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  m_header = new AppHeader(central);
  m_pages = new QStackedWidget(central);
  m_dashboard = new DashboardPage(m_pages);
  m_videos = new VideosPage(m_pages);
  m_review = new ReviewPage(m_pages);
  m_analytics = new AnalyticsPage(m_pages);
  m_datasets = new DatasetsPage(m_pages);
  m_models = new ModelsPage(m_pages);
  m_settings = new SettingsPage(m_pages);
  m_pages->addWidget(m_dashboard);
  m_pages->addWidget(m_videos);
  m_pages->addWidget(m_review);
  m_pages->addWidget(m_analytics);
  m_pages->addWidget(m_datasets);
  m_pages->addWidget(m_models);
  m_pages->addWidget(m_settings);
  layout->addWidget(m_header);
  layout->addWidget(m_pages, 1);
  setCentralWidget(central);
  setStyleSheet(aquatrack::ui::applicationStyleSheet());
  statusBar()->showMessage(QStringLiteral("Ready | Native C++ runtime"));

  connect(m_header, &AppHeader::pageRequested, this, [this](int page) {
    if (page ==
        static_cast<int>(aquatrack::application::AppState::Page::Analytics))
      refreshAnalytics();
    m_appState->setCurrentPage(
        static_cast<aquatrack::application::AppState::Page>(page));
  });
  connect(m_dashboard, &DashboardPage::reviewRequested, this, [this] {
    m_appState->setCurrentPage(aquatrack::application::AppState::Page::Review);
  });
  connect(m_dashboard, &DashboardPage::saveProjectRequested, this, [this] {
    QString error;
    if (!m_projectController->saveProject(&error))
      QMessageBox::warning(this, QStringLiteral("Could not save project"), error);
    else
      refreshDashboard();
  });
  connect(m_dashboard, &DashboardPage::openSequenceRequested, this, [this] {
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Open BrackishMOT sequence"));
    if (directory.isEmpty())
      return;
    m_review->loadSequence(directory);
    refreshAnalytics();
    refreshDashboard();
    m_appState->setCurrentPage(aquatrack::application::AppState::Page::Review);
  });
  connect(m_dashboard, &DashboardPage::scanDatasetsRequested, this, [this] {
    const QString root = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select dataset root"));
    if (root.isEmpty())
      return;
    m_datasetController->scan(root);
    m_appState->setCurrentPage(aquatrack::application::AppState::Page::Datasets);
  });
  connect(m_dashboard, &DashboardPage::scanModelsRequested, this, [this] {
    const QString root = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select model registry"));
    if (root.isEmpty())
      return;
    m_modelController->scan(root);
    m_appState->setCurrentPage(aquatrack::application::AppState::Page::Models);
  });
  connect(m_dashboard, &DashboardPage::recentProjectActivated, this,
          [this](const QString &path) {
            QString error;
            if (!m_projectController->openProject(path, &error))
              QMessageBox::warning(this, QStringLiteral("Could not open project"),
                                   error);
          });
  connect(m_dashboard, &DashboardPage::newProjectRequested, this, [this] {
    bool accepted = false;
    const QString name =
        QInputDialog::getText(this, QStringLiteral("New AquaTrack project"),
                              QStringLiteral("Project name"), QLineEdit::Normal,
                              QString(), &accepted);
    if (!accepted || name.trimmed().isEmpty())
      return;
    QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Create project"), name + QStringLiteral(".aqp"),
        QStringLiteral("AquaTrack project (*.aqp)"));
    if (path.isEmpty())
      return;
    if (!path.endsWith(QStringLiteral(".aqp"), Qt::CaseInsensitive))
      path += QStringLiteral(".aqp");
    QString error;
    if (!m_projectController->createProject(path, name, &error))
      QMessageBox::warning(this, QStringLiteral("Could not create project"),
                           error);
  });
  connect(m_dashboard, &DashboardPage::openProjectRequested, this, [this] {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open project"), QString(),
        QStringLiteral("AquaTrack project (*.aqp)"));
    if (path.isEmpty())
      return;
    QString error;
    if (!m_projectController->openProject(path, &error))
      QMessageBox::warning(this, QStringLiteral("Could not open project"),
                           error);
  });
  connect(m_videos, &VideosPage::addMediaRequested, this, [this] {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Add survey videos"), QString(),
        QStringLiteral("Video files (*.mp4 *.mov *.mkv *.avi);;All files (*)"));
    addProjectMedia(files);
  });
  connect(m_videos, &VideosPage::addSequenceRequested, this, [this] {
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Add BrackishMOT sequence"));
    if (!directory.isEmpty())
      addProjectMedia({directory});
  });
  connect(m_videos, &VideosPage::removeMediaRequested, this,
          [this](const QString &path) {
            QString error;
            if (!m_projectController->removeMedia(path, &error)) {
              QMessageBox::warning(this, QStringLiteral("Could not remove media"),
                                   error);
              return;
            }
            if (!m_projectController->saveProject(&error))
              QMessageBox::warning(this, QStringLiteral("Could not save project"),
                                   error);
            refreshDashboard();
          });
  // BISECT: reviewRequested temporarily disconnected.
  connect(
      m_projectController,
      &aquatrack::application::ProjectController::projectChanged, this, [this] {
        const auto *project = m_projectController->project();
        const QString projectName =
            project ? QString::fromStdString(project->name) : QString();
        m_header->setProjectName(projectName);
        m_videos->setProject(project);
        refreshDashboard();
        if (project) {
          QString storageError;
          if (!m_review->setObservationStore(
                  m_projectController->observationStorePath(), &storageError))
            QMessageBox::warning(
                this, QStringLiteral("Could not open observation store"),
                storageError);
        }
        statusBar()->showMessage(
            project ? QStringLiteral("Project ready: %1")
                          .arg(QString::fromStdString(project->name))
                    : QStringLiteral("No project open"));
      });
  connect(m_datasets, &DatasetsPage::scanRequested, this, [this] {
    const QString root = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select dataset root"));
    if (!root.isEmpty())
      m_datasetController->scan(root);
  });
  connect(m_projectController,
          &aquatrack::application::ProjectController::projectSaved, this,
          [this](const QString &path) {
            m_settingsController->rememberRecentProject(path);
            refreshDashboard();
          });
  connect(m_datasetController,
          &aquatrack::application::DatasetController::catalogChanged, this,
          [this] { refreshDashboard(); });
  connect(m_modelController,
          &aquatrack::application::ModelController::registryChanged, this,
          [this] { refreshDashboard(); });
  connect(m_datasets, &DatasetsPage::refreshRequested, m_datasetController,
          &aquatrack::application::DatasetController::rescan);
  connect(m_datasetController,
          &aquatrack::application::DatasetController::catalogChanged, this, [this] {
            m_datasets->setCatalog(m_datasetController->root(),
                                   m_datasetController->result());
          });
  connect(m_analytics, &AnalyticsPage::exportSummaryRequested, this, [this] {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export survey summary"),
        QStringLiteral("survey_observations.csv"),
        QStringLiteral("CSV files (*.csv)"));
    if (path.isEmpty())
      return;
    QString error;
    aquatrack::application::ExportController exporter;
    if (!exporter.exportCsv(path, m_review->observations(), &error))
      QMessageBox::warning(this, QStringLiteral("Export failed"), error);
  });
  connect(m_datasets, &DatasetsPage::reviewRequested, this,
          [this](const QString &path) {
            m_review->loadSequence(path);
            refreshAnalytics();
            m_appState->setCurrentPage(
                aquatrack::application::AppState::Page::Review);
          });
  connect(m_models, &ModelsPage::scanRequested, this, [this] {
    const QString root = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select model registry"));
    if (!root.isEmpty())
      m_modelController->scan(root);
  });
  connect(m_models, &ModelsPage::refreshRequested, m_modelController,
          &aquatrack::application::ModelController::rescan);
  connect(m_modelController,
          &aquatrack::application::ModelController::registryChanged, this,
          [this] {
            m_models->setRegistry(m_modelController->root(),
                                  m_modelController->entries(),
                                  m_modelController->warnings());
            m_models->setActiveDetector(QString::fromStdString(
                m_settingsController->settings().detection.modelId));
          });
  // Choosing a detector here is the only thing that writes settings'
  // detection.modelId; the settings page has no field for it.
  connect(m_models, &ModelsPage::activeDetectorRequested, this,
          [this](const QString &modelId) {
            auto updated = m_settingsController->settings();
            updated.detection.modelId = modelId.toStdString();
            QString error;
            if (!m_settingsController->save(updated, &error)) {
              QMessageBox::warning(this,
                                   QStringLiteral("Could not save active model"),
                                   error);
              return;
            }
            m_models->setActiveDetector(modelId);
            statusBar()->showMessage(
                QStringLiteral("Active detector set to %1").arg(modelId), 4000);
          });
  connect(
      m_settingsController,
      &aquatrack::application::SettingsController::settingsChanged, this,
      [this] {
        m_settings->setSettings(m_settingsController->settings());
        m_models->setActiveDetector(QString::fromStdString(
            m_settingsController->settings().detection.modelId));
      });
  connect(m_settings, &SettingsPage::saveRequested, this, [this] {
    QString error;
    auto updated = m_settings->settings();
    // SettingsPage has no detector field, so carry the stored id through
    // rather than clearing it on every save.
    updated.detection.modelId = m_settingsController->settings().detection.modelId;
    if (!m_settingsController->save(updated, &error))
      QMessageBox::warning(this, QStringLiteral("Could not save settings"),
                           error);
    else
      statusBar()->showMessage(QStringLiteral("Settings saved"), 3000);
  });
  QString settingsError;
  if (!m_settingsController->load(&settingsError))
    statusBar()->showMessage(
        QStringLiteral("Settings warning: %1").arg(settingsError));
  connect(m_appState, &aquatrack::application::AppState::currentPageChanged,
          this, [this](aquatrack::application::AppState::Page page) {
            const int index = static_cast<int>(page);
            m_pages->setCurrentIndex(index);
            m_header->setCurrentPage(index);
          });
  m_pages->setCurrentIndex(static_cast<int>(m_appState->currentPage()));
  m_header->setCurrentPage(static_cast<int>(m_appState->currentPage()));
}

void MainWindow::openSequence(const QString &directory) {
  m_review->loadSequence(directory);
  m_appState->setCurrentPage(aquatrack::application::AppState::Page::Review);
}

void MainWindow::openRegistry(const QString &directory) {
  m_modelController->scan(directory);
  m_appState->setCurrentPage(aquatrack::application::AppState::Page::Models);
}

void MainWindow::refreshAnalytics() {
  const auto *project = m_projectController->project();
  m_analytics->setObservations(
      m_review->observations(),
      project ? QString::fromStdString(project->name)
              : QStringLiteral("Active review sequence"));
}

void MainWindow::openDatasets(const QString &directory) {
  m_datasetController->scan(directory);
  m_appState->setCurrentPage(aquatrack::application::AppState::Page::Datasets);
}

void MainWindow::addProjectMedia(const QStringList &paths) {
  if (paths.isEmpty())
    return;
  QString error;
  for (const auto &path : paths) {
    if (!m_projectController->addMedia(path, &error)) {
      QMessageBox::warning(this, QStringLiteral("Could not add media"), error);
      return;
    }
  }
  if (!m_projectController->saveProject(&error))
    QMessageBox::warning(this, QStringLiteral("Could not save project"), error);
}

void MainWindow::refreshDashboard() {
  const auto *project = m_projectController->project();
  m_dashboard->setProject(
      project ? QString::fromStdString(project->name) : QString(),
      m_projectController->projectPath(),
      project ? int(project->mediaPaths.size()) : 0, false);
  m_dashboard->setRecentProjects(recentProjects());

  const auto observations = m_review->observations();
  std::set<aquatrack::domain::LocalTrackId> tracks;
  for (const auto &observation : observations)
    tracks.insert(observation.localTrackId);
  m_dashboard->setWorkspaceTotals(
      int(m_datasetController->result().datasets.size()),
      int(m_modelController->entries().size()), int(observations.size()),
      int(tracks.size()));
}

QStringList MainWindow::recentProjects() const {
  QStringList paths;
  for (const auto &path :
       m_settingsController->settings().storage.recentProjects)
    paths << QString::fromStdString(path);
  return paths;
}

void MainWindow::openProject(const QString &path) {
  QString error;
  if (!m_projectController->openProject(path, &error))
    QMessageBox::warning(this, QStringLiteral("Could not open project"), error);
}
