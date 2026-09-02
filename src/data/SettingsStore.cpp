#include "data/SettingsStore.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
namespace {
void setError(QString *e, const QString &m) {
  if (e)
    *e = m;
}
} // namespace
namespace aquatrack::data {
bool SettingsStore::save(const QString &path,
                         const system::settings::AquaTrackSettings &s,
                         QString *error) const {
  std::string reason;
  if (!system::settings::isValid(s, &reason)) {
    setError(error, QString::fromStdString(reason));
    return false;
  }
  QJsonArray recent;
  for (const auto &path : s.storage.recentProjects)
    recent.append(QString::fromStdString(path));
  const QJsonObject storage{
      {"defaultProjectDirectory",
       QString::fromStdString(s.storage.defaultProjectDirectory)},
      {"externalMedia", s.storage.useExternalMediaReferences},
      {"recentProjects", recent}};
  QJsonObject root{
      {"video", QJsonObject{{"decodeThreads", s.video.decodeThreads},
                            {"frameBufferSize", s.video.frameBufferSize},
                            {"hardwareDecode", s.video.hardwareDecode}}},
      {"detection",
       QJsonObject{{"modelId", QString::fromStdString(s.detection.modelId)},
                   {"confidence", s.detection.confidenceThreshold},
                   {"nms", s.detection.nmsThreshold},
                   {"inputSize", s.detection.inputSize}}},
      {"tracking",
       QJsonObject{{"matchIou", s.tracking.matchIou},
                   {"minimumHits", s.tracking.minimumHits},
                   {"maximumMissedFrames", s.tracking.maximumMissedFrames}}},
      {"pipeline",
       QJsonObject{{"heavyJobLimit", s.pipeline.heavyJobLimit},
                   {"visibleFirst", s.pipeline.processVisibleFirst}}},
      {"storage", storage},
      {"export",
       QJsonObject{{"includeConfidence", s.exportSettings.includeConfidence},
                   {"includeUnverified", s.exportSettings.includeUnverified}}},
      {"appearance",
       QJsonObject{{"theme", QString::fromStdString(s.appearance.theme)},
                   {"interfaceScale", s.appearance.interfaceScale}}}};
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    setError(error, file.errorString());
    return false;
  }
  if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 ||
      !file.commit()) {
    setError(error, file.errorString());
    return false;
  }
  return true;
}
bool SettingsStore::load(const QString &path,
                         system::settings::AquaTrackSettings &s,
                         QString *error) const {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    setError(error, file.errorString());
    return false;
  }
  QJsonParseError parse;
  const auto doc = QJsonDocument::fromJson(file.readAll(), &parse);
  if (parse.error != QJsonParseError::NoError || !doc.isObject()) {
    setError(error, parse.errorString());
    return false;
  }
  auto candidate = system::settings::AquaTrackSettings{};
  const auto root = doc.object(), v = root["video"].toObject(),
             d = root["detection"].toObject(), t = root["tracking"].toObject(),
             p = root["pipeline"].toObject(), st = root["storage"].toObject(),
             e = root["export"].toObject(), a = root["appearance"].toObject();
  candidate.video.decodeThreads =
      v["decodeThreads"].toInt(candidate.video.decodeThreads);
  candidate.video.frameBufferSize =
      v["frameBufferSize"].toInt(candidate.video.frameBufferSize);
  candidate.video.hardwareDecode =
      v["hardwareDecode"].toBool(candidate.video.hardwareDecode);
  candidate.detection.modelId = d["modelId"].toString().toStdString();
  candidate.detection.confidenceThreshold =
      d["confidence"].toDouble(candidate.detection.confidenceThreshold);
  candidate.detection.nmsThreshold =
      d["nms"].toDouble(candidate.detection.nmsThreshold);
  candidate.detection.inputSize =
      d["inputSize"].toInt(candidate.detection.inputSize);
  candidate.tracking.matchIou =
      t["matchIou"].toDouble(candidate.tracking.matchIou);
  candidate.tracking.minimumHits =
      t["minimumHits"].toInt(candidate.tracking.minimumHits);
  candidate.tracking.maximumMissedFrames =
      t["maximumMissedFrames"].toInt(candidate.tracking.maximumMissedFrames);
  candidate.pipeline.heavyJobLimit =
      p["heavyJobLimit"].toInt(candidate.pipeline.heavyJobLimit);
  candidate.pipeline.processVisibleFirst =
      p["visibleFirst"].toBool(candidate.pipeline.processVisibleFirst);
  candidate.storage.defaultProjectDirectory =
      st["defaultProjectDirectory"].toString().toStdString();
  candidate.storage.useExternalMediaReferences =
      st["externalMedia"].toBool(candidate.storage.useExternalMediaReferences);
  for (const auto value : st["recentProjects"].toArray()) {
    if (candidate.storage.recentProjects.size() >=
        system::settings::MaxRecentProjects)
      break;
    const auto path = value.toString();
    if (!path.isEmpty())
      candidate.storage.recentProjects.push_back(path.toStdString());
  }
  candidate.exportSettings.includeConfidence =
      e["includeConfidence"].toBool(candidate.exportSettings.includeConfidence);
  candidate.exportSettings.includeUnverified =
      e["includeUnverified"].toBool(candidate.exportSettings.includeUnverified);
  candidate.appearance.theme =
      a["theme"]
          .toString(QString::fromStdString(candidate.appearance.theme))
          .toStdString();
  candidate.appearance.interfaceScale =
      a["interfaceScale"].toDouble(candidate.appearance.interfaceScale);
  std::string reason;
  if (!system::settings::isValid(candidate, &reason)) {
    setError(error, QString::fromStdString(reason));
    return false;
  }
  s = std::move(candidate);
  return true;
}
} // namespace aquatrack::data
