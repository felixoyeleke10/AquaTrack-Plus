// End-to-end checks of how MainWindow wires its pages and controllers
// together. Each block pins a bug that the per-page tests could not see
// because it lived in the connections between pages.
#include "TestSupport.h"

#include "application/controllers/ExportController.h"
#include "application/controllers/ProjectController.h"
#include "application/controllers/ReviewController.h"
#include "data/SettingsStore.h"
#include "ui/MainWindow.h"
#include "ui/dashboard/DashboardPage.h"
#include "ui/datasets/DatasetsPage.h"
#include "ui/export/ExportPage.h"
#include "ui/navigation/NavigationDock.h"
#include "ui/review/ReviewPage.h"
#include "ui/review/center/Timeline.h"
#include "ui/review/right/InsightPanel.h"
#include "ui/settings/SettingsPage.h"
#include "ui/videos/VideosPage.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

#include <algorithm>

namespace {

// A tiny BrackishMOT sequence: two tracks per frame plus one zero-size box,
// PNG frames so no image plugin is needed. `missingFrame` leaves one image out.
QString writeSequence(const QDir& root, const QString& name, int frames, int missingFrame = 0)
{
    const QString directory = root.filePath(name);
    QDir().mkpath(directory + QStringLiteral("/img1"));
    QDir().mkpath(directory + QStringLiteral("/gt"));
    {
        QFile info(directory + QStringLiteral("/seqinfo.ini"));
        requireTest(info.open(QIODevice::WriteOnly | QIODevice::Text), "write seqinfo.ini");
        QTextStream(&info) << "[Sequence]\nname=" << name
                           << "\nimDir=img1\nframeRate=10\nseqLength=" << frames
                           << "\nimWidth=64\nimHeight=48\nimExt=.png\n";
    }
    QFile truth(directory + QStringLiteral("/gt/gt.txt"));
    requireTest(truth.open(QIODevice::WriteOnly | QIODevice::Text), "write gt.txt");
    QTextStream rows(&truth);
    for (int frame = 1; frame <= frames; ++frame) {
        if (frame != missingFrame) {
            QImage image(64, 48, QImage::Format_RGB32);
            image.fill(Qt::darkBlue);
            requireTest(image.save(QStringLiteral("%1/img1/%2.png")
                                       .arg(directory)
                                       .arg(frame, 6, 10, QLatin1Char('0'))),
                        "write frame image");
        }
        rows << frame << ",1,5,5,10,10,1,1,1\n";
        rows << frame << ",2,30,20,8,8,1,2,1\n";
    }
    rows << "1,3,0,0,0,0,1,1,1\n";  // zero-size: must be skipped, not crash
    return directory;
}

bool containsPath(const std::vector<std::string>& paths, const QString& path)
{
    const auto wanted = QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toStdString();
    return std::find(paths.cbegin(), paths.cend(), wanted) != paths.cend();
}

}  // namespace

