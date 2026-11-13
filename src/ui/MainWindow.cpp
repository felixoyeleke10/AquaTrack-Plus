#include "ui/MainWindow.h"

#include "application/AnalysisRunner.h"
#include "application/AppState.h"
#include "application/MediaImporter.h"
#include "application/ModelImporter.h"
#include "application/ProjectLayout.h"
#include "application/controllers/DatasetController.h"
#include "application/controllers/ExportController.h"
#include "application/controllers/ModelController.h"
#include "application/controllers/ProjectController.h"
#include "application/controllers/SettingsController.h"
#include "ui/AppTheme.h"
#include "ui/analytics/AnalyticsPage.h"
#include "ui/dashboard/DashboardPage.h"
#include "ui/datasets/DatasetsPage.h"
#include "ui/export/ExportPage.h"
#include "ui/models/ModelImportDialog.h"
#include "ui/models/ModelsPage.h"
#include "ui/navigation/AppHeader.h"
#include "ui/navigation/NavigationDock.h"
#include "ui/review/ReviewPage.h"
#include "ui/settings/SettingsPage.h"
#include "ui/videos/VideosPage.h"
#include "data/media/MediaSource.h"
#include "data/media/MediaTools.h"
#include "data/media/VideoProbe.h"
#include "data/media/VideoSequenceManifest.h"
#include "application/ReviewSession.h"
#include "application/SequenceEditing.h"
#include "ui/videos/SequenceDialog.h"

#include <QCheckBox>
#include <QCollator>
#include <QRegularExpression>
#include <QDirIterator>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QProgressDialog>
#include <QStorageInfo>
#include <QUrl>
#include <QDir>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPushButton>
#include <QWindow>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QVBoxLayout>

#include <algorithm>
#include <set>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

using Page = aquatrack::application::AppState::Page;

