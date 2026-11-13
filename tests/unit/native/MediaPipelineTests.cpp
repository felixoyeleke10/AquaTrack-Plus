#include "TestSupport.h"

#include "application/AnalysisRunner.h"
#include "application/AnnotationImporter.h"
#include "application/ModelImporter.h"
#include "application/controllers/ReviewController.h"
#include "data/media/FrameDecoder.h"
#include "data/media/MediaSource.h"
#include "data/media/MediaTools.h"
#include "data/media/VideoProbe.h"
#include "data/media/VideoSequenceManifest.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>
#include <iostream>
#include <map>

namespace media = aquatrack::media;
using aquatrack::application::AnalysisResult;
using aquatrack::application::AnalysisRunner;
using aquatrack::application::AnnotationImport;
using aquatrack::application::AnnotationImporter;
using aquatrack::application::ModelImporter;
using aquatrack::application::ReviewController;

namespace {

void writeFile(const QString& path, const QByteArray& content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    requireTest(file.open(QIODevice::WriteOnly), "write fixture");
    file.write(content);
}

// testsrc draws a moving pattern and a frame counter, so every frame differs.
bool makeVideo(const QString& path, const QString& rate, int frames)
{
    QProcess ffmpeg;
    ffmpeg.start(media::ffmpegTool(QStringLiteral("ffmpeg")),
                 {QStringLiteral("-v"), QStringLiteral("error"), QStringLiteral("-y"),
                  QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
                  QStringLiteral("testsrc=size=160x120:rate=%1").arg(rate), QStringLiteral("-frames:v"),
                  QString::number(frames), QStringLiteral("-g"), QStringLiteral("12"),
                  QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), path});
    return ffmpeg.waitForFinished(60000) && ffmpeg.exitCode() == 0;
}

// Runs the event loop until `done` or the timeout.
template <typename Predicate>
bool waitFor(Predicate done, int milliseconds = 20000)
{
    QElapsedTimer clock;
    clock.start();
    while (!done() && clock.elapsed() < milliseconds)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return done();
}

std::map<int, QImage> decodeAll(media::FrameDecoder& decoder, const QList<int>& frames)
{
    std::map<int, QImage> decoded;
    QObject context;
    QObject::connect(&decoder, &media::FrameDecoder::frameDecoded, &context,
                     [&decoded](quint64, int frame, const QImage& image) { decoded[frame] = image; });
    for (const int frame : frames) {
        decoder.request(frame);
        requireTest(waitFor([&] { return decoded.count(frame) > 0; }), "frame decoded in time");
    }
    return decoded;
}

}  // namespace

