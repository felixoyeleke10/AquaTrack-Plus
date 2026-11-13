// Opening one project after another must not carry anything across: not the
// media on screen, not its annotations, not the model registry. Also times
// how long opening a project blocks the window.

#include "TestSupport.h"

#include "application/controllers/ModelController.h"
#include "application/controllers/ProjectController.h"
#include "data/observations/JsonObservationRepository.h"
#include "ui/MainWindow.h"
#include "ui/review/ReviewPage.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

#include <iostream>

namespace {

QString writeSequence(const QDir& root, const QString& name)
{
    const QString directory = root.filePath(name);
    QDir().mkpath(directory + QStringLiteral("/img1"));
    QDir().mkpath(directory + QStringLiteral("/gt"));
    QFile info(directory + QStringLiteral("/seqinfo.ini"));
    requireTest(info.open(QIODevice::WriteOnly | QIODevice::Text), "write seqinfo.ini");
    QTextStream(&info) << "[Sequence]\nname=" << name
                       << "\nimDir=img1\nframeRate=10\nseqLength=3\nimWidth=64\nimHeight=48\nimExt=.png\n";
    QFile truth(directory + QStringLiteral("/gt/gt.txt"));
    requireTest(truth.open(QIODevice::WriteOnly | QIODevice::Text), "write gt.txt");
    for (int frame = 1; frame <= 3; ++frame) {
        QImage image(64, 48, QImage::Format_RGB32);
        image.fill(Qt::darkBlue);
        image.save(QStringLiteral("%1/img1/%2.png").arg(directory).arg(frame, 6, 10, QLatin1Char('0')));
        QTextStream(&truth) << frame << ",1,5,5,10,10,1,1,1\n";
    }
    return directory;
}

QString createProject(const QDir& root, const QString& name)
{
    QDir().mkpath(root.filePath(name));
    const QString path = root.filePath(name + QStringLiteral("/") + name + QStringLiteral(".aqp"));
    aquatrack::application::ProjectController creator;
    requireTest(creator.createProject(path, name), "create project");
    return path;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QStandardPaths::setTestModeEnabled(true);

    if (const QString demo = qEnvironmentVariable("AQUATRACK_DEMO_PROJECT"); !demo.isEmpty()) {
        {
            QElapsedTimer storeClock;
            storeClock.start();
            aquatrack::data::JsonObservationRepository store(
                QDir(QFileInfo(demo).absolutePath()).filePath(QStringLiteral("annotations/observations.json")));
            std::string message;
            (void)store.open(message);
            std::cout << "loading the observation store alone: " << storeClock.elapsed() << " ms" << std::endl;
        }
        MainWindow window;
        window.show();
        QElapsedTimer clock;
        clock.start();
        window.openProject(demo);
        std::cout << "opening " << demo.toStdString() << " blocked the window for " << clock.elapsed()
                  << " ms\n";
        return 0;
    }

    QTemporaryDir temporary;
    requireTest(temporary.isValid(), "temporary directory");
    const QDir root(temporary.path());
    const QString sequence = writeSequence(root, QStringLiteral("seq-a"));
    const QString projectA = createProject(root, QStringLiteral("Alpha"));
    const QString projectB = createProject(root, QStringLiteral("Bravo"));

    MainWindow window;
    window.show();
    auto* review = window.findChild<ReviewPage*>();
    auto* projects = window.findChild<aquatrack::application::ProjectController*>();
    auto* models = window.findChild<aquatrack::application::ModelController*>();

    // ---- Project A with media on screen ------------------------------------
    window.openProject(projectA);
    window.openSequence(sequence);
    requireTest(projects->project()->mediaPaths.size() == 1, "media opened in A joins A");
    requireTest(models->root() == QDir(QFileInfo(projectA).absolutePath()).filePath(QStringLiteral("models")),
                "A's model registry is shown");

    // ---- Switching to project B --------------------------------------------
    window.openProject(projectB);
    requireTest(projects->project()->name == "Bravo", "B is open");
    requireTest(projects->project()->mediaPaths.empty(),
                "B does not gain the media that was on screen for A");
    requireTest(!review->source().isOpen() && review->observations().empty(),
                "Review closes A's media instead of showing it under B");
    aquatrack::data::JsonObservationRepository storeB(
        QDir(QFileInfo(projectB).absolutePath()).filePath(QStringLiteral("annotations/observations.json")));
    std::string message;
    requireTest(!storeB.open(message) || !storeB.containsVideo("seq-a"),
                "A's annotations are not written into B's store");
    requireTest(models->root() == QDir(QFileInfo(projectB).absolutePath()).filePath(QStringLiteral("models")),
                "the Models page follows the open project");

    // ---- Back to A: its media and annotations are still there --------------
    window.openProject(projectA);
    requireTest(projects->project()->mediaPaths.size() == 1, "A still lists its media");

    // ---- Opening media before any project still joins the first project ---
    {
        MainWindow fresh;
        fresh.openSequence(sequence);
        const QString projectC = createProject(root, QStringLiteral("Charlie"));
        fresh.openProject(projectC);
        auto* freshProjects = fresh.findChild<aquatrack::application::ProjectController*>();
        requireTest(freshProjects->project()->mediaPaths.size() == 1,
                    "media reviewed without a project joins the project then opened");
    }
    return 0;
}
