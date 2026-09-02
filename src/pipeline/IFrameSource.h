#pragma once

#include "domain/ScientificTypes.h"

#include <QImage>
#include <QString>

namespace aquatrack::pipeline {
struct DecodedFrame {
  QImage image;
  std::int64_t index = 0;
  domain::TimestampMs timestampMs = 0;
};
class IFrameSource {
public:
  virtual ~IFrameSource() = default;
  [[nodiscard]] virtual QString name() const = 0;
  [[nodiscard]] virtual std::int64_t frameCount() const = 0;
  virtual bool read(std::int64_t index, DecodedFrame &frame,
                    QString *error) = 0;
};
} // namespace aquatrack::pipeline