namespace {
// How far inside the window edge the pointer can be and still grab it.
constexpr int kResizeMargin = 6;
}  // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_appState(new aquatrack::application::AppState(this)),
      m_projectController(new aquatrack::application::ProjectController(this)),
      m_modelController(new aquatrack::application::ModelController(this)),
      m_settingsController(new aquatrack::application::SettingsController(
          QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
              QStringLiteral("/settings.json"),
          this)),
      m_datasetController(new aquatrack::application::DatasetController(this)) {
  setWindowTitle(QStringLiteral("AquaTrack+"));
  // Frameless: the app header is the title bar. The system-menu and
  // min/max hints keep the taskbar button able to minimize and restore the
  // window, which a bare frameless window on Windows cannot do.
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                 Qt::WindowMinMaxButtonsHint);
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
  m_export = new ExportPage(m_pages);
  m_pages->addWidget(m_dashboard);
  m_pages->addWidget(m_videos);
  m_pages->addWidget(m_review);
  m_pages->addWidget(m_analytics);
  m_pages->addWidget(m_datasets);
  m_pages->addWidget(m_models);
  m_pages->addWidget(m_settings);
  m_pages->addWidget(m_export);  // index 7: AppState::Page::Export
  layout->addWidget(m_header);
  layout->addWidget(m_pages, 1);
  // Page navigation docks along the bottom; the title bar carries the project.
  m_dock = new NavigationDock(central);
  layout->addWidget(m_dock);
  setCentralWidget(central);
  setStyleSheet(aquatrack::ui::applicationStyleSheet());
  // Status messages show in the page bar, beside the product name, so the
  // window ends in one strip rather than a page bar over a status bar.
  connect(statusBar(), &QStatusBar::messageChanged, m_dock,
          &NavigationDock::setStatusMessage);
  statusBar()->hide();
  statusBar()->showMessage(QStringLiteral("Ready | Native C++ runtime"));

  // ---- Header ---------------------------------------------------------
  connect(m_dock, &NavigationDock::pageRequested, this, [this](int page) {
    if (page == static_cast<int>(Page::Analytics))
      refreshAnalytics();
    if (page == static_cast<int>(Page::Export))
      refreshExport();
    m_appState->setCurrentPage(static_cast<Page>(page));
  });
  connect(m_header, &AppHeader::settingsRequested, this,
          [this] { m_appState->setCurrentPage(Page::Settings); });
  connect(m_header, &AppHeader::helpRequested, this, &MainWindow::showHelp);
  connect(m_header, &AppHeader::warningsRequested, this, &MainWindow::showWarnings);
  connect(m_header, &AppHeader::projectRequested, this,
          [this] { m_appState->setCurrentPage(Page::Dashboard); });

  // ---- Dashboard ------------------------------------------------------
  connect(m_dashboard, &DashboardPage::reviewRequested, this,
          [this] { m_appState->setCurrentPage(Page::Review); });
  connect(m_dashboard, &DashboardPage::mediaReviewRequested, this,
          [this](const QString &path, int frame) {
            if (reviewSequence(path) && frame > 0)
              m_review->goToFrame(frame);
          });
  connect(m_dashboard, &DashboardPage::createSequenceRequested, this,
          [this] { createVideoSequence(); });
  connect(m_dashboard, &DashboardPage::editSequenceRequested, this,
          &MainWindow::editVideoSequence);
  connect(m_videos, &VideosPage::editSequenceRequested, this, &MainWindow::editVideoSequence);
  connect(m_dashboard, &DashboardPage::manageMediaRequested, this,
          [this] { m_appState->setCurrentPage(Page::Videos); });
  connect(m_dashboard, &DashboardPage::addVideosRequested, this, [this] {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Add survey videos"), QString(),
        aquatrack::media::videoFileDialogFilter());
    addProjectMedia(files);
  });
  connect(m_dashboard, &DashboardPage::exportRequested, this, [this] {
    refreshExport();
    m_appState->setCurrentPage(Page::Export);
  });
  connect(m_dashboard, &DashboardPage::saveProjectRequested, this, [this] {
    QString error;
    if (!m_projectController->saveProject(&error))
      QMessageBox::warning(this, QStringLiteral("Could not save project"), error);
    refreshDashboard();
  });
  connect(m_dashboard, &DashboardPage::openSequenceRequested, this, [this] {
    // One dialog for every kind of media: pick a video, or the seqinfo.ini /
    // sequence.json that marks a sequence folder.
    QStringList patterns;
    for (const auto &extension : aquatrack::media::videoExtensions())
      patterns << QStringLiteral("*.") + extension;
    const QString chosen = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open media"), QString(),
        QStringLiteral("Videos and sequences (%1 seqinfo.ini sequence.json);;%2")
            .arg(patterns.join(QLatin1Char(' ')), aquatrack::media::videoFileDialogFilter()));
    if (chosen.isEmpty())
      return;
    const QString name = QFileInfo(chosen).fileName().toLower();
    reviewSequence(name == QLatin1String("seqinfo.ini") || name == QLatin1String("sequence.json")
                       ? QFileInfo(chosen).absolutePath()
                       : chosen);
  });
  connect(m_dashboard, &DashboardPage::scanDatasetsRequested, this, [this] {
    const QString root = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select dataset root"));
    if (!root.isEmpty())
      openDatasets(root);
  });
  connect(m_dashboard, &DashboardPage::scanModelsRequested, this, [this] {
    const QString root = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select model registry"));
    if (!root.isEmpty())
      openRegistry(root);
  });
  connect(m_dashboard, &DashboardPage::recentProjectActivated, this,
          [this](const QString &path) {
            if (QFileInfo::exists(path))
              openProjectFile(path);
            else
              resolveMissingRecentProject(path);
          });
  connect(m_dashboard, &DashboardPage::recentProjectRemoveRequested, this,
          [this](const QString &path) {
            if (m_settingsController->forgetRecentProject(path))
              statusBar()->showMessage(QStringLiteral("Removed %1 from recent projects")
                                           .arg(QFileInfo(path).completeBaseName()),
                                       4000);
          });
  connect(m_dashboard, &DashboardPage::clearMissingRecentRequested, this, [this] {
    const int removed = m_settingsController->forgetMissingRecentProjects();
    statusBar()->showMessage(
        QStringLiteral("Removed %1 missing project(s) from the recent list").arg(removed), 4000);
  });
  connect(m_dashboard, &DashboardPage::newProjectRequested, this,
          &MainWindow::createProject);
  connect(m_dashboard, &DashboardPage::openProjectRequested, this, [this] {
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open project"), QString(),
        QStringLiteral("AquaTrack project (*.aqp)"));
    if (!path.isEmpty())
      openProjectFile(path);
  });

  // ---- Videos ---------------------------------------------------------
  connect(m_videos, &VideosPage::addMediaRequested, this, [this] {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Add survey videos"), QString(),
        aquatrack::media::videoFileDialogFilter());
    addProjectMedia(files);
  });
  connect(m_videos, &VideosPage::addVideoFolderRequested, this, &MainWindow::addVideoFolder);
  connect(m_videos, &VideosPage::addSequenceRequested, this, &MainWindow::addSequenceFolder);
  connect(m_videos, &VideosPage::createSequenceRequested, this,
          [this](const QStringList &videos) { createVideoSequence(videos); });
  connect(m_videos, &VideosPage::reviewAtFrameRequested, this,
          [this](const QString &path, int frame) {
            if (reviewSequence(path))
              m_review->goToFrame(frame);
          });
  connect(m_videos, &VideosPage::analyzeRequested, this, [this](const QString &path) {
    if (reviewSequence(path))
      runAnalysis();
  });
  connect(m_dashboard, &DashboardPage::addSequenceFolderRequested, this,
          &MainWindow::addSequenceFolder);
  connect(m_dashboard, &DashboardPage::browseDatasetsRequested, this, [this] {
    // Straight to the catalog if one is open; otherwise ask for its root.
    if (!m_datasetController->root().isEmpty()) {
      m_appState->setCurrentPage(Page::Datasets);
      return;
    }
    const QString root = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select a folder of sequences to browse"));
    if (!root.isEmpty())
      openDatasets(root);
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
  connect(m_videos, &VideosPage::reviewRequested, this,
          [this](const QString &path) { reviewSequence(path); });
  connect(m_videos, &VideosPage::copyIntoProjectRequested, this, [this](const QString &path) {
    if (!m_review->maybeSaveAnnotations())
      return;
    const QString before = aquatrack::media::MediaSource::reviewIdFor(path);
    const QStringList copies = copyIntoProject({path});
    if (copies.isEmpty())
      return;
    // The copy replaces the link. Annotations are stored under the media's
    // name; if the copy had to be renamed, move them to the new name.
    QString error;
    const QString after = aquatrack::media::MediaSource::reviewIdFor(copies.first());
    if (!m_review->migrateAnnotations(before, after, &error))
      QMessageBox::warning(this, QStringLiteral("Annotations not moved"),
                           QStringLiteral("The copy is named %1 but its annotations are still "
                                          "stored under %2: %3")
                               .arg(after, before, error));
    m_projectController->removeMedia(path);
    m_projectController->addMedia(copies.first());
    if (!m_projectController->saveProject(&error))
      QMessageBox::warning(this, QStringLiteral("Could not save project"), error);
    statusBar()->showMessage(
        QStringLiteral("Copied %1 into the project folder").arg(QFileInfo(path).fileName()), 5000);
  });
  connect(m_dashboard, &DashboardPage::openProjectFolderRequested, this, [this] {
    if (m_projectController->project())
      QDesktopServices::openUrl(QUrl::fromLocalFile(
          aquatrack::application::ProjectLayout(m_projectController->projectPath()).root()));
  });

  // ---- Review ---------------------------------------------------------
  // Edits, sequence loads and store switches all change the totals shown
  // on the dashboard and the analytics page.
  connect(m_review, &ReviewPage::sequenceOpened, this, &MainWindow::includeInProject);
  connect(m_review, &ReviewPage::analyzeRequested, this, &MainWindow::runAnalysis);
  connect(m_review, &ReviewPage::observationsChanged, this, [this] {
    refreshDashboard();
    refreshAnalytics();
    refreshExport();
  });

  // ---- Export ---------------------------------------------------------
  connect(m_export, &ExportPage::reviewRequested, this,
          [this] { m_appState->setCurrentPage(Page::Review); });
  connect(m_export, &ExportPage::includeUnverifiedChanged, this, [this](bool include) {
    auto updated = m_settingsController->settings();
    if (updated.exportSettings.includeUnverified == include)
      return;
    updated.exportSettings.includeUnverified = include;
    QString error;
    if (!m_settingsController->save(updated, &error))
      statusBar()->showMessage(QStringLiteral("Could not save export option: %1").arg(error),
                               5000);
  });

  // ---- Project --------------------------------------------------------
  connect(
      m_projectController,
      &aquatrack::application::ProjectController::projectChanged, this, [this] {
        const auto *project = m_projectController->project();
        const QString projectName =
            project ? QString::fromStdString(project->name) : QString();
        m_header->setProjectName(projectName);
        m_videos->setProject(project,
                             project ? aquatrack::application::ProjectLayout(
                                           m_projectController->projectPath())
                                           .root()
                                     : QString());
        applyProjectFolders();
        QStringList media;
        if (project)
          for (const auto &path : project->mediaPaths)
            media << QString::fromStdString(path);
        m_datasets->setProjectMedia(project != nullptr, media);
        refreshDashboard();
        statusBar()->showMessage(
            project ? QStringLiteral("Project ready: %1")
                          .arg(QString::fromStdString(project->name))
                    : QStringLiteral("No project open"));
      });
  // Opening or creating a project switches the observation store. Media
  // edits also emit projectChanged, so this is tied to the path instead.
  connect(m_projectController,
          &aquatrack::application::ProjectController::projectChanged, this,
          [this, store = QString()]() mutable {
            if (!m_projectController->project())
              return;
            const QString path = m_projectController->observationStorePath();
            if (path == store)
              return;
            // Media on screen belongs to the project being closed: close it
            // rather than carry it (and its annotations) into this one. With
            // no project before, it is adopted by the first project opened.
            const bool switching = !store.isEmpty();
            if (switching)
              m_review->closeMedia();
            QString storageError;
            if (m_review->setObservationStore(path, &storageError)) {
              store = path;
              if (!switching)
                includeInProject(m_review->sequenceDirectory());
            } else
              QMessageBox::warning(this,
                                   QStringLiteral("Could not open observation store"),
                                   storageError);
          });
  connect(m_projectController,
          &aquatrack::application::ProjectController::projectSaved, this,
          [this](const QString &path) {
            m_settingsController->rememberRecentProject(path);
            refreshDashboard();
          });

  // ---- Datasets -------------------------------------------------------
  connect(m_datasets, &DatasetsPage::scanRequested, this, [this] {
    const QString root = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select dataset root"));
    if (!root.isEmpty())
      m_datasetController->scan(root);
  });
  connect(m_datasets, &DatasetsPage::refreshRequested, m_datasetController,
          &aquatrack::application::DatasetController::rescan);
  connect(m_datasets, &DatasetsPage::reviewRequested, this,
          [this](const QString &path) { reviewSequence(path); });
  connect(m_datasets, &DatasetsPage::addToProjectRequested, this,
          [this](const QStringList &paths) {
            if (!m_projectController->project()) {
              const auto answer = QMessageBox::question(
                  this, QStringLiteral("Add to a project"),
                  QStringLiteral("Sequences are added to a project, and none is open. "
                                 "Create a new project for these %1 sequence(s)?")
                      .arg(paths.size()),
                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
              if (answer != QMessageBox::Yes)
                return;
              createProject();
              if (!m_projectController->project())
                return;  // cancelled or failed; createProject already said why
            }
            const auto before = m_projectController->project()->mediaPaths.size();
            addProjectMedia(paths);
            const auto added = m_projectController->project()->mediaPaths.size() - before;
            statusBar()->showMessage(
                QStringLiteral("Added %1 sequence(s) to %2. They are listed on the Videos page.")
                    .arg(added)
                    .arg(QString::fromStdString(m_projectController->project()->name)),
                6000);
          });
  connect(m_datasetController,
          &aquatrack::application::DatasetController::catalogChanged, this, [this] {
            m_datasets->setCatalog(m_datasetController->root(),
                                   m_datasetController->result());
            refreshDashboard();
            refreshWarnings();
          });

  // ---- Analytics ------------------------------------------------------
  connect(m_analytics, &AnalyticsPage::exportSummaryRequested, this, [this] {
    QString suggested = QStringLiteral("survey_observations.csv");
    if (m_projectController->project()) {
      const aquatrack::application::ProjectLayout layout(m_projectController->projectPath());
      if (!layout.ensure(layout.exports()).isEmpty())
        suggested = QDir(layout.exports()).filePath(suggested);
    }
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Export survey summary"), suggested,
        QStringLiteral("CSV files (*.csv)"));
    if (path.isEmpty())
      return;
    QString error;
    aquatrack::application::ExportController exporter;
    if (!exporter.exportCsv(path, m_review->observations(), &error))
      QMessageBox::warning(this, QStringLiteral("Export failed"), error);
  });

  // ---- Models ---------------------------------------------------------
  connect(m_models, &ModelsPage::scanRequested, this, [this] {
    const QString root = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select model registry"));
    if (!root.isEmpty())
      m_modelController->scan(root);
  });
  connect(m_models, &ModelsPage::importRunsRequested, this, [this] {
    if (!m_projectController->project()) {
      QMessageBox::information(
          this, QStringLiteral("Open a project first"),
          QStringLiteral("Imported models are stored in the open project's models folder."));
      return;
    }
    // Start where this checkout keeps its training runs, if it can be found.
    QString python;
    QString package;
    QString runs;
    if (aquatrack::application::AnalysisRunner::locatePython(
            QString::fromStdString(m_settingsController->settings().analysis.pythonExecutable),
            &python, &package, nullptr) &&
        !package.isEmpty())
      runs = QDir(QFileInfo(package).absolutePath()).filePath(QStringLiteral("runs"));
    const QString root = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Select training runs folder"),
        QFileInfo(runs).isDir() ? runs : QString());
    if (root.isEmpty())
      return;
    const auto candidates = aquatrack::application::ModelImporter::discover(root);
    if (candidates.isEmpty()) {
      QMessageBox::information(
          this, QStringLiteral("No trained models found"),
          QStringLiteral("No best.pt, best.onnx, best.engine or best.torchscript checkpoint "
                         "was found below %1.")
              .arg(QDir::toNativeSeparators(root)));
      return;
    }
    importModelCandidates(candidates);
  });
  connect(m_models, &ModelsPage::importWeightsRequested, this, [this] {
    if (!m_projectController->project()) {
      QMessageBox::information(
          this, QStringLiteral("Open a project first"),
          QStringLiteral("Imported models are stored in the open project's models folder."));
      return;
    }
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Select model weights"), QString(),
        QStringLiteral("Model weights (*.pt *.pth *.onnx *.engine *.torchscript);;"
                       "All files (*)"));
    if (files.isEmpty())
      return;
    QVector<aquatrack::application::ModelCandidate> candidates;
    candidates.reserve(files.size());
    for (const auto &file : files)
      candidates.push_back(aquatrack::application::ModelImporter::describe(file));
    importModelCandidates(std::move(candidates));
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
            refreshDashboard();
            refreshWarnings();
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

  // ---- Settings -------------------------------------------------------
  connect(
      m_settingsController,
      &aquatrack::application::SettingsController::settingsChanged, this,
      [this] {
        m_settings->setSettings(m_settingsController->settings());
        m_review->setFrameCacheSize(m_settingsController->settings().video.frameBufferSize);
        m_export->setIncludeUnverified(
            m_settingsController->settings().exportSettings.includeUnverified);
        m_models->setActiveDetector(QString::fromStdString(
            m_settingsController->settings().detection.modelId));
        refreshDashboard();
      });
  connect(m_settings, &SettingsPage::saveRequested, this, [this] {
    QString error;
    // SettingsPage starts from the settings it was last shown, so fields
    // it has no control for (recent projects, export options...) survive.
    auto updated = m_settings->settings();
    // The active model may have changed on the Models page since then.
    updated.detection.modelId = m_settingsController->settings().detection.modelId;
    updated.storage.recentProjects =
        m_settingsController->settings().storage.recentProjects;
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
          this, [this](Page page) {
            const int index = static_cast<int>(page);
            // Leaving Review: remember where it stopped, before the
            // dashboard reads it.
            if (page != Page::Review)
              recordReviewSession();
            m_pages->setCurrentIndex(index);
            m_dock->setCurrentPage(index);
            m_header->setSettingsActive(page == Page::Settings);
          });
  m_pages->setCurrentIndex(static_cast<int>(m_appState->currentPage()));
  m_dock->setCurrentPage(static_cast<int>(m_appState->currentPage()));
  refreshDashboard();
  refreshWarnings();
  if (aquatrack::media::ffmpegTool(QStringLiteral("ffmpeg")).isEmpty())
    statusBar()->showMessage(aquatrack::media::ffmpegMissingMessage());
  applyNativeWindowStyle();
}

void MainWindow::applyNativeWindowStyle() {
  // Creating the native window now gives a QWindow to watch for resizing.
  winId();
  if (QWindow *handle = windowHandle())
    handle->installEventFilter(this);
#ifdef Q_OS_WIN
  // Windows 11: rounded corners and a hairline border in the theme colour,
  // so the frameless window still reads as a window against any backdrop.
  // Both attributes are ignored on older Windows, which is harmless.
  const auto hwnd = reinterpret_cast<HWND>(winId());
  constexpr DWORD kCornerPreference = 33;  // DWMWA_WINDOW_CORNER_PREFERENCE
  constexpr DWORD kBorderColor = 34;       // DWMWA_BORDER_COLOR
  const int rounded = 2;                   // DWMWCP_ROUND
  DwmSetWindowAttribute(hwnd, kCornerPreference, &rounded, sizeof(rounded));
  const QColor line = aquatrack::ui::palette::Line;
  const COLORREF border = RGB(line.red(), line.green(), line.blue());
  DwmSetWindowAttribute(hwnd, kBorderColor, &border, sizeof(border));
#endif
}

Qt::Edges MainWindow::resizeEdgesAt(const QPoint &position) const {
  if (isMaximized() || isFullScreen())
    return {};
  Qt::Edges edges;
  if (position.x() < kResizeMargin)
    edges |= Qt::LeftEdge;
  if (position.x() >= width() - kResizeMargin)
    edges |= Qt::RightEdge;
  if (position.y() < kResizeMargin)
    edges |= Qt::TopEdge;
  if (position.y() >= height() - kResizeMargin)
    edges |= Qt::BottomEdge;
  return edges;
}

void MainWindow::updateResizeCursor(Qt::Edges edges) {
  if (edges == m_resizeCursorEdges)
    return;
  Qt::CursorShape shape = Qt::ArrowCursor;
  if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge))
    shape = Qt::SizeFDiagCursor;
  else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge))
    shape = Qt::SizeBDiagCursor;
  else if (edges & (Qt::LeftEdge | Qt::RightEdge))
    shape = Qt::SizeHorCursor;
  else if (edges & (Qt::TopEdge | Qt::BottomEdge))
    shape = Qt::SizeVerCursor;
  // An override cursor wins over whatever widget sits under the edge.
  if (!m_resizeCursorEdges && edges)
    QGuiApplication::setOverrideCursor(shape);
  else if (m_resizeCursorEdges && edges)
    QGuiApplication::changeOverrideCursor(shape);
  else
    QGuiApplication::restoreOverrideCursor();
  m_resizeCursorEdges = edges;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
  if (watched == windowHandle()) {
    switch (event->type()) {
    case QEvent::MouseMove: {
      const auto *mouse = static_cast<QMouseEvent *>(event);
      if (mouse->buttons() == Qt::NoButton)
        updateResizeCursor(resizeEdgesAt(mouse->position().toPoint()));
      break;
    }
    case QEvent::MouseButtonPress: {
      const auto *mouse = static_cast<QMouseEvent *>(event);
      const Qt::Edges edges = resizeEdgesAt(mouse->position().toPoint());
      if (mouse->button() == Qt::LeftButton && edges) {
        updateResizeCursor({});
        windowHandle()->startSystemResize(edges);
        return true;
      }
      break;
    }
    case QEvent::Leave:
      updateResizeCursor({});
      break;
    default:
      break;
    }
  }
  return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent *event) {
  if (event->type() == QEvent::WindowStateChange) {
    m_header->setWindowMaximized(isMaximized());
    if (isMaximized() || isFullScreen())
      updateResizeCursor({});
  }
  QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (!m_review->maybeSaveAnnotations()) {
    event->ignore();
    return;
  }
  recordReviewSession();
  event->accept();
}

