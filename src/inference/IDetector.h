#pragma once

#include "domain/DetectionTypes.h"

#include <QImage>
#include <QString>
#include <vector>

namespace aquatrack {

class IDetector {
public:
    virtual ~IDetector() = default;
    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual bool loadModel(const QString& path, QString* error) = 0;
    [[nodiscard]] virtual std::vector<Detection> detect(const QImage& frame) = 0;
};

}  // namespace aquatrack
