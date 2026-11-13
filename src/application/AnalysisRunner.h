#pragma once

#include "data/media/MediaSource.h"
#include "domain/ScientificTypes.h"

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include <vector>

namespace aquatrack::application {

// Everything the Python analysis needs, resolved by the caller from settings
// and the model registry.
struct AnalysisRequest {
    QString python;        // interpreter
    QString packageRoot;   // folder holding the aquatrack package (added to PYTHONPATH)
    QString source;        // media path, as Review opened it
    QString weights;
    QString backend;       // "ultralytics", "torchvision", ...
    QString modelName;
    QStringList classNames;  // the model's classes, in index order
    double confidence = 0.35;
    double iou = 0.7;
    int imageSize = 640;
    QString tracker = QStringLiteral("bytetrack");  // or "iou"
    double matchIou = 0.3;
    int minimumHits = 3;
    int maximumMissed = 30;
    QString output;        // results JSON to write
};

struct AnalysisResult {
    std::vector<domain::Observation> observations;
    QString modelName;
    int tracks = 0;
    double seconds = 0.0;
};

// Runs `python -m aquatrack.analyze` without blocking the UI, reporting its
// JSON-lines progress, and reads the results it writes.
class AnalysisRunner final : public QObject {
    Q_OBJECT

public:
    explicit AnalysisRunner(QObject* parent = nullptr);
    ~AnalysisRunner() override;

    [[nodiscard]] bool isRunning() const;
    void start(const AnalysisRequest& request);
    void cancel();

    // Finds an interpreter that can import aquatrack: `configured` if set,
    // else a .venv next to (or above) the app or the source tree, else
    // python on PATH. Also reports where the package lives.
    [[nodiscard]] static bool locatePython(const QString& configured, QString* python,
                                           QString* packageRoot, QString* error);
    // Reads a results file into observations of `source`.
    [[nodiscard]] static bool readResults(const QString& path, const media::MediaSource& source,
                                          AnalysisResult& result, QString* error = nullptr);

signals:
    void progress(int done, int total, QString text);
    void finished(QString resultsPath);
    void failed(QString error);

private:
    void readOutput();
    void processFinished(int exitCode, QProcess::ExitStatus status);

    QProcess* m_process = nullptr;
    QByteArray m_buffer;
    QStringList m_stderrTail;
    QString m_output;
    QString m_reportedError;
    QString m_stage;
    bool m_cancelled = false;
};

}  // namespace aquatrack::application
