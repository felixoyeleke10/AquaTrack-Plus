#include "ui/MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("AquaTrack"));
    application.setOrganizationName(QStringLiteral("AquaTrack"));
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    MainWindow window;
    window.show();
    return application.exec();
}
