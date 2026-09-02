#pragma once

#include "data/BrackishMotSequence.h"
#include "pipeline/IFrameSource.h"

namespace aquatrack::pipeline {
class BrackishFrameSource final : public IFrameSource {
public:
  bool open(const QString &directory, QString *error = nullptr);
  [[nodiscard]] QString name() const override { return m_sequence.name(); }
  [[nodiscard]] std::int64_t frameCount() const override {
    return m_sequence.frameCount();
  }
  bool read(std::int64_t index, DecodedFrame &frame, QString *error) override;

private:
  BrackishMotSequence m_sequence;
};
} // namespace aquatrack::pipeline