int main(int argc, char* argv[])
{
    // Keep the developer's real settings.json out of reach.
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("AquaTrack"));
    app.setApplicationName(QStringLiteral("AquaTrackWiringTests"));

    const QString settingsPath =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
        QStringLiteral("/settings.json");
    QFile::remove(settingsPath);
    QTemporaryDir temporary;
    requireTest(temporary.isValid(), "temporary directory");
    const QDir root(temporary.path());
    const QString sequenceA = writeSequence(root, QStringLiteral("seq-a"), 3);
    const QString sequenceB = writeSequence(root, QStringLiteral("seq-b"), 4);

    // Values the Settings page has no control for; saving must keep them.
    aquatrack::system::settings::AquaTrackSettings seeded;
    seeded.detection.nmsThreshold = 0.5;
    seeded.tracking.minimumHits = 5;
    // This test is about wiring, not the copy-or-link prompt.
    seeded.storage.mediaImport = "link";
    // A recent entry whose project file has since disappeared.
    const QString missingProject = root.filePath(QStringLiteral("moved-away/old.aqp"));
    seeded.storage.recentProjects = {
        QDir::cleanPath(QFileInfo(missingProject).absoluteFilePath()).toStdString()};
    requireTest(QDir().mkpath(QFileInfo(settingsPath).absolutePath()), "settings directory");
    aquatrack::data::SettingsStore store;
    requireTest(store.save(settingsPath, seeded), "seed settings");

    QString projectPath = root.filePath(QStringLiteral("wiring.aqp"));
    {
        aquatrack::application::ProjectController creator;
        requireTest(creator.createProject(projectPath, QStringLiteral("Wiring")),
                    "create project fixture");
    }

    {
        MainWindow window;
        auto* review = window.findChild<ReviewPage*>();
        auto* pages = window.findChild<QStackedWidget*>();
        requireTest(review && pages, "review page exists");

        // ---- Ground truth loads, invalid rows skipped, MOT stays 1-based ----
        window.openSequence(sequenceA);
        auto observations = review->observations();
        requireTest(observations.size() == 6, "ground truth loads without the zero-size box");
        auto* lanes = window.findChild<aquatrack::ui::TrackLanes*>();
        requireTest(lanes && lanes->laneCount() == 2, "one lane per track in the sequence");
        aquatrack::application::ExportController exporter;
        const QString motPath = root.filePath(QStringLiteral("gt_export.txt"));
        requireTest(exporter.exportMot(motPath, observations), "MOT export");
        QFile mot(motPath);
        requireTest(mot.open(QIODevice::ReadOnly | QIODevice::Text), "open MOT export");
        const auto lines = QString::fromUtf8(mot.readAll()).split('\n', Qt::SkipEmptyParts);
        requireTest(std::all_of(lines.cbegin(), lines.cend(),
                                [](const QString& line) {
                                    const int frame = line.section(',', 0, 0).toInt();
                                    return frame >= 1 && frame <= 3;
                                }),
                    "MOT frames match the source gt.txt (no off-by-one)");

        // ---- Opening a project with a sequence on screen keeps it reviewable
        window.openProject(projectPath);
        requireTest(review->observations().size() == 6,
                    "sequence survives switching to a project store");
        requireTest(QFile::exists(root.filePath(QStringLiteral("annotations/observations.json"))),
                    "observation store written in the project's annotations folder");
        auto* projectsController = window.findChild<aquatrack::application::ProjectController*>();
        requireTest(projectsController->project()->mediaPaths.size() == 1,
                    "the sequence on screen joins the project it is saved in");

        // ---- Datasets: select several sequences, add them in one go -------
        window.openDatasets(temporary.path());
        auto* datasetsPage = window.findChild<DatasetsPage*>();
        auto* datasetTable = window.findChild<QTableWidget*>(QStringLiteral("datasetTable"));
        auto* addButton = window.findChild<QPushButton*>(QStringLiteral("addToProject"));
        requireTest(datasetsPage && datasetTable && addButton, "datasets page controls exist");
        requireTest(datasetTable->rowCount() == 2, "scan finds both sequences");
        datasetTable->selectAll();
        requireTest(datasetsPage->selectedSequencePaths().size() == 2, "multi-select works");
        requireTest(addButton->isEnabled() && addButton->text() == QStringLiteral("Add sequence to project"),
                    "add action offers only the sequence not yet in the project");
        addButton->click();
        requireTest(projectsController->project()->mediaPaths.size() == 2,
                    "selected sequences are added to the project");
        datasetTable->selectAll();
        requireTest(!addButton->isEnabled() && datasetTable->item(0, 5)->text() == QStringLiteral("Added"),
                    "added sequences are marked and not offered again");

        // ---- Opening a project records it as recent ----------------------
        aquatrack::system::settings::AquaTrackSettings saved;
        requireTest(store.load(settingsPath, saved), "reload settings");
        requireTest(containsPath(saved.storage.recentProjects, projectPath),
                    "opened project is listed as recent");

        // ---- Missing recent projects are flagged and can be cleared -------
        auto* dashboard = window.findChild<DashboardPage*>();
        auto* recentList = window.findChild<QListWidget*>(QStringLiteral("recentProjects"));
        requireTest(dashboard && recentList && recentList->count() == 2,
                    "recent list shows the opened and the missing project");
        requireTest(recentList->item(1)->toolTip().startsWith(QStringLiteral("Missing:")),
                    "missing project is flagged");
        emit dashboard->clearMissingRecentRequested();
        requireTest(store.load(settingsPath, saved), "reload settings after clearing");
        requireTest(saved.storage.recentProjects.size() == 1 &&
                        containsPath(saved.storage.recentProjects, projectPath),
                    "clearing missing keeps existing projects only");
        requireTest(recentList->count() == 1, "dashboard list refreshes after clearing");

        // ---- Saving settings keeps what the page does not show ------------
        auto* settingsPage = window.findChild<SettingsPage*>();
        requireTest(settingsPage != nullptr, "settings page exists");
        emit settingsPage->saveRequested();
        requireTest(store.load(settingsPath, saved), "reload settings after save");
        requireTest(containsPath(saved.storage.recentProjects, projectPath),
                    "settings save keeps recent projects");
        requireTest(saved.detection.nmsThreshold == 0.5 && saved.tracking.minimumHits == 5,
                    "settings save keeps fields the page does not show");

        // ---- Edits apply at once and mark the session dirty ---------------
        auto* insight = window.findChild<InsightPanel*>();
        requireTest(insight != nullptr, "insight panel exists");
        emit insight->deleteRequested(QStringLiteral("seq-a:1:1"));
        requireTest(review->observations().size() == 5, "delete applies");
        requireTest(review->hasUnsavedAnnotations(), "delete marks edits unsaved");
        emit insight->saveRequested();
        requireTest(!review->hasUnsavedAnnotations(), "save clears unsaved state");

        // ---- Videos page "Open in review" is connected --------------------
        auto* videos = window.findChild<VideosPage*>();
        requireTest(videos != nullptr, "videos page exists");
        auto* dock = window.findChild<NavigationDock*>();
        requireTest(dock != nullptr, "navigation dock exists");
        emit dock->pageRequested(1);
        requireTest(pages->currentIndex() == 1, "dock navigates to videos");
        emit videos->reviewRequested(sequenceB);
        observations = review->observations();
        requireTest(observations.size() == 8 &&
                        std::all_of(observations.cbegin(), observations.cend(),
                                    [](const auto& item) { return item.videoId == "seq-b"; }),
                    "videos page opens the sequence in review");
        requireTest(pages->currentIndex() == 2, "videos page switches to review");

        // ---- Export is the last step and honours "include unverified" ----
        emit dock->pageRequested(7);
        requireTest(pages->currentIndex() == 7, "dock opens the export page");
        auto* exportPage = window.findChild<ExportPage*>();
        requireTest(exportPage && exportPage->exportable().size() == 8,
                    "export page offers the reviewed sequence");
        auto* includeUnverified = exportPage->findChild<QCheckBox*>(QStringLiteral("includeUnverified"));
        includeUnverified->setChecked(false);
        requireTest(exportPage->exportable().size() == 8,
                    "ground truth counts as verified");
        requireTest(store.load(settingsPath, saved) && !saved.exportSettings.includeUnverified,
                    "export option is saved to settings");
        includeUnverified->setChecked(true);

        // ---- A recent-project double-click opens it once ------------------
        auto* projects = window.findChild<aquatrack::application::ProjectController*>();
        auto* recent = window.findChild<QListWidget*>(QStringLiteral("recentProjects"));
        requireTest(projects && recent && recent->count() > 0, "recent list populated");
        int opened = 0;
        QObject::connect(projects, &aquatrack::application::ProjectController::projectChanged,
                         [&opened] { ++opened; });
        // What Qt emits for one double-click on a platform that activates
        // items on double-click.
        emit recent->itemDoubleClicked(recent->item(0));
        emit recent->itemActivated(recent->item(0));
        requireTest(opened == 1, "double-click opens the recent project once");
    }

    // ---- Playback stops at an unreadable frame instead of looping ---------
    {
        const QString broken = writeSequence(root, QStringLiteral("seq-c"), 3, 2);
        aquatrack::application::ReviewController controller;
        controller.loadSequence(broken);
        int errors = 0;
        bool stopped = false;
        QObject::connect(&controller, &aquatrack::application::ReviewController::errorOccurred,
                         [&errors] { ++errors; });
        QObject::connect(&controller, &aquatrack::application::ReviewController::playbackChanged,
                         [&stopped](bool playing) { stopped = stopped || !playing; });
        controller.togglePlayback();
        QElapsedTimer clock;
        clock.start();
        while (clock.elapsed() < 1500)
            app.processEvents(QEventLoop::AllEvents, 20);
        requireTest(stopped, "playback stops at the missing frame");
        requireTest(errors == 1, "missing frame is reported once");
        requireTest(controller.currentFrame() == 1, "playhead stays on the last good frame");
    }

    QFile::remove(settingsPath);
    return 0;
}
