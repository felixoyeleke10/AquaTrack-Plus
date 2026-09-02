#include "TestSupport.h"
#include "system/logging/StructuredLogger.h"
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>

int main() {
  QTemporaryDir directory;
  requireTest(directory.isValid(), "temporary directory");
  const QString path =
      directory.filePath(QStringLiteral("nested/aquatrack.jsonl"));
  aquatrack::system::logging::StructuredLogger logger(path);
  QString error;
  requireTest(logger.write(aquatrack::system::logging::Level::Info,
                           QStringLiteral("pipeline"),
                           QStringLiteral("started"),
                           {{QStringLiteral("workers"), 2}}, &error),
              "first structured record");
  requireTest(logger.write(aquatrack::system::logging::Level::Warning,
                           QStringLiteral("decoder"),
                           QStringLiteral("frame skipped"), {}, &error),
              "second structured record");
  QFile file(path);
  requireTest(file.open(QIODevice::ReadOnly | QIODevice::Text), "open log");
  const auto lines = file.readAll().split('\n');
  requireTest(lines.size() == 3 && lines.back().isEmpty(),
              "newline-delimited records");
  const auto first = QJsonDocument::fromJson(lines[0]).object();
  requireTest(first.value(QStringLiteral("level")).toString() ==
                  QStringLiteral("info"),
              "level field");
  requireTest(first.value(QStringLiteral("component")).toString() ==
                  QStringLiteral("pipeline"),
              "component field");
  requireTest(first.value(QStringLiteral("context"))
                      .toObject()
                      .value(QStringLiteral("workers"))
                      .toInt() == 2,
              "context field");
  requireTest(!first.value(QStringLiteral("timestamp")).toString().isEmpty(),
              "timestamp field");
  requireTest(!logger.write(aquatrack::system::logging::Level::Error, QString(),
                            QStringLiteral("invalid"), {}, &error),
              "reject invalid record");
  return 0;
}
