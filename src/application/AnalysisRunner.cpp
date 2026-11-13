#include "application/AnalysisRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QProcessEnvironment>
#include <QStandardPaths>

#include <cmath>
#include <set>

namespace aquatrack::application {
namespace {

void setError(QString* error, const QString& message)
{
    if (error)
        *error = message;
}

QString venvPython(const QDir& folder)
{
#ifdef Q_OS_WIN
    const QString candidate = folder.filePath(QStringLiteral(".venv/Scripts/python.exe"));
#else
    const QString candidate = folder.filePath(QStringLiteral(".venv/bin/python"));
#endif
    return QFileInfo(candidate).isFile() ? QDir::cleanPath(candidate) : QString();
}

QString packageIn(const QDir& folder)
{
    const QString source = folder.filePath(QStringLiteral("src"));
    return QFileInfo::exists(QDir(source).filePath(QStringLiteral("aquatrack/analyze.py")))
               ? QDir::cleanPath(source)
               : QString();
}

// The app folder and its parents (a build folder sits inside the checkout),
// then the source tree the app was built from.
QStringList searchRoots()
{
    QStringList roots;
    if (QCoreApplication::instance()) {
        QDir folder(QCoreApplication::applicationDirPath());
        for (int depth = 0; depth < 5; ++depth) {
            roots << folder.absolutePath();
            if (!folder.cdUp())
                break;
        }
    }
#ifdef AQUATRACK_SOURCE_DIR
    roots << QStringLiteral(AQUATRACK_SOURCE_DIR);
#endif
    roots.removeDuplicates();
    return roots;
}

}  // namespace

AnalysisRunner::AnalysisRunner(QObject* parent) : QObject(parent) {}

AnalysisRunner::~AnalysisRunner()
{
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool AnalysisRunner::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

bool AnalysisRunner::locatePython(const QString& configured, QString* python, QString* packageRoot,
                                  QString* error)
{
    QString interpreter;
    QString package;
    for (const auto& root : searchRoots()) {
        const QDir folder(root);
        if (package.isEmpty())
            package = packageIn(folder);
        if (interpreter.isEmpty())
            interpreter = venvPython(folder);
    }
    if (!configured.trimmed().isEmpty()) {
        if (!QFileInfo(configured).isFile()) {
            setError(error, QStringLiteral("The Python interpreter set in Settings does not exist: %1")
                                .arg(QDir::toNativeSeparators(configured)));
            return false;
        }
        interpreter = QDir::cleanPath(configured);
    }
    if (interpreter.isEmpty())
        interpreter = QStandardPaths::findExecutable(QStringLiteral("python"));
    if (interpreter.isEmpty()) {
        setError(error, QStringLiteral(
                            "No Python environment was found for detection. Create the project's "
                            ".venv (see README) or set the interpreter in Settings."));
        return false;
    }
    if (python)
        *python = interpreter;
    if (packageRoot)
        *packageRoot = package;  // empty: the interpreter must have aquatrack installed
    return true;
}

void AnalysisRunner::start(const AnalysisRequest& request)
{
    if (isRunning())
        return;
    delete m_process;
    m_process = new QProcess(this);
    m_buffer.clear();
    m_stderrTail.clear();
    m_reportedError.clear();
    m_stage.clear();
    m_cancelled = false;
    m_output = request.output;
    QFile::remove(m_output);

    auto environment = QProcessEnvironment::systemEnvironment();
    if (!request.packageRoot.isEmpty()) {
        const QString existing = environment.value(QStringLiteral("PYTHONPATH"));
        environment.insert(QStringLiteral("PYTHONPATH"),
                           existing.isEmpty() ? QDir::toNativeSeparators(request.packageRoot)
                                              : QDir::toNativeSeparators(request.packageRoot) +
                                                    QDir::listSeparator() + existing);
    }
    environment.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
    environment.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    // Ultralytics must not try to pip-install packages from inside the app.
    environment.insert(QStringLiteral("YOLO_AUTOINSTALL"), QStringLiteral("False"));
    environment.insert(QStringLiteral("YOLO_OFFLINE"), QStringLiteral("True"));
    m_process->setProcessEnvironment(environment);
    if (!request.packageRoot.isEmpty())
        m_process->setWorkingDirectory(QFileInfo(request.packageRoot).absolutePath());

    QStringList arguments{QStringLiteral("-u"), QStringLiteral("-m"), QStringLiteral("aquatrack.analyze"),
                          QStringLiteral("--source"), request.source,
                          QStringLiteral("--weights"), request.weights,
                          QStringLiteral("--backend"), request.backend,
                          QStringLiteral("--model-name"), request.modelName,
                          QStringLiteral("--output"), request.output,
                          QStringLiteral("--confidence"), QString::number(request.confidence),
                          QStringLiteral("--iou"), QString::number(request.iou),
                          QStringLiteral("--imgsz"), QString::number(request.imageSize),
                          QStringLiteral("--tracker"), request.tracker,
                          QStringLiteral("--match-iou"), QString::number(request.matchIou),
                          QStringLiteral("--min-hits"), QString::number(request.minimumHits),
                          QStringLiteral("--max-missed"), QString::number(request.maximumMissed)};
    if (!request.classNames.isEmpty())
        arguments << QStringLiteral("--class-names") << request.classNames;

    connect(m_process, &QProcess::readyReadStandardOutput, this, &AnalysisRunner::readOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, [this] {
        const auto lines = QString::fromUtf8(m_process->readAllStandardError())
                               .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const auto& line : lines) {
            const QString trimmed = line.trimmed();
            if (!trimmed.isEmpty() && !trimmed.startsWith(QLatin1String("WARNING")))
                m_stderrTail << trimmed;
        }
        while (m_stderrTail.size() > 12)
            m_stderrTail.removeFirst();
    });
    connect(m_process, &QProcess::finished, this, &AnalysisRunner::processFinished);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError failure) {
        if (failure == QProcess::FailedToStart)
            emit failed(QStringLiteral("Could not start Python: %1").arg(m_process->errorString()));
    });
    emit progress(0, 0, QStringLiteral("Starting Python..."));
    m_process->start(request.python, arguments, QIODevice::ReadOnly);
}

