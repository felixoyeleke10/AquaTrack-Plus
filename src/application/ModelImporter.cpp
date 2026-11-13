#include "application/ModelImporter.h"

#include "models/ModelManifest.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTextStream>

#include <algorithm>

namespace aquatrack::application {
namespace {

const QStringList kWeightSuffixes{QStringLiteral("pt"), QStringLiteral("pth"),
                                  QStringLiteral("onnx"), QStringLiteral("engine"),
                                  QStringLiteral("torchscript")};

// Flat "key: value" YAML, as Ultralytics writes args.yaml.
QHash<QString, QString> readFlatYaml(const QString& path)
{
    QHash<QString, QString> values;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return values;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        if (line.startsWith(QLatin1Char(' ')) || line.startsWith(QLatin1Char('#')))
            continue;
        const int colon = line.indexOf(QLatin1Char(':'));
        if (colon > 0)
            values.insert(line.left(colon).trimmed(), line.mid(colon + 1).trimmed());
    }
    return values;
}

// Class names from a YOLO dataset manifest: "names: [a, b]", or a block of
// "- a" / "0: a" lines.
QStringList readClassNames(const QString& path)
{
    QStringList names;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return names;
    QTextStream stream(&file);
    bool inBlock = false;
    const auto clean = [](QString value) {
        return value.remove(QLatin1Char('\'')).remove(QLatin1Char('"')).trimmed();
    };
    while (!stream.atEnd()) {
        const QString raw = stream.readLine();
        const QString line = raw.trimmed();
        if (inBlock) {
            if (!raw.startsWith(QLatin1Char(' ')) && !raw.startsWith(QLatin1Char('\t'))) {
                inBlock = false;
            } else {
                if (line.startsWith(QLatin1Char('-')))
                    names << clean(line.mid(1));
                else if (line.contains(QLatin1Char(':')))
                    names << clean(line.section(QLatin1Char(':'), 1));
                continue;
            }
        }
        if (line.startsWith(QLatin1String("names:"))) {
            const QString value = line.mid(6).trimmed();
            if (value.startsWith(QLatin1Char('['))) {
                for (const auto& name : value.mid(1, value.lastIndexOf(QLatin1Char(']')) - 1)
                                            .split(QLatin1Char(','), Qt::SkipEmptyParts))
                    names << clean(name);
            } else {
                inBlock = true;
            }
        }
    }
    return names;
}

QString registryId(QString name)
{
    name = name.trimmed().toLower();
    for (QChar& character : name)
        if (!character.isLetterOrNumber() && character != QLatin1Char('-'))
            character = QLatin1Char('_');
    return name.isEmpty() ? QStringLiteral("model") : name;
}

// "yolo11n.pt" -> "YOLO11n", "rtdetr-l.pt" -> "RT-DETR-L"
QString architectureFromBaseModel(const QString& model)
{
    QString base = QFileInfo(model).completeBaseName();
    if (base.startsWith(QLatin1String("yolo"), Qt::CaseInsensitive))
        return QStringLiteral("YOLO") + base.mid(4);
    if (base.startsWith(QLatin1String("rtdetr"), Qt::CaseInsensitive))
        return QStringLiteral("RT-DETR") + base.mid(6).toUpper();
    return base;
}

}  // namespace

