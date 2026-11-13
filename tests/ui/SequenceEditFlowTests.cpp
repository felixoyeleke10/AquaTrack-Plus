#include "TestSupport.h"

#include "application/ProjectLayout.h"
#include "application/ReviewSession.h"
#include "application/controllers/ProjectController.h"
#include "data/media/MediaSource.h"
#include "data/media/MediaTools.h"
#include "data/media/VideoProbe.h"
#include "data/media/VideoSequenceManifest.h"
#include "data/observations/JsonObservationRepository.h"
#include "ui/MainWindow.h"
#include "ui/videos/SequenceDialog.h"
#include "ui/videos/VideosPage.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>

#include <iostream>

namespace media = aquatrack::media;

namespace {

bool makeVideo(const QString& path, int frames)
{
    QProcess ffmpeg;
    ffmpeg.start(media::ffmpegTool(QStringLiteral("ffmpeg")),
                 {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-y"),
                  QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
                  QStringLiteral("testsrc=size=160x120:rate=10"), QStringLiteral("-frames:v"),
                  QString::number(frames), QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"),
                  path});
    return ffmpeg.waitForFinished(60000) && ffmpeg.exitCode() == 0;
}

aquatrack::domain::Observation observation(qint64 frame, const std::string& id)
{
    aquatrack::domain::Observation item;
    item.id = "Seq:" + id;
    item.videoId = "Seq";
    item.frameIndex = frame;
    item.localTrackId = 1;
    item.box = {10, 10, 20, 20};
    item.className = "Fish";
    item.confidence = 0.9F;
    item.humanVerified = true;
    return item;
}

// Answers the modal dialogs of an edit the way a reviewer would: in the
// sequence editor, puts the last clip first, removes the middle one and
// renames the sequence; agrees to delete the removed clip's annotations.
class EditScript final : public QObject {
public:
    EditScript()
    {
        m_timer.setInterval(40);
        connect(&m_timer, &QTimer::timeout, this, [this] { answer(); });
        m_timer.start();
    }
    QStringList seen;

private:
    void answer()
    {
        QWidget* modal = QApplication::activeModalWidget();
        if (!modal || modal == m_last)
            return;
        m_last = modal;
        if (auto* dialog = qobject_cast<SequenceDialog*>(modal)) {
            auto* clips = dialog->findChild<QListWidget*>(QStringLiteral("sequenceClips"));
            auto* name = dialog->findChild<QLineEdit*>();
            seen << QStringLiteral("editor with %1 clips").arg(clips ? clips->count() : -1);
            if (clips && clips->count() == 3 && name) {
                clips->insertItem(0, clips->takeItem(2));  // c, a, b
                delete clips->takeItem(2);                  // c, a
                name->setText(QStringLiteral("Seq renamed"));
                dialog->accept();
            } else {
                dialog->reject();
            }
        } else if (auto* box = qobject_cast<QMessageBox*>(modal)) {
            seen << QStringLiteral("message: %1").arg(box->windowTitle());
            QAbstractButton* choice = box->button(QMessageBox::Save);
            (choice ? choice : box->buttons().constFirst())->click();
        } else {
            seen << QStringLiteral("unexpected: %1").arg(modal->metaObject()->className());
            modal->close();
        }
    }

