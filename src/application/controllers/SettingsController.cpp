#include "application/controllers/SettingsController.h"
#include <QDir>
#include <algorithm>
#include <QFileInfo>
namespace aquatrack::application {
SettingsController::SettingsController(QString path, QObject *parent)
    : QObject(parent), m_path(std::move(path)) {}
bool SettingsController::load(QString *error) {
  if (!QFileInfo::exists(m_path)) {
    emit settingsChanged();
    return true;
  }
  system::settings::AquaTrackSettings candidate;
  if (!m_store.load(m_path, candidate, error))
    return false;
  m_settings = std::move(candidate);
  emit settingsChanged();
  return true;
}
bool SettingsController::save(const system::settings::AquaTrackSettings &value,
                              QString *error) {
  const QFileInfo destination(m_path);
  if (!QDir().mkpath(destination.absolutePath())) {
    if (error)
      *error = QStringLiteral("Could not create settings directory: %1")
                   .arg(destination.absolutePath());
    return false;
  }
  if (!m_store.save(m_path, value, error))
    return false;
  m_settings = value;
  emit settingsChanged();
  return true;
}
void SettingsController::rememberRecentProject(const QString &path) {
  if (path.isEmpty())
    return;
  const auto entry = QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toStdString();
  auto updated = m_settings;
  auto &recent = updated.storage.recentProjects;
  recent.erase(std::remove(recent.begin(), recent.end(), entry), recent.end());
  recent.insert(recent.begin(), entry);
  if (recent.size() > system::settings::MaxRecentProjects)
    recent.resize(system::settings::MaxRecentProjects);
  save(updated);
}

namespace {
std::string normalizedProjectPath(const QString &path) {
  return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toStdString();
}
} // namespace

bool SettingsController::forgetRecentProject(const QString &path) {
  auto updated = m_settings;
  auto &recent = updated.storage.recentProjects;
  const auto entry = normalizedProjectPath(path);
  const auto before = recent.size();
  recent.erase(std::remove_if(recent.begin(), recent.end(),
                              [&entry](const std::string &item) {
                                return item == entry ||
                                       normalizedProjectPath(
                                           QString::fromStdString(item)) == entry;
                              }),
               recent.end());
  if (recent.size() == before)
    return false;
  return save(updated);
}

int SettingsController::forgetMissingRecentProjects() {
  auto updated = m_settings;
  auto &recent = updated.storage.recentProjects;
  const auto before = recent.size();
  recent.erase(std::remove_if(recent.begin(), recent.end(),
                              [](const std::string &item) {
                                return !QFileInfo::exists(
                                    QString::fromStdString(item));
                              }),
               recent.end());
  const int removed = int(before - recent.size());
  if (removed > 0 && !save(updated))
    return 0;
  return removed;
}

} // namespace aquatrack::application
