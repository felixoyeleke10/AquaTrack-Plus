#include "ui/AppStyle.h"

#include <QPainter>
#include <QStyleOptionButton>
#include <QWindow>

namespace aquatrack::ui {
namespace {

constexpr int kIconTextSpacing = 8;
// What Qt already reserves between icon and text in a button's size hint.
constexpr int kQtSpacing = 4;

bool hasIconAndText(const QStyleOption* option)
{
    const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option);
    return button && !button->icon.isNull() && !button->text.isEmpty();
}

}  // namespace

AppStyle::AppStyle(QStyle* base) : QProxyStyle(base) {}

void AppStyle::drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                           const QWidget* widget) const
{
    if (element != CE_PushButtonLabel || !hasIconAndText(option)) {
        QProxyStyle::drawControl(element, option, painter, widget);
        return;
    }
    const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option);
    const bool enabled = button->state & State_Enabled;
    const QIcon::Mode mode = !enabled                           ? QIcon::Disabled
                             : (button->state & State_MouseOver) ? QIcon::Active
                                                                 : QIcon::Normal;
    const QIcon::State state = (button->state & State_On) ? QIcon::On : QIcon::Off;
    const qreal ratio = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
    const QPixmap pixmap = button->icon.pixmap(button->iconSize, ratio, mode, state);

    const QSize iconSize = button->iconSize;
    const int textWidth = button->fontMetrics.horizontalAdvance(button->text);
    const int total = iconSize.width() + kIconTextSpacing + textWidth;
    const QRect area = button->rect;
    int x = area.x() + (area.width() - total) / 2;
    if (button->direction == Qt::RightToLeft)
        x = area.right() - (area.width() - total) / 2 - total + 1;

    const QRect iconRect(x, area.y() + (area.height() - iconSize.height()) / 2, iconSize.width(),
                         iconSize.height());
    painter->drawPixmap(iconRect, pixmap);
    const QRect textRect(iconRect.right() + 1 + kIconTextSpacing, area.y(), textWidth + 2,
                         area.height());
    drawItemText(painter, textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextShowMnemonic,
                 button->palette, enabled, button->text, QPalette::ButtonText);
}

QSize AppStyle::sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size,
                                 const QWidget* widget) const
{
    QSize result = QProxyStyle::sizeFromContents(type, option, size, widget);
    if (type == CT_PushButton && hasIconAndText(option))
        result.rwidth() += kIconTextSpacing - kQtSpacing;
    return result;
}

}  // namespace aquatrack::ui
