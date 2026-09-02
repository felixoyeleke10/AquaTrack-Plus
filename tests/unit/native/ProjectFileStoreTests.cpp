#include "data/ProjectFileStore.h"
#include "TestSupport.h"

#include <QCoreApplication>
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
    requireTest(actual.mediaPaths == expected.mediaPaths, "media paths round trip");
    return 0;
}