void AnalysisRunner::cancel()
{
    if (!isRunning())
        return;
    m_cancelled = true;
    m_process->kill();
}

void AnalysisRunner::readOutput()
{
    m_buffer += m_process->readAllStandardOutput();
    int newline = -1;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(newline).trimmed();
        m_buffer.remove(0, newline + 1);
        if (!line.startsWith('{'))
            continue;  // library chatter
        const auto object = QJsonDocument::fromJson(line).object();
        const QString event = object.value(QStringLiteral("event")).toString();
        if (event == QLatin1String("start")) {
            m_stage = QStringLiteral("Loading %1 on %2")
                          .arg(object.value(QStringLiteral("model")).toString(),
                               object.value(QStringLiteral("device")).toString().toUpper());
            emit progress(0, 0, m_stage);
        } else if (event == QLatin1String("stage")) {
            m_stage = object.value(QStringLiteral("text")).toString();
            emit progress(0, 0, m_stage + QStringLiteral("..."));
        } else if (event == QLatin1String("progress")) {
            const int done = object.value(QStringLiteral("done")).toInt();
            const int total = object.value(QStringLiteral("total")).toInt();
            const double fps = object.value(QStringLiteral("fps")).toDouble();
            QString text = QStringLiteral("Frame %1 of %2")
                               .arg(QLocale::system().toString(done), QLocale::system().toString(total));
            if (fps > 0.0 && total > done)
                text += QStringLiteral("  ·  %1 fps  ·  ~%2 s left")
                            .arg(fps, 0, 'f', 0)
                            .arg(int(std::ceil((total - done) / fps)));
            emit progress(done, total, text);
        } else if (event == QLatin1String("error")) {
            m_reportedError = object.value(QStringLiteral("message")).toString();
        }
    }
}

