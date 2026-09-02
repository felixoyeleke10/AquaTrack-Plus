#pragma once

#include "models/ModelManifest.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace aquatrack::data {

// A manifest plus what the filesystem says about it. The manifest itself is a
// domain type and stays free of paths and file state; this is the registry's
// view of one entry on disk.
struct ModelEntry {
    models::ModelManifest manifest;
    QString manifestPath;
    QString artifactPath;
    bool artifactExists = false;
    qint64 artifactBytes = 0;

    // A manifest can be perfectly valid and still point at a weights file that
    // was never copied across, which is the failure worth surfacing.
    [[nodiscard]] bool isUsable() const { return artifactExists && artifactBytes > 0; }
};

struct ModelRegistryResult {
    QVector<ModelEntry> entries;
    QStringList warnings;
};

class ModelRegistry final {
public:
    [[nodiscard]] ModelRegistryResult scan(const QString& root) const;
    [[nodiscard]] bool loadManifest(const QString& path, models::ModelManifest& manifest,
                                    QString* error = nullptr) const;
};

}  // namespace aquatrack::data
