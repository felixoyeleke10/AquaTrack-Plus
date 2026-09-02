#pragma once

#include "domain/ProjectManifest.h"

#include <QString>

namespace aquatrack::data {

class ProjectFileStore final {
public:
    [[nodiscard]] bool save(const QString& path, const domain::ProjectManifest& manifest,
                            QString* error = nullptr) const;
    [[nodiscard]] bool load(const QString& path, domain::ProjectManifest& manifest,
                            QString* error = nullptr) const;
};

}  // namespace aquatrack::data
