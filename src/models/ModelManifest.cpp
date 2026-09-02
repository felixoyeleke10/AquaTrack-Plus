#include "models/ModelManifest.h"
#include <algorithm>
#include <cctype>
namespace aquatrack::models {
ModelType modelTypeFromString(const std::string &value) {
  std::string n = value;
  std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (n == "detector")
    return ModelType::Detector;
  if (n == "tracker")
    return ModelType::Tracker;
  if (n == "reid" || n == "re-identification")
    return ModelType::ReIdentification;
  if (n == "species")
    return ModelType::Species;
  if (n == "behavior")
    return ModelType::Behavior;
  return ModelType::Unknown;
}
std::string modelTypeToString(ModelType type) {
  switch (type) {
  case ModelType::Detector:
    return "detector";
  case ModelType::Tracker:
    return "tracker";
  case ModelType::ReIdentification:
    return "reid";
  case ModelType::Species:
    return "species";
  case ModelType::Behavior:
    return "behavior";
  default:
    return "unknown";
  }
}
bool isValid(const ModelManifest &m, std::string *reason) {
  auto fail = [reason](const char *x) {
    if (reason)
      *reason = x;
    return false;
  };
  if (m.id.empty())
    return fail("model id is required");
  if (m.displayName.empty())
    return fail("model display name is required");
  if (m.type == ModelType::Unknown)
    return fail("known model type is required");
  if (m.backend.empty())
    return fail("model backend is required");
  if (m.artifactPath.empty())
    return fail("model artifact path is required");
  if (m.inputWidth <= 0 || m.inputHeight <= 0)
    return fail("positive input dimensions are required");
  return true;
}
} // namespace aquatrack::models
