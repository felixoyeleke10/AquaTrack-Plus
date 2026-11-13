#include "TestSupport.h"
#include "application/MediaImporter.h"
#include "application/ProjectLayout.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

namespace {
void writeFile(const QString& path, const QByteArray& content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    requireTest(file.open(QIODevice::WriteOnly), "create fixture");
    file.write(content);
}
}  // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    requireTest(temporary.isValid(), "temporary directory");
    const QDir root(temporary.path());
    using aquatrack::application::MediaImporter;

    // A sequence folder: nested files, sizes known.
    const QString sequence = root.filePath(QStringLiteral("source/seq-01"));
    writeFile(sequence + QStringLiteral("/seqinfo.ini"), QByteArray(10, 'i'));
    writeFile(sequence + QStringLiteral("/gt/gt.txt"), QByteArray(20, 'g'));
    writeFile(sequence + QStringLiteral("/img1/000001.png"), QByteArray(70, 'p'));
    requireTest(MediaImporter::sizeOf(sequence) == 100, "folder size sums its files");

    const QString media = root.filePath(QStringLiteral("project/media"));
    qint64 lastDone = -1;
    MediaImporter importer(100, [&lastDone](qint64 done, qint64 total) {
        requireTest(total == 100 && done > lastDone, "progress advances towards the total");
        lastDone = done;
        return true;
    });
    QString copied;
    requireTest(importer.copy(sequence, media, &copied) == MediaImporter::Result::Copied,
                "sequence folder copied");
    requireTest(copied == QDir::cleanPath(media + QStringLiteral("/seq-01")), "copy keeps its name");
    requireTest(QFileInfo(copied + QStringLiteral("/gt/gt.txt")).size() == 20,
                "nested files copied intact");
    requireTest(lastDone == 100, "progress reaches the total");

    // The same name again gets a suffix instead of overwriting.
    MediaImporter again(100, {});
    QString second;
    requireTest(again.copy(sequence, media, &second) == MediaImporter::Result::Copied &&
                    second.endsWith(QStringLiteral("/seq-01-2")),
                "name clash gets a numbered suffix");

    // Cancelling leaves nothing half written behind.
    MediaImporter cancelled(100, [](qint64, qint64) { return false; });
    QString none;
    requireTest(cancelled.copy(sequence, media, &none) == MediaImporter::Result::Cancelled,
                "copy can be cancelled");
    requireTest(!QFileInfo::exists(media + QStringLiteral("/seq-01-3")),
                "a cancelled copy is removed");

    // Copying the same file again reuses the copy (annotations are stored
    // under the file's name, so a "-2" duplicate would orphan them).
    const QString video = root.filePath(QStringLiteral("source/dive.mp4"));
    writeFile(video, QByteArray(5, 'v'));
    MediaImporter files(20, {});
    QString firstVideo;
    QString secondVideo;
    requireTest(files.copy(video, media, &firstVideo) == MediaImporter::Result::Copied &&
                    files.copy(video, media, &secondVideo) == MediaImporter::Result::Copied,
                "video files copied");
    requireTest(firstVideo.endsWith(QStringLiteral("/dive.mp4")) && secondVideo == firstVideo,
                "the same file is not copied twice");

    // A different file with the same name is renamed and keeps its extension.
    QDir(root.filePath(QStringLiteral("source"))).mkpath(QStringLiteral("other"));
    const QString otherVideo = root.filePath(QStringLiteral("source/other/dive.mp4"));
    writeFile(otherVideo, QByteArray(9, 'w'));
    QString thirdVideo;
    requireTest(files.copy(otherVideo, media, &thirdVideo) == MediaImporter::Result::Copied &&
                    thirdVideo.endsWith(QStringLiteral("/dive-2.mp4")),
                "a different file with the same name keeps its extension when renamed");

    // Large files are copied in chunks and can be cancelled part way.
    const QString large = root.filePath(QStringLiteral("source/large.mp4"));
    writeFile(large, QByteArray(9 * 1024 * 1024, 'x'));
    int reports = 0;
    MediaImporter chunked(9 * 1024 * 1024, [&reports](qint64, qint64) { return ++reports < 2; });
    QString partial;
    requireTest(chunked.copy(large, media, &partial) == MediaImporter::Result::Cancelled &&
                    reports == 2 && !QFileInfo::exists(media + QStringLiteral("/large.mp4")),
                "a large copy reports per chunk and is removed when cancelled mid-file");

    // Project layout: folders and containment.
    const aquatrack::application::ProjectLayout layout(
        root.filePath(QStringLiteral("project/Survey.aqp")));
    requireTest(layout.create(), "layout created");
    requireTest(layout.contains(copied) && !layout.contains(sequence),
                "layout knows what is inside the project");
    return 0;
}
