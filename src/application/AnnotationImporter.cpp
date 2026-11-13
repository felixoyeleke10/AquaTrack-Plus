#include "application/AnnotationImporter.h"

#include "data/BrackishMotSequence.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace aquatrack::application {
namespace {

void setError(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

QString displayClassName(const QString& name)
{
    static const QSet<QString> generic{QString(), QStringLiteral("object"), QStringLiteral("objects"),
                                       QStringLiteral("target"), QStringLiteral("animal"),
                                       QStringLiteral("fish"), QStringLiteral("0")};
    const QString cleaned = name.trimmed();
    if (generic.contains(cleaned.toLower()))
        return QStringLiteral("Fish");
    if (const int known = BrackishMotSequence::classIdForName(cleaned); known > 0)
        return BrackishMotSequence::className(known);
    return cleaned.left(1).toUpper() + cleaned.mid(1);
}

// Builds observations, keeping ids unique if a file repeats a track within a
// frame (it happens in hand-merged files).
class Collector {
public:
    Collector(const media::MediaSource& source, AnnotationImport& result)
        : m_source(source), m_result(result), m_videoId(source.name().toStdString())
    {
    }

    void add(int frame, qint64 track, double x, double y, double width, double height,
             const QString& className, double confidence)
    {
        if (frame < 1 || frame > m_source.frameCount()) {
            ++m_result.outOfRange;
            return;
        }
        domain::Observation observation;
        std::string id = m_videoId + ":" + std::to_string(frame) + ":" + std::to_string(track);
        if (!m_ids.insert(id).second) {
            int suffix = 2;
            while (!m_ids.insert(id + "#" + std::to_string(suffix)).second)
                ++suffix;
            id += "#" + std::to_string(suffix);
        }
        observation.id = std::move(id);
        observation.videoId = m_videoId;
        observation.frameIndex = frame;
        observation.timestampMs = m_source.timestampMs(frame);
        observation.localTrackId = track;
        observation.box = {float(x), float(y), float(x + width), float(y + height)};
        observation.className = className.toStdString();
        observation.confidence = float(std::clamp(confidence, 0.0, 1.0));
        observation.humanVerified = true;
        if (!std::isfinite(x) || !std::isfinite(y) || !domain::isValid(observation)) {
            ++m_result.skipped;
            return;
        }
        m_tracks.insert(track);
        m_frames.insert(frame);
        m_result.observations.push_back(std::move(observation));
    }

    void finish()
    {
        m_result.tracks = int(m_tracks.size());
        m_result.frames = int(m_frames.size());
        std::sort(m_result.observations.begin(), m_result.observations.end(),
                  [](const auto& left, const auto& right) {
                      return left.frameIndex != right.frameIndex ? left.frameIndex < right.frameIndex
                                                                 : left.localTrackId < right.localTrackId;
                  });
    }

private:
    const media::MediaSource& m_source;
    AnnotationImport& m_result;
    std::string m_videoId;
    std::set<std::string> m_ids;
    std::set<qint64> m_tracks;
    std::set<int> m_frames;
};

bool readMot(const QString& path, Collector& collector, AnnotationImport& result, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(error, file.errorString());
        return false;
    }
    const bool groundTruth = QFileInfo(path).fileName().compare(QLatin1String("gt.txt"),
                                                                Qt::CaseInsensitive) == 0;
    QTextStream stream(&file);
    int lineNumber = 0;
    int rows = 0;
    while (!stream.atEnd()) {
        ++lineNumber;
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QStringList fields = line.split(QRegularExpression(QStringLiteral("[,\\s]+")),
                                              Qt::SkipEmptyParts);
        if (fields.size() < 6) {
            setError(error, QStringLiteral("Line %1 has %2 values; a MOTChallenge row needs at least "
                                           "frame,id,x,y,w,h")
                                .arg(lineNumber)
                                .arg(fields.size()));
            return false;
        }
        bool ok = true;
        double values[9] = {0, 0, 0, 0, 0, 0, 1, -1, 1};
        for (int index = 0; index < std::min<qsizetype>(9, fields.size()); ++index) {
            bool parsed = false;
            values[index] = fields.at(index).toDouble(&parsed);
            ok = ok && parsed;
        }
        if (!ok) {
            setError(error, QStringLiteral("Line %1 is not numeric: %2").arg(lineNumber).arg(line));
            return false;
        }
        ++rows;
        double confidence = fields.size() > 6 ? values[6] : 1.0;
        if (groundTruth && fields.size() > 6 && confidence == 0.0) {
            ++result.skipped;  // MOT ignore region
            continue;
        }
        if (confidence < 0.0)
            confidence = 1.0;
        const int classId = fields.size() > 7 ? int(values[7]) : -1;
        const QString className = classId >= 1 && classId <= 6
                                      ? BrackishMotSequence::className(classId)
                                      : QStringLiteral("Fish");
        collector.add(int(std::lround(values[0])), qint64(std::llround(values[1])), values[2],
                      values[3], values[4], values[5], className, confidence);
    }
    if (rows == 0) {
        setError(error, QStringLiteral("%1 contains no annotation rows").arg(QFileInfo(path).fileName()));
        return false;
    }
    result.format = QStringLiteral("MOTChallenge");
    return true;
}

bool readCoco(const QJsonObject& root, Collector& collector, AnnotationImport& result, QString* error)
{
    const auto images = root.value(QStringLiteral("images")).toArray();
    const auto annotations = root.value(QStringLiteral("annotations")).toArray();
    if (images.isEmpty() || annotations.isEmpty()) {
        setError(error, QStringLiteral("The JSON has no images or no annotations"));
        return false;
    }

    // Only the first video of a multi-video file: the rest belong to other media.
    QSet<qint64> videos;
    for (const auto value : images)
        if (value.toObject().contains(QStringLiteral("video_id")))
            videos.insert(qint64(value.toObject().value(QStringLiteral("video_id")).toDouble()));
    const qint64 firstVideo = videos.isEmpty() ? -1 : *std::min_element(videos.cbegin(), videos.cend());

    static const QRegularExpression digits(QStringLiteral("(\\d+)(?!.*\\d)"));
    QHash<qint64, int> frameOfImage;
    int smallest = std::numeric_limits<int>::max();
    for (const auto value : images) {
        const auto image = value.toObject();
        if (firstVideo >= 0 && qint64(image.value(QStringLiteral("video_id")).toDouble()) != firstVideo)
            continue;
        int frame = -1;
        if (image.contains(QStringLiteral("frame_id")))
            frame = image.value(QStringLiteral("frame_id")).toInt();
        else if (image.contains(QStringLiteral("frame_index")))
            frame = image.value(QStringLiteral("frame_index")).toInt();
        else if (const auto match = digits.match(QFileInfo(image.value(QStringLiteral("file_name")).toString())
                                                     .completeBaseName());
                 match.hasMatch())
            frame = match.captured(1).toInt();
        if (frame < 0)
            continue;
        frameOfImage.insert(qint64(image.value(QStringLiteral("id")).toDouble()), frame);
        smallest = std::min(smallest, frame);
    }
    if (frameOfImage.isEmpty()) {
        setError(error, QStringLiteral("No image in the JSON says which frame it is (frame_id or a "
                                       "numbered file_name)"));
        return false;
    }
    const int shift = smallest == 0 ? 1 : 0;  // 0-based frame ids

    QHash<qint64, QString> categories;
    for (const auto value : root.value(QStringLiteral("categories")).toArray()) {
        const auto category = value.toObject();
        categories.insert(qint64(category.value(QStringLiteral("id")).toDouble()),
                          displayClassName(category.value(QStringLiteral("name")).toString()));
    }

    for (const auto value : annotations) {
        const auto annotation = value.toObject();
        const auto frame = frameOfImage.constFind(qint64(annotation.value(QStringLiteral("image_id")).toDouble()));
        if (frame == frameOfImage.constEnd())
            continue;  // another video's image
        const auto box = annotation.value(QStringLiteral("bbox")).toArray();
        if (box.size() != 4) {
            ++result.skipped;
            continue;
        }
        QJsonValue track = annotation.value(QStringLiteral("track_id"));
        if (track.isUndefined())
            track = annotation.value(QStringLiteral("instance_id"));
        if (track.isUndefined()) {
            setError(error, QStringLiteral("Annotations have no track_id: this is a detection "
                                           "dataset, not a tracking one"));
            return false;
        }
        const QString className =
            categories.value(qint64(annotation.value(QStringLiteral("category_id")).toDouble()),
                             QStringLiteral("Fish"));
        const double score = annotation.contains(QStringLiteral("score"))
                                 ? annotation.value(QStringLiteral("score")).toDouble(1.0)
                                 : 1.0;
        collector.add(*frame + shift, qint64(track.toDouble()), box.at(0).toDouble(),
                      box.at(1).toDouble(), box.at(2).toDouble(), box.at(3).toDouble(), className,
                      score);
    }
    result.format = QStringLiteral("COCO tracking JSON");
    return true;
}

}  // namespace