bool MainWindow::reviewSequence(const QString &directory) {
  if (!m_review->loadSequence(directory))
    return false;
  m_appState->setCurrentPage(Page::Review);
  return true;
}

void MainWindow::openSequence(const QString &directory) {
  reviewSequence(directory);
}

void MainWindow::openRegistry(const QString &directory) {
  m_modelController->scan(directory);
  m_appState->setCurrentPage(Page::Models);
}

void MainWindow::importModelCandidates(
    QVector<aquatrack::application::ModelCandidate> candidates) {
  if (candidates.isEmpty() || !m_projectController->project())
    return;

  const aquatrack::application::ProjectLayout layout(m_projectController->projectPath());
  const QString destination = layout.ensure(layout.models());
  if (destination.isEmpty()) {
    QMessageBox::warning(
        this, QStringLiteral("Could not prepare model registry"),
        QStringLiteral("Could not create %1")
            .arg(QDir::toNativeSeparators(layout.models())));
    return;
  }

  ModelImportDialog dialog(std::move(candidates), destination, this);
  if (dialog.exec() != QDialog::Accepted)
    return;

  int imported = 0;
  QStringList errors;
  for (const auto &candidate : dialog.selected()) {
    QString error;
    if (aquatrack::application::ModelImporter::import(candidate, destination, nullptr,
                                                       &error))
      ++imported;
    else
      errors << error;
  }

  // The project registry is the source of truth after an import, even if an
  // external registry had been open before.
  m_modelController->scan(destination);
  if (!errors.isEmpty()) {
    QMessageBox::warning(
        this, QStringLiteral("Some models could not be imported"),
        errors.join(QLatin1Char('\n')));
  }
  statusBar()->showMessage(
      imported == 1 ? QStringLiteral("Imported 1 model into the project registry")
                    : QStringLiteral("Imported %1 models into the project registry").arg(imported),
      5000);
}

