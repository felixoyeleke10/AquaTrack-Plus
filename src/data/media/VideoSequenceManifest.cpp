#include "data/media/VideoSequenceManifest.h"

#include "data/media/MediaTools.h"

#include <QDir>
#include <QTextStream>
#include <QLocale>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <cstdlib>

namespace aquatrack::media {
namespace {

constexpr auto kFormat = "aquatrack-video-sequence";

void setError(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

// Relative when the clip shares a drive with the sequence folder and is not
// far outside it, so a project folder can be moved as a whole.
QString storedPath(const QDir& folder, const QString& absolute)
{
    const QString relative = folder.relativeFilePath(absolute);
    if (QDir::isAbsolutePath(relative))
        return QDir::fromNativeSeparators(absolute);
    if (relative.startsWith(QLatin1String("../../../")))
        return QDir::fromNativeSeparators(absolute);
    return relative;
}

// First line of the README this class writes. A README.txt that does not
// start with it was written by someone else and is left alone.
constexpr char kGuideMarker[] = "AquaTrack+ sequence: ";

// Human-readable companion to sequence.json: what the folder is, how to
// open it, and the clips in play order.
QString guideText(const VideoSequenceManifest& manifest, const QDir& base)
{
    const QLocale locale = QLocale::c();
    QString text;
    QTextStream out(&text);
    const QString title = QString::fromLatin1(kGuideMarker) + manifest.name;
    out << title << "\n" << QString(title.size(), QLatin1Char('=')) << "\n\n";
    out << "This folder is a sequence: " << manifest.clips.size()
        << (manifest.clips.size() == 1 ? " video" : " videos")
        << " that AquaTrack+ plays back to back as one timeline ("
        << locale.toString(manifest.totalFrames()) << " frames, "
        << formatDuration(manifest.totalDurationMs()) << ").\n\n";
    out << "How to open it\n"
        << "  - Open the project (.aqp) in AquaTrack+, then choose \"" << manifest.name
        << "\" on the Dashboard or the Videos page.\n"
        << "  - Or, without the project: Dashboard > Open media..., and pick the\n"
        << "    sequence.json in this folder.\n"
        << "  A media player opens the videos one at a time only; this folder is\n"
        << "  what joins them.\n\n";
    out << "Play order\n";
    int index = 1;
    for (const auto& clip : manifest.clips) {
        out << "  " << index++ << ". " << QFileInfo(clip.path).fileName() << "   "
            << formatDuration(clip.durationMs) << "   " << locale.toString(clip.frameCount)
            << " frames   " << QString::number(clip.frameRate, 'f', 2) << " fps\n"
            << "     " << QDir::toNativeSeparators(storedPath(base, clip.path)) << "\n";
    }
    out << "\nThe videos are referenced, not copied: keep them where the paths above\n"
        << "point (paths are relative to this folder). To change the order, add or\n"
        << "remove clips, use Edit sequence... in AquaTrack+ rather than editing\n"
        << "sequence.json by hand, so annotations move with their clips.\n";
    return text;
}

void writeGuide(const VideoSequenceManifest& manifest, const QString& folder)
{
    const QString path = QDir(folder).filePath(QStringLiteral("README.txt"));
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly) &&
        !existing.readLine().startsWith(QByteArray(kGuideMarker)))
        return;
    existing.close();
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(guideText(manifest, QDir(folder)).toUtf8());
        (void)file.commit();  // the guide is a courtesy; never fail a save over it
    }
}

}  // namespace

QString VideoSequenceManifest::manifestPath(const QString& folder)
{
    return QDir(folder).filePath(QString::fromLatin1(VideoSequenceManifestName));
}

