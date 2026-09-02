#pragma once
#include "domain/ScientificTypes.h"
#include <span>
#include <string>
namespace aquatrack::exporting {
[[nodiscard]] std::string
observationsToCsv(std::span<const domain::Observation> observations);
[[nodiscard]] std::string
observationsToMotChallenge(std::span<const domain::Observation> observations);
} // namespace aquatrack::exporting
