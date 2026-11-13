#pragma once

#include <QString>
#include <QStringList>

#include <functional>

namespace aquatrack::application {

// Copies media (a video file or a whole sequence folder) into a project's
// media folder, reporting progress in bytes so a dialog can show it and
// cancel it. A copy that fails or is cancelled is removed, never left half
// written for the project to trip over later.
class MediaImporter final {
public:
    enum class Result { Copied, Cancelled, Failed };

    // Called after each file; return false to cancel.
    using Progress = std::function<bool(qint64 bytesDone, qint64 bytesTotal)>;

    // Bytes a copy of `path` would write (a file's size, or a folder's total).
    [[nodiscard]] static qint64 sizeOf(const QString& path);
    [[nodiscard]] static qint64 sizeOf(const QStringList& paths);

    // `total` is the size of the whole batch, so progress spans several calls.
    MediaImporter(qint64 total, Progress progress);

    // Copies `source` into `destinationFolder` under its own name, or with a
    // -2, -3... suffix if that name is taken. On success `copiedTo` holds the
    // new path.
    Result copy(const QString& source, const QString& destinationFolder, QString* copiedTo,
                QString* error = nullptr);

private:
    bool report();
    Result copyFile(const QString& source, const QString& target, QString* error);

    qint64 m_total = 0;
    qint64 m_done = 0;
    Progress m_progress;
};

}  // namespace aquatrack::application
