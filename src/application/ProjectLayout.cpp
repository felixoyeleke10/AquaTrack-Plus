#include "application/ProjectLayout.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace aquatrack::application {
namespace {

const char* const kReadme =
    "AquaTrack+ project folder\n"
    "=========================\n"
    "\n"
    "Open the .aqp file in AquaTrack+ to work on this project.\n"
    "\n"
    "media/        Sequences and video files copied into the project. A BrackishMOT\n"
    "              sequence is a folder holding seqinfo.ini, img1/ and gt/gt.txt.\n"
    "annotations/  Your reviewed edits (observations.json) and MOTChallenge gt.txt\n"
    "              exports.\n"
    "models/       Detector weights with their model.json manifests. The Models page\n"
    "              lists whatever is here.\n"
    "exports/      CSV and other exports.\n"
    "snapshots/    Frame snapshots saved from Review.\n"
    "\n"
    "Paths inside this folder are stored relative to it, so the whole folder can be\n"
    "moved, copied or zipped and still open. Media linked from elsewhere is stored\n"
    "as an absolute path and must stay where it is.\n";

}  // namespace

QString ProjectLayout::folderNameFor(const QString& projectName)
{
    static const QString kInvalid = QStringLiteral("<>:\"/\\|?*");
    QString folder = projectName.trimmed();
    for (QChar& character : folder)
        if (kInvalid.contains(character) || character.unicode() < 32)
            character = QLatin1Char('_');
    while (folder.endsWith(QLatin1Char('.')) || folder.endsWith(QLatin1Char(' ')))
        folder.chop(1);
    return folder.isEmpty() ? QStringLiteral("Project") : folder;
}

ProjectLayout::ProjectLayout(const QString& projectFile)
    : m_root(QDir::cleanPath(QFileInfo(projectFile).absolutePath()))
{
}

QString ProjectLayout::media() const { return m_root + QStringLiteral("/media"); }
QString ProjectLayout::annotations() const { return m_root + QStringLiteral("/annotations"); }
QString ProjectLayout::models() const { return m_root + QStringLiteral("/models"); }
QString ProjectLayout::exports() const { return m_root + QStringLiteral("/exports"); }
QString ProjectLayout::snapshots() const { return m_root + QStringLiteral("/snapshots"); }

bool ProjectLayout::create(QString* error) const
{
    for (const auto& folder : {media(), annotations(), models(), exports(), snapshots()}) {
        if (!QDir().mkpath(folder)) {
            if (error)
                *error = QStringLiteral("Could not create %1").arg(QDir::toNativeSeparators(folder));
            return false;
        }
    }
    const QString readme = m_root + QStringLiteral("/README.txt");
    if (!QFileInfo::exists(readme)) {
        QSaveFile file(readme);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(kReadme);
            file.commit();  // a missing README is not worth failing a project over
        }
    }
    return true;
}

QString ProjectLayout::ensure(const QString& folder) const
{
    return QDir().mkpath(folder) ? folder : QString();
}

bool ProjectLayout::contains(const QString& path) const
{
    const QString candidate = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QString prefix = m_root + QLatin1Char('/');
    return candidate.startsWith(prefix, Qt::CaseInsensitive);
}

}  // namespace aquatrack::application
