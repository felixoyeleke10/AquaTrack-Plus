#include "system/logging/StructuredLogger.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QMutexLocker>

namespace aquatrack::system::logging {
StructuredLogger::StructuredLogger(QString path) : m_path(std::move(path)) {}
bool StructuredLogger::write(Level level, const QString &component,
                             const QString &message, const QJsonObject &context,
                             QString *error) {
  if (component.trimmed().isEmpty() || message.trimmed().isEmpty()) {
    if (error)
      *error = QStringLiteral("Log component and message are required");
    return false;
  }
  const QMutexLocker lock(&m_mutex);
  const QFileInfo info(m_path);
  if (!QDir().mkpath(info.absolutePath())) {
    if (error)
      *error = QStringLiteral("Could not create log directory: %1")
                   .arg(info.absolutePath());
    return false;
  }
  QFile file(m_path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    if (error)
      *error = file.errorString();
    return false;
  }
  QJsonObject record{
      {QStringLiteral("timestamp"),
       QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
      {QStringLiteral("level"), levelName(level)},
      {QStringLiteral("component"), component},
      {QStringLiteral("message"), message},
      {QStringLiteral("context"), context}};
  const QByteArray line =
      QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n';
  if (file.write(line) != line.size() || !file.flush()) {
    if (error)
      *error = file.errorString();
    return false;
  }
  return true;
}
QString levelName(Level level) {
  switch (level) {
  case Level::Debug:
    return QStringLiteral("debug");
  case Level::Info:
    return QStringLiteral("info");
  case Level::Warning:
    return QStringLiteral("warning");
  case Level::Error:
    return QStringLiteral("error");
  }
  return QStringLiteral("unknown");
}
} // namespace aquatrack::system::logging
