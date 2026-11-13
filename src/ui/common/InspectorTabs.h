#pragma once

#include "ui/common/Glyphs.h"

#include <QString>
#include <QVector>
#include <QWidget>

class QStackedWidget;

namespace aquatrack::ui {

// A panel of pages behind a row of icon tabs, after the inspectors of the
// editing suites: an icon over a short name per page, the chosen one lit and
// underlined, pages with nothing to show greyed out, and a small count on a
// tab when its page has items. Only the chosen page is on screen and it gets
// the panel's full height.
//
// Adding a page is one call:
//     panel->addPage(Glyph::Events, QStringLiteral("Events"), widget);
class InspectorTabs final : public QWidget {
    Q_OBJECT

public:
    explicit InspectorTabs(QWidget* parent = nullptr);

    // Returns the page's index. Pages are shown in the order added.
    int addPage(Glyph glyph, const QString& label, QWidget* page, const QString& tooltip = {});

    // A count drawn on the tab (0 hides it).
    void setPageCount(int index, int count);
    // A disabled page's tab is greyed and cannot be chosen. If it was the
    // current page, the first enabled page takes over.
    void setPageEnabled(int index, bool enabled);
    void setCurrentPage(int index);
    [[nodiscard]] int currentPage() const { return m_current; }
    [[nodiscard]] int pageCount() const { return int(m_tabs.size()); }

signals:
    void currentPageChanged(int index);

private:
    class TabBar;
    struct Tab {
        Glyph glyph;
        QString label;
        QString tooltip;
        int count = 0;
        bool enabled = true;
    };

    TabBar* m_bar = nullptr;
    QStackedWidget* m_stack = nullptr;
    QVector<Tab> m_tabs;
    int m_current = -1;
};

}  // namespace aquatrack::ui
