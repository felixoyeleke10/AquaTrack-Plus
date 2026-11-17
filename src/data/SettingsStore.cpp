#include "data/SettingsStore.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>
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
      {"mediaImport", QString::fromStdString(s.storage.mediaImport)},
      {"recentProjects", recent}};
  QJsonArray folded;
  for (const auto &id : s.layout.foldedSections)
    folded.append(QString::fromStdString(id));
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
      {"analysis",
       QJsonObject{
           {"pythonExecutable",
            QString::fromStdString(s.analysis.pythonExecutable)},
           {"tracker", QString::fromStdString(s.analysis.tracker)}}},
      {"appearance",
       QJsonObject{{"theme", QString::fromStdString(s.appearance.theme)},
                   {"interfaceScale", s.appearance.interfaceScale}}},
      {"layout", QJsonObject{{"reviewActivityHeight", s.layout.reviewActivityHeight},
                             {"reviewLogHeight", s.layout.reviewLogHeight},
                             {"foldedSections", folded}}}};
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
             e = root["export"].toObject(), a = root["appearance"].toObject(),
             an = root["analysis"].toObject();
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
  // "externalMedia" (a yes/no flag that nothing used) is superseded.
  candidate.storage.mediaImport =
      st["mediaImport"]
          .toString(QString::fromStdString(candidate.storage.mediaImport))
          .toStdString();
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
  candidate.analysis.pythonExecutable =
      an["pythonExecutable"].toString().toStdString();
  candidate.analysis.tracker =
      an["tracker"]
          .toString(QString::fromStdString(candidate.analysis.tracker))
          .toStdString();
  candidate.appearance.theme =
      a["theme"]
          .toString(QString::fromStdString(candidate.appearance.theme))
          .toStdString();
  candidate.appearance.interfaceScale =
      a["interfaceScale"].toDouble(candidate.appearance.interfaceScale);
  // Layout is a convenience: an odd value is brought into range rather
  // than rejecting the whole settings file.
  const auto l = root["layout"].toObject();
  candidate.layout.reviewActivityHeight =
      std::clamp(l["reviewActivityHeight"].toInt(candidate.layout.reviewActivityHeight), 0,
                 system::settings::MaxReviewPanelHeight);
  candidate.layout.reviewLogHeight =
      std::clamp(l["reviewLogHeight"].toInt(candidate.layout.reviewLogHeight), 0,
                 system::settings::MaxReviewPanelHeight);
  for (const auto item : l["foldedSections"].toArray())
    if (item.isString() && candidate.layout.foldedSections.size() < 32)
      candidate.layout.foldedSections.push_back(item.toString().toStdString());
  std::string reason;
  if (!system::settings::isValid(candidate, &reason)) {
    setError(error, QString::fromStdString(reason));
    return false;
  }
  s = std::move(candidate);
  return true;
}
} // namespace aquatrack::data
