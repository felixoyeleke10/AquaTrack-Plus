#include "ui/MainWindow.h"
#include "ui/AppStyle.h"
#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("AquaTrack"));
    application.setOrganizationName(QStringLiteral("AquaTrack"));
    application.setStyle(new aquatrack::ui::AppStyle(QStyleFactory::create(QStringLiteral("Fusion"))));
    // Painted rather than shipped as an asset, so the window, taskbar, and
    // in-app brand mark can never drift apart.
    application.setWindowIcon(QIcon(aquatrack::ui::glyphPixmap(
        aquatrack::ui::Glyph::Drop, 64, aquatrack::ui::palette::Accent, 2.4)));
    application.setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("AquaTrack+ underwater video review workstation"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption sequenceOption(
        QStringList{QStringLiteral("s"), QStringLiteral("sequence")},
        QStringLiteral("Open a BrackishMOT sequence directory on start-up."),
        QStringLiteral("directory"));
    parser.addOption(sequenceOption);
    const QCommandLineOption registryOption(
        QStringList{QStringLiteral("r"), QStringLiteral("registry")},
        QStringLiteral("Scan a model registry directory on start-up."),
        QStringLiteral("directory"));
    parser.addOption(registryOption);
    const QCommandLineOption datasetsOption(
        QStringList{QStringLiteral("d"), QStringLiteral("datasets")},
        QStringLiteral("Scan a dataset root on start-up."),
        QStringLiteral("directory"));
    parser.addOption(datasetsOption);
    const QCommandLineOption projectOption(
        QStringList{QStringLiteral("p"), QStringLiteral("project")},
        QStringLiteral("Open an existing .aqp project on start-up."),
        QStringLiteral("file"));
    parser.addOption(projectOption);
    parser.process(application);

    MainWindow window;
    window.show();
    if (parser.isSet(projectOption))
        window.openProject(parser.value(projectOption));
    if (parser.isSet(sequenceOption))
        window.openSequence(parser.value(sequenceOption));
    if (parser.isSet(registryOption))
        window.openRegistry(parser.value(registryOption));
    if (parser.isSet(datasetsOption))
        window.openDatasets(parser.value(datasetsOption));
    return application.exec();
}
