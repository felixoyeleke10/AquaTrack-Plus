#include "ui/common/SectionHeader.h"

#include "ui/AppTheme.h"
#include "ui/common/Widgets.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>

namespace aquatrack::ui {

namespace {
constexpr int kChevronBox = 18;  // room the chevron is drawn in, at the left
}

SectionHeader::SectionHeader(const QString& title, QWidget* parent)
    : QWidget(parent), m_chevronOnly(title.isEmpty())
{
    setProperty("role", "bare");
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(kChevronBox + 2, 3, 6, 3);
    row->setSpacing(8);
    if (m_chevronOnly) {
        setFixedSize(kChevronBox + 8, 26);
        row->setContentsMargins(0, 0, 0, 0);
    } else {
        m_title = sectionLabel(title, this);
        row->addWidget(m_title);
        m_detail = typedLabel(QString(), "dim", this);
        m_detail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        m_detail->hide();
        row->addWidget(m_detail, 1);
    }
    updateTip();
}

void SectionHeader::setExpanded(bool expanded)
{
    if (expanded == m_expanded)
        return;
    m_expanded = expanded;
    updateTip();
    update();
}

void SectionHeader::toggle()
{
    m_expanded = !m_expanded;
    updateTip();
    update();
    emit toggled(m_expanded);
}

void SectionHeader::updateTip()
{
    setToolTip(m_expanded ? QStringLiteral("Collapse") : QStringLiteral("Expand"));
}

void SectionHeader::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (m_hovered || hasFocus()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette::Surface);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0, 0, 0, 0), 6, 6);
    }
    // The chevron: pointing down when open, right when folded.
    const QPointF centre(m_chevronOnly ? width() / 2.0 : kChevronBox / 2.0 + 2, height() / 2.0);
    QPen pen(m_hovered ? palette::TextPrimary : palette::TextSecondary, 1.6, Qt::SolidLine,
             Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    if (m_expanded)
        painter.drawPolyline(QVector<QPointF>{centre + QPointF(-3.5, -1.5), centre + QPointF(0, 2),
                                              centre + QPointF(3.5, -1.5)});
    else
        painter.drawPolyline(QVector<QPointF>{centre + QPointF(-1.5, -3.5), centre + QPointF(2, 0),
                                              centre + QPointF(-1.5, 3.5)});
}

void SectionHeader::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        toggle();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void SectionHeader::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return ||
        event->key() == Qt::Key_Enter) {
        toggle();
        return;
    }
    QWidget::keyPressEvent(event);
}

void SectionHeader::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void SectionHeader::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

}  // namespace aquatrack::ui