bool AnnotationImporter::read(const QString& path, const media::MediaSource& source,
                              AnnotationImport& result, QString* error)
{
    if (!source.isOpen()) {
        setError(error, QStringLiteral("Open the media the annotations belong to first"));
        return false;
    }
    AnnotationImport imported;
    Collector collector(source, imported);

    if (QFileInfo(path).suffix().compare(QLatin1String("json"), Qt::CaseInsensitive) == 0) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            setError(error, file.errorString());
            return false;
        }
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
        if (parse.error != QJsonParseError::NoError || !document.isObject()) {
            setError(error, QStringLiteral("Not valid JSON: %1").arg(parse.errorString()));
            return false;
        }
        if (!readCoco(document.object(), collector, imported, error))
            return false;
    } else if (!readMot(path, collector, imported, error)) {
        return false;
    }

    collector.finish();
    if (imported.observations.empty()) {
        setError(error, imported.outOfRange > 0
                            ? QStringLiteral("None of the %1 annotated rows fall within the %2 frames "
                                             "of %3. Were they made for different media?")
                                  .arg(imported.outOfRange)
                                  .arg(source.frameCount())
                                  .arg(source.name())
                            : QStringLiteral("The file has no usable boxes"));
        return false;
    }
    result = std::move(imported);
    return true;
}

QString AnnotationImporter::fileDialogFilter()
{
    return QStringLiteral("Tracking annotations (*.txt *.json);;MOTChallenge (*.txt);;"
                          "COCO tracking JSON (*.json);;All files (*)");
}

}  // namespace aquatrack::application
