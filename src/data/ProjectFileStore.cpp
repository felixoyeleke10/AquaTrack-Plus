#include "data/ProjectFileStore.h"

#include <QFile>
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
    QJsonArray media;
    for (const auto& item : manifest.mediaPaths)
        media.append(QString::fromStdString(item));
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
    for (const auto item : object.value(QStringLiteral("media")).toArray())
        candidate.mediaPaths.push_back(item.toString().toStdString());
    std::string reason;
    if (!domain::isValid(candidate, &reason)) {
        setError(error, QStringLiteral("Invalid project: %1").arg(QString::fromStdString(reason)));
        return false;
    }
    manifest = std::move(candidate);
    return true;
}

}  // namespace aquatrack::data
