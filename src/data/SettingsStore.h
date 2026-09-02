#pragma once
#include "system/settings/AquaTrackSettings.h"
#include <QString>
namespace aquatrack::data {
class SettingsStore final {
public:
  [[nodiscard]] bool save(const QString &,
                          const system::settings::AquaTrackSettings &,
                          QString *error = nullptr) const;
  [[nodiscard]] bool load(const QString &,
                          system::settings::AquaTrackSettings &,
                          QString *error = nullptr) const;
};
} // namespace aquatrack::data
