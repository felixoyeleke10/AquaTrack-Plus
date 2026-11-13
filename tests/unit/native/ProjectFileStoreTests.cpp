#include "data/ProjectFileStore.h"
#include "TestSupport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    requireTest(directory.isValid(), "temporary project directory");
    aquatrack::domain::ProjectManifest expected{
        .projectId = "project-1", .name = "Holyrood Bay Survey",
        .createdAtMs = 100, .updatedAtMs = 200, .databasePath = "survey.db",
        .mediaPaths = {"media/station_014.mp4", "D:/external/station_015.mp4"},
    };
    const QString path = directory.filePath(QStringLiteral("survey.aqp"));
    aquatrack::data::ProjectFileStore store;
    QString error;
    requireTest(store.save(path, expected, &error), "save project manifest");
    aquatrack::domain::ProjectManifest actual;
    requireTest(store.load(path, actual, &error), "load project manifest");
    requireTest(actual.projectId == expected.projectId, "project id round trip");
    // In memory paths are absolute; on disk, media inside the project folder
    // is relative so the folder can move.
    requireTest(actual.mediaPaths.size() == 2
                    && QString::fromStdString(actual.mediaPaths[0])
                           == QDir::cleanPath(directory.filePath(QStringLiteral("media/station_014.mp4")))
                    && actual.mediaPaths[1] == "D:/external/station_015.mp4",
                "media paths resolve against the project folder");
    aquatrack::domain::ProjectManifest absolute = actual;
    requireTest(store.save(path, absolute, &error), "re-save with absolute paths");
    QFile written(path);
    requireTest(written.open(QIODevice::ReadOnly), "read manifest back");
    const QByteArray text = written.readAll();
    requireTest(text.contains("\"media/station_014.mp4\"") && !text.contains(directory.path().toUtf8()),
                "media inside the project is stored relative");
    requireTest(text.contains("D:/external/station_015.mp4"), "linked media stays absolute");
    return 0;
}