void MainWindow::openDatasets(const QString &directory) {
  m_datasetController->scan(directory);
  m_appState->setCurrentPage(Page::Datasets);
}

void MainWindow::openProject(const QString &path) { openProjectFile(path); }

bool MainWindow::openProjectFile(const QString &path) {
  if (!m_review->maybeSaveAnnotations())
    return false;
  recordReviewSession();
  QString error;
  if (!m_projectController->openProject(path, &error)) {
    QMessageBox::warning(this, QStringLiteral("Could not open project"), error);
    return false;
  }
  m_settingsController->rememberRecentProject(m_projectController->projectPath());
  refreshDashboard();
  return true;
}

void MainWindow::resolveMissingRecentProject(const QString &path) {
  const QFileInfo info(path);
  QMessageBox box(QMessageBox::Warning, QStringLiteral("Project not found"),
                  QStringLiteral("<b>%1</b> is no longer at<br>%2")
                      .arg(info.completeBaseName().toHtmlEscaped(),
                           QDir::toNativeSeparators(info.absolutePath()).toHtmlEscaped()),
                  QMessageBox::NoButton, this);
  box.setInformativeText(QStringLiteral(
      "If it was moved or renamed, locate the .aqp file. If its drive is not "
      "connected, connect it and try again."));
  QPushButton *locate = box.addButton(QStringLiteral("Locate..."), QMessageBox::AcceptRole);
  QPushButton *remove =
      box.addButton(QStringLiteral("Remove from list"), QMessageBox::DestructiveRole);
  box.addButton(QMessageBox::Cancel);
  box.setDefaultButton(locate);
  box.exec();
  if (box.clickedButton() == remove) {
    m_settingsController->forgetRecentProject(path);
    return;
  }
  if (box.clickedButton() != locate)
    return;
  // Start the search from the closest folder that still exists.
  QDir start = info.absoluteDir();
  while (!start.exists() && start.cdUp()) {
  }
  const QString found = QFileDialog::getOpenFileName(
      this, QStringLiteral("Locate %1").arg(info.fileName()),
      start.exists() ? start.absolutePath() : QString(),
      QStringLiteral("AquaTrack project (*.aqp)"));
  if (found.isEmpty())
    return;
  // The new location becomes the recent entry; the stale one goes.
  if (openProjectFile(found))
    m_settingsController->forgetRecentProject(path);
}

void MainWindow::createProject() {
  if (!m_review->maybeSaveAnnotations())
    return;
  bool accepted = false;
  const QString name =
      QInputDialog::getText(this, QStringLiteral("New AquaTrack project"),
                            QStringLiteral("Project name"), QLineEdit::Normal,
                            QString(), &accepted)
          .trimmed();
  if (!accepted || name.isEmpty())
    return;

  // A project is a folder: the .aqp manifest and the observation store live
  // together, instead of scattering observations.json into whatever folder
  // the file happened to be saved in.
  const QString folderName = aquatrack::application::ProjectLayout::folderNameFor(name);

  QString start = QString::fromStdString(
      m_settingsController->settings().storage.defaultProjectDirectory);
  if (start.isEmpty() || !QFileInfo(start).isDir())
    start = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const QString parent = QFileDialog::getExistingDirectory(
      this, QStringLiteral("Choose where to create the \u201c%1\u201d project folder").arg(name),
      start);
  if (parent.isEmpty())
    return;

  const QString folder = QDir(parent).filePath(folderName);
  const QString path = QDir(folder).filePath(folderName + QStringLiteral(".aqp"));
  if (QFileInfo::exists(path)) {
    QMessageBox::warning(
        this, QStringLiteral("Project already exists"),
        QStringLiteral("There is already a project called \u201c%1\u201d in\n%2\n\nOpen it "
                       "from the Dashboard, or choose another name or location.")
            .arg(name, QDir::toNativeSeparators(parent)));
    return;
  }
  if (!QDir().mkpath(folder)) {
    QMessageBox::warning(this, QStringLiteral("Could not create project"),
                         QStringLiteral("Could not create the folder %1")
                             .arg(QDir::toNativeSeparators(folder)));
    return;
  }
  QString error;
  if (!m_projectController->createProject(path, name, &error)) {
    QMessageBox::warning(this, QStringLiteral("Could not create project"), error);
    return;
  }

  // Next time, offer the same place.
  auto updated = m_settingsController->settings();
  updated.storage.defaultProjectDirectory = QDir::cleanPath(parent).toStdString();
  m_settingsController->save(updated);
  statusBar()->showMessage(
      QStringLiteral("Created %1 in %2").arg(name, QDir::toNativeSeparators(folder)), 6000);
}

