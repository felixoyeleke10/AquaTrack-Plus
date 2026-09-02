#include "data/ModelRegistry.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <utility>

namespace aquatrack::data {

bool ModelRegistry::loadManifest(const QString& path, models::ModelManifest& manifest,
                                 QString* error) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()) {
        if (error)
            *error = QStringLiteral("Invalid model manifest: %1").arg(parse.errorString());
        return false;
    }

    const auto object = document.object();
    models::ModelManifest candidate;
    candidate.id = object[QStringLiteral("id")].toString().toStdString();
    candidate.displayName = object[QStringLiteral("name")].toString().toStdString();
    candidate.type = models::modelTypeFromString(
        object[QStringLiteral("type")].toString().toStdString());
    candidate.backend = object[QStringLiteral("backend")].toString().toStdString();
    candidate.precision = object[QStringLiteral("precision")].toString().toStdString();

    const auto input = object[QStringLiteral("input")].toArray();
    if (input.size() == 2) {
        candidate.inputWidth = input[0].toInt();
        candidate.inputHeight = input[1].toInt();
    }
    for (const auto value : object[QStringLiteral("classes")].toArray())
        candidate.classes.push_back(value.toString().toStdString());
    const auto metrics = object[QStringLiteral("metrics")].toObject();
    for (auto it = metrics.begin(); it != metrics.end(); ++it)
        candidate.metrics[it.key().toStdString()] = it.value().toDouble();

    const QString artifact = object[QStringLiteral("artifact")].toString();
    if (artifact.isEmpty()) {
        if (error)
            *error = QStringLiteral("model artifact path is required");
        return false;
    }
    // Artifact paths are relative to the manifest, so a registry stays portable.
    candidate.artifactPath =
        QFileInfo(QFileInfo(path).dir(), artifact).absoluteFilePath().toStdString();

    std::string reason;
    if (!models::isValid(candidate, &reason)) {
        if (error)
            *error = QString::fromStdString(reason);
        return false;
    }
    manifest = std::move(candidate);
    return true;
}

ModelRegistryResult ModelRegistry::scan(const QString& root) const
{
    ModelRegistryResult result;
    if (root.isEmpty() || !QFileInfo(root).isDir()) {
        result.warnings.push_back(QStringLiteral("%1 is not a directory").arg(root));
        return result;
    }

    QDirIterator iterator(root, {QStringLiteral("model.json")}, QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const auto path = iterator.next();
        models::ModelManifest manifest;
        QString error;
        if (!loadManifest(path, manifest, &error)) {
            result.warnings.push_back(QStringLiteral("%1: %2").arg(path, error));
            continue;
        }

        ModelEntry entry;
        entry.manifestPath = QFileInfo(path).absoluteFilePath();
        entry.artifactPath = QString::fromStdString(manifest.artifactPath);
        const QFileInfo artifact(entry.artifactPath);
        entry.artifactExists = artifact.isFile();
        entry.artifactBytes = entry.artifactExists ? artifact.size() : 0;
        entry.manifest = std::move(manifest);

        // A missing or empty weights file is a warning, not a rejection: the
        // manifest is still worth listing so the operator can see what is
        // meant to be there and fix the gap.
        if (!entry.artifactExists)
            result.warnings.push_back(QStringLiteral("%1: artifact not found at %2")
                                          .arg(entry.manifestPath, entry.artifactPath));
        else if (entry.artifactBytes == 0)
            result.warnings.push_back(
                QStringLiteral("%1: artifact is empty").arg(entry.artifactPath));

        result.entries.push_back(std::move(entry));
    }

    // Stable, human-meaningful order: registries are browsed, not indexed.
    std::sort(result.entries.begin(), result.entries.end(),
              [](const ModelEntry& left, const ModelEntry& right) {
                  if (left.manifest.type != right.manifest.type)
                      return left.manifest.type < right.manifest.type;
                  return left.manifest.displayName < right.manifest.displayName;
              });
    return result;
}

}  // namespace aquatrack::data