ModelCandidate ModelImporter::describe(const QString& weights)
{
    ModelCandidate candidate;
    const QFileInfo file(weights);
    candidate.weights = QDir::cleanPath(file.absoluteFilePath());
    candidate.bytes = file.size();

    // A checkpoint in "<run>/weights/" belongs to a training run, but only if
    // the run left its record there: a plain "weights" folder (for example a
    // repository's weights/yolo26n.pt) must not be named after its parent.
    QDir runDir = file.absoluteDir();
    const bool inRun = runDir.dirName().compare(QLatin1String("weights"), Qt::CaseInsensitive) == 0
                       && runDir.cdUp()
                       && (runDir.exists(QStringLiteral("args.yaml"))
                           || runDir.exists(QStringLiteral("results.csv")));
    if (!inRun)
        runDir = file.absoluteDir();
    candidate.runName = inRun ? runDir.dirName() : file.completeBaseName();
    candidate.id = registryId(candidate.runName);

    const auto args = inRun ? readFlatYaml(runDir.filePath(QStringLiteral("args.yaml")))
                            : QHash<QString, QString>{};
    if (args.contains(QStringLiteral("imgsz")))
        candidate.inputSize = std::max(32, args.value(QStringLiteral("imgsz")).toInt());
    const QString task = args.value(QStringLiteral("task"));
    if (!task.isEmpty() && task != QLatin1String("detect"))
        candidate.type = task;  // the registry rejects types it does not know
    candidate.dataset = args.value(QStringLiteral("data"));
    if (!candidate.dataset.isEmpty())
        candidate.classes = readClassNames(candidate.dataset);

    // Benchmark scores: "<runs>/metrics/<run>.json", as the benchmark tools
    // write them.
    QString architecture = args.contains(QStringLiteral("model"))
                               ? architectureFromBaseModel(args.value(QStringLiteral("model")))
                               : QString();
    QString framework = args.isEmpty() ? QString() : QStringLiteral("ultralytics");
    if (inRun) {
        QDir runs = runDir;
        runs.cdUp();
        // "<runs>/metrics/<run>.json", or a benchmark folder next to it
        // (runs/fish_detector_benchmark/metrics/ scores runs/detect/ runs).
        const QString metricsName = QStringLiteral("metrics/%1.json").arg(candidate.runName);
        QString metricsPath = runs.filePath(metricsName);
        QDir siblings = runs;
        if (!QFileInfo::exists(metricsPath) && siblings.cdUp()) {
            for (const auto& folder : siblings.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                const QString candidatePath = QDir(siblings.filePath(folder)).filePath(metricsName);
                if (QFileInfo::exists(candidatePath)) {
                    metricsPath = candidatePath;
                    break;
                }
            }
        }
        QFile metricsFile(metricsPath);
        if (metricsFile.open(QIODevice::ReadOnly)) {
            const auto object = QJsonDocument::fromJson(metricsFile.readAll()).object();
            for (const char* key : {"precision", "recall", "map50", "map50_95"})
                if (object.contains(QLatin1String(key)))
                    candidate.metrics.insert(QLatin1String(key),
                                             object.value(QLatin1String(key)).toDouble());
            if (object.contains(QStringLiteral("model")))
                architecture = object.value(QStringLiteral("model")).toString();
            if (object.contains(QStringLiteral("framework")))
                framework = object.value(QStringLiteral("framework")).toString();
            candidate.pilot = object.value(QStringLiteral("pilot")).toBool();
        }
    }
    const QString lowerRun = candidate.runName.toLower();
    candidate.pilot = candidate.pilot || lowerRun.contains(QLatin1String("smoke"))
                      || lowerRun.contains(QLatin1String("pilot"));

    if (framework.contains(QLatin1String("ultralytics")))
        candidate.backend = QStringLiteral("ultralytics");
    else if (framework.contains(QLatin1String("torchvision")))
        candidate.backend = QStringLiteral("torchvision");
    else if (file.suffix().compare(QLatin1String("onnx"), Qt::CaseInsensitive) == 0)
        candidate.backend = QStringLiteral("onnxruntime");
    else if (file.suffix().compare(QLatin1String("engine"), Qt::CaseInsensitive) == 0)
        candidate.backend = QStringLiteral("tensorrt");
    else
        candidate.backend = QStringLiteral("pytorch");

    if (!inRun) {
        // A loose checkpoint: its file name is all there is ("yolo26n.pt").
        architecture = architectureFromBaseModel(file.fileName());
        candidate.name = architecture;
    } else {
        candidate.name = architecture.isEmpty()
                             ? candidate.runName
                             : QStringLiteral("%1 (%2)").arg(architecture, candidate.runName);
    }
    return candidate;
}

