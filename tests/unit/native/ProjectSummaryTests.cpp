#include "TestSupport.h"

#include "application/ProjectSummary.h"
#include "data/media/MediaTools.h"
#include "data/media/VideoProbe.h"
#include "data/media/VideoSequenceManifest.h"
#include "data/observations/JsonObservationRepository.h"

#include <QCoreApplication>
#include <QDir>
#include <QProcess>
#include <QTemporaryDir>

#include <cstdlib>
#include <iostream>

namespace media = aquatrack::media;
using aquatrack::application::FootageState;

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

aquatrack::domain::Observation box(const std::string& video, qint64 frame, qint64 track,
                                   bool verified)
{
    aquatrack::domain::Observation item;
    item.id = video + ":" + std::to_string(frame) + ":" + std::to_string(track);
    item.videoId = video;
    item.frameIndex = frame;
    item.timestampMs = (frame - 1) * 100;
    item.localTrackId = track;
    item.box = {10, 10, 20, 20};
    item.className = "Fish";
    item.confidence = 0.8F;
    item.humanVerified = verified;
    return item;
}

}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    if (media::ffmpegTool(QStringLiteral("ffmpeg")).isEmpty()) {
        std::cout << "SKIPPED: ffmpeg not found\n";
        return 0;
    }
    QTemporaryDir temporary;
    requireTest(temporary.isValid(), "temporary directory");
    const QDir root(temporary.path());
    const QString a = root.filePath(QStringLiteral("a.mp4"));
    const QString b = root.filePath(QStringLiteral("b.mp4"));
    const QString c = root.filePath(QStringLiteral("c.mp4"));  // in the sequence; no fish
    const QString d = root.filePath(QStringLiteral("d.mp4"));  // on its own; never analysed
    requireTest(makeVideo(a, 10) && makeVideo(b, 10) && makeVideo(c, 10) && makeVideo(d, 10),
                "generate four 1 s clips");

    // a and b on their own and, with c, joined as a sequence.
    media::VideoSequenceManifest manifest;
    manifest.name = QStringLiteral("Seq");
    for (const QString& path : {a, b, c}) {
        media::VideoInfo info;
        requireTest(media::probeVideo(path, info), "probe a clip");
        manifest.clips.push_back(info);
    }
    const QString sequence = root.filePath(QStringLiteral("Seq"));
    requireTest(manifest.save(sequence), "save the sequence");

    // A reviewed on its own (verified); the sequence detected on both clips.
    const QString store = root.filePath(QStringLiteral("observations.json"));
    {
        aquatrack::data::JsonObservationRepository repository(store);
        std::string error;
        requireTest(repository.open(error), "open the store");
        const std::vector<aquatrack::domain::Observation> onA{box("a", 1, 1, true), box("a", 2, 1, true)};
        const std::vector<aquatrack::domain::Observation> onSequence{
            box("Seq", 3, 5, false),    // clip a
            box("Seq", 12, 6, false),   // clip b
            box("Seq", 15, 6, false)};  // clip b
        requireTest(repository.replaceObservationsForVideo("a", onA, error) &&
                        repository.replaceObservationsForVideo("Seq", onSequence, error),
                    "store the annotations");
    }

    const auto summary = aquatrack::application::summarizeProject({a, b, sequence, d}, store);

    // Footage once: four videos (a and b are also inside the sequence).
    requireTest(summary.footage == 4 && summary.frames == 40, "each video is counted once");
    requireTest(summary.durationMs >= 3900 && summary.durationMs <= 4100, "duration is counted once");
    requireTest(summary.overlapping, "the overlap is reported");

    // Clip a from its verified item, clip b from the sequence's detections.
    requireTest(summary.observations == 4 && summary.verified == 2,
                "each video's figures come from the item that reviewed it best");
    requireTest(summary.tracks == 2, "tracks are counted from the chosen items only");
    // a verified; b and c analysed (c found nothing, which is not "skipped");
    // d never analysed.
    const qint64 second = summary.durationMs / 4;
    const auto about = [second](qint64 value, int clips) {
        return std::abs(value - clips * second) <= 100 * clips;
    };
    requireTest(about(summary.msIn(FootageState::Verified), 1), "one video's worth is verified");
    requireTest(about(summary.msIn(FootageState::Detected), 2),
                "analysed footage counts as analysed even where nothing was found");
    requireTest(about(summary.msIn(FootageState::NotAnalysed), 1),
                "footage nothing has looked at is not analysed");
    requireTest(summary.msIn(FootageState::InReview) == 0, "nothing is part-verified");
    requireTest(summary.msIn(FootageState::Verified) + summary.msIn(FootageState::Detected) +
                        summary.msIn(FootageState::NotAnalysed) ==
                    summary.durationMs,
                "the states add up to the footage");

    // Items keep their own figures for their cards.
    requireTest(summary.media.size() == 4 && summary.media[0].observations == 2 &&
                    summary.media[1].observations == 0 && summary.media[2].observations == 3 &&
                    summary.media[3].observations == 0,
                "per-item figures are unchanged");
    requireTest(summary.species.size() == 1 && summary.species[0].tracks == 2,
                "individuals are counted from the chosen items only");
    return 0;
}
