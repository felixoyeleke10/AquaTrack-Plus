#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace aquatrack::data {

enum class DatasetFormat { BrackishMot, YoloDetection };

struct DatasetSplit {
    QString name;       // train / val / test
    QString imagePath;
    qint64 imageCount = 0;
    qint64 labelCount = 0;
};

struct DatasetDescriptor {
    QString id;
    QString name;
    QString rootPath;
    DatasetFormat format = DatasetFormat::BrackishMot;
    int sequenceCount = 0;
    qint64 frameCount = 0;
    bool hasAnnotations = false;

    // Whether the imagery the manifest promises is actually on disk. A
    // sequence whose frames were never copied still parses perfectly.
    bool mediaPresent = false;
    qint64 imageCount = 0;
    qint64 annotationCount = 0;
    QStringList classNames;
    QVector<DatasetSplit> splits;
    QString manifestPath;

    [[nodiscard]] bool isReviewable() const
    {
        return format == DatasetFormat::BrackishMot && mediaPresent && hasAnnotations;
    }
};

struct CatalogScanResult {
    QVector<DatasetDescriptor> datasets;
    QStringList warnings;
};

class DatasetCatalog final {
public:
    [[nodiscard]] CatalogScanResult scan(const QString& rootPath) const;
};

[[nodiscard]] QString datasetFormatName(DatasetFormat format);

}  // namespace aquatrack::data
