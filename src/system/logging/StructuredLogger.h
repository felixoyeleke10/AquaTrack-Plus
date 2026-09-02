#pragma once

#include <QJsonObject>
#include <QMutex>
#include <QString>

namespace aquatrack::system::logging {
enum class Level { Debug, Info, Warning, Error };
class StructuredLogger final {
public:
  explicit StructuredLogger(QString path);
  [[nodiscard]] bool write(Level level, const QString &component,
                           const QString &message,
                           const QJsonObject &context = {},
                           QString *error = nullptr);
  [[nodiscard]] QString path() const { return m_path; }

private:
  QString m_path;
  QMutex m_mutex;
};
[[nodiscard]] QString levelName(Level level);
} // namespace aquatrack::system::logging
