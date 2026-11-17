#pragma once

#include <QMainWindow>
#include <QStringList>
#include <QVector>

namespace aquatrack::application {
class AppState;
class ProjectController;
class ModelController;
class SettingsController;
class DatasetController;
struct ModelCandidate;
} // namespace aquatrack::application
namespace aquatrack::data {
struct ModelEntry;
}
class AppHeader;
class NavigationDock;
class DashboardPage;
class QStackedWidget;
class VideosPage;
class ModelsPage;
class SettingsPage;
class SettingsWindow;
class ExportPage;
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

protected:
  void closeEvent(QCloseEvent *event) override;
  void changeEvent(QEvent *event) override;
  // Watches the native window for pointer moves near the border, which is
  // how a frameless window offers resizing.
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void applyNativeWindowStyle();
  [[nodiscard]] Qt::Edges resizeEdgesAt(const QPoint &position) const;
  void updateResizeCursor(Qt::Edges edges);

  // Loads a sequence into Review and switches to it. Returns false when the
  // reviewer cancelled at the unsaved-edits prompt.
  bool reviewSequence(const QString &directory);
  // Opens a project, asking first about unsaved annotation edits, and
  // records it in the recent list.
  bool openProjectFile(const QString &path);
  // A recent entry whose file is gone: offer to locate it or drop it.
  void resolveMissingRecentProject(const QString &path);
  void createProject();
  // Asks for a BrackishMOT sequence folder and adds it to the project.
  void addSequenceFolder();
  // Groups videos into one sequence (sequence.json in the project's
  // media folder) and adds it. Asks for videos when none are given.
  void createVideoSequence(const QStringList &selectedVideos = {});
  // Reorders, adds or removes a sequence's clips, or renames it. Stored
  // annotations move with their clips; the sequence is closed in Review
  // while it changes and reopened at the same moment of footage.
  void editVideoSequence(const QString &folder);
  // Why `name` cannot name a sequence in this project (empty when it can).
  // `exceptPath` is the sequence being renamed, which may keep its name.
  [[nodiscard]] QString sequenceNameProblem(const QString &name,
                                            const QString &exceptPath = {}) const;
  // Adds every video below a chosen folder.
  void addVideoFolder();
  // Resolves the active detector and Python, then runs detection + tracking
  // over the media open in Review.
  void runAnalysis();
  // Shows the settings window with the current settings (or raises it).
  void openSettings();
  // Detectors Detect & track can run, and the one it will: the open
  // project's choice when it is still usable, else the app's active one.
  [[nodiscard]] QVector<const aquatrack::data::ModelEntry *> usableDetectors() const;
  [[nodiscard]] QString effectiveDetectorId() const;
  void refreshDetectorChoices();
  // Makes `modelId` the detector: for the open project, and also as the
  // app's active detector when `appDefault` (or when no project is open).
  bool chooseDetector(const QString &modelId, bool appDefault);
  // Media already in the project that would share annotations with `path`
  // (same review name, different file); empty when there is none.
  [[nodiscard]] QString conflictingMedia(const QString &path) const;
  // Problems with the workstation that stop video or detection working.
  [[nodiscard]] QStringList environmentWarnings() const;
  // Adds a sequence to the open project if it is not already listed. The
  // sequence being reviewed always belongs to the project it is saved in.
  void includeInProject(const QString &directory);
  void refreshAnalytics();
  void refreshExport();
  void refreshDashboard();
  // Saves where review of the open project stopped, for the dashboard's
  // "continue" card. Does nothing for media outside the project.
  void recordReviewSession();
  void refreshWarnings();
  void showWarnings();
  void showHelp();
  // Adds media to the project, copying it into the project's media folder
  // or linking it in place according to the reviewer's choice.
  void addProjectMedia(const QStringList &paths);
  // "copy", "link", or empty when the reviewer cancelled.
  [[nodiscard]] QString chooseMediaImport(const QStringList &paths);
  // Copies `sources` into the project's media folder with a progress dialog.
  // Returns the copies made, which is fewer than asked if cancelled.
  [[nodiscard]] QStringList copyIntoProject(const QStringList &sources);
  // Shows the model chooser and imports selected weights into the project.
  void importModelCandidates(QVector<aquatrack::application::ModelCandidate> candidates);
  // Points pages at the open project's folders.
  void applyProjectFolders();
  [[nodiscard]] QStringList recentProjects() const;

  aquatrack::application::AppState *m_appState = nullptr;
  aquatrack::application::ProjectController *m_projectController = nullptr;
  aquatrack::application::ModelController *m_modelController = nullptr;
  aquatrack::application::SettingsController *m_settingsController = nullptr;
  aquatrack::application::DatasetController *m_datasetController = nullptr;
  AppHeader *m_header = nullptr;
  NavigationDock *m_dock = nullptr;
  QStackedWidget *m_pages = nullptr;
  DashboardPage *m_dashboard = nullptr;
  VideosPage *m_videos = nullptr;
  ReviewPage *m_review = nullptr;
  AnalyticsPage *m_analytics = nullptr;
  DatasetsPage *m_datasets = nullptr;
  ModelsPage *m_models = nullptr;
  SettingsWindow *m_settingsWindow = nullptr;  // settings, in a window of their own
  SettingsPage *m_settings = nullptr;          // its contents
  ExportPage *m_export = nullptr;
  // The open project's models folder, so switching projects can tell its
  // registry from one the reviewer opened elsewhere.
  QString m_projectModels;
  // Edges under the pointer while a resize cursor is overriding the normal
  // one; empty when no override is active.
  Qt::Edges m_resizeCursorEdges;
};
