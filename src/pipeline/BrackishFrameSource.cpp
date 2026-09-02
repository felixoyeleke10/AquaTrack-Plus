#include "pipeline/BrackishFrameSource.h"

namespace aquatrack::pipeline {
bool BrackishFrameSource::open(const QString &directory, QString *error) {
  return m_sequence.load(directory, error);
}
bool BrackishFrameSource::read(std::int64_t index, DecodedFrame &frame,
                               QString *error) {
  if (index < 0 || index >= m_sequence.frameCount()) {
    if (error)
      *error = QStringLiteral("Frame index is outside the sequence");
    return false;
  }
  QImage image(m_sequence.imagePath(static_cast<int>(index + 1)));
  if (image.isNull()) {
    if (error)
      *error = QStringLiteral("Could not decode frame %1").arg(index + 1);
    return false;
  }
  frame = {std::move(image), index,
           index * 1000 /
               static_cast<domain::TimestampMs>(m_sequence.frameRate())};
  return true;
}
} // namespace aquatrack::pipeline
