#pragma once
#include <map>
#include <string>
#include <vector>
namespace aquatrack::models {
enum class ModelType {
  Detector,
  Tracker,
  ReIdentification,
  Species,
  Behavior,
  Unknown
};
struct ModelManifest {
  std::string id;
  std::string displayName;
  ModelType type = ModelType::Unknown;
  std::string backend;
  std::string precision;
  int inputWidth = 0;
  int inputHeight = 0;
  std::vector<std::string> classes;
  std::map<std::string, double> metrics;
  std::string artifactPath;
};
[[nodiscard]] bool isValid(const ModelManifest &,
                           std::string *reason = nullptr);
[[nodiscard]] ModelType modelTypeFromString(const std::string &);
[[nodiscard]] std::string modelTypeToString(ModelType);
} // namespace aquatrack::models
