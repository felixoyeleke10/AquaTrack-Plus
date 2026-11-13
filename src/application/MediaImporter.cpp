#include "application/MediaImporter.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <utility>

namespace aquatrack::application {
namespace {

constexpr qint64 kChunk = 4 * 1024 * 1024;

QString uniqueDestination(const QString& folder, const QFileInfo& source)
{
    const QDir target(folder);
    const QString base = source.isDir() ? source.fileName() : source.completeBaseName();
    const QString suffix = source.isDir() || source.suffix().isEmpty()
                               ? QString()
                               : QLatin1Char('.') + source.suffix();
    QString candidate = target.filePath(base + suffix);
    for (int index = 2; QFileInfo::exists(candidate); ++index)
        candidate = target.filePath(QStringLiteral("%1-%2%3").arg(base).arg(index).arg(suffix));
    return candidate;
}

}  // namespace

qint64 MediaImporter::sizeOf(const QString& path)
{
    const QFileInfo info(path);
    if (info.isFile())
        return info.size();
    if (!info.isDir())
        return 0;
    qint64 total = 0;
    QDirIterator walk(path, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                      QDirIterator::Subdirectories);
    while (walk.hasNext()) {
        walk.next();
        total += walk.fileInfo().size();
    }
    return total;
}

qint64 MediaImporter::sizeOf(const QStringList& paths)
{
    qint64 total = 0;
    for (const auto& path : paths)
        total += sizeOf(path);
    return total;
}

MediaImporter::MediaImporter(qint64 total, Progress progress)
    : m_total(total), m_progress(std::move(progress))
{
}

bool MediaImporter::report()
{
    return !m_progress || m_progress(m_done, m_total);
}

MediaImporter::Result MediaImporter::copyFile(const QString& source, const QString& target,
                                              QString* error)
{
    QFile input(source);
    if (!input.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("Could not read %1: %2")
                         .arg(QFileInfo(source).fileName(), input.errorString());
        return Result::Failed;
    }
    QFile output(target);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("Could not write %1: %2")
                         .arg(QDir::toNativeSeparators(target), output.errorString());
        return Result::Failed;
    }
    // Chunked so a multi-gigabyte video reports progress and can be cancelled
    // part way, instead of freezing the dialog until it is done.
    QByteArray buffer;
    while (!input.atEnd()) {
        buffer = input.read(kChunk);
        if (buffer.isEmpty() && input.error() != QFileDevice::NoError) {
            if (error)
                *error = QStringLiteral("Could not read %1: %2")
                             .arg(QFileInfo(source).fileName(), input.errorString());
            return Result::Failed;
        }
        if (output.write(buffer) != buffer.size()) {
            if (error)
                *error = QStringLiteral("Could not write %1: %2")
                             .arg(QDir::toNativeSeparators(target), output.errorString());
            return Result::Failed;
        }
        m_done += buffer.size();
        if (!report())
            return Result::Cancelled;
    }
    output.close();
    // Keep the original date: video sequences use size and date to tell
    // whether a clip changed.
    if (output.open(QIODevice::ReadWrite)) {
        output.setFileTime(QFileInfo(source).lastModified(), QFileDevice::FileModificationTime);
        output.close();
    }
    return Result::Copied;
}

MediaImporter::Result MediaImporter::copy(const QString& source, const QString& destinationFolder,
                                          QString* copiedTo, QString* error)
{
    const QFileInfo info(source);
    const auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return Result::Failed;
    };
    if (!info.exists())
        return fail(QStringLiteral("%1 does not exist").arg(QDir::toNativeSeparators(source)));
    if (!QDir().mkpath(destinationFolder))
        return fail(QStringLiteral("Could not create %1")
                        .arg(QDir::toNativeSeparators(destinationFolder)));

    // The same file copied before: reuse it rather than making name-2, which
    // would also orphan annotations stored under the original name.
    if (info.isFile()) {
        const QFileInfo existing(QDir(destinationFolder).filePath(info.fileName()));
        if (existing.isFile() && existing.size() == info.size()) {
            m_done += info.size();
            if (!report())
                return Result::Cancelled;
            if (copiedTo)
                *copiedTo = QDir::cleanPath(existing.absoluteFilePath());
            return Result::Copied;
        }
    }

    const QString destination = uniqueDestination(destinationFolder, info);
    // Undo a partial copy: nothing half written stays in the project.
    const auto discard = [&destination, &info] {
        if (info.isDir())
            QDir(destination).removeRecursively();
        else
            QFile::remove(destination);
    };

    if (info.isFile()) {
        const Result result = copyFile(source, destination, error);
        if (result != Result::Copied) {
            discard();
            return result;
        }
    } else {
        const QDir from(source);
        QDirIterator walk(source, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
        if (!QDir().mkpath(destination))
            return fail(QStringLiteral("Could not create %1")
                            .arg(QDir::toNativeSeparators(destination)));
        while (walk.hasNext()) {
            const QString file = walk.next();
            const QString target = QDir(destination).filePath(from.relativeFilePath(file));
            if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
                discard();
                return fail(QStringLiteral("Could not create %1")
                                .arg(QDir::toNativeSeparators(QFileInfo(target).absolutePath())));
            }
            const Result result = copyFile(file, target, error);
            if (result != Result::Copied) {
                discard();
                return result;
            }
        }
    }
    if (copiedTo)
        *copiedTo = QDir::cleanPath(destination);
    return Result::Copied;
}

}  // namespace aquatrack::application
