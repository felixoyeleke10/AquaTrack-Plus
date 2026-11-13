#include "TestSupport.h"

#include "application/SequenceEditing.h"

#include <QDir>

using aquatrack::application::remapSequenceFrame;
using aquatrack::application::remapSequenceObservations;
using aquatrack::domain::Observation;
using aquatrack::media::VideoInfo;

namespace {

VideoInfo clip(const QString& name, int frames, double rate)
{
    VideoInfo info;
    info.path = QDir::cleanPath(QDir::temp().filePath(name));
    info.frameCount = frames;
    info.frameRate = rate;
    info.width = 640;
    info.height = 360;
    info.durationMs = qint64(frames * 1000.0 / rate);
    return info;
}

Observation at(qint64 frame, const std::string& id)
{
    Observation observation;
    observation.id = id;
    observation.videoId = "Transect";
    observation.frameIndex = frame;
    observation.localTrackId = 7;
    observation.className = "fish";
    observation.humanVerified = true;
    return observation;
}

}  // namespace

int main()
{
    // A: frames 1-10 (10 fps), B: 11-15 (5 fps), C: 16-23 (8 fps); 1 s each.
    const VideoInfo a = clip(QStringLiteral("a.mp4"), 10, 10.0);
    const VideoInfo b = clip(QStringLiteral("b.mp4"), 5, 5.0);
    const VideoInfo c = clip(QStringLiteral("c.mp4"), 8, 8.0);
    const QVector<VideoInfo> before{a, b, c};

    // ---- Reorder and remove: C first, then A; B is gone -------------------
    const QVector<VideoInfo> after{c, a};
    const std::vector<Observation> observations{at(3, "on-a"), at(12, "on-b"), at(16, "first-of-c"),
                                                at(23, "last-of-c")};
    const auto remap = remapSequenceObservations(observations, before, after, "Transect A");
    requireTest(remap.kept.size() == 3 && remap.dropped == 1,
                "observations on a removed clip are dropped, the rest kept");
    requireTest(remap.droppedClips == QStringList{QStringLiteral("b.mp4")},
                "the removed clip is named");
    const auto find = [&](const std::string& id) {
        for (const auto& observation : remap.kept)
            if (observation.id == id)
                return observation;
        requireTest(false, "observation kept");
        return Observation{};
    };
    // A now starts at frame 9 and 1000 ms: its 3rd frame is 11, at 1200 ms.
    requireTest(find("on-a").frameIndex == 11 && find("on-a").timestampMs == 1200,
                "an observation moves with its clip");
    requireTest(find("first-of-c").frameIndex == 1 && find("first-of-c").timestampMs == 0,
                "a clip moved to the front starts the sequence");
    requireTest(find("last-of-c").frameIndex == 8 && find("last-of-c").timestampMs == 875,
                "timestamps use the clip's own frame rate");
    requireTest(find("on-a").videoId == "Transect A" && find("on-a").localTrackId == 7 &&
                    find("on-a").humanVerified && find("on-a").className == "fish",
                "only the position and the sequence name change");

    // ---- Appending a clip leaves earlier frames where they were -----------
    const auto appended = remapSequenceObservations(observations, before, {a, b, c, clip("d.mp4", 4, 4.0)},
                                                    "Transect");
    requireTest(appended.dropped == 0 && appended.kept.size() == 4 &&
                    appended.kept[0].frameIndex == 3 && appended.kept[3].frameIndex == 23,
                "adding a clip at the end moves nothing");

    // ---- Paths match regardless of case -----------------------------------
    VideoInfo shouted = a;
    shouted.path = a.path.toUpper();
    requireTest(remapSequenceObservations({at(5, "x")}, before, {shouted}, "T").kept.size() == 1,
                "clips are matched case-insensitively, as Windows paths are");

    // ---- Single frames (where review stopped) -----------------------------
    requireTest(remapSequenceFrame(3, before, after) == 11, "a frame follows its clip");
    requireTest(remapSequenceFrame(12, before, after) == 0, "a frame on a removed clip maps to 0");
    return 0;
}
