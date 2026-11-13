#include "application/SequenceEditing.h"

#include <QDir>
#include <QFileInfo>

#include <cmath>

namespace aquatrack::application {
namespace {

QString key(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toLower();
}

// Where each clip sits on the sequence's timeline, as MediaSource lays it out.
struct Placement {
    int firstFrame = 1;
    qint64 startMs = 0;
};

QVector<Placement> layOut(const QVector<media::VideoInfo>& clips)
{
    QVector<Placement> placements;
    int first = 1;
    qint64 start = 0;
    for (const auto& clip : clips) {
        placements.push_back({first, start});
        first += clip.frameCount;
        start += clip.durationMs > 0
                     ? clip.durationMs
                     : qint64(std::llround(clip.frameCount * 1000.0 / clip.frameRate));
    }
    return placements;
}

int clipAt(const QVector<media::VideoInfo>& clips, const QVector<Placement>& placements,
           qint64 frame)
{
    for (int i = 0; i < clips.size(); ++i)
        if (frame >= placements[i].firstFrame &&
            frame < placements[i].firstFrame + clips[i].frameCount)
            return i;
    return -1;
}

int indexOf(const QVector<media::VideoInfo>& clips, const QString& path)
{
    const QString wanted = key(path);
    for (int i = 0; i < clips.size(); ++i)
        if (key(clips[i].path) == wanted)
            return i;
    return -1;
}

// Old clip index -> new clip index (or -1), and the new frame for an old one.
struct Mapping {
    QVector<media::VideoInfo> before;
    QVector<media::VideoInfo> after;
    QVector<Placement> oldPlaces;
    QVector<Placement> newPlaces;
    QVector<int> target;

    Mapping(const QVector<media::VideoInfo>& from, const QVector<media::VideoInfo>& to)
        : before(from), after(to), oldPlaces(layOut(from)), newPlaces(layOut(to))
    {
        for (const auto& clip : before)
            target.push_back(indexOf(after, clip.path));
    }

    // {new clip, offset within it}, or {-1, 0}.
    std::pair<int, qint64> locate(qint64 frame) const
    {
        const int old = clipAt(before, oldPlaces, frame);
        if (old < 0 || target[old] < 0)
            return {-1, 0};
        const qint64 offset = frame - oldPlaces[old].firstFrame;
        if (offset >= after[target[old]].frameCount)
            return {-1, 0};
        return {target[old], offset};
    }
};

}  // namespace

SequenceRemap remapSequenceObservations(const std::vector<domain::Observation>& observations,
                                        const QVector<media::VideoInfo>& before,
                                        const QVector<media::VideoInfo>& after,
                                        const std::string& videoId)
{
    const Mapping mapping(before, after);
    SequenceRemap result;
    QStringList dropped;
    for (const auto& observation : observations) {
        const auto [clip, offset] = mapping.locate(observation.frameIndex);
        if (clip < 0) {
            ++result.dropped;
            const int old = clipAt(before, mapping.oldPlaces, observation.frameIndex);
            const QString name = old >= 0 ? QFileInfo(before[old].path).fileName()
                                          : QStringLiteral("frame %1").arg(observation.frameIndex);
            if (!dropped.contains(name))
                dropped << name;
            continue;
        }
        domain::Observation moved = observation;
        const auto& info = after[clip];
        moved.videoId = videoId;
        moved.frameIndex = mapping.newPlaces[clip].firstFrame + offset;
        moved.timestampMs =
            mapping.newPlaces[clip].startMs + qint64(std::llround(offset * 1000.0 / info.frameRate));
        result.kept.push_back(std::move(moved));
    }
    result.droppedClips = dropped;
    return result;
}

int remapSequenceFrame(int frame, const QVector<media::VideoInfo>& before,
                       const QVector<media::VideoInfo>& after)
{
    const Mapping mapping(before, after);
    const auto [clip, offset] = mapping.locate(frame);
    return clip < 0 ? 0 : int(mapping.newPlaces[clip].firstFrame + offset);
}

}  // namespace aquatrack::application
