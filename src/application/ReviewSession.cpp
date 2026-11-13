#include "application/ReviewSession.h"

#include "application/ProjectLayout.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace aquatrack::application {
namespace {

QString sessionFile(const QString& projectFile)
{
    return ProjectLayout(projectFile).root() + QStringLiteral("/.aquatrack/session.json");
}

}  // namespace

std::optional<ReviewSession> loadReviewSession(const QString& projectFile)
{
    if (projectFile.isEmpty())
        return std::nullopt;
    QFile file(sessionFile(projectFile));
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    const QString media = object.value(QStringLiteral("media")).toString();
    const int frame = object.value(QStringLiteral("frame")).toInt();
    if (media.isEmpty() || frame < 1)
        return std::nullopt;
    // Media inside the project is stored relative, so the folder can move.
    const QDir root(ProjectLayout(projectFile).root());
    ReviewSession session;
    session.mediaPath = QDir::cleanPath(QFileInfo(media).isAbsolute() ? media
                                                                      : root.absoluteFilePath(media));
    session.frame = frame;
    session.savedAtMs = object.value(QStringLiteral("savedAtMs")).toInteger();
    return session;
}

bool saveReviewSession(const QString& projectFile, const ReviewSession& session)
{
    if (projectFile.isEmpty() || session.mediaPath.isEmpty() || session.frame < 1)
        return false;
    const ProjectLayout layout(projectFile);
    const QString file = sessionFile(projectFile);
    if (!QDir().mkpath(QFileInfo(file).absolutePath()))
        return false;
    const QString media = layout.contains(session.mediaPath)
                              ? QDir(layout.root()).relativeFilePath(session.mediaPath)
                              : QDir::cleanPath(session.mediaPath);
    const QJsonObject object{
        {QStringLiteral("media"), media},
        {QStringLiteral("frame"), session.frame},
        {QStringLiteral("savedAtMs"), session.savedAtMs},
    };
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly))
        return false;
    out.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    return out.commit();
}

}  // namespace aquatrack::application