bool VideoSequenceManifest::load(const QString& folder, QString* error)
{
    QFile file(manifestPath(folder));
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Could not read %1: %2")
                            .arg(QDir::toNativeSeparators(file.fileName()), file.errorString()));
        return false;
    }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("%1 is not valid JSON: %2")
                            .arg(QDir::toNativeSeparators(file.fileName()), parse.errorString()));
        return false;
    }
    const auto root = document.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(kFormat)) {
        setError(error, QStringLiteral("%1 is not an AquaTrack sequence")
                            .arg(QDir::toNativeSeparators(file.fileName())));
        return false;
    }

    const QDir base(folder);
    VideoSequenceManifest loaded;
    loaded.name = root.value(QStringLiteral("name")).toString().trimmed();
    if (loaded.name.isEmpty())
        loaded.name = base.dirName();
    for (const auto value : root.value(QStringLiteral("clips")).toArray()) {
        const auto object = value.toObject();
        const QString stored = object.value(QStringLiteral("path")).toString();
        if (stored.isEmpty())
            continue;
        VideoInfo clip;
        clip.path = QDir::cleanPath(QDir::isAbsolutePath(stored) ? stored : base.filePath(stored));
        clip.frameCount = object.value(QStringLiteral("frames")).toInt();
        clip.frameRate = object.value(QStringLiteral("frameRate")).toDouble();
        clip.width = object.value(QStringLiteral("width")).toInt();
        clip.height = object.value(QStringLiteral("height")).toInt();
        clip.durationMs = qint64(object.value(QStringLiteral("durationMs")).toDouble());
        clip.bytes = qint64(object.value(QStringLiteral("bytes")).toDouble());
        clip.modified = QDateTime::fromString(object.value(QStringLiteral("modified")).toString(),
                                              Qt::ISODate);
        clip.codec = object.value(QStringLiteral("codec")).toString();
        loaded.clips.push_back(std::move(clip));
    }
    if (loaded.clips.isEmpty()) {
        setError(error, QStringLiteral("The sequence “%1” lists no videos")
                            .arg(loaded.name));
        return false;
    }
    *this = std::move(loaded);
    return true;
}

bool VideoSequenceManifest::save(const QString& folder, QString* error) const
{
    if (!QDir().mkpath(folder)) {
        setError(error, QStringLiteral("Could not create %1").arg(QDir::toNativeSeparators(folder)));
        return false;
    }
    const QDir base(folder);
    QJsonArray clipArray;
    for (const auto& clip : clips) {
        clipArray.append(QJsonObject{
            {QStringLiteral("path"), storedPath(base, clip.path)},
            {QStringLiteral("frames"), clip.frameCount},
            {QStringLiteral("frameRate"), clip.frameRate},
            {QStringLiteral("width"), clip.width},
            {QStringLiteral("height"), clip.height},
            {QStringLiteral("durationMs"), double(clip.durationMs)},
            {QStringLiteral("bytes"), double(clip.bytes)},
            {QStringLiteral("modified"), clip.modified.toUTC().toString(Qt::ISODate)},
            {QStringLiteral("codec"), clip.codec},
        });
    }
    const QJsonObject root{{QStringLiteral("format"), QString::fromLatin1(kFormat)},
                           {QStringLiteral("version"), 1},
                           {QStringLiteral("name"), name},
                           {QStringLiteral("clips"), clipArray}};
    QSaveFile file(manifestPath(folder));
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        setError(error, QStringLiteral("Could not write %1: %2")
                            .arg(QDir::toNativeSeparators(file.fileName()), file.errorString()));
        return false;
    }
    writeGuide(*this, folder);
    return true;
}

bool VideoSequenceManifest::refresh(QString* error, bool* changed)
{
    if (changed)
        *changed = false;
    for (auto& clip : clips) {
        const QFileInfo file(clip.path);
        if (!file.isFile()) {
            setError(error, QStringLiteral("A video in this sequence is missing: %1")
                                .arg(QDir::toNativeSeparators(clip.path)));
            return false;
        }
        const bool current = clip.isValid() && clip.bytes == file.size() &&
                             clip.modified.isValid() &&
                             std::abs(clip.modified.secsTo(file.lastModified())) <= 2;
        if (current)
            continue;
        VideoInfo probed;
        QString reason;
        if (!probeVideo(clip.path, probed, &reason)) {
            setError(error, QStringLiteral("%1: %2").arg(file.fileName(), reason));
            return false;
        }
        clip = std::move(probed);
        if (changed)
            *changed = true;
    }
    return true;
}

int VideoSequenceManifest::totalFrames() const
{
    int total = 0;
    for (const auto& clip : clips)
        total += clip.frameCount;
    return total;
}

qint64 VideoSequenceManifest::totalDurationMs() const
{
    qint64 total = 0;
    for (const auto& clip : clips)
        total += clip.durationMs;
    return total;
}

}  // namespace aquatrack::media
