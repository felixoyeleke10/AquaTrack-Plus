#include "data/ProjectFileStore.h"
#include "data/catalog/DatasetCatalog.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {
void usage(QTextStream &stream) {
  stream << "AquaTrack+ native CLI\nUsage:\n"
            "  aquatrack-cli datasets <root> [--json]\n"
            "  aquatrack-cli project <file.aqp> [--json]\n";
}
int datasets(const QString &root, bool json) {
  const auto result = aquatrack::data::DatasetCatalog().scan(root);
  QTextStream out(stdout);
  if (json) {
    QJsonArray values;
    for (const auto &item : result.datasets)
      values.append(QJsonObject{
          {"id", item.id},
          {"name", item.name},
          {"path", item.rootPath},
          {"format", aquatrack::data::datasetFormatName(item.format)},
          {"frames", item.frameCount},
          {"annotations", item.hasAnnotations}});
    QJsonArray warnings;
    for (const auto &warning : result.warnings)
      warnings.append(warning);
    out << QJsonDocument(
               QJsonObject{{"datasets", values}, {"warnings", warnings}})
               .toJson(QJsonDocument::Indented);
  } else {
    for (const auto &item : result.datasets)
      out << item.id << "  " << aquatrack::data::datasetFormatName(item.format)
          << "  " << item.frameCount << " frames  " << item.rootPath << '\n';
    for (const auto &warning : result.warnings)
      out << "warning: " << warning << '\n';
  }
  return result.datasets.isEmpty() ? 2 : 0;
}
int project(const QString &path, bool json) {
  aquatrack::domain::ProjectManifest manifest;
  QString error;
  if (!aquatrack::data::ProjectFileStore().load(path, manifest, &error)) {
    QTextStream(stderr) << "error: " << error << '\n';
    return 2;
  }
  QTextStream out(stdout);
  if (json) {
    QJsonArray media;
    for (const auto &value : manifest.mediaPaths)
      media.append(QString::fromStdString(value));
    out << QJsonDocument(
               QJsonObject{
                   {"valid", true},
                   {"schemaVersion", manifest.schemaVersion},
                   {"projectId", QString::fromStdString(manifest.projectId)},
                   {"name", QString::fromStdString(manifest.name)},
                   {"media", media}})
               .toJson(QJsonDocument::Indented);
  } else {
    out << "valid AquaTrack project\nname: "
        << QString::fromStdString(manifest.name)
        << "\nschema: " << manifest.schemaVersion
        << "\nmedia: " << manifest.mediaPaths.size() << '\n';
  }
  return 0;
}
} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  const QStringList arguments = app.arguments();
  if (arguments.size() == 1) {
    QTextStream err(stderr);
    usage(err);
    return 1;
  }
  if (arguments.contains(QStringLiteral("--help"))) {
    QTextStream out(stdout);
    usage(out);
    return 0;
  }
  if (arguments.size() < 3) {
    QTextStream err(stderr);
    usage(err);
    return 1;
  }
  const bool json = arguments.contains(QStringLiteral("--json"));
  if (arguments[1] == QStringLiteral("datasets"))
    return datasets(arguments[2], json);
  if (arguments[1] == QStringLiteral("project"))
    return project(arguments[2], json);
  QTextStream err(stderr);
  err << "error: unknown command " << arguments[1] << '\n';
  usage(err);
  return 1;
}