    QTimer m_timer;
    QWidget* m_last = nullptr;
};

}  // namespace

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("AquaTrack"));
    app.setApplicationName(QStringLiteral("AquaTrackSequenceEditTests"));
    if (media::ffmpegTool(QStringLiteral("ffmpeg")).isEmpty()) {
        std::cout << "SKIPPED: ffmpeg not found\n";
        return 0;
    }
    QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
                  QStringLiteral("/settings.json"));

    QTemporaryDir temporary;
    requireTest(temporary.isValid(), "temporary directory");
    const QString projectFile = QDir(temporary.path()).filePath(QStringLiteral("Survey/Survey.aqp"));
    const aquatrack::application::ProjectLayout layout(projectFile);
    requireTest(layout.create(), "project folder");

    // Three clips at 10 fps: a = frames 1-10, b = 11-15, c = 16-23.
    QVector<media::VideoInfo> clips;
    for (const auto& [name, frames] : {std::pair{"a", 10}, {"b", 5}, {"c", 8}}) {
        const QString path = QDir(layout.videos()).filePath(QStringLiteral("%1.mp4").arg(QLatin1String(name)));
        requireTest(makeVideo(path, frames), "generate a clip");
        media::VideoInfo info;
        requireTest(media::probeVideo(path, info) && info.frameCount == frames, "probe a clip");
        clips.push_back(info);
    }
    const QString sequence = QDir(layout.sequences()).filePath(QStringLiteral("Seq"));
    media::VideoSequenceManifest manifest;
    manifest.name = QStringLiteral("Seq");
    manifest.clips = clips;
    requireTest(manifest.save(sequence), "save the sequence");

    {
        aquatrack::application::ProjectController projects;
        requireTest(projects.createProject(projectFile, QStringLiteral("Survey")) &&
                        projects.addMedia(sequence) && projects.saveProject(),
                    "project with the sequence");
        aquatrack::data::JsonObservationRepository store(projects.observationStorePath());
        std::string error;
        const std::vector<aquatrack::domain::Observation> saved{
            observation(3, "on-a"), observation(12, "on-b"), observation(16, "on-c")};
        requireTest(store.open(error) && store.replaceObservationsForVideo("Seq", saved, error),
                    "store annotations on each clip");
    }
    // Review had stopped on clip a's 3rd frame.
    requireTest(aquatrack::application::saveReviewSession(projectFile, {sequence, 3, 1}),
                "record where review stopped");

    {
        MainWindow window;
        window.show();
        window.openProject(projectFile);
        auto* videos = window.findChild<VideosPage*>();
        requireTest(videos != nullptr, "videos page");

        EditScript script;
        emit videos->editSequenceRequested(sequence);
        for (const auto& line : script.seen)
            std::cout << "  dialog -> " << line.toStdString() << '\n';
        requireTest(script.seen.size() >= 2 && script.seen.first() == QStringLiteral("editor with 3 clips"),
                    "the editor opens with the sequence's clips");
        requireTest(script.seen.contains(QStringLiteral("message: Delete annotations?")),
                    "removing a clip with annotations asks first");
    }

    // ---- The sequence on disk: new order, new name ------------------------
    media::VideoSequenceManifest edited;
    requireTest(edited.load(sequence) && edited.name == QStringLiteral("Seq renamed") &&
                    edited.clips.size() == 2 &&
                    QFileInfo(edited.clips[0].path).fileName() == QStringLiteral("c.mp4") &&
                    QFileInfo(edited.clips[1].path).fileName() == QStringLiteral("a.mp4"),
                "the sequence is saved in its new order under its new name");
    requireTest(media::MediaSource::reviewIdFor(sequence) == QStringLiteral("Seq renamed"),
                "the sequence is now known by its new name");

    // ---- The folder explains itself ------------------------------------------
    QFile guide(QDir(sequence).filePath(QStringLiteral("README.txt")));
    requireTest(guide.open(QIODevice::ReadOnly | QIODevice::Text), "the sequence has a README");
    const QString guideText = QString::fromUtf8(guide.readAll());
    const qsizetype first = guideText.indexOf(QStringLiteral("1. c.mp4"));
    const qsizetype second = guideText.indexOf(QStringLiteral("2. a.mp4"));
    requireTest(guideText.startsWith(QStringLiteral("AquaTrack+ sequence: Seq renamed")),
                "the README is titled with the sequence's current name");
    requireTest(first >= 0 && second > first && !guideText.contains(QStringLiteral("b.mp4")),
                "the README lists the clips in their new play order");
    requireTest(guideText.contains(QStringLiteral("How to open it")) &&
                    guideText.contains(QDir::toNativeSeparators(QStringLiteral("../../videos/a.mp4"))),
                "the README says how to open the sequence and where its videos are");

    // ---- Annotations moved with their clips --------------------------------
    aquatrack::data::JsonObservationRepository store(
        QDir(layout.root()).filePath(QString::fromLatin1(aquatrack::application::ProjectLayout::ObservationStore)));
    std::string error;
    requireTest(store.open(error), "reopen the store");
    requireTest(!store.containsVideo("Seq"), "nothing is left under the old name");
    const auto moved = store.observationsForVideo("Seq renamed");
    requireTest(moved.size() == 2, "annotations on the removed clip are gone, the rest kept");
    for (const auto& item : moved) {
        if (item.id == "Seq renamed:on-c")
            requireTest(item.frameIndex == 1 && item.timestampMs == 0,
                        "clip c now opens the sequence");
        else if (item.id == "Seq renamed:on-a")
            requireTest(item.frameIndex == 11 && item.timestampMs == 1000,
                        "clip a follows c: its 3rd frame is now frame 11");
        else
            requireTest(false, "only the annotations on a and c remain, renamed with the sequence");
        requireTest(item.humanVerified && item.className == "Fish",
                    "moving keeps what the reviewer recorded");
    }

    // ---- Where review stopped follows its clip ------------------------------
    const auto session = aquatrack::application::loadReviewSession(projectFile);
    requireTest(session && session->frame == 11, "the resume point moves with its clip");
    return 0;
}
