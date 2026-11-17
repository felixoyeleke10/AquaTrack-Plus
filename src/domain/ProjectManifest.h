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
  // The detector this project runs, overriding the app's active detector;
  // empty to use the app's.
  std::string detectorModelId;
};

[[nodiscard]] bool isValid(const ProjectManifest &manifest,
                           std::string *reason = nullptr);

} // namespace aquatrack::domain
