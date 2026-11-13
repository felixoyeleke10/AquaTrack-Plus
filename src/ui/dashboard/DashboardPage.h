#pragma once

#include "application/ProjectSummary.h"

#include <QHash>
#include <QPixmap>
#include <QSet>
#include <QStringList>
#include <QThreadPool>
#include <QWidget>

class QLabel;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;

namespace aquatrack::ui {
class Badge;
namespace dashboard {
class CardGrid;
class FramePreview;
class HeroBanner;
class RatioBar;
class SpeciesChart;
}  // namespace dashboard
}  // namespace aquatrack::ui

// Home screen, in two modes.
//
// With no project open it is a project browser: recent projects as poster
// cards, and the ways to start one.
//
// With a project open it is that project's overview: a hero built from its
// footage, the one next action (resume where review stopped, or start, or
// add media), survey progress and species found, and every media item with
// its own review progress.
//
// Metadata, the observation summary and thumbnails are read on a thread
// pool and cached, so the page appears at once and fills in.
class DashboardPage final : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(QWidget* parent = nullptr);
    ~DashboardPage() override;

    void setProject(const QString& name, const QString& path, const QStringList& mediaPaths,
                    const QString& observationStore, bool unsaved);
    void setRecentProjects(const QStringList& paths);
    // Forgets what was read about one media item (its poster, its summary),
    // e.g. after a sequence's clips changed, so it is read again.
    void invalidateMedia(const QString& path);

signals:
    void newProjectRequested();
    void openProjectRequested();
    void saveProjectRequested();
    void reviewRequested();
    void exportRequested();
    void openSequenceRequested();
    void scanDatasetsRequested();
    void scanModelsRequested();
    void recentProjectActivated(QString path);
    void recentProjectRemoveRequested(QString path);
    void browseDatasetsRequested();
    void addSequenceFolderRequested();
    void addVideosRequested();
    void createSequenceRequested();
    void editSequenceRequested(QString path);
    void manageMediaRequested();
    void openProjectFolderRequested();
    void clearMissingRecentRequested();
    // Open one media item in Review, optionally at a frame (0: wherever).
    void mediaReviewRequested(QString path, int frame);

protected:
    void showEvent(QShowEvent* event) override;

private:
    QWidget* buildBrowser();
    QWidget* buildOverview();
    void showMode();
    void rebuildRecent();
    void rebuildMedia();
    void refreshOverview();
    void applySummary(const aquatrack::application::ProjectSummary& summary);
    void updateHero();
    void updateContinue();
    void requestThumbnail(const QString& mediaPath);
    void requestResumeFrame(const QString& mediaPath, int frame);
    void requestPoster(const QString& projectFile);
    void thumbnailReady(const QString& mediaPath, const QImage& image);
    void posterReady(const QString& projectFile, int mediaCount, const QImage& image);
    void openRecentItem(QListWidgetItem* item);

    // ---- Mode
    QStackedWidget* m_modes = nullptr;
    bool m_browsing = false;  // project open, but the browser was asked for

    // ---- Browser
    aquatrack::ui::dashboard::CardGrid* m_recent = nullptr;
    QWidget* m_recentEmpty = nullptr;
    QLabel* m_browserSubtitle = nullptr;
    QPushButton* m_clearMissing = nullptr;
    QPushButton* m_backToProject = nullptr;

    // ---- Overview
    aquatrack::ui::Badge* m_projectState = nullptr;
    QPushButton* m_save = nullptr;
    aquatrack::ui::dashboard::HeroBanner* m_hero = nullptr;
    QStackedWidget* m_continue = nullptr;
    aquatrack::ui::dashboard::FramePreview* m_continueFrame = nullptr;
    QLabel* m_continueEyebrow = nullptr;
    QLabel* m_continueTitle = nullptr;
    QLabel* m_continueDetail = nullptr;
    aquatrack::ui::dashboard::RatioBar* m_continuePosition = nullptr;
    QPushButton* m_continueButton = nullptr;
    QLabel* m_verifiedPercent = nullptr;
    QLabel* m_verifiedCaption = nullptr;
    aquatrack::ui::dashboard::RatioBar* m_verifiedBar = nullptr;
    QLabel* m_figureDetections = nullptr;
    QLabel* m_figureTracks = nullptr;
    QLabel* m_figureSpecies = nullptr;
    QLabel* m_figureFrames = nullptr;
    aquatrack::ui::dashboard::SpeciesChart* m_species = nullptr;
    QLabel* m_speciesEmpty = nullptr;
    QPushButton* m_export = nullptr;
    QLabel* m_mediaCount = nullptr;
    aquatrack::ui::dashboard::CardGrid* m_media = nullptr;

    // ---- State
    QString m_projectName;
    QString m_projectPath;
    QStringList m_mediaPaths;
    QString m_store;
    QString m_overviewSignature;
    QStringList m_recentPaths;
    aquatrack::application::ProjectSummary m_summary;
    bool m_haveSummary = false;
    QString m_continuePath;  // media the continue card opens
    int m_continueFrameNumber = 0;

    // ---- Background work and its results
    QThreadPool m_pool;
    quint64 m_generation = 0;
    QHash<QString, QPixmap> m_thumbnails;  // media path -> poster
    QSet<QString> m_thumbnailsPending;
    QHash<QString, QPixmap> m_posters;     // project file -> poster
    QHash<QString, int> m_projectMedia;    // project file -> media count
    QSet<QString> m_postersPending;
    QString m_resumeKey;
    QPixmap m_resumePixmap;
};
