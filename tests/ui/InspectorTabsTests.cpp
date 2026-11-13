#include "TestSupport.h"

#include "ui/common/InspectorTabs.h"

#include <QApplication>
#include <QLabel>
#include <QStackedWidget>

using aquatrack::ui::Glyph;
using aquatrack::ui::InspectorTabs;

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    InspectorTabs tabs;
    int changes = 0;
    QObject::connect(&tabs, &InspectorTabs::currentPageChanged, [&changes] { ++changes; });

    auto* frame = new QLabel(QStringLiteral("frame"));
    auto* switches = new QLabel(QStringLiteral("switches"));
    auto* confidence = new QLabel(QStringLiteral("confidence"));
    const int framePage = tabs.addPage(Glyph::Annotations, QStringLiteral("In frame"), frame);
    const int switchPage = tabs.addPage(Glyph::Merge, QStringLiteral("ID switches"), switches);
    const int confidencePage = tabs.addPage(Glyph::Analytics, QStringLiteral("Confidence"), confidence);
    auto* stack = tabs.findChild<QStackedWidget*>();

    requireTest(framePage == 0 && switchPage == 1 && confidencePage == 2 && tabs.pageCount() == 3,
                "pages are numbered in the order added");
    requireTest(tabs.currentPage() == framePage && stack && stack->currentWidget() == frame,
                "the first page added is shown");

    tabs.setCurrentPage(confidencePage);
    requireTest(tabs.currentPage() == confidencePage && stack->currentWidget() == confidence,
                "choosing a tab shows its page");

    tabs.setPageEnabled(confidencePage, false);
    requireTest(tabs.currentPage() == framePage && stack->currentWidget() == frame,
                "disabling the open page falls back to the first enabled one");

    tabs.setPageEnabled(framePage, false);
    requireTest(tabs.currentPage() == switchPage, "the fallback skips disabled pages");

    tabs.setPageEnabled(framePage, true);
    tabs.setPageCount(switchPage, 10);
    tabs.setCurrentPage(-1);
    tabs.setCurrentPage(7);
    requireTest(tabs.currentPage() == switchPage, "out-of-range pages are ignored");
    // First page shown, Confidence chosen, two fallbacks: four real changes.
    requireTest(changes == 4, "each real change is announced once");
    return 0;
}
