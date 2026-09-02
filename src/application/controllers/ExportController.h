#pragma once
#include "domain/ScientificTypes.h"
#include <QObject>
namespace aquatrack::application {
class ExportController final : public QObject {
public:
  explicit ExportController(QObject *parent = nullptr) : QObject(parent) {}
  [[nodiscard]] bool
  exportCsv(const QString &path,
            const std::vector<domain::Observation> &observations,
            QString *error = nullptr) const;
  [[nodiscard]] bool
  exportMot(const QString &path,
            const std::vector<domain::Observation> &observations,
            QString *error = nullptr) const;

private:
  [[nodiscard]] bool write(const QString &path, const std::string &content,
                           QString *error) const;
};
} // namespace aquatrack::application
