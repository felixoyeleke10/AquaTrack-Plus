#include "data/media/MediaTools.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>

namespace aquatrack::media {
namespace {

QString executableName(const QString& name)
{
#ifdef Q_OS_WIN
    return name.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive) ? name
                                                                     : name + QStringLiteral(".exe");
#else
    return name;
#endif
}

QString existingExecutable(const QString& folder, const QString& name)
{
    if (folder.isEmpty())
        return {};
    const QFileInfo candidate(QDir(folder).filePath(executableName(name)));
    return candidate.isFile() ? QDir::cleanPath(candidate.absoluteFilePath()) : QString();
}

QString locate(const QString& name)
{
    if (const QString found = existingExecutable(qEnvironmentVariable("AQUATRACK_FFMPEG_DIR"), name);
        !found.isEmpty())
        return found;
    if (QCoreApplication::instance()) {
        if (const QString found = existingExecutable(QCoreApplication::applicationDirPath(), name);
            !found.isEmpty())
            return found;
    }
    if (const QString found = QStandardPaths::findExecutable(name); !found.isEmpty())
        return QDir::cleanPath(found);
#ifdef Q_OS_WIN
    // WinGet installs FFmpeg under the user's package folder and only adds it
    // to PATH for shells started afterwards.
    const QString packages =
        QDir(qEnvironmentVariable("LOCALAPPDATA")).filePath(QStringLiteral("Microsoft/WinGet/Packages"));
    if (QFileInfo(packages).isDir()) {
        const QStringList vendors =
            QDir(packages).entryList({QStringLiteral("*FFmpeg*")}, QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& vendor : vendors) {
            QDirIterator walk(QDir(packages).filePath(vendor), {executableName(name)}, QDir::Files,
                              QDirIterator::Subdirectories);
            if (walk.hasNext())
                return QDir::cleanPath(walk.next());
        }
    }
#endif
    return {};
}

}  // namespace

const QStringList& videoExtensions()
{
    static const QStringList extensions{
        QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("m4v"), QStringLiteral("mkv"),
        QStringLiteral("avi"), QStringLiteral("wmv"), QStringLiteral("webm"), QStringLiteral("mpg"),
        QStringLiteral("mpeg"), QStringLiteral("mts"), QStringLiteral("m2ts"), QStringLiteral("ts")};
    return extensions;
}

bool hasVideoExtension(const QString& path)
{
    return videoExtensions().contains(QFileInfo(path).suffix().toLower());
}

QString videoFileDialogFilter()
{
    QStringList patterns;
    for (const auto& extension : videoExtensions())
        patterns << QStringLiteral("*.") + extension;
    return QStringLiteral("Video files (%1);;All files (*)").arg(patterns.join(QLatin1Char(' ')));
}

MediaKind classifyMedia(const QString& path)
{
    const QFileInfo info(path);
    if (info.isDir()) {
        const QDir folder(path);
        if (QFileInfo::exists(folder.filePath(QStringLiteral("seqinfo.ini"))))
            return MediaKind::ImageSequence;
        if (QFileInfo::exists(folder.filePath(QString::fromLatin1(VideoSequenceManifestName))))
            return MediaKind::VideoSequence;
        return MediaKind::Folder;
    }
    if (info.isFile())
        return hasVideoExtension(path) ? MediaKind::VideoFile : MediaKind::Unsupported;
    return MediaKind::Missing;
}

bool isReviewable(MediaKind kind)
{
    return kind == MediaKind::ImageSequence || kind == MediaKind::VideoFile ||
           kind == MediaKind::VideoSequence;
}

QString kindLabel(MediaKind kind)
{
    switch (kind) {
    // A sequence is a sequence: whether its frames are stored as images or
    // as video clips is a detail shown next to it, not a different thing.
    case MediaKind::ImageSequence:
    case MediaKind::VideoSequence:
        return QStringLiteral("Sequence");
    case MediaKind::VideoFile:
        return QStringLiteral("Video");
    case MediaKind::Folder:
        return QStringLiteral("Not a sequence");
    case MediaKind::Unsupported:
        return QStringLiteral("Not a video");
    case MediaKind::Missing:
        return QStringLiteral("Missing");
    }
    return {};
}

QString ffmpegTool(const QString& name)
{
    // Lookups touch the disk; the answer does not change while the app runs
    // except when it is empty (the user may install FFmpeg meanwhile).
    static QMutex mutex;
    static QHash<QString, QString> cache;
    QMutexLocker lock(&mutex);
    if (const auto known = cache.constFind(name); known != cache.constEnd() && !known->isEmpty())
        return *known;
    const QString found = locate(name);
    cache.insert(name, found);
    return found;
}

QString ffmpegMissingMessage()
{
    return QStringLiteral(
        "FFmpeg was not found, so video files cannot be decoded. Install it (for example "
        "\"winget install Gyan.FFmpeg\") or place ffmpeg.exe and ffprobe.exe next to "
        "AquaTrack.exe, then try again.");
}

}  // namespace aquatrack::media
