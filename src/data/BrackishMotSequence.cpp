#include "data/BrackishMotSequence.h"

#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStringList>
#include <QTextStream>

#include <utility>

namespace aquatrack {

bool BrackishMotSequence::load(const QString& directory, QString* error)
{
    const QDir sequenceDir(directory);
    const QString infoPath = sequenceDir.filePath(QStringLiteral("seqinfo.ini"));
    const QString groundTruthPath = sequenceDir.filePath(QStringLiteral("gt/gt.txt"));
    if (!QFile::exists(infoPath) || !QFile::exists(groundTruthPath)) {
        if (error)
            *error = QStringLiteral("Select a BrackishMOT sequence containing seqinfo.ini and gt/gt.txt.");
        return false;
    }
    QSettings settings(infoPath, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Sequence"));
    const QString name = settings.value(QStringLiteral("name"), sequenceDir.dirName()).toString();
    const QString imageDirectory = settings.value(QStringLiteral("imDir"), QStringLiteral("img1")).toString();
    const QString extension = settings.value(QStringLiteral("imExt"), QStringLiteral(".jpg")).toString();
    const int frameRate = settings.value(QStringLiteral("frameRate"), 15).toInt();
    const int frameCount = settings.value(QStringLiteral("seqLength")).toInt();
    const int width = settings.value(QStringLiteral("imWidth")).toInt();
    const int height = settings.value(QStringLiteral("imHeight")).toInt();
    settings.endGroup();
    if (frameCount <= 0 || width <= 0 || height <= 0) {
        if (error)
            *error = QStringLiteral("The sequence metadata contains invalid dimensions or frame count.");
        return false;
    }
    QFile groundTruthFile(groundTruthPath);
    if (!groundTruthFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error)
            *error = groundTruthFile.errorString();
        return false;
    }
    QHash<int, QVector<GroundTruth>> parsed;
    QTextStream stream(&groundTruthFile);
    int lineNumber = 0;
    while (!stream.atEnd()) {
        ++lineNumber;
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty())
            continue;
        const QStringList fields = line.split(QLatin1Char(','));
        if (fields.size() != 9) {
            if (error)
                *error = QStringLiteral("Invalid ground-truth row at line %1.").arg(lineNumber);
            return false;
        }
        bool ok = false;
        const int frame = fields[0].toInt(&ok);
        if (!ok) {
            if (error)
                *error = QStringLiteral("Invalid frame number at line %1.").arg(lineNumber);
            return false;
        }
        GroundTruth annotation;
        annotation.frame = frame;
        annotation.trackId = fields[1].toInt();
        annotation.box = QRectF(fields[2].toDouble(), fields[3].toDouble(),
                                fields[4].toDouble(), fields[5].toDouble());
        annotation.confidence = fields[6].toFloat();
        annotation.classId = fields[7].toInt();
        annotation.visibility = fields[8].toFloat();
        if (annotation.classId < 1 || annotation.classId > 6) {
            if (error)
                *error = QStringLiteral("Unknown class ID at line %1.").arg(lineNumber);
            return false;
        }
        parsed[frame].push_back(annotation);
    }
    m_directory = sequenceDir.absolutePath();
    m_name = name;
    m_imageDirectory = imageDirectory;
    m_extension = extension;
    m_frameRate = qMax(1, frameRate);
    m_frameCount = frameCount;
    m_width = width;
    m_height = height;
    m_annotations = std::move(parsed);
    return true;
}

QString BrackishMotSequence::imagePath(int frame) const
{
    return QDir(m_directory).filePath(
        QStringLiteral("%1/%2%3").arg(m_imageDirectory).arg(frame, 6, 10, QLatin1Char('0')).arg(m_extension));
}

QVector<GroundTruth> BrackishMotSequence::annotations(int frame) const
{
    return m_annotations.value(frame);
}

QString BrackishMotSequence::className(int classId)
{
    static const QStringList names{
        QString(), QStringLiteral("Fish"), QStringLiteral("Crab"),
        QStringLiteral("Shrimp"), QStringLiteral("Starfish"),
        QStringLiteral("Small fish"), QStringLiteral("Jellyfish")};
    return classId >= 1 && classId < names.size() ? names[classId] : QStringLiteral("Unknown");
}

int BrackishMotSequence::classIdForName(const QString& name)
{
    // Case-insensitive: YOLO manifests spell the same classes "crab",
    // "small fish", and should share their colours and filters.
    for (int classId = 1; classId <= 6; ++classId)
        if (className(classId).compare(name.trimmed(), Qt::CaseInsensitive) == 0)
            return classId;
    return 0;
}

}  // namespace aquatrack
