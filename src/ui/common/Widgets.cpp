#include "ui/common/Widgets.h"

#include "ui/AppTheme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace aquatrack::ui {

QLabel* typedLabel(const QString& text, const char* type, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("type", type);
    return label;
}

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    return typedLabel(text.toUpper(), "label", parent);
}

QFrame* card(QWidget* parent, const char* role)
{
    auto* frame = new QFrame(parent);
    frame->setProperty("role", role);
    return frame;
}

QFrame* divider(QWidget* parent)
{
    auto* frame = new QFrame(parent);
    frame->setProperty("role", "divider");
    frame->setFixedHeight(1);
    return frame;
}

QPushButton* iconButton(Glyph glyph, const QString& tooltip, QWidget* parent, const char* role,
                        int size)
{
    auto* button = new QPushButton(parent);
    button->setProperty("role", role);
    button->setIcon(glyphIcon(glyph, size, palette::TextSecondary, palette::TextPrimary));
    button->setIconSize(QSize(size, size));
    button->setToolTip(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QPushButton* textButton(const QString& text, QWidget* parent, const char* role)
{
    auto* button = new QPushButton(text, parent);
    if (role && *role)
        button->setProperty("role", role);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QWidget* segmentedSwitch(const QStringList& labels, int current, QWidget* parent,
                         std::function<void(int)> chosen)
{
    auto* frame = new QFrame(parent);
    frame->setProperty("role", "segmented");
    auto* row = new QHBoxLayout(frame);
    row->setContentsMargins(3, 3, 3, 3);
    row->setSpacing(2);
    for (int index = 0; index < labels.size(); ++index) {
        auto* option = new QPushButton(labels.at(index), frame);
        option->setProperty("role", index == current ? "segmentActive" : "segment");
        option->setCursor(index == current ? Qt::ArrowCursor : Qt::PointingHandCursor);
        if (index != current)
            QObject::connect(option, &QPushButton::clicked, frame, [chosen, index] { chosen(index); });
        row->addWidget(option);
    }
    frame->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return frame;
}

// ---------------------------------------------------------------- Badge

Badge::Badge(QWidget* parent) : QWidget(parent), m_accent(palette::Success) {}

void Badge::setText(const QString& text)
{
    m_text = text;
    updateGeometry();
    update();
}

void Badge::setAccent(const QColor& color)
{
    m_accent = color;
    update();
}

QSize Badge::sizeHint() const
{
    QFont font = this->font();
    font.setPointSizeF(7.8);
    font.setBold(true);
    return {QFontMetrics(font).horizontalAdvance(m_text) + 16, 18};
}

void Badge::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    if (m_text.isEmpty())
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QFont font = this->font();
    font.setPointSizeF(7.8);
    font.setBold(true);
    painter.setFont(font);

    QColor fill = m_accent;
    fill.setAlpha(38);
    painter.setBrush(fill);
    painter.setPen(Qt::NoPen);
    const QRectF pill(0, (height() - 18) / 2.0, width(), 18);
    painter.drawRoundedRect(pill, 9, 9);
    painter.setPen(m_accent);
    painter.drawText(pill, Qt::AlignCenter, m_text);
}

// ---------------------------------------------------------- ToggleSwitch

ToggleSwitch::ToggleSwitch(QWidget* parent) : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFixedSize(sizeHint());
}

void ToggleSwitch::setChecked(bool checked)
{
    if (m_checked == checked)
        return;
    m_checked = checked;
    update();
    emit toggled(m_checked);
}

QSize ToggleSwitch::sizeHint() const
{
    return {34, 19};
}

void ToggleSwitch::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF track(0, 0, width(), height());
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_checked ? palette::Accent : palette::Line);
    painter.drawRoundedRect(track, height() / 2.0, height() / 2.0);

    const qreal diameter = height() - 5;
    const qreal x = m_checked ? width() - diameter - 2.5 : 2.5;
    painter.setBrush(Qt::white);
    painter.drawEllipse(QRectF(x, 2.5, diameter, diameter));
}

void ToggleSwitch::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->position().toPoint()))
        setChecked(!m_checked);
}

// ------------------------------------------------------------- ToggleRow

ToggleRow::ToggleRow(const QString& text, bool checked, QWidget* parent) : QWidget(parent)
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->addWidget(typedLabel(text, "small", this));
    row->addStretch();
    m_toggle = new ToggleSwitch(this);
    m_toggle->setChecked(checked);
    row->addWidget(m_toggle);
    connect(m_toggle, &ToggleSwitch::toggled, this, &ToggleRow::toggled);
}

// ------------------------------------------------------------ MetricTile

