#include "application/controllers/ProjectController.h"

#include "application/ProjectLayout.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QUuid>

#include <algorithm>

namespace aquatrack::application {

ProjectController::ProjectController(QObject *parent) : QObject(parent) {}

const domain::ProjectManifest *ProjectController::project() const {
  return m_manifest ? &*m_manifest : nullptr;
}

QString ProjectController::observationStorePath() const {
  if (!m_manifest)
    return {};
  const QString configured = QString::fromStdString(m_manifest->databasePath);
  if (QFileInfo(configured).isAbsolute())
    return QDir::cleanPath(configured);
  return QDir(QFileInfo(m_path).absolutePath()).filePath(configured);
}

bool ProjectController::createProject(const QString &path, const QString &name,
                                      QString *error) {
  if (name.trimmed().isEmpty()) {
    if (error)
      *error = QStringLiteral("Project name is required");
    return false;
  }
  // The folder around the manifest becomes the project: media, annotations,
  // models, exports and snapshots each get a home inside it.
  const ProjectLayout layout(path);
  if (!layout.create(error))
    return false;
  const auto now = QDateTime::currentMSecsSinceEpoch();
  domain::ProjectManifest candidate{
      .projectId =
          QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString(),
      .name = name.trimmed().toStdString(),
      .createdAtMs = now,
      .updatedAtMs = now,
      .databasePath = ProjectLayout::ObservationStore,
  };
  if (!m_store.save(path, candidate, error))
    return false;
  m_manifest = std::move(candidate);
  m_path = QFileInfo(path).absoluteFilePath();
  m_dirty = false;
  emit projectChanged();
  emit projectSaved(m_path);
  return true;
}

bool ProjectController::openProject(const QString &path, QString *error) {
  domain::ProjectManifest candidate;
  if (!m_store.load(path, candidate, error))
    return false;
  m_manifest = std::move(candidate);
  m_path = QFileInfo(path).absoluteFilePath();
  m_dirty = false;
  emit projectChanged();
  return true;
}

bool ProjectController::saveProject(QString *error) {
  if (!m_manifest) {
    if (error)
      *error = QStringLiteral("No project is open");
    return false;
  }
  m_manifest->updatedAtMs = QDateTime::currentMSecsSinceEpoch();
  if (!m_store.save(m_path, *m_manifest, error))
    return false;
  m_dirty = false;
  emit projectSaved(m_path);
  return true;
}

bool ProjectController::addMedia(const QString &path, QString *error) {
  if (!m_manifest) {
    if (error)
      *error = QStringLiteral("Open or create a project before adding media");
    return false;
  }
  const auto normalized =
      QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toStdString();
  if (std::find(m_manifest->mediaPaths.begin(), m_manifest->mediaPaths.end(),
                normalized) == m_manifest->mediaPaths.end()) {
    m_manifest->mediaPaths.push_back(normalized);
    m_dirty = true;
    emit projectChanged();
  }
  return true;
}

bool ProjectController::setDetector(const QString &modelId, QString *error) {
  if (!m_manifest) {
    if (error)
      *error = QStringLiteral("No project is open");
    return false;
  }
  const std::string id = modelId.toStdString();
  if (m_manifest->detectorModelId == id)
    return true;
  const std::string previous = m_manifest->detectorModelId;
  m_manifest->detectorModelId = id;
  // Written to disk now, like other settings, rather than waiting for Save.
  // Only this field: unsaved media edits stay unsaved.
  domain::ProjectManifest onDisk;
  if (m_store.load(m_path, onDisk)) {
    onDisk.detectorModelId = id;
    if (m_store.save(m_path, onDisk, error))
      return true;
  } else if (!m_dirty && m_store.save(m_path, *m_manifest, error)) {
    return true;
  }
  if (error && error->isEmpty())
    *error = QStringLiteral("Could not save the project file");
  m_manifest->detectorModelId = previous;
  return false;
}

bool ProjectController::removeMedia(const QString &path, QString *error) {
  if (!m_manifest) {
    if (error)
      *error = QStringLiteral("No project is open");
    return false;
  }
  const auto normalized =
      QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toStdString();
  auto &media = m_manifest->mediaPaths;
  const auto removed = std::remove(media.begin(), media.end(), normalized);
  if (removed == media.end()) {
    if (error)
      *error = QStringLiteral("That media is not part of this project");
    return false;
  }
  media.erase(removed, media.end());
  m_dirty = true;
  emit projectChanged();
  return true;
}

} // namespace aquatrack::application
