#pragma once

#include <QHash>
#include <QRectF>
#include <QString>
#include <QVector>

namespace aquatrack {

struct GroundTruth {
    int frame = 0;
    int trackId = 0;
    QRectF box;
    float confidence = 0.0F;
    int classId = 0;
    float visibility = 0.0F;
};

class BrackishMotSequence {
public:
    [[nodiscard]] bool load(const QString& directory, QString* error = nullptr);
    [[nodiscard]] bool isLoaded() const { return !m_directory.isEmpty(); }
    [[nodiscard]] QString name() const { return m_name; }
    [[nodiscard]] int frameRate() const { return m_frameRate; }
    [[nodiscard]] int frameCount() const { return m_frameCount; }
    [[nodiscard]] int imageWidth() const { return m_width; }
    [[nodiscard]] int imageHeight() const { return m_height; }
    [[nodiscard]] QString imagePath(int frame) const;
    [[nodiscard]] QVector<GroundTruth> annotations(int frame) const;
    [[nodiscard]] static QString className(int classId);
    // Inverse of className; 0 for anything that is not a BrackishMOT class.
    [[nodiscard]] static int classIdForName(const QString& name);

private:
    QString m_directory;
    QString m_name;
    QString m_imageDirectory;
    QString m_extension;
    int m_frameRate = 15;
    int m_frameCount = 0;
    int m_width = 0;
    int m_height = 0;
    QHash<int, QVector<GroundTruth>> m_annotations;
};

}  // namespace aquatrack

