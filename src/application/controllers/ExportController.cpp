#include "application/controllers/ExportController.h"
#include "export/ObservationExport.h"
#include <QSaveFile>
namespace aquatrack::application {
bool ExportController::exportCsv(const QString &path,
                                 const std::vector<domain::Observation> &values,
                                 QString *error) const {
  return write(path, exporting::observationsToCsv(values), error);
}
bool ExportController::exportMot(const QString &path,
                                 const std::vector<domain::Observation> &values,
                                 QString *error) const {
  return write(path, exporting::observationsToMotChallenge(values), error);
}
bool ExportController::write(const QString &path, const std::string &content,
                             QString *error) const {
  if (path.trimmed().isEmpty()) {
    if (error)
      *error = QStringLiteral("Export path is required");
    return false;
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    if (error)
      *error = file.errorString();
    return false;
  }
  const QByteArray bytes(content.data(),
                         static_cast<qsizetype>(content.size()));
  if (file.write(bytes) != bytes.size() || !file.commit()) {
    if (error)
      *error = file.errorString();
    return false;
  }
  return true;
}
} // namespace aquatrack::application
