#include "ui/common/InspectorTabs.h"

#include "ui/AppTheme.h"

#include <QFontMetrics>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStackedWidget>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>

namespace aquatrack::ui {
namespace {

constexpr int kBarHeight = 56;
constexpr int kIconSize = 18;

QColor withAlpha(QColor colour, int alpha)
{
    colour.setAlpha(alpha);
    return colour;
}

}  // namespace

// The row of tabs. Painted as one widget so the slots share the width
// exactly and the underline sits on the bar's bottom rule.
class InspectorTabs::TabBar final : public QWidget {
public:
    explicit TabBar(InspectorTabs* owner) : QWidget(owner), m_owner(owner)
    {
        setFixedHeight(kBarHeight);
        setMouseTracking(true);
        setFocusPolicy(Qt::TabFocus);
        setAttribute(Qt::WA_Hover);
        setProperty("role", "bare");
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(QRect(0, height() - 1, width(), 1), palette::Line);
        const auto& tabs = m_owner->m_tabs;
        if (tabs.isEmpty())
            return;

        QFont label = font();
        label.setPixelSize(11);
        QFont badgeFont = font();
        badgeFont.setPixelSize(9);
        badgeFont.setBold(true);

        for (int index = 0; index < tabs.size(); ++index) {
            const auto& tab = tabs[index];
            const QRectF slot = slotRect(index);
            const bool current = index == m_owner->m_current;
            const bool hovered = index == m_hovered && tab.enabled && !current;
            const QColor tone = !tab.enabled ? withAlpha(palette::TextDim, 120)
                                : current    ? palette::TextPrimary
                                : hovered    ? palette::TextPrimary
                                             : palette::TextSecondary;

            if (hovered) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(palette::Surface);
                painter.drawRoundedRect(slot.adjusted(2, 4, -2, -4), 7, 7);
            }
            if (current) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(palette::Accent);
                painter.drawRoundedRect(QRectF(slot.left() + 6, height() - 2.5, slot.width() - 12, 2.5),
                                        1.2, 1.2);
            }

            const QRectF icon(slot.center().x() - kIconSize / 2.0, 9, kIconSize, kIconSize);
            // The open tab's icon is duotone: outline over a soft tint.
            paintGlyph(painter, tab.glyph, icon, current ? palette::AccentHover : tone,
                       current ? GlyphStyle::Duotone : GlyphStyle::Regular);

            painter.setFont(label);
            painter.setPen(tone);
            const QRectF text(slot.left() + 2, icon.bottom() + 5, slot.width() - 4, 15);
            painter.drawText(text, Qt::AlignHCenter | Qt::AlignVCenter,
                             QFontMetrics(label).elidedText(tab.label, Qt::ElideRight,
                                                            int(text.width())));

            if (tab.count > 0 && tab.enabled) {
                const QString number = tab.count > 99 ? QStringLiteral("99+") : QString::number(tab.count);
                const qreal width = std::max<qreal>(15, QFontMetrics(badgeFont).horizontalAdvance(number) + 8);
                const QRectF badge(icon.right() - 3, icon.top() - 5, width, 14);
                painter.setPen(QPen(palette::Chrome, 1.5));
                painter.setBrush(current ? palette::Accent : palette::SurfaceHover);
                painter.drawRoundedRect(badge, 7, 7);
                painter.setFont(badgeFont);
                painter.setPen(current ? QColor(Qt::white) : palette::TextPrimary);
                painter.drawText(badge, Qt::AlignCenter, number);
            }
        }