void MainWindow::addSequenceFolder() {
  if (!m_projectController->project()) {
    QMessageBox::information(this, QStringLiteral("Add a sequence"),
                             QStringLiteral("Create or open a project first: sequences are "
                                            "added to a project."));
    return;
  }
  const QString directory = QFileDialog::getExistingDirectory(
      this, QStringLiteral("Add an existing sequence folder"));
  if (directory.isEmpty())
    return;
  const auto kind = aquatrack::media::classifyMedia(directory);
  if (kind != aquatrack::media::MediaKind::ImageSequence &&
      kind != aquatrack::media::MediaKind::VideoSequence) {
    QMessageBox::warning(
        this, QStringLiteral("Not a sequence folder"),
        QStringLiteral("%1 is not a sequence folder. Choose the folder that holds the sequence: "
                       "one with a sequence.json (made by New sequence), or a BrackishMOT folder "
                       "with seqinfo.ini, img1/ and gt/gt.txt. To make a sequence from videos, "
                       "select them and choose New sequence.")
            .arg(QDir::toNativeSeparators(directory)));
    return;
  }
  addProjectMedia({directory});
}

void MainWindow::addVideoFolder() {
  if (!m_projectController->project())
    return;
  const QString folder = QFileDialog::getExistingDirectory(
      this, QStringLiteral("Add every video in a folder"));
  if (folder.isEmpty())
    return;
  QStringList patterns;
  for (const auto &extension : aquatrack::media::videoExtensions())
    patterns << QStringLiteral("*.") + extension;
  QStringList videos;
  QDirIterator walk(folder, patterns, QDir::Files, QDirIterator::Subdirectories);
  while (walk.hasNext())
    videos << QDir::cleanPath(walk.next());
  if (videos.isEmpty()) {
    QMessageBox::information(this, QStringLiteral("No videos found"),
                             QStringLiteral("No video files were found in %1.")
                                 .arg(QDir::toNativeSeparators(folder)));
    return;
  }
  // Natural order: video_2 before video_10.
  QCollator collator;
  collator.setNumericMode(true);
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  std::sort(videos.begin(), videos.end(), collator);
  addProjectMedia(videos);
}

namespace {

bool samePathIgnoringCase(const QString &left, const QString &right) {
  return QDir::cleanPath(QFileInfo(left).absoluteFilePath())
             .compare(QDir::cleanPath(QFileInfo(right).absoluteFilePath()),
                      Qt::CaseInsensitive) == 0;
}

} // namespace

QString MainWindow::sequenceNameProblem(const QString &name,
                                        const QString &exceptPath) const {
  const auto *project = m_projectController->project();
  if (!project)
    return {};
  // Annotations are stored by name: two media items must not share one.
  for (const auto &existing : project->mediaPaths) {
    const QString path = QString::fromStdString(existing);
    if (!exceptPath.isEmpty() && samePathIgnoringCase(path, exceptPath))
      continue;
    if (aquatrack::media::MediaSource::reviewIdFor(path) == name)
      return QStringLiteral("Another media item is already called '%1'; "
                            "annotations are stored by name, so choose another.")
          .arg(name);
  }
  if (exceptPath.isEmpty()) {
    const aquatrack::application::ProjectLayout layout(m_projectController->projectPath());
    const QString folder =
        QDir(layout.sequences()).filePath(aquatrack::application::ProjectLayout::folderNameFor(name));
    if (QFileInfo::exists(folder))
      return QStringLiteral("The project's media folder already has '%1'.")
          .arg(QFileInfo(folder).fileName());
  }
  return {};
}

void MainWindow::createVideoSequence(const QStringList &selectedVideos) {
  if (!m_projectController->project()) {
    QMessageBox::information(
        this, QStringLiteral("New sequence"),
        QStringLiteral("Create or open a project first: sequences are stored in its media folder."));
    return;
  }
  QStringList sources = selectedVideos;
  if (sources.isEmpty())
    sources = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Choose the videos for the new sequence"), QString(),
        aquatrack::media::videoFileDialogFilter());
  if (sources.isEmpty())
    return;
  // Start in filename order (numbers in numeric order); the dialog lets the
  // reviewer put them in the order they were filmed.
  QCollator collator;
  collator.setNumericMode(true);
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  std::sort(sources.begin(), sources.end(), [&collator](const QString &left, const QString &right) {
    return collator.compare(QFileInfo(left).fileName(), QFileInfo(right).fileName()) < 0;
  });

  // Probing reads each file's frame count and rate once; the results are
  // cached in sequence.json so opening the sequence later is instant.
  QProgressDialog progress(QStringLiteral("Reading videos..."), QStringLiteral("Cancel"), 0,
                           int(sources.size()), this);
  progress.setWindowTitle(QStringLiteral("New sequence"));
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(300);
  QVector<aquatrack::media::VideoInfo> clips;
  QStringList unreadable;
  for (int index = 0; index < sources.size(); ++index) {
    progress.setValue(index);
    progress.setLabelText(QStringLiteral("Reading %1 (%2 of %3)...")
                              .arg(QFileInfo(sources.at(index)).fileName())
                              .arg(index + 1)
                              .arg(sources.size()));
    QCoreApplication::processEvents();
    if (progress.wasCanceled())
      return;
    aquatrack::media::VideoInfo info;
    QString error;
    if (aquatrack::media::probeVideo(sources.at(index), info, &error))
      clips.push_back(std::move(info));
    else
      unreadable << QStringLiteral("%1: %2").arg(QFileInfo(sources.at(index)).fileName(), error);
  }
  progress.setValue(int(sources.size()));
  if (!unreadable.isEmpty())
    QMessageBox::warning(this, QStringLiteral("Some videos cannot be used"),
                         unreadable.join(QLatin1Char('\n')));
  if (clips.isEmpty())
    return;

  // Suggest a name from the first and last file ("video_045 to video_048").
  const QString first = QFileInfo(clips.first().path).completeBaseName();
  const QString last = QFileInfo(clips.last().path).completeBaseName();
  const QString suggested =
      clips.size() == 1 ? first : QStringLiteral("%1 to %2").arg(first, last);
  SequenceDialog dialog(QStringLiteral("New sequence"), suggested, clips,
                        [this](const QString &name) { return sequenceNameProblem(name); }, this);
  if (dialog.exec() != QDialog::Accepted)
    return;

  const QString name = dialog.name();
  const aquatrack::application::ProjectLayout layout(m_projectController->projectPath());
  const QString folder =
      QDir(layout.sequences()).filePath(aquatrack::application::ProjectLayout::folderNameFor(name));
  aquatrack::media::VideoSequenceManifest manifest;
  manifest.name = name;
  manifest.clips = dialog.clips();
  QString error;
  if (!manifest.save(folder, &error)) {
    QMessageBox::warning(this, QStringLiteral("Could not create the sequence"), error);
    return;
  }
  if (!m_projectController->addMedia(folder, &error) || !m_projectController->saveProject(&error)) {
    QMessageBox::warning(this, QStringLiteral("Could not add the sequence"), error);
    return;
  }
  refreshDashboard();
  statusBar()->showMessage(
      QStringLiteral("Created sequence '%1': %2 videos, %3")
          .arg(name)
          .arg(manifest.clips.size())
          .arg(aquatrack::media::formatDuration(manifest.totalDurationMs())),
      8000);
  const auto answer = QMessageBox::question(
      this, QStringLiteral("Sequence created"),
      QStringLiteral("'%1' plays %2 videos back to back (%3 frames, %4). Open it in "
                     "Review now?")
          .arg(name)
          .arg(manifest.clips.size())
          .arg(QLocale::system().toString(manifest.totalFrames()),
               aquatrack::media::formatDuration(manifest.totalDurationMs())),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
  if (answer == QMessageBox::Yes)
    reviewSequence(folder);
  else
    m_videos->openFolder(folder);
}

