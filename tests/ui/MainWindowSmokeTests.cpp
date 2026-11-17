#include "TestSupport.h"
#include "ui/MainWindow.h"
#include "ui/datasets/DatasetsPage.h"
#include "ui/navigation/NavigationDock.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLayout>
#include <QPushButton>
#include <QScreen>
#include <QStackedWidget>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);
  MainWindow window;
  requireTest(window.windowTitle() == QStringLiteral("AquaTrack+"),
              "window title");
  auto *pages = window.findChild<QStackedWidget *>();
  requireTest(pages != nullptr, "page stack exists");
  requireTest(pages->count() == 8, "functional page count");
  requireTest(window.windowFlags().testFlag(Qt::FramelessWindowHint),
              "main window is frameless");
  for (const char *name : {"captionMinimize", "captionMaximize", "captionClose"})
    requireTest(window.findChild<QPushButton *>(QString::fromLatin1(name)) != nullptr,
                "caption button present");
  // Review can switch detectors without going to the Models page.
  auto *picker = window.findChild<QPushButton *>(QStringLiteral("detectorPicker"));
  requireTest(picker != nullptr && picker->menu() != nullptr, "detector picker beside Detect & track");
  auto *dock = window.findChild<NavigationDock *>();
  requireTest(dock != nullptr, "navigation dock exists");
  // The dock runs in workflow order and ends on Export; Settings is a
  // utility in the title bar, not a workflow step.
  const char *workflow[] = {"navDashboard", "navVideos", "navModels",
                            "navReview",    "navAnalytics", "navExport"};
  int previousSlot = -1;
  for (const char *name : workflow) {
    auto *button = dock->findChild<QPushButton *>(QString::fromLatin1(name));
    requireTest(button != nullptr, "every workflow step is in the dock");
    // Slot in the dock's row: left to right is the order the user sees.
    const int slot = button->parentWidget()->layout()->indexOf(button);
    requireTest(slot > previousSlot, "dock follows the workflow order");
    previousSlot = slot;
  }
  requireTest(dock->findChild<QPushButton *>(QStringLiteral("navSettings")) == nullptr,
              "settings is not a dock step");
  // Training data is part of "Models & data", not a step of every survey.
  requireTest(dock->findChild<QPushButton *>(QStringLiteral("navDatasets")) == nullptr,
              "datasets is reached through Models, not the dock");
  requireTest(window.findChild<QPushButton *>(QStringLiteral("settingsButton")) != nullptr,
              "settings sits in the title bar");
  requireTest(window.findChild<QPushButton *>(QStringLiteral("projectChip")) != nullptr,
              "title bar shows the project");
  const QString screenshot = qEnvironmentVariable("AQUATRACK_UI_SCREENSHOT");
  if (!screenshot.isEmpty()) {
    bool pageOk = false;
    const int requestedPage =
        qEnvironmentVariableIntValue("AQUATRACK_UI_PAGE", &pageOk);
    // Optionally load a sequence so pages are shown with real data, then
    // navigate the way a click on the dock does (page refreshes included).
    const QString sequence = qEnvironmentVariable("AQUATRACK_UI_SEQUENCE");
    if (!sequence.isEmpty())
      window.openSequence(sequence);
    // Or scan a dataset root, optionally in grid view.
    const QString datasets = qEnvironmentVariable("AQUATRACK_UI_DATASETS");
    if (!datasets.isEmpty())
      window.openDatasets(datasets);
    if (pageOk)
      emit dock->pageRequested(requestedPage);
    auto *datasetsPage = window.findChild<DatasetsPage *>();
    if (qEnvironmentVariableIsSet("AQUATRACK_UI_GRID") && datasetsPage)
      datasetsPage->setGridView(true);
    // Optional WIDTHxHEIGHT, to check layouts at the user's window size.
    const QStringList size = qEnvironmentVariable("AQUATRACK_UI_SIZE").split(QLatin1Char('x'));
    if (size.size() == 2)
      window.resize(size[0].toInt(), size[1].toInt());
    window.show();
    QElapsedTimer settle;
    settle.start();
    // Longer when thumbnails are loading.
    const int settleMs = datasets.isEmpty() ? 150 : 2500;
    while (settle.elapsed() < settleMs)
      app.processEvents(QEventLoop::AllEvents, 25);
    window.repaint();
    app.processEvents();
    requireTest(QDir().mkpath(QFileInfo(screenshot).absolutePath()),
                "screenshot directory");
    requireTest(window.grab().save(screenshot), "save UI screenshot");
    // Optionally also capture the inspector on the current dataset.
    const QString inspectShot = qEnvironmentVariable("AQUATRACK_UI_INSPECT");
    if (!inspectShot.isEmpty() && datasetsPage) {
      datasetsPage->inspectCurrent();
      auto *inspector = window.findChild<QDialog *>(QStringLiteral("datasetInspector"));
      requireTest(inspector != nullptr, "inspector opens");
      QElapsedTimer wait;
      wait.start();
      while (wait.elapsed() < 400)
        app.processEvents(QEventLoop::AllEvents, 25);
      requireTest(inspector->grab().save(inspectShot), "save inspector screenshot");
    }
    return 0;
  }

  // A maximized frameless window must stop at the taskbar, not cover it.
  window.show();
  window.showMaximized();
  QElapsedTimer settle;
  settle.start();
  while (settle.elapsed() < 400)
    app.processEvents(QEventLoop::AllEvents, 25);
  requireTest(window.isMaximized(), "window maximizes");
  const QRect available = window.screen()->availableGeometry();
  requireTest(available.contains(window.geometry()),
              "maximized frameless window stays inside the available screen area");
  window.showNormal();
  return 0;
}
