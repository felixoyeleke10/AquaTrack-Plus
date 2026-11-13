#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace aquatrack::application {

// A trained model found on disk, described well enough to register it: what
// it is, how it was trained, and how well it scored.
struct ModelCandidate {
    QString weights;        // absolute path of the weights file
    QString runName;        // training run folder, or the file's base name
    QString id;             // registry id derived from the run name
    QString name;           // display name, e.g. "RT-DETR-L (rtdetr_l_converged)"
    QString type = QStringLiteral("detector");
    QString backend;        // ultralytics, torchvision, pytorch, onnxruntime...
    int inputSize = 640;
    QStringList classes;
    QString dataset;        // dataset manifest the run trained on, if known
    QMap<QString, double> metrics;  // precision, recall, map50, map50_95
    qint64 bytes = 0;
    bool pilot = false;     // smoke tests and pilots: listed, not preselected

    // The figure to rank by: mAP@50 when measured.
    [[nodiscard]] double headline() const { return metrics.value(QStringLiteral("map50"), -1.0); }
};

// Turns training output into registry entries. Understands the layout the
// project's training tools write (Ultralytics and torchvision runs):
//
//   <runs>/<run>/args.yaml            task, model, imgsz, data
//   <runs>/<run>/weights/best.pt      the checkpoint to register
//   <runs>/metrics/<run>.json         benchmark scores, if benchmarked
//
// and falls back to sensible defaults for a lone weights file.
class ModelImporter final {
public:
    // Every run under `root` with a weights/best.* checkpoint, benchmarked
    // runs first, best mAP first.
    [[nodiscard]] static QVector<ModelCandidate> discover(const QString& root);

    // Describes one weights file, reading its run's metadata when present.
    [[nodiscard]] static ModelCandidate describe(const QString& weights);

    // Copies the weights into `modelsFolder/<id>/` and writes model.json
    // beside them. The id gets a numeric suffix if already taken.
    static bool import(const ModelCandidate& candidate, const QString& modelsFolder,
                       QString* manifestPath = nullptr, QString* error = nullptr);
};

}  // namespace aquatrack::application
