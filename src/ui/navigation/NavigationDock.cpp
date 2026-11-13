#include "ui/navigation/NavigationDock.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/HoverHint.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QFontMetrics>
#include <QPainter>
#include <QPushButton>

using aquatrack::ui::Glyph;
namespace palette = aquatrack::ui::palette;

namespace {

struct Destination {
    Glyph glyph;
    const char* label;
    int page;  // application::AppState::Page
};

// Left to right in the order a survey is worked, grouped by stage:
//   home | prepare: find data, add it, pick a model
//        | analyse: review and correct, then read the results
//        | deliver: export.
// Settings is a utility, not a step, so it lives in the title bar.
const QVector<QVector<Destination>> kStages{
    {{Glyph::Dashboard, "Dashboard", 0}},
    {{Glyph::Datasets, "Datasets", 4}, {Glyph::Video, "Videos", 1}, {Glyph::Models, "Models", 5}},
    {{Glyph::Review, "Review", 2}, {Glyph::Analytics, "Analytics", 3}},
    {{Glyph::Export, "Export", 7}},
};

constexpr int kIconSize = 18;
constexpr int kBarHeight = 40;
constexpr int kSlotWidth = 60;

// One page slot: an icon, lit when current. Painted so the slot can fill the
// bar's full height and carry the marker on its top edge. No Q_OBJECT: it is
// still found and cast as a QPushButton.
class PageButton final : public QPushButton {
public:
    PageButton(Glyph glyph, const QString& label, QWidget* parent)
        : QPushButton(parent), m_glyph(glyph)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setAccessibleName(label);
        setFixedSize(kSlotWidth, kBarHeight);
        setAttribute(Qt::WA_Hover);
        aquatrack::ui::attachHoverHint(this, label, aquatrack::ui::HintPlacement::Above);
    }

    void setActive(bool active)
    {
        m_active = active;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const bool hovered = underMouse();

        if (m_active)
            painter.fillRect(rect(), palette::SurfaceRaised);
        else if (isDown())
            painter.fillRect(rect(), palette::SurfaceRaised);
        else if (hovered)
            painter.fillRect(rect(), palette::Surface);
        if (m_active)
            painter.fillRect(QRectF(0, 0, width(), 2), palette::Accent);
        if (hasFocus()) {
            painter.setPen(QPen(palette::Accent, 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(QRectF(rect()).adjusted(1.5, 3.5, -1.5, -1.5));
        }

        const QColor colour = m_active ? palette::TextPrimary
                              : hovered ? palette::TextPrimary
                                        : palette::TextSecondary;
        const QRectF iconBox((width() - kIconSize) / 2.0, (height() - kIconSize) / 2.0 + 1,
                             kIconSize, kIconSize);
        // The current page's icon is duotone, in the accent: lit, not just white.
        aquatrack::ui::paintGlyph(painter, m_glyph, iconBox,
                                  m_active ? palette::AccentHover : colour,
                                  m_active ? aquatrack::ui::GlyphStyle::Duotone
                                           : aquatrack::ui::GlyphStyle::Regular);
    }

private:
    Glyph m_glyph;
    bool m_active = false;
};

}  // namespace

NavigationDock::NavigationDock(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("navigationDock"));
    setFixedHeight(kBarHeight);

    // Equal stretch either side keeps the page slots centred on the window
    // whatever the status text says; the left side is painted.
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    m_left = new QWidget(this);
    m_left->setProperty("role", "bare");
    m_left->setAttribute(Qt::WA_TransparentForMouseEvents);
    row->addWidget(m_left, 1);

    auto* pages = new QWidget(this);
    pages->setProperty("role", "bare");
    auto* pageRow = new QHBoxLayout(pages);
    pageRow->setContentsMargins(0, 0, 0, 0);
    pageRow->setSpacing(2);
    for (int stage = 0; stage < kStages.size(); ++stage) {
        // Stages are set apart by space, not rules.
        if (stage > 0)
            pageRow->addSpacing(14);
        for (const auto& destination : kStages.at(stage)) {
            const QString label = QString::fromLatin1(destination.label);
            auto* button = new PageButton(destination.glyph, label, pages);
            button->setObjectName(QStringLiteral("nav%1").arg(label));
            const int page = destination.page;
            connect(button, &QPushButton::clicked, this, [this, page] { emit pageRequested(page); });
            pageRow->addWidget(button);
            m_entries.push_back({button, page});
        }
    }
    row->addWidget(pages);

    auto* right = new QWidget(this);
    right->setProperty("role", "bare");
    row->addWidget(right, 1);
    setCurrentPage(0);
}

void NavigationDock::setCurrentPage(int page)
{
    for (const auto& entry : m_entries)
        static_cast<PageButton*>(entry.button)->setActive(entry.page == page);
}

void NavigationDock::setStatusMessage(const QString& message)
{
    m_status = message;
    update(m_left->geometry());
}

void NavigationDock::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), palette::Chrome);
    painter.fillRect(QRect(0, 0, width(), 1), palette::LineSoft);

    // Product mark and name, then the latest status message, elided to the
    // space left of the page slots.
    const QRect area = m_left->geometry().adjusted(14, 0, -16, 0);
    const QRectF mark(area.left(), (height() - 18) / 2.0, 18, 18);
    painter.setPen(Qt::NoPen);
    painter.setBrush(palette::AccentSunken);
    painter.drawRoundedRect(mark, 5, 5);
    aquatrack::ui::paintGlyph(painter, Glyph::Drop, mark.adjusted(3, 3, -3, -3), Qt::white);

    QFont name = font();
    name.setPixelSize(12);
    name.setWeight(QFont::DemiBold);
    const QString product = QStringLiteral("AquaTrack+");
    qreal x = mark.right() + 9;
    const int nameWidth = QFontMetrics(name).horizontalAdvance(product);
    if (x + nameWidth > area.right())
        return;
    painter.setFont(name);
    painter.setPen(palette::TextSecondary);
    painter.drawText(QRectF(x, 0, nameWidth, height()), Qt::AlignLeft | Qt::AlignVCenter, product);
    x += nameWidth + 14;

    QFont status = font();
    status.setPixelSize(12);
    const int room = int(area.right() - x);
    if (room < 40)
        return;
    painter.setFont(status);
    painter.setPen(palette::TextDim);
    const QString text = m_status.isEmpty() ? QStringLiteral("Ready") : m_status;
    painter.drawText(QRectF(x, 0, room, height()), Qt::AlignLeft | Qt::AlignVCenter,
                     QFontMetrics(status).elidedText(text, Qt::ElideRight, room));
}
