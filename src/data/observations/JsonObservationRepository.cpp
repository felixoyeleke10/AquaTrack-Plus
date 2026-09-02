#include "data/observations/JsonObservationRepository.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace aquatrack::data {
namespace {
QJsonObject surveyJson(const domain::Survey &value) {
  QJsonArray videos;
  for (const auto &id : value.videos)
    videos.append(QString::fromStdString(id));
  QJsonObject object{{"id", QString::fromStdString(value.id)},
                     {"name", QString::fromStdString(value.name)},
                     {"createdAtMs", value.createdAtMs},
                     {"videos", videos}};
  if (value.location)
    object.insert("location", QString::fromStdString(*value.location));
  return object;
}
QJsonObject observationJson(const domain::Observation &value) {
  QJsonObject object{{"id", QString::fromStdString(value.id)},
                     {"videoId", QString::fromStdString(value.videoId)},
                     {"timestampMs", value.timestampMs},
                     {"frameIndex", value.frameIndex},
                     {"localTrackId", value.localTrackId},
                     {"x1", value.box.x1},
                     {"y1", value.box.y1},
                     {"x2", value.box.x2},
                     {"y2", value.box.y2},
                     {"className", QString::fromStdString(value.className)},
                     {"confidence", value.confidence},
                     {"humanVerified", value.humanVerified}};
  if (value.globalTrackId)
    object.insert("globalTrackId", *value.globalTrackId);
  return object;
}
} // namespace

JsonObservationRepository::JsonObservationRepository(QString path)
    : m_path(std::move(path)) {}
bool JsonObservationRepository::open(std::string &error) {
  m_surveys.clear();
  m_observations.clear();
  m_videos.clear();
  QFile file(m_path);
  if (!file.exists())
    return true;
  if (!file.open(QIODevice::ReadOnly)) {
    error = file.errorString().toStdString();
    return false;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject() ||
      document.object().value("version").toInt() != 1) {
    error = "invalid or unsupported observation repository";
    return false;
  }
  for (const auto &item : document.object().value("surveys").toArray()) {
    const auto object = item.toObject();
    domain::Survey value{object.value("id").toString().toStdString(),
                         object.value("name").toString().toStdString(),
                         object.value("createdAtMs").toInteger()};
    if (object.contains("location"))
      value.location = object.value("location").toString().toStdString();
    for (const auto &video : object.value("videos").toArray())
      value.videos.push_back(video.toString().toStdString());
    if (!domain::isValid(value)) {
      error = "invalid survey record";
      return false;
    }
    m_surveys[value.id] = std::move(value);
  }
  for (const auto &item : document.object().value("observations").toArray()) {
    const auto object = item.toObject();
    domain::Observation value;
    value.id = object.value("id").toString().toStdString();
    value.videoId = object.value("videoId").toString().toStdString();
    value.timestampMs = object.value("timestampMs").toInteger(-1);
    value.frameIndex = object.value("frameIndex").toInteger(-1);
    value.localTrackId = object.value("localTrackId").toInteger();
    if (object.contains("globalTrackId"))
      value.globalTrackId = object.value("globalTrackId").toInteger();
    value.box = {static_cast<float>(object.value("x1").toDouble()),
                 static_cast<float>(object.value("y1").toDouble()),
                 static_cast<float>(object.value("x2").toDouble()),
                 static_cast<float>(object.value("y2").toDouble())};
    value.className = object.value("className").toString().toStdString();
    value.confidence =
        static_cast<float>(object.value("confidence").toDouble(-1));
    value.humanVerified = object.value("humanVerified").toBool();
    if (!domain::isValid(value)) {
      error = "invalid observation record";
      return false;
    }
    m_observations[value.id] = std::move(value);
  }
  for (const auto &item : document.object().value("videos").toArray())
    m_videos.insert(item.toString().toStdString());
  for (const auto &[id, value] : m_observations)
    m_videos.insert(value.videoId);
  return true;
}
bool JsonObservationRepository::saveSurvey(const domain::Survey &value,
                                           std::string &error) {
  if (!domain::isValid(value)) {
    error = "invalid survey";
    return false;
  }
  m_surveys[value.id] = value;
  return persist(error);
}
bool JsonObservationRepository::saveObservations(
    std::span<const domain::Observation> values, std::string &error) {
  for (const auto &value : values)
    if (!domain::isValid(value)) {
      error = "invalid observation";
      return false;
    }
  for (const auto &value : values)
    m_observations[value.id] = value;
  for (const auto &value : values)
    m_videos.insert(value.videoId);
  return persist(error);
}

bool JsonObservationRepository::replaceObservationsForVideo(
    const domain::VideoId &id, std::span<const domain::Observation> values,
    std::string &error) {
  if (id.empty()) {
    error = "video id is required";
    return false;
  }
  for (const auto &value : values) {
    if (value.videoId != id || !domain::isValid(value)) {
      error = "replacement contains an invalid or mismatched observation";
      return false;
    }
  }
  const auto previous = m_observations;
  const auto previousVideos = m_videos;
  std::erase_if(m_observations,
                [&id](const auto &item) { return item.second.videoId == id; });
  for (const auto &value : values)
    m_observations[value.id] = value;
  m_videos.insert(id);
  if (!persist(error)) {
    m_observations = previous;
    m_videos = previousVideos;
    return false;
  }
  return true;
}
std::optional<domain::Survey>
JsonObservationRepository::survey(const domain::SurveyId &id) const {
  const auto found = m_surveys.find(id);
  return found == m_surveys.end() ? std::nullopt : std::optional(found->second);
}
std::vector<domain::Observation>
JsonObservationRepository::observationsForVideo(
    const domain::VideoId &id) const {
  std::vector<domain::Observation> values;
  for (const auto &[key, value] : m_observations)
    if (value.videoId == id)
      values.push_back(value);
  return values;
}
bool JsonObservationRepository::containsVideo(const domain::VideoId &id) const {
  return m_videos.contains(id);
}
bool JsonObservationRepository::persist(std::string &error) const {
  QJsonArray surveys;
  for (const auto &[id, value] : m_surveys)
    surveys.append(surveyJson(value));
  QJsonArray observations;
  for (const auto &[id, value] : m_observations)
    observations.append(observationJson(value));
  QJsonArray videos;
  for (const auto &id : m_videos)
    videos.append(QString::fromStdString(id));
  QSaveFile file(m_path);
  if (!file.open(QIODevice::WriteOnly)) {
    error = file.errorString().toStdString();
    return false;
  }
  if (file.write(QJsonDocument(QJsonObject{{"version", 1},
                                           {"surveys", surveys},
                                           {"videos", videos},
                                           {"observations", observations}})
                     .toJson(QJsonDocument::Indented)) < 0 ||
      !file.commit()) {
    error = file.errorString().toStdString();
    return false;
  }
  return true;
}
} // namespace aquatrack::data