MetricTile::MetricTile(const QString& caption, QWidget* parent) : QWidget(parent)
{
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(1);
    m_value = typedLabel(QStringLiteral("—"), "metric", this);
    m_value->setAlignment(Qt::AlignCenter);
    m_caption = typedLabel(caption, "dim", this);
    m_caption->setAlignment(Qt::AlignCenter);
    m_caption->setWordWrap(true);
    column->addWidget(m_value);
    column->addWidget(m_caption);
}

void MetricTile::setValue(const QString& value)
{
    m_value->setText(value);
}

void MetricTile::setValueColor(const QColor& color)
{
    m_value->setStyleSheet(QStringLiteral("color:%1").arg(color.name()));
}

// ------------------------------------------------------------ StatusChip

StatusChip::StatusChip(Glyph glyph, const QString& name, QWidget* parent) : QFrame(parent)
{
    setProperty("role", "cardFlat");
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(10, 7, 12, 7);
    row->setSpacing(9);

    auto* icon = new QLabel(this);
    icon->setPixmap(glyphPixmap(glyph, 18, palette::Accent));
    icon->setFixedWidth(18);
    row->addWidget(icon);

    auto* column = new QVBoxLayout;
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(1);
    column->addWidget(typedLabel(name, "small", this));

    auto* stateRow = new QHBoxLayout;
    stateRow->setContentsMargins(0, 0, 0, 0);
    stateRow->setSpacing(5);
    m_dot = new QWidget(this);
    m_dot->setFixedSize(6, 6);
    m_dot->setStyleSheet(
        QStringLiteral("background:%1;border-radius:3px").arg(palette::Success.name()));
    m_state = typedLabel(QStringLiteral("Ready"), "dim", this);
    stateRow->addWidget(m_dot);
    stateRow->addWidget(m_state);
    stateRow->addStretch();
    column->addLayout(stateRow);
    row->addLayout(column);
    row->addStretch();
}

void StatusChip::setState(const QString& text, const QColor& color)
{
    m_state->setText(text);
    m_dot->setStyleSheet(QStringLiteral("background:%1;border-radius:3px").arg(color.name()));
}

// ------------------------------------------------------------- MeterCard

class MeterCard::Bar final : public QWidget {
public:
    explicit Bar(QWidget* parent) : QWidget(parent) { setFixedHeight(5); }

    void setRatio(double ratio)
    {
        m_ratio = std::clamp(ratio, 0.0, 1.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette::LineSoft);
        painter.drawRoundedRect(rect(), 2.5, 2.5);
        // Amber past 75%, red past 90% — a full disk is a failure the operator
        // needs to see coming, not a neutral fact.
        QColor fill = palette::Success;
        if (m_ratio > 0.9)
            fill = palette::Danger;
        else if (m_ratio > 0.75)
            fill = palette::Warning;
        painter.setBrush(fill);
        QRectF value = rect();
        value.setWidth(std::max(3.0, rect().width() * m_ratio));
        painter.drawRoundedRect(value, 2.5, 2.5);
    }

private:
    double m_ratio = 0.0;
};

MeterCard::MeterCard(Glyph glyph, const QString& name, QWidget* parent) : QFrame(parent)
{
    setProperty("role", "cardFlat");
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(11, 7, 11, 8);
    column->setSpacing(4);

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(7);
    auto* icon = new QLabel(this);
    icon->setPixmap(glyphPixmap(glyph, 15, palette::Accent));
    head->addWidget(icon);
    head->addWidget(typedLabel(name, "small", this));
    head->addStretch();
    m_readout = typedLabel(QStringLiteral("—"), "dim", this);
    head->addWidget(m_readout);
    column->addLayout(head);

    m_bar = new Bar(this);
    column->addWidget(m_bar);

    m_percent = typedLabel(QString(), "dim", this);
    column->addWidget(m_percent);
}

void MeterCard::setReadout(const QString& text)
{
    m_readout->setText(text);
}

void MeterCard::setRatio(double ratio, const QString& percentText)
{
    m_bar->setRatio(ratio);
    m_percent->setText(percentText);
}

// -------------------------------------------------------------- EventRow

EventRow::EventRow(const QColor& severity, const QString& text, const QString& timestamp,
                   QWidget* parent)
    : QWidget(parent)
{
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 3, 0, 3);
    row->setSpacing(8);

    auto* dot = new QWidget(this);
    dot->setFixedSize(7, 7);
    dot->setStyleSheet(QStringLiteral("background:%1;border-radius:3px").arg(severity.name()));
    row->addWidget(dot, 0, Qt::AlignTop);

    auto* description = typedLabel(text, "small", this);
    description->setWordWrap(true);
    row->addWidget(description, 1);

    auto* time = typedLabel(timestamp, "mono", this);
    time->setAlignment(Qt::AlignRight | Qt::AlignTop);
    row->addWidget(time, 0, Qt::AlignTop);
}

}  // namespace aquatrack::ui
