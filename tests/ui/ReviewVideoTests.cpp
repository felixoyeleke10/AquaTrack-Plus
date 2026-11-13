// Video review end to end in the real main window: open a video, play it
// while measuring how long the UI thread is ever blocked, import ground
// truth, and (optionally) screenshot a real project for visual checks.
//
// Screenshots: set AQUATRACK_SCREENSHOT_DIR, and AQUATRACK_DEMO_PROJECT plus
// AQUATRACK_DEMO_MEDIA (and optionally AQUATRACK_DEMO_FRAME) to capture a real
// project instead of the generated fixture.

#include "TestSupport.h"

#include "application/controllers/ProjectController.h"
#include "data/media/MediaTools.h"
#include "ui/MainWindow.h"
#include "ui/review/ReviewPage.h"
#include "ui/videos/VideosPage.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <iostream>

namespace {

template <typename Predicate>
bool waitFor(Predicate done, int milliseconds = 20000)
{
    QElapsedTimer clock;
    clock.start();
    while (!done() && clock.elapsed() < milliseconds)
        QApplication::processEvents(QEventLoop::AllEvents, 15);
    return done();
}

void settle(int milliseconds)
{
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < milliseconds)
        QApplication::processEvents(QEventLoop::AllEvents, 15);
}

void screenshot(QWidget& window, const QString& name)
{
    const QString folder = qEnvironmentVariable("AQUATRACK_SCREENSHOT_DIR");
    if (folder.isEmpty())
        return;
    QDir().mkpath(folder);
    settle(300);
    window.grab().save(QDir(folder).filePath(name));
}

QLabel* label(QWidget& window, const char* name)
{
    return window.findChild<QLabel*>(QLatin1String(name));
}

