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
    "media/\n"
    "  videos/       Video files copied into the project.\n"
    "  sequences/    One folder per sequence. A sequence of videos holds\n"
    "                sequence.json (its clips, in play order) and a README.txt\n"
    "                saying how to open it; its videos stay in media/videos/.\n"
    "                A BrackishMOT image sequence holds seqinfo.ini, img1/ and\n"
    "                gt/gt.txt.\n"
    "annotations/    Your reviewed edits (observations.json), imported ground\n"
    "                truth, and detector output.\n"
    "models/         Detector weights with their model.json manifests. The\n"
    "                Models page lists whatever is here.\n"
    "exports/        CSV and MOTChallenge exports.\n"
    "snapshots/      Frame snapshots saved from Review.\n"
    "\n"
    "Add, rename and reorder media from inside AquaTrack+ (Dashboard or Videos\n"
    "page) rather than in this folder: annotations are stored by media name.\n"
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
QString ProjectLayout::videos() const { return media() + QStringLiteral("/videos"); }
QString ProjectLayout::sequences() const { return media() + QStringLiteral("/sequences"); }

QString ProjectLayout::mediaFolderFor(const QString& source) const
{
    // A single video file goes with the other videos; anything that is a
    // folder (an image sequence, a sequence of videos) is a sequence.
    return QFileInfo(source).isDir() ? sequences() : videos();
}
QString ProjectLayout::annotations() const { return m_root + QStringLiteral("/annotations"); }
QString ProjectLayout::models() const { return m_root + QStringLiteral("/models"); }
QString ProjectLayout::exports() const { return m_root + QStringLiteral("/exports"); }
QString ProjectLayout::snapshots() const { return m_root + QStringLiteral("/snapshots"); }

bool ProjectLayout::create(QString* error) const
{
    for (const auto& folder :
         {videos(), sequences(), annotations(), models(), exports(), snapshots()}) {
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
