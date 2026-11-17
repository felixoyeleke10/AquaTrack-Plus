#pragma once

#include "data/ProjectFileStore.h"

#include <QObject>
#include <optional>

namespace aquatrack::application {

class ProjectController final : public QObject {
  Q_OBJECT

public:
  explicit ProjectController(QObject *parent = nullptr);
  [[nodiscard]] bool hasProject() const { return m_manifest.has_value(); }
  [[nodiscard]] const domain::ProjectManifest *project() const;
  [[nodiscard]] QString projectPath() const { return m_path; }
  // True when the manifest in memory differs from the file on disk.
  [[nodiscard]] bool isDirty() const { return m_dirty; }
  [[nodiscard]] QString observationStorePath() const;
  bool createProject(const QString &path, const QString &name,
                     QString *error = nullptr);
  bool openProject(const QString &path, QString *error = nullptr);
  bool saveProject(QString *error = nullptr);
  bool addMedia(const QString &path, QString *error = nullptr);
  bool removeMedia(const QString &path, QString *error = nullptr);
  // The detector this project remembers (empty: the app's active one).
  // Saved straight away; does not emit projectChanged.
  bool setDetector(const QString &modelId, QString *error = nullptr);

signals:
  void projectChanged();
  void projectSaved(QString path);

private:
  data::ProjectFileStore m_store;
  std::optional<domain::ProjectManifest> m_manifest;
  QString m_path;
  bool m_dirty = false;
};

} // namespace aquatrack::application