        if (m_keyboardFocus && hasFocus() && m_owner->m_current >= 0) {
            painter.setPen(QPen(palette::Accent, 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(slotRect(m_owner->m_current).adjusted(2.5, 3.5, -2.5, -4.5), 7, 7);
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton)
            return;
        const int index = indexAt(event->position().toPoint());
        if (index >= 0 && m_owner->m_tabs[index].enabled)
            m_owner->setCurrentPage(index);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const int index = indexAt(event->position().toPoint());
        if (index != m_hovered) {
            m_hovered = index;
            setCursor(index >= 0 && m_owner->m_tabs[index].enabled ? Qt::PointingHandCursor
                                                                    : Qt::ArrowCursor);
            update();
        }
    }

    void leaveEvent(QEvent*) override
    {
        m_hovered = -1;
        update();
    }

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ToolTip) {
            auto* help = static_cast<QHelpEvent*>(event);
            const int index = indexAt(help->pos());
            if (index >= 0) {
                const auto& tab = m_owner->m_tabs[index];
                QString text = tab.tooltip.isEmpty() ? tab.label : tab.tooltip;
                if (!tab.enabled)
                    text += QStringLiteral("\nNothing to show yet");
                QToolTip::showText(help->globalPos(), text, this);
            } else {
                QToolTip::hideText();
            }
            return true;
        }
        return QWidget::event(event);
    }

    // The focus ring is for keyboard users: shown when Tab lands here, not
    // when focus arrives some other way (a click, the window opening).
    void focusInEvent(QFocusEvent* event) override
    {
        m_keyboardFocus = event->reason() == Qt::TabFocusReason ||
                          event->reason() == Qt::BacktabFocusReason;
        QWidget::focusInEvent(event);
        update();
    }

    void focusOutEvent(QFocusEvent* event) override
    {
        m_keyboardFocus = false;
        QWidget::focusOutEvent(event);
        update();
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        const int step = event->key() == Qt::Key_Left ? -1 : event->key() == Qt::Key_Right ? 1 : 0;
        if (step == 0) {
            QWidget::keyPressEvent(event);
            return;
        }
        const auto& tabs = m_owner->m_tabs;
        for (int index = m_owner->m_current + step; index >= 0 && index < tabs.size(); index += step)
            if (tabs[index].enabled) {
                m_owner->setCurrentPage(index);
                return;
            }
    }

private:
    QRectF slotRect(int index) const
    {
        const qreal each = qreal(width()) / std::max<int>(1, int(m_owner->m_tabs.size()));
        return {index * each, 0, each, qreal(height())};
    }

    int indexAt(const QPoint& point) const
    {
        const auto count = int(m_owner->m_tabs.size());
        if (count == 0 || !rect().contains(point))
            return -1;
        return std::clamp(int(point.x() * count / std::max(1, width())), 0, count - 1);
    }

    InspectorTabs* m_owner;
    int m_hovered = -1;
    bool m_keyboardFocus = false;
};

InspectorTabs::InspectorTabs(QWidget* parent) : QWidget(parent)
{
    setProperty("role", "bare");
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(10);
    m_bar = new TabBar(this);
    column->addWidget(m_bar);
    m_stack = new QStackedWidget(this);
    m_stack->setProperty("role", "bare");
    column->addWidget(m_stack, 1);
}

int InspectorTabs::addPage(Glyph glyph, const QString& label, QWidget* page, const QString& tooltip)
{
    m_tabs.push_back({glyph, label, tooltip});
    m_stack->addWidget(page);
    const int index = int(m_tabs.size()) - 1;
    if (m_current < 0)
        setCurrentPage(index);
    m_bar->update();
    return index;
}

void InspectorTabs::setPageCount(int index, int count)
{
    if (index < 0 || index >= m_tabs.size() || m_tabs[index].count == count)
        return;
    m_tabs[index].count = count;
    m_bar->update();
}

void InspectorTabs::setPageEnabled(int index, bool enabled)
{
    if (index < 0 || index >= m_tabs.size() || m_tabs[index].enabled == enabled)
        return;
    m_tabs[index].enabled = enabled;
    if (!enabled && index == m_current) {
        for (int other = 0; other < m_tabs.size(); ++other)
            if (m_tabs[other].enabled) {
                setCurrentPage(other);
                break;
            }
    }
    m_bar->update();
}

void InspectorTabs::setCurrentPage(int index)
{
    if (index < 0 || index >= m_tabs.size() || index == m_current)
        return;
    m_current = index;
    m_stack->setCurrentIndex(index);
    m_bar->update();
    emit currentPageChanged(index);
}

}  // namespace aquatrack::ui
