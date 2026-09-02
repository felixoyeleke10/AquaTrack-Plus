#pragma once

#include "domain/ScientificTypes.h"

#include <string>
#include <vector>

namespace aquatrack::domain {

inline constexpr int CurrentProjectSchemaVersion = 1;

struct ProjectManifest {
  int schemaVersion = CurrentProjectSchemaVersion;
  std::string projectId;
  std::string name;
  TimestampMs createdAtMs = 0;
  TimestampMs updatedAtMs = 0;
  std::string databasePath = "observations.json";
  std::vector<std::string> mediaPaths;
};

[[nodiscard]] bool isValid(const ProjectManifest &manifest,
                           std::string *reason = nullptr);

} // namespace aquatrack::domain
