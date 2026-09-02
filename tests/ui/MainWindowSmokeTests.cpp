#include "TestSupport.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QStackedWidget>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  MainWindow window;
  requireTest(window.windowTitle() == QStringLiteral("AquaTrack+"),
              "window title");
  auto *pages = window.findChild<QStackedWidget *>();
  requireTest(pages != nullptr, "page stack exists");
  requireTest(pages->count() == 7, "functional page count");
  const QString screenshot = qEnvironmentVariable("AQUATRACK_UI_SCREENSHOT");
  if (!screenshot.isEmpty()) {
    bool pageOk = false;
    const int requestedPage =
        qEnvironmentVariableIntValue("AQUATRACK_UI_PAGE", &pageOk);
    if (pageOk)
      pages->setCurrentIndex(requestedPage);
    window.show();
    QElapsedTimer settle;
    settle.start();
    while (settle.elapsed() < 150)
      app.processEvents(QEventLoop::AllEvents, 25);
    window.repaint();
    app.processEvents();
    requireTest(QDir().mkpath(QFileInfo(screenshot).absolutePath()),
                "screenshot directory");
    requireTest(window.grab().save(screenshot), "save UI screenshot");
  }
  return 0;
}