void MainWindow::editVideoSequence(const QString &folder) {
  aquatrack::media::VideoSequenceManifest before;
  QString error;
  if (!before.load(folder, &error)) {
    QMessageBox::warning(this, QStringLiteral("Cannot edit this sequence"), error);
    return;
  }
  // Fills in probe data that is missing or stale. A missing clip is not an
  // error here: removing it is one reason to edit.
  (void)before.refresh();

  const bool openInReview = samePathIgnoringCase(m_review->sequenceDirectory(), folder);
  const int frameOnScreen = openInReview ? m_review->currentFrame() : 0;
  if (openInReview && !m_review->maybeSaveAnnotations())
    return;

  const QString oldName = before.name;
  const auto stored = m_review->storedObservations(oldName);
  SequenceDialog dialog(QStringLiteral("Edit sequence"), oldName, before.clips,
                        [this, folder, oldName](const QString &name) {
                          return name == oldName ? QString() : sequenceNameProblem(name, folder);
                        },
                        this);
  if (!stored.empty())
    dialog.setNote(QStringLiteral("%1 saved annotations move with their clips. Annotations on a "
                                  "clip you remove are deleted.")
                       .arg(QLocale::system().toString(int(stored.size()))));
  if (dialog.exec() != QDialog::Accepted)
    return;

  const QString name = dialog.name();
  const auto after = dialog.clips();
  bool changed = name != oldName || after.size() != before.clips.size();
  for (int i = 0; !changed && i < after.size(); ++i)
    changed = !samePathIgnoringCase(after.at(i).path, before.clips.at(i).path);
  if (!changed)
    return;

  const auto remap = aquatrack::application::remapSequenceObservations(
      stored, before.clips, after, name.toStdString());
  if (remap.dropped > 0) {
    const auto answer = QMessageBox::warning(
        this, QStringLiteral("Delete annotations?"),
        QStringLiteral("%1 annotation(s) were made on %2, which you removed from the sequence. "
                       "They will be deleted.\n\nSave the sequence anyway?")
            .arg(QLocale::system().toString(remap.dropped),
                 remap.droppedClips.join(QStringLiteral(", "))),
        QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::Save)
      return;
  }

  // Nothing may hold the old numbering while it changes: close the
  // sequence in Review, write the clips, then move the annotations.
  if (openInReview)
    m_review->closeMedia();
  aquatrack::media::VideoSequenceManifest updated;
  updated.name = name;
  updated.clips = after;
  if (!updated.save(folder, &error)) {
    QMessageBox::warning(this, QStringLiteral("Could not save the sequence"), error);
    if (openInReview)
      reviewSequence(folder);
    return;
  }
  if (!m_review->replaceStoredObservations(oldName, name, remap.kept, &error))
    QMessageBox::warning(this, QStringLiteral("Annotations not moved"),
                         QStringLiteral("The sequence was saved, but its annotations could not be "
                                        "updated: %1")
                             .arg(error));

  // Where review last stopped follows its clip too.
  const QString projectFile = m_projectController->projectPath();
  if (auto session = aquatrack::application::loadReviewSession(projectFile);
      session && samePathIgnoringCase(session->mediaPath, folder)) {
    session->frame =
        std::max(1, aquatrack::application::remapSequenceFrame(session->frame, before.clips, after));
    aquatrack::application::saveReviewSession(projectFile, *session);
  }

  m_dashboard->invalidateMedia(folder);
  if (const auto *project = m_projectController->project())
    m_videos->setProject(project, aquatrack::application::ProjectLayout(projectFile).root());
  refreshDashboard();
  statusBar()->showMessage(
      QStringLiteral("Saved sequence '%1': %2 clips, %3%4")
          .arg(name)
          .arg(after.size())
          .arg(aquatrack::media::formatDuration(updated.totalDurationMs()),
               remap.kept.empty() ? QString()
                                  : QStringLiteral("; %1 annotations moved")
                                        .arg(QLocale::system().toString(int(remap.kept.size())))),
      8000);
  if (openInReview && reviewSequence(folder)) {
    const int frame = aquatrack::application::remapSequenceFrame(frameOnScreen, before.clips, after);
    if (frame > 0)
      m_review->goToFrame(frame);
  }
}

void MainWindow::runAnalysis() {
  if (m_review->isAnalysisRunning() || !m_review->source().isOpen())
    return;
  const auto settings = m_settingsController->settings();

  // The detector: the active one if it is usable, else let the reviewer
  // choose among the usable detectors in the registry.
  const auto &entries = m_modelController->entries();
  const aquatrack::data::ModelEntry *chosen = nullptr;
  QVector<const aquatrack::data::ModelEntry *> usable;
  for (const auto &entry : entries) {
    if (!entry.isUsable() || entry.manifest.type != aquatrack::models::ModelType::Detector)
      continue;
    usable.push_back(&entry);
    if (entry.manifest.id == settings.detection.modelId)
      chosen = &entry;
  }
  if (!chosen) {
    if (usable.isEmpty()) {
      const auto answer = QMessageBox::question(
          this, QStringLiteral("No detector available"),
          QStringLiteral("Detect & track needs a fish detector in the model registry. Import "
                         "one from your training runs on the Models page?"),
          QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Yes);
      if (answer == QMessageBox::Yes)
        m_appState->setCurrentPage(Page::Models);
      return;
    }
    QStringList names;
    for (const auto *entry : usable)
      names << QString::fromStdString(entry->manifest.displayName);
    bool accepted = false;
    const QString picked = QInputDialog::getItem(
        this, QStringLiteral("Choose a detector"),
        QStringLiteral("Which model should detect fish? It becomes the active detector."), names, 0,
        false, &accepted);
    if (!accepted)
      return;
    chosen = usable.at(names.indexOf(picked));
    auto updated = settings;
    updated.detection.modelId = chosen->manifest.id;
    m_settingsController->save(updated);
    m_models->setActiveDetector(QString::fromStdString(chosen->manifest.id));
  }

  aquatrack::application::AnalysisRequest request;
  QString error;
  if (!aquatrack::application::AnalysisRunner::locatePython(
          QString::fromStdString(settings.analysis.pythonExecutable), &request.python,
          &request.packageRoot, &error)) {
    QMessageBox::warning(this, QStringLiteral("Python not found"), error);
    return;
  }
  const auto &manifest = chosen->manifest;
  request.weights = chosen->artifactPath;
  request.backend = QString::fromStdString(manifest.backend);
  request.modelName = QString::fromStdString(manifest.displayName);
  for (const auto &name : manifest.classes)
    request.classNames << QString::fromStdString(name);
  request.confidence = settings.detection.confidenceThreshold;
  request.iou = settings.detection.nmsThreshold;
  // The size the model was trained at; the global input size is a fallback.
  request.imageSize = manifest.inputWidth > 0 ? manifest.inputWidth : settings.detection.inputSize;
  request.tracker = QString::fromStdString(settings.analysis.tracker);
  request.matchIou = settings.tracking.matchIou;
  request.minimumHits = settings.tracking.minimumHits;
  request.maximumMissed = settings.tracking.maximumMissedFrames;
  if (m_projectController->project()) {
    const aquatrack::application::ProjectLayout layout(m_projectController->projectPath());
    const QString folder = layout.ensure(layout.annotations());
    if (!folder.isEmpty())
      request.output = QDir(folder).filePath(
          QStringLiteral("%1.detections.json").arg(m_review->source().name()));
  }
  m_appState->setCurrentPage(Page::Review);
  m_review->startAnalysis(request);
}

