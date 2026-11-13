#include "data/ProjectFileStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <utility>

namespace {
void setError(QString* target, const QString& message)
{
    if (target)
        *target = message;
}
}

namespace aquatrack::data {

bool ProjectFileStore::save(const QString& path, const domain::ProjectManifest& manifest,
                            QString* error) const
{
    std::string reason;
    if (!domain::isValid(manifest, &reason)) {
        setError(error, QString::fromStdString(reason));
        return false;
    }
    // Media inside the project folder is written relative to it, so the
    // folder can be moved or zipped and still open; linked media elsewhere
    // stays absolute.
    const QDir projectFolder = QFileInfo(path).absoluteDir();
    const QString prefix = QDir::cleanPath(projectFolder.absolutePath()) + QLatin1Char('/');
    QJsonArray media;
    for (const auto& item : manifest.mediaPaths) {
        const QString value = QString::fromStdString(item);
        const QString absolute = QDir::cleanPath(projectFolder.absoluteFilePath(value));
        media.append(QFileInfo(value).isAbsolute() && absolute.startsWith(prefix, Qt::CaseInsensitive)
                         ? projectFolder.relativeFilePath(absolute)
                         : value);
    }
    const QJsonObject object{
        {QStringLiteral("version"), manifest.schemaVersion},
        {QStringLiteral("projectId"), QString::fromStdString(manifest.projectId)},
        {QStringLiteral("name"), QString::fromStdString(manifest.name)},
        {QStringLiteral("createdAtMs"), static_cast<qint64>(manifest.createdAtMs)},
        {QStringLiteral("updatedAtMs"), static_cast<qint64>(manifest.updatedAtMs)},
        {QStringLiteral("database"), QString::fromStdString(manifest.databasePath)},
        {QStringLiteral("media"), media},
    };
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, file.errorString());
        return false;
    }
    if (file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0 || !file.commit()) {
        setError(error, file.errorString());
        return false;
    }
    return true;
}

bool ProjectFileStore::load(const QString& path, domain::ProjectManifest& manifest,
                            QString* error) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Invalid project JSON: %1").arg(parseError.errorString()));
        return false;
    }
    const auto object = document.object();
    domain::ProjectManifest candidate;
    candidate.schemaVersion = object.value(QStringLiteral("version")).toInt(-1);
    candidate.projectId = object.value(QStringLiteral("projectId")).toString().toStdString();
    candidate.name = object.value(QStringLiteral("name")).toString().toStdString();
    candidate.createdAtMs = object.value(QStringLiteral("createdAtMs")).toInteger(-1);
    candidate.updatedAtMs = object.value(QStringLiteral("updatedAtMs")).toInteger(-1);
    candidate.databasePath = object.value(QStringLiteral("database")).toString().toStdString();
    // Relative entries are inside the project folder; resolve them so the
    // rest of the application always works with absolute paths.
    const QDir projectFolder = QFileInfo(path).absoluteDir();
    for (const auto item : object.value(QStringLiteral("media")).toArray()) {
        const QString value = item.toString();
        candidate.mediaPaths.push_back(
            (QFileInfo(value).isAbsolute() ? QDir::cleanPath(value)
                                           : QDir::cleanPath(projectFolder.absoluteFilePath(value)))
                .toStdString());
    }
    std::string reason;
    if (!domain::isValid(candidate, &reason)) {
        setError(error, QStringLiteral("Invalid project: %1").arg(QString::fromStdString(reason)));
        return false;
    }
    manifest = std::move(candidate);
    return true;
}

}  // namespace aquatrack::data