int main(int argc, char* argv[])
{
    QGuiApplication application(argc, argv);
    QTemporaryDir temporary;
    requireTest(temporary.isValid(), "temporary directory");
    const QDir root(temporary.path());

    // ---- Classification needs no decoding --------------------------------
    writeFile(root.filePath(QStringLiteral("images/seqinfo.ini")), "[Sequence]\nseqLength=3\n");
    writeFile(root.filePath(QStringLiteral("clips/sequence.json")), "{}");
    writeFile(root.filePath(QStringLiteral("notes.txt")), "text");
    writeFile(root.filePath(QStringLiteral("fake.MP4")), "not a video");
    QDir().mkpath(root.filePath(QStringLiteral("plain")));
    requireTest(media::classifyMedia(root.filePath(QStringLiteral("images"))) ==
                    media::MediaKind::ImageSequence, "seqinfo.ini marks an image sequence");
    requireTest(media::classifyMedia(root.filePath(QStringLiteral("clips"))) ==
                    media::MediaKind::VideoSequence, "sequence.json marks a video sequence");
    requireTest(media::classifyMedia(root.filePath(QStringLiteral("fake.MP4"))) ==
                    media::MediaKind::VideoFile, "video extension, any case");
    requireTest(media::classifyMedia(root.filePath(QStringLiteral("notes.txt"))) ==
                    media::MediaKind::Unsupported, "a text file is not a video");
    requireTest(media::classifyMedia(root.filePath(QStringLiteral("plain"))) == media::MediaKind::Folder,
                "a plain folder is not a sequence");
    requireTest(media::classifyMedia(root.filePath(QStringLiteral("gone.mp4"))) ==
                    media::MediaKind::Missing, "missing paths are flagged");

    // ---- Model importer: a plain "weights" folder is not a training run ----
    writeFile(root.filePath(QStringLiteral("repo/weights/yolo26n.pt")), "w");
    const auto loose = ModelImporter::describe(root.filePath(QStringLiteral("repo/weights/yolo26n.pt")));
    requireTest(loose.runName == QStringLiteral("yolo26n") && loose.name == QStringLiteral("YOLO26n"),
                "loose weights are named after the file, not the folder above weights/");
    writeFile(root.filePath(QStringLiteral("runs/fish/weights/best.pt")), "w");
    writeFile(root.filePath(QStringLiteral("runs/fish/args.yaml")), "model: yolo11n.pt\nimgsz: 640\n");
    const auto run = ModelImporter::describe(root.filePath(QStringLiteral("runs/fish/weights/best.pt")));
    requireTest(run.runName == QStringLiteral("fish") && run.name == QStringLiteral("YOLO11n (fish)") &&
                    run.inputSize == 640,
                "a training run is still named after the run");

    if (media::ffmpegTool(QStringLiteral("ffmpeg")).isEmpty() ||
        media::ffmpegTool(QStringLiteral("ffprobe")).isEmpty()) {
        std::cout << "SKIP: FFmpeg not available; video decoding tests not run\n";
        return 0;
    }

    // ---- Probing: exact rates, not rounded ---------------------------------
    const QString clipA = root.filePath(QStringLiteral("a.mp4"));
    const QString clipB = root.filePath(QStringLiteral("b.mp4"));
    requireTest(makeVideo(clipA, QStringLiteral("25"), 30), "generate clip A");
    requireTest(makeVideo(clipB, QStringLiteral("30000/1001"), 20), "generate clip B");
    media::VideoInfo infoB;
    requireTest(media::probeVideo(clipB, infoB) && infoB.frameCount == 20 &&
                    std::abs(infoB.frameRate - 29.97) < 0.01 && infoB.width == 160,
                "29.97 fps is kept exact (it used to be rounded to 30)");
    media::VideoInfo notVideo;
    QString probeError;
    requireTest(!media::probeVideo(root.filePath(QStringLiteral("fake.MP4")), notVideo, &probeError) &&
                    !probeError.isEmpty(),
                "a file that is not really a video is rejected with a reason");

    // ---- A video sequence is one continuous timeline -----------------------
    media::VideoSequenceManifest manifest;
    manifest.name = QStringLiteral("Transect");
    media::VideoInfo infoA;
    requireTest(media::probeVideo(clipA, infoA), "probe clip A");
    manifest.clips = {infoA, infoB};
    const QString sequenceFolder = root.filePath(QStringLiteral("media/Transect"));
    requireTest(manifest.save(sequenceFolder), "save sequence.json");
    QFile stored(media::VideoSequenceManifest::manifestPath(sequenceFolder));
    requireTest(stored.open(QIODevice::ReadOnly) && stored.readAll().contains("\"../../a.mp4\""),
                "clips near the sequence are stored relative, so the project can move");

    media::MediaSource sequence;
    QString error;
    requireTest(sequence.open(sequenceFolder, &error), "open the video sequence");
    requireTest(sequence.kind() == media::MediaKind::VideoSequence && sequence.name() == QStringLiteral("Transect") &&
                    sequence.frameCount() == 50 && sequence.clips().size() == 2,
                "frames continue across clips");
    requireTest(sequence.clipIndexForFrame(30) == 0 && sequence.clipIndexForFrame(31) == 1,
                "frame 31 is the first frame of the second clip");
    requireTest(sequence.timestampMs(26) == 1000, "clip A runs at 25 fps");
    requireTest(sequence.timestampMs(31) == sequence.clips().at(1).startMs &&
                    std::abs(sequence.clips().at(1).startMs - 1200) <= 40,
                "clip B starts when clip A ends");
    requireTest(std::abs(sequence.timestampMs(32) - sequence.timestampMs(31) - 33) <= 1,
                "clip B keeps its own 29.97 fps clock");
    for (int frame = 1; frame <= sequence.frameCount(); ++frame)
        requireTest(sequence.frameAtTimestamp(sequence.timestampMs(frame)) == frame,
                    "time and frame convert both ways");

    // ---- Decoding: seeks land on exactly the frame sequential reading gives
    {
        media::FrameDecoder decoder;
        decoder.setSource(sequence);
        QList<int> inOrder;
        for (int frame = 1; frame <= sequence.frameCount(); ++frame)
            inOrder << frame;
        const auto sequential = decodeAll(decoder, inOrder);
        requireTest(sequential.at(1) != sequential.at(2), "frames differ, so the comparison means something");
        requireTest(sequential.at(30).size() == QSize(160, 120), "decoded at source size");

        media::FrameDecoder seeking;
        seeking.setSource(sequence);
        const auto jumped = decodeAll(seeking, {37, 5, 29, 31, 50, 2, 18, 44, 1});
        for (const auto& [frame, image] : jumped)
            requireTest(image == sequential.at(frame), "a seek decodes the same pixels as reading in order");
    }

    // ---- Review: frames arrive asynchronously, playback ends cleanly -------
    {
        ReviewController controller;
        int shown = 0;
        int last = 0;
        bool stopped = false;
        int errors = 0;
        QObject::connect(&controller, &ReviewController::frameReady,
                         [&](const QImage&, const QVector<aquatrack::GroundTruth>&, int frame, int) {
                             ++shown;
                             last = frame;
                         });
        QObject::connect(&controller, &ReviewController::playbackChanged,
                         [&](bool playing) { stopped = stopped || !playing; });
        QObject::connect(&controller, &ReviewController::errorOccurred, [&] { ++errors; });

        QElapsedTimer clock;
        clock.start();
        requireTest(controller.loadSequence(clipA), "open a video for review");
        requireTest(clock.elapsed() < 3000, "opening reads metadata only");
        requireTest(waitFor([&] { return last == 1; }), "first frame shown");
        controller.togglePlayback();
        requireTest(waitFor([&] { return stopped; }), "playback runs to the end and stops");
        requireTest(last == 30 && controller.currentFrame() == 30 && errors == 0,
                    "every frame up to the last is shown, without errors");

        // A failed load keeps the media on screen.
        requireTest(!controller.loadSequence(root.filePath(QStringLiteral("fake.MP4"))) && errors == 1,
                    "an unreadable video is reported");
        requireTest(controller.isLoaded() && controller.source().name() == QStringLiteral("a"),
                    "and the video already open stays open");
    }

    // ---- Annotation import -------------------------------------------------
    media::MediaSource clip;
    requireTest(clip.open(clipA), "open clip A");
    {
        const QString mot = root.filePath(QStringLiteral("gt/gt.txt"));
        writeFile(mot, "1,7,10,10,20,20,1,1,1\n"
                       "2,7,12,10,20,20,1,1,1\n"
                       "2,8,40,40,10,10,0,1,1\n"     // ignore region in gt.txt
                       "31,7,10,10,20,20,1,1,1\n"    // past the 30 frames
                       "3,9,50,50,0,10,1,1,1\n");    // zero width
        AnnotationImport imported;
        requireTest(AnnotationImporter::read(mot, clip, imported, &error), "read MOTChallenge");
        requireTest(imported.observations.size() == 2 && imported.tracks == 1 &&
                        imported.outOfRange == 1 && imported.skipped == 2,
                    "MOT rows: ignore regions, bad boxes and out-of-range frames are left out");
        requireTest(imported.observations.front().videoId == "a" &&
                        imported.observations.back().timestampMs == 40 &&
                        imported.observations.front().humanVerified,
                    "imported ground truth uses the media's name and clock");
    }
    {
        const QString coco = root.filePath(QStringLiteral("video019.json"));
        writeFile(coco, R"json({"videos":[{"id":1}],
            "images":[{"id":11,"file_name":"000000.png","frame_id":0,"video_id":1},
                      {"id":12,"file_name":"000001.png","frame_id":1,"video_id":1}],
            "annotations":[{"id":1,"image_id":11,"category_id":1,"bbox":[5,6,30,20],"track_id":14},
                           {"id":2,"image_id":12,"category_id":1,"bbox":[7,6,30,20],"track_id":14},
                           {"id":3,"image_id":12,"category_id":1,"bbox":[70,60,10,12],"track_id":19}],
            "categories":[{"id":1,"name":"object"}]})json");
        AnnotationImport imported;
        requireTest(AnnotationImporter::read(coco, clip, imported, &error), "read COCO tracking JSON");
        requireTest(imported.observations.size() == 3 && imported.tracks == 2 && imported.frames == 2,
                    "COCO annotations keep their tracks");
        requireTest(imported.observations.front().frameIndex == 1 &&
                        imported.observations.front().className == "Fish",
                    "0-based frame ids shift to 1-based; a generic category is named Fish");
        writeFile(coco, R"json({"images":[{"id":1,"frame_id":0}],
            "annotations":[{"id":1,"image_id":1,"category_id":1,"bbox":[1,1,5,5]}]})json");
        requireTest(!AnnotationImporter::read(coco, clip, imported, &error) &&
                        error.contains(QStringLiteral("track_id")),
                    "a detection-only dataset is refused with a reason");
    }

    // ---- Detector results ---------------------------------------------------
    {
        const QString results = root.filePath(QStringLiteral("a.detections.json"));
        writeFile(results, R"json({"format":"aquatrack-analysis","version":1,"frames":30,
            "model":{"name":"YOLO11n (fish)"},"classes":["Fish"],
            "observations":[[1,3,0,0.81,10,10,40,30],[26,3,0,0.77,12,10,42,30],
                            [26,3,0,0.5,0,0,5,5],[40,4,0,0.9,1,1,9,9]]})json");
        AnalysisResult result;
        requireTest(AnalysisRunner::readResults(results, clip, result, &error), "read detector results");
        requireTest(result.observations.size() == 2 && result.tracks == 1 &&
                        result.modelName == QStringLiteral("YOLO11n (fish)"),
                    "one box per track per frame, frames past the end dropped");
        requireTest(result.observations.back().timestampMs == 1000 &&
                        !result.observations.back().humanVerified,
                    "detections are timed by the media clock and marked unverified");
        writeFile(results, R"json({"format":"aquatrack-analysis","frames":29,"observations":[]})json");
        requireTest(!AnalysisRunner::readResults(results, clip, result, &error),
                    "results for a different frame count are refused");
    }
    return 0;
}
