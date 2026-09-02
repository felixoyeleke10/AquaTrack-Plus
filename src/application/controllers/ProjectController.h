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
  [[nodiscard]] QString observationStorePath() const;
  bool createProject(const QString &path, const QString &name,
                     QString *error = nullptr);
  bool openProject(const QString &path, QString *error = nullptr);
  bool saveProject(QString *error = nullptr);
  bool addMedia(const QString &path, QString *error = nullptr);
  bool removeMedia(const QString &path, QString *error = nullptr);

signals:
  void projectChanged();
  void projectSaved(QString path);

private:
  data::ProjectFileStore m_store;
  std::optional<domain::ProjectManifest> m_manifest;
  QString m_path;
};

} // namespace aquatrack::application