int captureDemo()
{
    const QString project = qEnvironmentVariable("AQUATRACK_DEMO_PROJECT");
    const QString mediaPath = qEnvironmentVariable("AQUATRACK_DEMO_MEDIA");
    const int frame = qEnvironmentVariableIntValue("AQUATRACK_DEMO_FRAME");
    MainWindow window;
    window.resize(1680, 1000);
    window.show();
    window.openProject(project);
    auto* videos = window.findChild<VideosPage*>();
    auto* pages = window.findChild<QStackedWidget*>();
    pages->setCurrentIndex(1);
    waitFor([videos] { return !videos->isProbing(); }, 60000);
    screenshot(window, QStringLiteral("library-list.png"));
    videos->findChild<QPushButton*>(QStringLiteral("mediaGridView"))->click();
    screenshot(window, QStringLiteral("library-grid.png"));
    for (const auto& entry : QDir(QFileInfo(project).absoluteDir().filePath(QStringLiteral("media")))
                                 .entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (QFileInfo::exists(entry.absoluteFilePath() + QStringLiteral("/sequence.json"))) {
            videos->openFolder(entry.absoluteFilePath());
            screenshot(window, QStringLiteral("library-sequence-folder.png"));
            videos->closeFolder();
        }
    }
    window.openSequence(mediaPath);
    auto* review = window.findChild<ReviewPage*>();
    if (frame > 0)
        review->goToFrame(frame);
    settle(2500);  // frame, filmstrip thumbnails
    screenshot(window, QStringLiteral("review.png"));
    if (!qEnvironmentVariable("AQUATRACK_DEMO_ALL_PAGES").isEmpty()) {
        static const char* names[] = {"dashboard", "videos", "review", "analytics",
                                      "datasets",  "models", "settings", "export"};
        auto* dock = window.findChild<QWidget*>(QStringLiteral("navigationDock"));
        Q_UNUSED(dock);
        for (int index = 0; index < 8; ++index) {
            if (index == 2)
                continue;
            QMetaObject::invokeMethod(&window, [&window, index] {
                for (auto* child : window.findChildren<QObject*>())
                    if (child->inherits("NavigationDock"))
                        QMetaObject::invokeMethod(child, "pageRequested", Q_ARG(int, index));
            });
            settle(1200);
            screenshot(window, QStringLiteral("page-%1-%2.png").arg(index).arg(QLatin1String(names[index])));
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QStandardPaths::setTestModeEnabled(true);  // keep the user's settings out of it

    if (!qEnvironmentVariable("AQUATRACK_DEMO_PROJECT").isEmpty())
        return captureDemo();

    if (aquatrack::media::ffmpegTool(QStringLiteral("ffmpeg")).isEmpty()) {
        std::cout << "SKIP: FFmpeg not available\n";
        return 0;
    }
    QTemporaryDir temporary;
    requireTest(temporary.isValid(), "temporary directory");
    const QDir root(temporary.path());

    // A 1080p clip, so decode cost is realistic.
    const QString video = root.filePath(QStringLiteral("dive.mp4"));
    QProcess ffmpeg;
    ffmpeg.start(aquatrack::media::ffmpegTool(QStringLiteral("ffmpeg")),
                 {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-y"), QStringLiteral("-f"),
                  QStringLiteral("lavfi"), QStringLiteral("-i"),
                  QStringLiteral("testsrc2=size=1920x1080:rate=30"), QStringLiteral("-frames:v"),
                  QStringLiteral("90"), QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), video});
    requireTest(ffmpeg.waitForFinished(120000) && ffmpeg.exitCode() == 0, "generate a 1080p clip");

    QFile truth(root.filePath(QStringLiteral("gt.txt")));
    requireTest(truth.open(QIODevice::WriteOnly | QIODevice::Text), "write ground truth");
    for (int frame = 1; frame <= 90; ++frame) {
        truth.write(QStringLiteral("%1,1,%2,400,120,60,1,1,1\n").arg(frame).arg(200 + frame * 8).toUtf8());
        truth.write(QStringLiteral("%1,2,%2,700,90,50,1,1,1\n").arg(frame).arg(1500 - frame * 6).toUtf8());
    }
    truth.close();

    const QString projectPath = root.filePath(QStringLiteral("Dive/Dive.aqp"));
    {
        QDir().mkpath(root.filePath(QStringLiteral("Dive")));
        aquatrack::application::ProjectController creator;
        requireTest(creator.createProject(projectPath, QStringLiteral("Dive")), "create project");
    }

    MainWindow window;
    window.resize(1600, 960);
    window.show();
    window.openProject(projectPath);
    auto* review = window.findChild<ReviewPage*>();
    auto* pages = window.findChild<QStackedWidget*>();
    requireTest(review && pages, "review page exists");

    // ---- Opening a video is quick and shows its first frame ----------------
    QElapsedTimer opening;
    opening.start();
    window.openSequence(video);
    requireTest(opening.elapsed() < 4000, "opening a video does not decode it up front");
    requireTest(pages->currentIndex() == 2, "review opens");
    requireTest(label(window, "reviewTitle")->text() == QStringLiteral("dive"), "header names the video");
    requireTest(waitFor([&] { return review->source().isOpen(); }), "video open");

    // ---- A failed open keeps the video on screen and stays on the page ----
    // (The error dialog is closed by a timer.)
    QTimer::singleShot(600, [] {
        for (QWidget* widget : QApplication::topLevelWidgets())
            if (widget->inherits("QMessageBox"))
                widget->close();
    });
    window.openSequence(root.filePath(QStringLiteral("gone.mp4")));
    requireTest(review->source().name() == QStringLiteral("dive"), "a failed open keeps the current media");

    // ---- Playback never blocks the UI thread for long ----------------------
    // A 5 ms heartbeat timer records the longest gap between its ticks: any
    // work that blocks the UI thread (as per-frame FFmpeg calls used to, for
    // ~400 ms a frame) shows up as a long gap.
    qint64 longestStall = 0;
    QElapsedTimer heartbeat;
    heartbeat.start();
    QTimer probe;
    probe.setInterval(5);
    QObject::connect(&probe, &QTimer::timeout, [&] {
        longestStall = std::max(longestStall, heartbeat.restart());
    });
    probe.start();
    review->goToFrame(1);
    settle(300);
    heartbeat.restart();
    longestStall = 0;
    QPushButton* playButton = nullptr;
    for (auto* button : window.findChildren<QPushButton*>())
        if (button->toolTip() == QStringLiteral("Play or pause"))
            playButton = button;
    requireTest(playButton && playButton->isEnabled(), "play button available");
    playButton->click();
    settle(2500);
    playButton->click();
    probe.stop();
    std::cout << "longest UI stall during 1080p playback: " << longestStall << " ms\n";
    requireTest(longestStall < 120, "playback keeps the UI responsive (no frame-sized freezes)");

    // ---- Ground truth import from the Review page --------------------------
    requireTest(review->importAnnotations(truth.fileName()), "import ground truth");
    requireTest(review->observations().size() == 180, "every box imported");
    requireTest(label(window, "annotationSource")->text().startsWith(QStringLiteral("Verified annotations")),
                "the header says where the boxes come from");
    review->goToFrame(45);
    settle(800);
    screenshot(window, QStringLiteral("generated-review.png"));
    return 0;
}
