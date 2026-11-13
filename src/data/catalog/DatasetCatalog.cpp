#include "data/catalog/DatasetCatalog.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTextStream>

#include <algorithm>

namespace aquatrack::data {
namespace {

const QStringList kImageFilters{QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
                                QStringLiteral("*.png"), QStringLiteral("*.bmp")};

QString stableId(const QString& path)
{
    const auto normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toLower().toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(normalized, QCryptographicHash::Sha256).toHex().left(16));
}

// Synthetic datasets announce themselves by name: BrackishMOT's simulated
// release ships as brackishMOTSynth, brackishMOTSynth_B, ... A folder on the
// way to the dataset, or the dataset itself, carrying "synth" marks it.
bool looksSynthetic(const QString& path, const QString& name)
{
    if (name.contains(QLatin1String("synth"), Qt::CaseInsensitive))
        return true;
    const auto segments = QDir::fromNativeSeparators(path).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return std::any_of(segments.cbegin(), segments.cend(), [](const QString& segment) {
        return segment.contains(QLatin1String("synth"), Qt::CaseInsensitive);
    });
}

// Counts matching files; `first` receives the alphabetically first one,
// which for numbered frames is frame 1.
qint64 countFiles(const QString& directory, const QStringList& filters, QString* first = nullptr)
{
    const QDir dir(directory);
    if (!dir.exists())
        return 0;
    const auto names = dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
    if (first && !names.isEmpty())
        *first = dir.filePath(names.first());
    return names.size();
}

qint64 countLines(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;
    qint64 lines = 0;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        if (!stream.readLine().trimmed().isEmpty())
            ++lines;
    }
    return lines;
}

// A deliberately small YAML reader. Ultralytics dataset manifests only ever
// use a flat `key: value` shape plus an inline or block `names` list, and
// pulling in a YAML library to read five keys is not a trade worth making.
struct YoloManifest {
    QString base;
    QStringList classNames;
    QHash<QString, QString> splitPaths;  // train/val/test -> declared path
};

YoloManifest readYoloManifest(const QString& path)
{
    YoloManifest manifest;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return manifest;

    QTextStream stream(&file);
    bool inNamesBlock = false;
    while (!stream.atEnd()) {
        const QString raw = stream.readLine();
        const QString line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        // Block-style names, either "- fish" or "0: fish".
        if (inNamesBlock) {
            if (!raw.startsWith(QLatin1Char(' ')) && !raw.startsWith(QLatin1Char('\t'))) {
                inNamesBlock = false;
            } else if (line.startsWith(QLatin1Char('-'))) {
                manifest.classNames << line.mid(1).trimmed().remove(QLatin1Char('\''))
                                           .remove(QLatin1Char('"'));
                continue;
            } else if (line.contains(QLatin1Char(':'))) {
                manifest.classNames << line.section(QLatin1Char(':'), 1).trimmed()
                                           .remove(QLatin1Char('\'')).remove(QLatin1Char('"'));
                continue;
            }
        }

        const int separator = line.indexOf(QLatin1Char(':'));
        if (separator <= 0)
            continue;
        const QString key = line.left(separator).trimmed();
        QString value = line.mid(separator + 1).trimmed();

        if (key == QLatin1String("names")) {
            if (value.startsWith(QLatin1Char('['))) {
                value = value.mid(1, value.lastIndexOf(QLatin1Char(']')) - 1);
                for (const auto& name : value.split(QLatin1Char(','), Qt::SkipEmptyParts))
                    manifest.classNames << name.trimmed().remove(QLatin1Char('\''))
                                               .remove(QLatin1Char('"'));
            } else {
                inNamesBlock = true;
            }
            continue;
        }
        if (key == QLatin1String("path")) {
            manifest.base = value;
        } else if (key == QLatin1String("train") || key == QLatin1String("val")
                   || key == QLatin1String("test")) {
            manifest.splitPaths.insert(key, value);
        }
    }
    return manifest;
}

}  // namespace