QString MainWindow::conflictingMedia(const QString &path) const {
  const auto *project = m_projectController->project();
  if (!project)
    return {};
  const QString id = aquatrack::media::MediaSource::reviewIdFor(path);
  if (id.isEmpty())
    return {};
  const QString cleaned = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  for (const auto &stored : project->mediaPaths) {
    const QString existing = QString::fromStdString(stored);
    if (QDir::cleanPath(existing).compare(cleaned, Qt::CaseInsensitive) == 0)
      continue;
    if (aquatrack::media::MediaSource::reviewIdFor(existing) == id)
      return existing;
  }
  return {};
}

QStringList MainWindow::environmentWarnings() const {
  QStringList warnings;
  if (aquatrack::media::ffmpegTool(QStringLiteral("ffmpeg")).isEmpty() ||
      aquatrack::media::ffmpegTool(QStringLiteral("ffprobe")).isEmpty())
    warnings << aquatrack::media::ffmpegMissingMessage();
  QString error;
  if (!aquatrack::application::AnalysisRunner::locatePython(
          QString::fromStdString(m_settingsController->settings().analysis.pythonExecutable),
          nullptr, nullptr, &error))
    warnings << error;
  return warnings;
}

QString MainWindow::chooseMediaImport(const QStringList &paths) {
  const std::string preference = m_settingsController->settings().storage.mediaImport;
  if (preference == "copy" || preference == "link")
    return QString::fromStdString(preference);

  const qint64 bytes = aquatrack::application::MediaImporter::sizeOf(paths);
  const QString size = QLocale::system().formattedDataSize(bytes);
  QMessageBox box(QMessageBox::Question, QStringLiteral("Add to project"),
                  paths.size() == 1
                      ? QStringLiteral("Add %1 (%2) to the project?")
                            .arg(QFileInfo(paths.first()).fileName(), size)
                      : QStringLiteral("Add %1 items (%2) to the project?")
                            .arg(paths.size())
                            .arg(size),
                  QMessageBox::NoButton, this);
  box.setInformativeText(QStringLiteral(
      "<b>Copy into project</b> puts the files in the project's media folder: the "
      "project is self-contained and can be moved or shared as one folder.<br><br>"
      "<b>Link in place</b> leaves them where they are and uses no extra disk space, "
      "but the project breaks if they move."));
  QPushButton *copy = box.addButton(QStringLiteral("Copy into project"), QMessageBox::AcceptRole);
  QPushButton *link = box.addButton(QStringLiteral("Link in place"), QMessageBox::ActionRole);
  box.addButton(QMessageBox::Cancel);
  box.setDefaultButton(copy);
  auto *remember = new QCheckBox(QStringLiteral("Remember my choice (change it in Settings)"));
  box.setCheckBox(remember);
  box.exec();

  QString mode;
  if (box.clickedButton() == copy)
    mode = QStringLiteral("copy");
  else if (box.clickedButton() == link)
    mode = QStringLiteral("link");
  if (!mode.isEmpty() && remember->isChecked()) {
    auto updated = m_settingsController->settings();
    updated.storage.mediaImport = mode.toStdString();
    m_settingsController->save(updated);
  }
  return mode;
}

QStringList MainWindow::copyIntoProject(const QStringList &sources) {
  QStringList copies;
  if (sources.isEmpty() || !m_projectController->project())
    return copies;
  const aquatrack::application::ProjectLayout layout(m_projectController->projectPath());
  // Videos and sequences each have their own folder under media/.
  const QString media = layout.ensure(layout.videos());
  if (media.isEmpty() || layout.ensure(layout.sequences()).isEmpty()) {
    QMessageBox::warning(this, QStringLiteral("Could not copy media"),
                         QStringLiteral("Could not create %1")
                             .arg(QDir::toNativeSeparators(layout.media())));
    return copies;
  }

  const qint64 total = aquatrack::application::MediaImporter::sizeOf(sources);
  const QStorageInfo volume(media);
  // Leave headroom: a disk filled to the last byte is its own problem.
  constexpr qint64 kHeadroom = 64LL * 1024 * 1024;
  if (volume.isValid() && volume.bytesAvailable() < total + kHeadroom) {
    QMessageBox::warning(
        this, QStringLiteral("Not enough disk space"),
        QStringLiteral("Copying needs %1 but only %2 is free on %3. Link the media in place "
                       "instead, or free some space.")
            .arg(QLocale::system().formattedDataSize(total),
                 QLocale::system().formattedDataSize(volume.bytesAvailable()),
                 volume.rootPath()));
    return copies;
  }

  QProgressDialog progress(QStringLiteral("Copying media into the project..."),
                           QStringLiteral("Cancel"), 0, 1000, this);
  progress.setWindowTitle(QStringLiteral("Copying media"));
  progress.setWindowModality(Qt::WindowModal);
  progress.setMinimumDuration(300);
  progress.setValue(0);
  aquatrack::application::MediaImporter importer(
      total, [&progress, total](qint64 done, qint64) {
        progress.setValue(total > 0 ? int(done * 1000 / total) : 1000);
        progress.setLabelText(QStringLiteral("Copying media into the project...  %1 of %2")
                                  .arg(QLocale::system().formattedDataSize(done),
                                       QLocale::system().formattedDataSize(total)));
        QCoreApplication::processEvents();
        return !progress.wasCanceled();
      });

  for (const auto &source : sources) {
    QString copied;
    QString error;
    const auto result = importer.copy(source, layout.mediaFolderFor(source), &copied, &error);
    if (result == aquatrack::application::MediaImporter::Result::Copied) {
      // A sequence of videos references its clips by paths relative to its
      // own folder; copied byte for byte they would point at the wrong
      // place. Re-save it from the original so the paths are recomputed for
      // the copy (and its README describes it).
      if (aquatrack::media::classifyMedia(source) == aquatrack::media::MediaKind::VideoSequence) {
        aquatrack::media::VideoSequenceManifest manifest;
        if (manifest.load(source))
          (void)manifest.save(copied);
      }
      copies << copied;
    } else if (result == aquatrack::application::MediaImporter::Result::Cancelled) {
      statusBar()->showMessage(QStringLiteral("Copy cancelled; %1 item(s) were copied")
                                   .arg(copies.size()),
                               6000);
      break;
    } else {
      QMessageBox::warning(this, QStringLiteral("Could not copy media"), error);
      break;
    }
  }
  progress.setValue(1000);
  return copies;
}

void MainWindow::applyProjectFolders() {
  if (!m_projectController->project()) {
    m_review->setProjectFolders({}, {}, {});
    m_export->setDefaultFolders({}, {});
    return;
  }
  const aquatrack::application::ProjectLayout layout(m_projectController->projectPath());
  m_review->setProjectFolders(layout.exports(), layout.annotations(), layout.snapshots());
  m_export->setDefaultFolders(layout.exports(), layout.annotations());
  // The project's models folder is its registry, replacing the previous
  // project's; a registry the reviewer opened elsewhere is left alone.
  const QString root = m_modelController->root();
  if (root.isEmpty() || root == m_projectModels) {
    const QString models = layout.ensure(layout.models());
    if (!models.isEmpty() && models != root)
      m_modelController->scan(models);
  }
  m_projectModels = layout.models();
}

void MainWindow::includeInProject(const QString &directory) {
  const auto *project = m_projectController->project();
  if (!project || directory.isEmpty())
    return;
  const auto before = project->mediaPaths.size();
  if (!m_projectController->addMedia(directory) ||
      m_projectController->project()->mediaPaths.size() == before)
    return;  // already listed
  QString error;
  if (!m_projectController->saveProject(&error)) {
    statusBar()->showMessage(QStringLiteral("Could not save project: %1").arg(error), 6000);
    return;
  }
  statusBar()->showMessage(QStringLiteral("Added %1 to %2")
                               .arg(QFileInfo(directory).fileName(),
                                    QString::fromStdString(project->name)),
                           5000);
}

