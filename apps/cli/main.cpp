#include "application/AnalysisRunner.h"
#include "application/AnnotationImporter.h"
#include "application/MediaImporter.h"
#include "application/ModelImporter.h"
#include "application/ProjectLayout.h"
#include "application/controllers/ProjectController.h"
#include "data/ModelRegistry.h"
#include "data/ProjectFileStore.h"
#include "data/media/MediaSource.h"
#include "data/media/VideoProbe.h"
#include "data/media/VideoSequenceManifest.h"
#include "data/observations/JsonObservationRepository.h"
#include "data/catalog/DatasetCatalog.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QTextStream>

namespace {
void usage(QTextStream &stream) {
  stream << "AquaTrack+ native CLI\nUsage:\n"
            "  aquatrack-cli datasets <root> [--json]\n"
            "  aquatrack-cli project <file.aqp> [--json]\n"
            "  aquatrack-cli project create <parent-folder> <name>\n"
            "      Creates <parent-folder>/<name>/<name>.aqp with media/, annotations/,\n"
            "      models/, exports/ and snapshots/.\n"
            "  aquatrack-cli project add <file.aqp> (--copy | --link) <media>...\n"
            "      Adds sequence folders or video files. --copy puts them in the\n"
            "      project's media/ folder; --link references them where they are.\n"
            "  aquatrack-cli project sequence <file.aqp> <name> <video>...\n"
            "      Creates a sequence of videos (played back to back, in the\n"
            "      order given) in media/<name>/ and adds it to the project.\n"
            "  aquatrack-cli project model <file.aqp> <weights>...\n"
            "      Imports detector weights into the project's models/ registry.\n"
            "  aquatrack-cli project annotations <file.aqp> <media> <gt.txt|tracks.json>\n"
            "      Imports ground-truth tracks for one media item (replaces any).\n"
            "  aquatrack-cli project analyze <file.aqp> <media> --model <id>\n"
            "                [--confidence 0.35] [--tracker bytetrack|iou]\n"
            "      Runs detection and tracking over the media and stores the tracks.\n";
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
          {"images", item.imageCount},
          {"annotations", item.hasAnnotations},
          {"mediaPresent", item.mediaPresent},
          {"reviewable", item.isReviewable()},
          {"synthetic", item.synthetic}});
    QJsonArray warnings;
    for (const auto &warning : result.warnings)
      warnings.append(warning);
    out << QJsonDocument(
               QJsonObject{{"datasets", values}, {"warnings", warnings}})
               .toJson(QJsonDocument::Indented);
  } else {
    for (const auto &item : result.datasets)
      out << item.id << "  " << aquatrack::data::datasetFormatName(item.format)
          << (item.synthetic ? " (synthetic)" : "") << "  " << item.frameCount
          << " frames  " << item.rootPath << '\n';
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

int createProject(const QString &parent, const QString &name) {
  QTextStream err(stderr);
  if (name.trimmed().isEmpty()) {
    err << "error: a project name is required\n";
    return 1;
  }
  const QString folderName = aquatrack::application::ProjectLayout::folderNameFor(name);
  const QString folder = QDir(parent).filePath(folderName);
  const QString file = QDir(folder).filePath(folderName + QStringLiteral(".aqp"));
  if (QFileInfo::exists(file)) {
    err << "error: a project already exists at " << QDir::toNativeSeparators(file) << '\n';
    return 2;
  }
  if (!QDir().mkpath(folder)) {
    err << "error: could not create " << QDir::toNativeSeparators(folder) << '\n';
    return 2;
  }
  aquatrack::application::ProjectController controller;
  QString error;
  if (!controller.createProject(file, name, &error)) {
    err << "error: " << error << '\n';
    return 2;
  }
  QTextStream(stdout) << QDir::toNativeSeparators(controller.projectPath()) << '\n';
  return 0;
}

int addMedia(const QString &file, bool copy, const QStringList &sources) {
  QTextStream out(stdout);
  QTextStream err(stderr);
  aquatrack::application::ProjectController controller;
  QString error;
  if (!controller.openProject(file, &error)) {
    err << "error: " << error << '\n';
    return 2;
  }
  const aquatrack::application::ProjectLayout layout(controller.projectPath());
  QStringList paths;
  QStringList toCopy;
  for (const auto &source : sources) {
    if (!QFileInfo::exists(source)) {
      err << "error: " << QDir::toNativeSeparators(source) << " does not exist\n";
      return 2;
    }
    (copy && !layout.contains(source) ? toCopy : paths) << source;
  }
  if (!toCopy.isEmpty()) {
    const qint64 total = aquatrack::application::MediaImporter::sizeOf(toCopy);
    out << "copying " << toCopy.size() << " item(s), "
        << QLocale::system().formattedDataSize(total) << ", into "
        << QDir::toNativeSeparators(layout.media()) << '\n';
    out.flush();
    aquatrack::application::MediaImporter importer(total, {});
    for (const auto &source : toCopy) {
      QString copied;
      if (importer.copy(source, layout.media(), &copied, &error) !=
          aquatrack::application::MediaImporter::Result::Copied) {
        err << "error: " << error << '\n';
        break;  // keep what was copied: it is added below
      }
      out << "  copied " << QFileInfo(source).fileName() << '\n';
      out.flush();
      paths << copied;
    }
  }
  for (const auto &path : paths)
    controller.addMedia(path);
  if (!controller.saveProject(&error)) {
    err << "error: " << error << '\n';
    return 2;
  }
  out << "project now lists " << controller.project()->mediaPaths.size()
      << " media item(s)\n";
  return paths.size() == sources.size() ? 0 : 3;
}

// Shared by the commands that work inside an existing project.
struct OpenProject {
  aquatrack::application::ProjectController controller;
  bool open(const QString &file) {
    QString error;
    if (controller.openProject(file, &error))
      return true;
    QTextStream(stderr) << "error: " << error << '\n';
    return false;
  }
  [[nodiscard]] aquatrack::application::ProjectLayout layout() const {
    return aquatrack::application::ProjectLayout(controller.projectPath());
  }
};

int createSequence(const QString &file, const QString &name, const QStringList &videos) {
  QTextStream out(stdout);
  QTextStream err(stderr);
  OpenProject project;
  if (!project.open(file))
    return 2;
  aquatrack::media::VideoSequenceManifest manifest;
  manifest.name = name.trimmed();
  for (const auto &video : videos) {
    aquatrack::media::VideoInfo info;
    QString error;
    if (!aquatrack::media::probeVideo(video, info, &error)) {
      err << "error: " << QFileInfo(video).fileName() << ": " << error << '\n';
      return 2;
    }
    out << "  " << QFileInfo(video).fileName() << "  " << info.frameCount << " frames  "
        << aquatrack::media::formatDuration(info.durationMs) << '\n';
    manifest.clips.push_back(std::move(info));
  }
  const QString folder = QDir(project.layout().media())
                             .filePath(aquatrack::application::ProjectLayout::folderNameFor(name));
  QString error;
  if (QFileInfo::exists(folder)) {
    err << "error: " << QDir::toNativeSeparators(folder) << " already exists\n";
    return 2;
  }
  if (!manifest.save(folder, &error) || !project.controller.addMedia(folder, &error) ||
      !project.controller.saveProject(&error)) {
    err << "error: " << error << '\n';
    return 2;
  }
  out << "created sequence " << manifest.name << ": " << manifest.clips.size()
      << " videos, " << manifest.totalFrames() << " frames, "
      << aquatrack::media::formatDuration(manifest.totalDurationMs()) << '\n';
  return 0;
}

int importModels(const QString &file, const QStringList &weights) {
  QTextStream out(stdout);
  OpenProject project;
  if (!project.open(file))
    return 2;
  const QString models = project.layout().ensure(project.layout().models());
  int failures = 0;
  for (const auto &path : weights) {
    const auto candidate = aquatrack::application::ModelImporter::describe(path);
    QString manifestPath;
    QString error;
    if (aquatrack::application::ModelImporter::import(candidate, models, &manifestPath, &error)) {
      out << "imported " << candidate.name << " -> "
          << QDir::toNativeSeparators(QFileInfo(manifestPath).absolutePath()) << '\n';
    } else {
      QTextStream(stderr) << "error: " << QFileInfo(path).fileName() << ": " << error << '\n';
      ++failures;
    }
  }
  return failures == 0 ? 0 : 3;
}

// Writes `observations` as the stored annotations of `media`.
bool storeObservations(const OpenProject &project, const aquatrack::media::MediaSource &media,
                       const std::vector<aquatrack::domain::Observation> &observations) {
  aquatrack::data::JsonObservationRepository repository(project.controller.observationStorePath());
  std::string message;
  if (!repository.open(message) ||
      !repository.replaceObservationsForVideo(media.name().toStdString(), observations, message)) {
    QTextStream(stderr) << "error: " << QString::fromStdString(message) << '\n';
    return false;
  }
  return true;
}

int importAnnotations(const QString &file, const QString &mediaPath, const QString &annotations) {
  OpenProject project;
  if (!project.open(file))
    return 2;
  aquatrack::media::MediaSource media;
  QString error;
  aquatrack::application::AnnotationImport imported;
  if (!media.open(mediaPath, &error) ||
      !aquatrack::application::AnnotationImporter::read(annotations, media, imported, &error)) {
    QTextStream(stderr) << "error: " << error << '\n';
    return 2;
  }
  if (!storeObservations(project, media, imported.observations))
    return 2;
  QTextStream(stdout) << "imported " << imported.observations.size() << " boxes in "
                      << imported.tracks << " tracks over " << imported.frames << " frames into "
                      << media.name() << " (" << imported.format << ")"
                      << (imported.outOfRange ? QStringLiteral(", %1 rows past the end left out")
                                                    .arg(imported.outOfRange)
                                              : QString())
                      << '\n';
  return 0;
}

int analyze(const QString &file, const QString &mediaPath, const QStringList &options) {
  QTextStream out(stdout);
  QTextStream err(stderr);
  OpenProject project;
  if (!project.open(file))
    return 2;
  const auto option = [&options](const QString &name, const QString &fallback) {
    const auto index = options.indexOf(name);
    return index >= 0 && index + 1 < options.size() ? options.at(index + 1) : fallback;
  };
  const QString modelId = option(QStringLiteral("--model"), QString());
  const auto registry = aquatrack::data::ModelRegistry().scan(project.layout().models());
  const aquatrack::data::ModelEntry *model = nullptr;
  for (const auto &entry : registry.entries)
    if (entry.isUsable() && (modelId.isEmpty() || QString::fromStdString(entry.manifest.id) == modelId))
      model = &entry;
  if (!model) {
    err << "error: no usable model " << modelId << " in " << project.layout().models() << '\n';
    return 2;
  }
  aquatrack::media::MediaSource media;
  QString error;
  if (!media.open(mediaPath, &error)) {
    err << "error: " << error << '\n';
    return 2;
  }
  aquatrack::application::AnalysisRequest request;
  if (!aquatrack::application::AnalysisRunner::locatePython(QString(), &request.python,
                                                           &request.packageRoot, &error)) {
    err << "error: " << error << '\n';
    return 2;
  }
  request.source = media.path();
  request.weights = model->artifactPath;
  request.backend = QString::fromStdString(model->manifest.backend);
  request.modelName = QString::fromStdString(model->manifest.displayName);
  for (const auto &name : model->manifest.classes)
    request.classNames << QString::fromStdString(name);
  request.imageSize = model->manifest.inputWidth > 0 ? model->manifest.inputWidth : 640;
  request.confidence = option(QStringLiteral("--confidence"), QStringLiteral("0.35")).toDouble();
  request.tracker = option(QStringLiteral("--tracker"), QStringLiteral("bytetrack"));
  request.output = QDir(project.layout().ensure(project.layout().annotations()))
                       .filePath(QStringLiteral("%1.detections.json").arg(media.name()));

  aquatrack::application::AnalysisRunner runner;
  QEventLoop loop;
  QString resultsPath;
  QString failure;
  int lastPercent = -1;
  QObject::connect(&runner, &aquatrack::application::AnalysisRunner::progress, &loop,
                   [&](int done, int total, const QString &text) {
                     const int percent = total > 0 ? done * 100 / total : -1;
                     if (percent == lastPercent && total > 0)
                       return;
                     lastPercent = percent;
                     out << "  " << text << '\n';
                     out.flush();
                   });
  QObject::connect(&runner, &aquatrack::application::AnalysisRunner::finished, &loop,
                   [&](const QString &path) {
                     resultsPath = path;
                     loop.quit();
                   });
  QObject::connect(&runner, &aquatrack::application::AnalysisRunner::failed, &loop,
                   [&](const QString &message) {
                     failure = message;
                     loop.quit();
                   });
  out << "detecting and tracking " << media.name() << " with " << request.modelName << '\n';
  out.flush();
  runner.start(request);
  loop.exec();
  if (!failure.isEmpty()) {
    err << "error: " << failure << '\n';
    return 2;
  }
  aquatrack::application::AnalysisResult result;
  if (!aquatrack::application::AnalysisRunner::readResults(resultsPath, media, result, &error)) {
    err << "error: " << error << '\n';
    return 2;
  }
  if (!storeObservations(project, media, result.observations))
    return 2;
  out << "stored " << result.observations.size() << " detections in " << result.tracks
      << " tracks for " << media.name() << " (" << result.seconds << " s)\n";
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
  if (arguments[1] == QStringLiteral("project")) {
    if (arguments[2] == QStringLiteral("create") && arguments.size() == 5)
      return createProject(arguments[3], arguments[4]);
    if (arguments[2] == QStringLiteral("add") && arguments.size() >= 6) {
      const QString mode = arguments[4];
      if (mode != QStringLiteral("--copy") && mode != QStringLiteral("--link")) {
        QTextStream err(stderr);
        usage(err);
        return 1;
      }
      return addMedia(arguments[3], mode == QStringLiteral("--copy"), arguments.mid(5));
    }
    if (arguments[2] == QStringLiteral("sequence") && arguments.size() >= 6)
      return createSequence(arguments[3], arguments[4], arguments.mid(5));
    if (arguments[2] == QStringLiteral("model") && arguments.size() >= 5)
      return importModels(arguments[3], arguments.mid(4));
    if (arguments[2] == QStringLiteral("annotations") && arguments.size() == 6)
      return importAnnotations(arguments[3], arguments[4], arguments[5]);
    if (arguments[2] == QStringLiteral("analyze") && arguments.size() >= 5)
      return analyze(arguments[3], arguments[4], arguments.mid(5));
    return project(arguments[2], json);
  }
  QTextStream err(stderr);
  err << "error: unknown command " << arguments[1] << '\n';
  usage(err);
  return 1;
}