CatalogScanResult DatasetCatalog::scan(const QString& rootPath) const
{
    CatalogScanResult result;
    const QFileInfo rootInfo(rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        result.warnings.push_back(QStringLiteral("Dataset root does not exist: %1").arg(rootPath));
        return result;
    }

    // One walk, not three. The tree can hold tens of thousands of frames, and
    // a separate recursive pass per manifest name made a scan take seconds.
    QStringList sequenceManifests;
    QStringList yoloManifests;
    QDirIterator walk(rootInfo.absoluteFilePath(),
                      {QStringLiteral("seqinfo.ini"), QStringLiteral("data.yaml"),
                       QStringLiteral("dataset.yaml")},
                      QDir::Files, QDirIterator::Subdirectories);
    while (walk.hasNext()) {
        const QString path = walk.next();
        if (QFileInfo(path).fileName() == QLatin1String("seqinfo.ini"))
            sequenceManifests.push_back(path);
        else
            yoloManifests.push_back(path);
    }
    sequenceManifests.sort();
    yoloManifests.sort();

    // ---- BrackishMOT sequences -----------------------------------------
    for (const QString& infoPath : sequenceManifests) {
        const QString sequencePath = QFileInfo(infoPath).absolutePath();
        const QString groundTruth = QDir(sequencePath).filePath(QStringLiteral("gt/gt.txt"));
        if (!QFile::exists(groundTruth)) {
            result.warnings.push_back(
                QStringLiteral("Skipped sequence without gt/gt.txt: %1").arg(sequencePath));
            continue;
        }

        QSettings settings(infoPath, QSettings::IniFormat);
        settings.beginGroup(QStringLiteral("Sequence"));
        const int frames = settings.value(QStringLiteral("seqLength"), 0).toInt();
        const QString name =
            settings.value(QStringLiteral("name"), QFileInfo(sequencePath).fileName()).toString();
        const QString imageDirectory =
            settings.value(QStringLiteral("imDir"), QStringLiteral("img1")).toString();
        settings.endGroup();

        if (frames <= 0) {
            result.warnings.push_back(
                QStringLiteral("Skipped sequence with invalid frame count: %1").arg(sequencePath));
            continue;
        }

        DatasetDescriptor descriptor;
        descriptor.id = stableId(sequencePath);
        descriptor.name = name;
        descriptor.rootPath = sequencePath;
        descriptor.format = DatasetFormat::BrackishMot;
        descriptor.sequenceCount = 1;
        descriptor.frameCount = frames;
        descriptor.hasAnnotations = true;
        descriptor.manifestPath = infoPath;
        descriptor.synthetic = looksSynthetic(sequencePath, name);
        descriptor.imageCount = countFiles(QDir(sequencePath).filePath(imageDirectory),
                                           kImageFilters, &descriptor.previewImage);
        descriptor.mediaPresent = descriptor.imageCount > 0;
        descriptor.annotationCount = countLines(groundTruth);

        // Frames declared but not present is the failure that turns into a
        // blank review canvas, so it is worth saying out loud at scan time.
        if (!descriptor.mediaPresent)
            result.warnings.push_back(
                QStringLiteral("%1: no images found in %2").arg(name, imageDirectory));
        else if (descriptor.imageCount < descriptor.frameCount)
            result.warnings.push_back(QStringLiteral("%1: declares %2 frames but %3 images exist")
                                          .arg(name)
                                          .arg(frames)
                                          .arg(descriptor.imageCount));

        result.datasets.push_back(std::move(descriptor));
    }

    // ---- YOLO detection datasets ----------------------------------------
    {
        for (const QString& path : yoloManifests) {
            const QString directory = QFileInfo(path).absolutePath();
            const bool alreadyKnown =
                std::any_of(result.datasets.cbegin(), result.datasets.cend(),
                            [&directory](const auto& item) { return item.rootPath == directory; });
            if (alreadyKnown)
                continue;

            const auto manifest = readYoloManifest(path);
            const QDir base(manifest.base.isEmpty() || manifest.base == QLatin1String(".")
                                ? directory
                                : QDir(directory).filePath(manifest.base));

            DatasetDescriptor descriptor;
            descriptor.id = stableId(directory);
            descriptor.name = QFileInfo(directory).fileName();
            descriptor.rootPath = directory;
            descriptor.format = DatasetFormat::YoloDetection;
            descriptor.manifestPath = path;
            descriptor.classNames = manifest.classNames;
            descriptor.synthetic = looksSynthetic(directory, descriptor.name);

            for (const auto& splitName :
                 {QStringLiteral("train"), QStringLiteral("val"), QStringLiteral("test")}) {
                const auto declared = manifest.splitPaths.value(splitName);
                if (declared.isEmpty())
                    continue;
                DatasetSplit split;
                split.name = splitName;
                split.imagePath = QDir::cleanPath(base.filePath(declared));
                QString first;
                split.imageCount = countFiles(split.imagePath, kImageFilters, &first);
                if (descriptor.previewImage.isEmpty())
                    descriptor.previewImage = first;
                // Ultralytics layouts mirror images/ into labels/.
                QString labelPath = split.imagePath;
                labelPath.replace(QStringLiteral("/images"), QStringLiteral("/labels"));
                split.labelCount = countFiles(labelPath, {QStringLiteral("*.txt")});
                descriptor.imageCount += split.imageCount;
                descriptor.annotationCount += split.labelCount;
                descriptor.splits.push_back(std::move(split));
            }

            descriptor.frameCount = descriptor.imageCount;
            descriptor.sequenceCount = int(descriptor.splits.size());
            descriptor.hasAnnotations = descriptor.annotationCount > 0;
            descriptor.mediaPresent = descriptor.imageCount > 0;

            if (descriptor.splits.isEmpty())
                result.warnings.push_back(
                    QStringLiteral("%1: manifest declares no train/val/test split").arg(path));
            else if (!descriptor.mediaPresent)
                result.warnings.push_back(
                    QStringLiteral("%1: no images found for any declared split").arg(path));

            result.datasets.push_back(std::move(descriptor));
        }
    }

    std::sort(result.datasets.begin(), result.datasets.end(),
              [](const auto& left, const auto& right) { return left.rootPath < right.rootPath; });
    if (result.datasets.isEmpty() && result.warnings.isEmpty())
        result.warnings.push_back(
            QStringLiteral("No supported datasets found under %1").arg(rootInfo.absoluteFilePath()));
    return result;
}

QString datasetFormatName(DatasetFormat format)
{
    return format == DatasetFormat::BrackishMot ? QStringLiteral("BrackishMOT")
                                                : QStringLiteral("YOLO detection");
}

}  // namespace aquatrack::data