void AnalysisRunner::processFinished(int exitCode, QProcess::ExitStatus status)
{
    readOutput();
    if (m_cancelled) {
        emit failed(QStringLiteral("Cancelled"));
        return;
    }
    if (status == QProcess::NormalExit && exitCode == 0 && QFileInfo::exists(m_output)) {
        emit finished(m_output);
        return;
    }
    QString message = m_reportedError;
    if (message.isEmpty() && !m_stderrTail.isEmpty())
        message = m_stderrTail.last();
    if (message.contains(QLatin1String("No module named"))) {
        message += QStringLiteral(
            "\n\nThe Python interpreter is missing a package. Use the project's .venv "
            "(Settings → Python for detection) or install requirements into this one.");
    }
    emit failed(message.isEmpty() ? QStringLiteral("Detection stopped unexpectedly (exit code %1)")
                                        .arg(exitCode)
                                  : message);
}

bool AnalysisRunner::readResults(const QString& path, const media::MediaSource& source,
                                 AnalysisResult& result, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, file.errorString());
        return false;
    }
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parse);
    const auto root = document.object();
    if (parse.error != QJsonParseError::NoError ||
        root.value(QStringLiteral("format")).toString() != QLatin1String("aquatrack-analysis")) {
        setError(error, QStringLiteral("The detector's results could not be read"));
        return false;
    }
    if (root.value(QStringLiteral("frames")).toInt() != source.frameCount()) {
        setError(error, QStringLiteral("The detector saw %1 frames but the media has %2; results "
                                       "were not loaded so boxes cannot land on the wrong frames.")
                            .arg(root.value(QStringLiteral("frames")).toInt())
                            .arg(source.frameCount()));
        return false;
    }

    QStringList classes;
    for (const auto value : root.value(QStringLiteral("classes")).toArray())
        classes << value.toString();
    const std::string videoId = source.name().toStdString();

    AnalysisResult loaded;
    loaded.modelName = root.value(QStringLiteral("model")).toObject().value(QStringLiteral("name")).toString();
    loaded.seconds = root.value(QStringLiteral("elapsedSeconds")).toDouble();
    std::set<qint64> tracks;
    std::set<std::string> ids;
    const auto rows = root.value(QStringLiteral("observations")).toArray();
    loaded.observations.reserve(std::size_t(rows.size()));
    for (const auto value : rows) {
        const auto row = value.toArray();
        if (row.size() < 8)
            continue;
        const int frame = row.at(0).toInt();
        const qint64 track = qint64(row.at(1).toDouble());
        const int classIndex = row.at(2).toInt();
        domain::Observation observation;
        observation.id = videoId + ":" + std::to_string(frame) + ":" + std::to_string(track);
        if (!ids.insert(observation.id).second)
            continue;  // one box per track per frame
        observation.videoId = videoId;
        observation.frameIndex = frame;
        observation.timestampMs = source.timestampMs(frame);
        observation.localTrackId = track;
        observation.box = {float(row.at(4).toDouble()), float(row.at(5).toDouble()),
                           float(row.at(6).toDouble()), float(row.at(7).toDouble())};
        observation.className =
            (classIndex >= 0 && classIndex < classes.size() ? classes.at(classIndex)
                                                            : QStringLiteral("Fish"))
                .toStdString();
        observation.confidence = float(row.at(3).toDouble());
        observation.humanVerified = false;  // model output until a reviewer confirms it
        if (frame < 1 || frame > source.frameCount() || !domain::isValid(observation))
            continue;
        tracks.insert(track);
        loaded.observations.push_back(std::move(observation));
    }
    loaded.tracks = int(tracks.size());
    result = std::move(loaded);
    return true;
}

}  // namespace aquatrack::application
