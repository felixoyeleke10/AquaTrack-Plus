#pragma once
#include "data/SettingsStore.h"
#include <QObject>
namespace aquatrack::application {
class SettingsController final : public QObject {
Q_OBJECT public
    : explicit SettingsController(QString path, QObject *parent = nullptr);
  [[nodiscard]] const system::settings::AquaTrackSettings &settings() const {
    return m_settings;
  }
  [[nodiscard]] QString path() const { return m_path; }
  bool load(QString *error = nullptr);
  bool save(const system::settings::AquaTrackSettings &,
            QString *error = nullptr);
  // Moves `path` to the front of the recent list, de-duplicating and
  // trimming, then persists. Failing to record a recent entry must never
  // block the project operation that triggered it, so this returns void.
  void rememberRecentProject(const QString &path);
  // Drops one entry from the recent list. Returns false if it was not there.
  bool forgetRecentProject(const QString &path);
  // Drops every entry whose file no longer exists; returns how many went.
  // Never automatic: a project on an unplugged drive is still a project.
  int forgetMissingRecentProjects();
signals:
  void settingsChanged();

private:
  data::SettingsStore m_store;
  system::settings::AquaTrackSettings m_settings;
  QString m_path;
};
} // namespace aquatrack::application