void MainWindow::refreshAnalytics() {
  const auto *project = m_projectController->project();
  // The figures describe the media open in Review, so name it, not just the
  // project (which may hold many more observations).
  const QString media = m_review->source().isOpen() ? m_review->source().name() : QString();
  QString source = media.isEmpty() ? QStringLiteral("Open media in Review to analyse it") : media;
  if (project && !media.isEmpty())
    source += QStringLiteral("  ·  project %1").arg(QString::fromStdString(project->name));
  m_analytics->setObservations(m_review->observations(), source);
}

void MainWindow::refreshExport() {
  auto observations = m_review->observations();
  const auto *project = m_projectController->project();
  QString source = observations.empty()
                       ? QString()
                       : QString::fromStdString(observations.front().videoId);
  if (project && !source.isEmpty())
    source += QStringLiteral("  ·  project %1").arg(QString::fromStdString(project->name));
  m_export->setObservations(std::move(observations), source);
}

void MainWindow::addProjectMedia(const QStringList &requested) {
  if (requested.isEmpty() || !m_projectController->project())
    return;
  // Annotations are stored under each media item's name, so two different
  // files with the same name would silently share one set. Refuse those.
  QStringList accepted;
  QStringList clashes;
  QStringList unsupported;
  for (const auto &path : requested) {
    const auto kind = aquatrack::media::classifyMedia(path);
    if (kind == aquatrack::media::MediaKind::Unsupported) {
      unsupported << QFileInfo(path).fileName();
      continue;
    }
    const QString existing = conflictingMedia(path);
    if (!existing.isEmpty()) {
      clashes << QStringLiteral("%1  (already in the project from %2)")
                     .arg(QDir::toNativeSeparators(path), QDir::toNativeSeparators(existing));
      continue;
    }
    bool duplicate = false;
    for (const auto &other : accepted)
      duplicate = duplicate || aquatrack::media::MediaSource::reviewIdFor(other) ==
                                   aquatrack::media::MediaSource::reviewIdFor(path);
    if (duplicate) {
      clashes << QDir::toNativeSeparators(path);
      continue;
    }
    accepted << path;
  }
  if (!clashes.isEmpty() || !unsupported.isEmpty()) {
    QString message;
    if (!clashes.isEmpty())
      message += QStringLiteral("These have the same name as media already being added or in the "
                                "project, and annotations are stored by name. Rename one of the "
                                "files to add it:\n\n%1")
                     .arg(clashes.join(QLatin1Char('\n')));
    if (!unsupported.isEmpty())
      message += QStringLiteral("%1These are not video files AquaTrack can open:\n\n%2")
                     .arg(message.isEmpty() ? QString() : QStringLiteral("\n\n"),
                          unsupported.join(QLatin1Char('\n')));
    QMessageBox::warning(this, QStringLiteral("Some media was not added"), message);
  }
  if (accepted.isEmpty())
    return;
  // Media already inside the project folder needs no decision.
  const aquatrack::application::ProjectLayout layout(m_projectController->projectPath());
  QStringList paths;
  QStringList outside;
  for (const auto &path : accepted)
    (layout.contains(path) ? paths : outside) << path;
  if (!outside.isEmpty()) {
    const QString mode = chooseMediaImport(outside);
    if (mode.isEmpty())
      return;
    if (mode == QLatin1String("link"))
      paths << outside;
    else
      paths << copyIntoProject(outside);
  }
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
  refreshDashboard();
  statusBar()->showMessage(QStringLiteral("Added %1 item(s) to the project").arg(paths.size()), 5000);
}

void MainWindow::refreshDashboard() {
  const auto *project = m_projectController->project();
  QStringList media;
  if (project)
    for (const auto &path : project->mediaPaths)
      media << QString::fromStdString(path);
  m_dashboard->setProject(project ? QString::fromStdString(project->name) : QString(),
                          m_projectController->projectPath(), media,
                          m_projectController->observationStorePath(),
                          m_projectController->isDirty());
  m_dashboard->setRecentProjects(recentProjects());
}

void MainWindow::recordReviewSession() {
  const auto *project = m_projectController->project();
  const QString media = m_review->sequenceDirectory();
  const int frame = m_review->currentFrame();
  if (!project || media.isEmpty() || frame < 1)
    return;
  // Only media that belongs to this project: something opened on its own
  // is not where this project's review stopped.
  const QString wanted = QDir::cleanPath(QFileInfo(media).absoluteFilePath());
  const bool inProject = std::any_of(
      project->mediaPaths.cbegin(), project->mediaPaths.cend(), [&](const std::string &path) {
        return QDir::cleanPath(QFileInfo(QString::fromStdString(path)).absoluteFilePath())
                   .compare(wanted, Qt::CaseInsensitive) == 0;
      });
  if (!inProject)
    return;
  aquatrack::application::saveReviewSession(
      m_projectController->projectPath(),
      {wanted, frame, QDateTime::currentMSecsSinceEpoch()});
}

void MainWindow::refreshWarnings() {
  m_header->setNotificationCount(
      int(m_datasetController->result().warnings.size() +
          m_modelController->warnings().size() + environmentWarnings().size()));
}

void MainWindow::showWarnings() {
  QStringList lines;
  for (const auto &warning : environmentWarnings())
    lines << QStringLiteral("Workstation: %1").arg(warning);
  for (const auto &warning : m_datasetController->result().warnings)
    lines << QStringLiteral("Datasets: %1").arg(warning);
  for (const auto &warning : m_modelController->warnings())
    lines << QStringLiteral("Models: %1").arg(warning);
  if (lines.isEmpty()) {
    QMessageBox::information(
        this, QStringLiteral("Validation warnings"),
        QStringLiteral("No warnings. Dataset and model scans report problems here."));
    return;
  }
  QMessageBox box(QMessageBox::Warning, QStringLiteral("Validation warnings"),
                  QStringLiteral("%1 warning(s) from the last dataset and model scans.")
                      .arg(lines.size()),
                  QMessageBox::Ok, this);
  box.setDetailedText(lines.join(QLatin1Char('\n')));
  box.exec();
}

void MainWindow::showHelp() {
  QMessageBox::information(
      this, QStringLiteral("AquaTrack+ help"),
      QStringLiteral(
          "<b>Getting started</b><ol>"
          "<li>Create a project on the Dashboard. It holds your media and annotations.</li>"
          "<li>On the Videos page add video files (or a whole folder of them). Select several and "
          "choose <i>New sequence</i> to review them back to back as one sequence; "
          "<i>Add existing sequence</i> adds a sequence folder made earlier, including "
          "BrackishMOT ones. Double-click a sequence to see what is in it.</li>"
          "<li>Import a fish detector from your training runs on the Models page and make "
          "it active.</li>"
          "<li>Open media in Review. <i>Detect &amp; track</i> runs the detector and "
          "tracker over every frame; <i>Import annotations</i> loads ground truth "
          "(MOTChallenge gt.txt, or COCO tracking JSON such as SFISHTRACK).</li>"
          "<li>Filter by species and confidence; relabel, delete or merge tracks; save.</li>"
          "<li>Finish on the Export page: CSV for analysis or MOTChallenge for tracker "
          "benchmarks.</li></ol>"
          "<b>Navigation</b><br>"
          "The dock at the bottom follows the workflow, left to right: Dashboard, then "
          "Datasets, Videos and Models to prepare, Review and Analytics to analyse, and "
          "Export to deliver. Settings and help are in the title bar.<br><br>"
          "<b>Review controls</b><br>"
          "Space or K plays and pauses; Left/Right step one frame, Shift+Left/Right one "
          "second; Home/End jump to the ends. Click or drag the timeline to seek; click a "
          "filmstrip frame or a clip in the left panel to jump to it. The camera button "
          "saves the canvas, with overlays, as an image.<br><br>"
          "The bell lists validation warnings from the latest dataset and model scans."));
}

QStringList MainWindow::recentProjects() const {
  QStringList paths;
  for (const auto &path :
       m_settingsController->settings().storage.recentProjects)
    paths << QString::fromStdString(path);
  return paths;
}