QVector<ModelCandidate> ModelImporter::discover(const QString& root)
{
    QVector<ModelCandidate> found;
    QDirIterator walk(root, QDir::Files, QDirIterator::Subdirectories);
    while (walk.hasNext()) {
        const QString path = walk.next();
        const QFileInfo info = walk.fileInfo();
        // One checkpoint per run: the best one.
        if (info.completeBaseName() != QLatin1String("best")
            || !kWeightSuffixes.contains(info.suffix().toLower())
            || info.absoluteDir().dirName().compare(QLatin1String("weights"), Qt::CaseInsensitive) != 0)
            continue;
        found.push_back(describe(path));
    }
    std::sort(found.begin(), found.end(), [](const ModelCandidate& left, const ModelCandidate& right) {
        if (left.pilot != right.pilot)
            return !left.pilot;
        if (left.headline() != right.headline())
            return left.headline() > right.headline();
        return left.runName < right.runName;
    });
    return found;
}

bool ModelImporter::import(const ModelCandidate& candidate, const QString& modelsFolder,
                           QString* manifestPath, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };
    QString id = candidate.id;
    QString folder = QDir(modelsFolder).filePath(id);
    for (int index = 2; QFileInfo::exists(folder); ++index) {
        id = QStringLiteral("%1-%2").arg(candidate.id).arg(index);
        folder = QDir(modelsFolder).filePath(id);
    }
    if (!QDir().mkpath(folder))
        return fail(QStringLiteral("Could not create %1").arg(QDir::toNativeSeparators(folder)));

    const QString artifact = QFileInfo(candidate.weights).fileName();
    if (!QFile::copy(candidate.weights, QDir(folder).filePath(artifact))) {
        QDir(folder).removeRecursively();
        return fail(QStringLiteral("Could not copy %1").arg(QDir::toNativeSeparators(candidate.weights)));
    }

    // Same rules the registry applies when it reads the file back.
    models::ModelManifest manifest;
    manifest.id = id.toStdString();
    manifest.displayName = candidate.name.toStdString();
    manifest.type = models::modelTypeFromString(candidate.type.toStdString());
    manifest.backend = candidate.backend.toStdString();
    manifest.precision = "fp32";
    manifest.inputWidth = manifest.inputHeight = candidate.inputSize;
    manifest.artifactPath = artifact.toStdString();
    std::string reason;
    if (!models::isValid(manifest, &reason)) {
        QDir(folder).removeRecursively();
        return fail(QStringLiteral("%1: %2").arg(candidate.runName, QString::fromStdString(reason)));
    }

    QJsonArray classes;
    for (const auto& name : candidate.classes)
        classes.append(name);
    QJsonObject metrics;
    for (auto it = candidate.metrics.cbegin(); it != candidate.metrics.cend(); ++it)
        metrics.insert(it.key(), it.value());
    const QJsonObject object{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), candidate.name},
        {QStringLiteral("type"), candidate.type},
        {QStringLiteral("backend"), candidate.backend},
        {QStringLiteral("precision"), QStringLiteral("fp32")},
        {QStringLiteral("input"), QJsonArray{candidate.inputSize, candidate.inputSize}},
        {QStringLiteral("classes"), classes},
        {QStringLiteral("metrics"), metrics},
        {QStringLiteral("artifact"), artifact},
        // Provenance, for whoever opens the folder later.
        {QStringLiteral("source"), QDir::toNativeSeparators(candidate.weights)},
        {QStringLiteral("trainedOn"), QDir::toNativeSeparators(candidate.dataset)},
    };
    const QString path = QDir(folder).filePath(QStringLiteral("model.json"));
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(object).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        QDir(folder).removeRecursively();
        return fail(QStringLiteral("Could not write %1").arg(QDir::toNativeSeparators(path)));
    }
    if (manifestPath)
        *manifestPath = path;
    return true;
}

}  // namespace aquatrack::application
