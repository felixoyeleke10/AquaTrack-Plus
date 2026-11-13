#include "ui/common/Charts.h"

#include "ui/AppTheme.h"

#include <QFontMetrics>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace aquatrack::ui {
namespace {

constexpr int kAxisLeft = 34;
constexpr int kAxisBottom = 20;
constexpr int kGridLines = 4;

void drawEmpty(QPainter& painter, const QRect& area, const QString& text)
{
    painter.setPen(palette::TextDim);
    QFont font = painter.font();
    font.setPointSizeF(9.0);
    painter.setFont(font);
    painter.drawText(area, Qt::AlignCenter, text);
}

QFont smallFont(const QPainter& painter, bool bold = false)
{
    QFont font = painter.font();
    font.setPointSizeF(7.5);
    font.setBold(bold);
    return font;
}

// Round a range up to a readable tick step so the y labels are 20/40/60
// rather than 17.3/34.6/51.9.
double niceCeiling(double value)
{
    if (value <= 0.0)
        return 1.0;
    const double magnitude = std::pow(10.0, std::floor(std::log10(value)));
    const double normalised = value / magnitude;
    double step = 10.0;
    if (normalised <= 1.0)
        step = 1.0;
    else if (normalised <= 2.0)
        step = 2.0;
    else if (normalised <= 5.0)
        step = 5.0;
    return step * magnitude;
}

}  // namespace

// ---------------------------------------------------------------- LineChart

LineChart::LineChart(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(110);
}

void LineChart::setSeries(QVector<ChartSeries> series)
{
    m_series = std::move(series);
    update();
}

void LineChart::setAxisLabels(QStringList xLabels)
{
    m_xLabels = std::move(xLabels);
    update();
}

void LineChart::setAxisCaptions(QString left, QString bottom, QString right)
{
    m_leftCaption = std::move(left);
    m_bottomCaption = std::move(bottom);
    m_rightCaption = std::move(right);
    update();
}

void LineChart::setPeakCallout(bool enabled)
{
    m_peakCallout = enabled;
    update();
}

void LineChart::setLegendVisible(bool visible)
{
    m_legend = visible;
    update();
}

void LineChart::setValueBadge(const QString& text)
{
    m_valueBadge = text;
    update();
}

void LineChart::setEmptyText(const QString& text)
{
    m_emptyText = text;
    update();
}

void LineChart::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const bool hasData = std::any_of(m_series.cbegin(), m_series.cend(),
                                     [](const ChartSeries& s) { return s.values.size() >= 2; });
    if (!hasData) {
        drawEmpty(painter, rect(), m_emptyText);
        return;
    }

    const int legendHeight = m_legend ? 14 : 0;
    const QRectF plot(kAxisLeft, legendHeight + 4.0, std::max(10, width() - kAxisLeft - 6),
                      std::max(10.0, height() - kAxisBottom - legendHeight - 8.0));

    double maximum = 0.0;
    for (const auto& series : m_series)
        for (const double value : series.values)
            maximum = std::max(maximum, value);
    maximum = niceCeiling(maximum);

    painter.setFont(smallFont(painter));

    // Gridlines and y labels.
    for (int i = 0; i <= kGridLines; ++i) {
        const double ratio = double(i) / kGridLines;
        const double y = plot.bottom() - ratio * plot.height();
        painter.setPen(QPen(palette::LineSoft, 1.0));
        painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(palette::TextDim);
        const double tick = maximum * ratio;
        painter.drawText(QRectF(0, y - 7, kAxisLeft - 6, 14), Qt::AlignRight | Qt::AlignVCenter,
                         tick >= 10.0 ? QString::number(std::llround(tick))
                                      : QString::number(tick, 'g', 2));
    }

    // Series.
    for (const auto& series : m_series) {
        if (series.values.size() < 2)
            continue;
        const double step = plot.width() / double(series.values.size() - 1);
        QPainterPath line;
        for (int i = 0; i < series.values.size(); ++i) {
            const double x = plot.left() + i * step;
            const double y = plot.bottom() - (series.values.at(i) / maximum) * plot.height();
            if (i == 0)
                line.moveTo(x, y);
            else
                line.lineTo(x, y);
        }
        if (series.filled) {
            QPainterPath area = line;
            area.lineTo(plot.right(), plot.bottom());
            area.lineTo(plot.left(), plot.bottom());
            area.closeSubpath();
            QLinearGradient gradient(0, plot.top(), 0, plot.bottom());
            QColor top = series.color;
            top.setAlpha(90);
            QColor bottom = series.color;
            bottom.setAlpha(0);
            gradient.setColorAt(0.0, top);
            gradient.setColorAt(1.0, bottom);
            painter.fillPath(area, gradient);
        }
        painter.setPen(QPen(series.color, 1.6));
        painter.drawPath(line);
    }

    // Peak callout on the first series.
    if (m_peakCallout && !m_series.isEmpty() && m_series.first().values.size() >= 2) {
        const auto& values = m_series.first().values;
        const auto peak = std::max_element(values.cbegin(), values.cend());
        const int index = int(std::distance(values.cbegin(), peak));
        const double step = plot.width() / double(values.size() - 1);
        const QPointF point(plot.left() + index * step,
                            plot.bottom() - (*peak / maximum) * plot.height());
        painter.setBrush(m_series.first().color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(point, 2.8, 2.8);

        const QString text = QStringLiteral("Peak: %1").arg(std::llround(*peak));
        painter.setFont(smallFont(painter, true));
        const QFontMetrics metrics(painter.font());
        QRectF badge(0, 0, metrics.horizontalAdvance(text) + 12, 16);
        badge.moveCenter(QPointF(point.x(), point.y() - 14));
        badge.moveLeft(std::clamp(badge.left(), plot.left(), plot.right() - badge.width()));
        badge.moveTop(std::max(plot.top(), badge.top()));
        painter.setBrush(palette::SurfaceRaised);
        painter.setPen(QPen(palette::Line, 1.0));
        painter.drawRoundedRect(badge, 4, 4);
        painter.setPen(palette::TextPrimary);
        painter.drawText(badge, Qt::AlignCenter, text);
        painter.setFont(smallFont(painter));
    }

    // Latest-value badge, pinned to the top right of the plot.
    if (!m_valueBadge.isEmpty()) {
        painter.setFont(smallFont(painter, true));
        const QFontMetrics metrics(painter.font());
        QRectF badge(0, 0, metrics.horizontalAdvance(m_valueBadge) + 14, 17);
        badge.moveTopRight(QPointF(plot.right(), plot.top()));
        painter.setBrush(palette::Accent);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(badge, 4, 4);
        painter.setPen(Qt::white);
        painter.drawText(badge, Qt::AlignCenter, m_valueBadge);
        painter.setFont(smallFont(painter));
    }

    // X labels.
    if (!m_xLabels.isEmpty()) {
        painter.setPen(palette::TextDim);
        const double span = plot.width() / double(std::max(1, int(m_xLabels.size()) - 1));
        for (int i = 0; i < m_xLabels.size(); ++i) {
            QRectF cell(plot.left() + i * span - 26, plot.bottom() + 3, 52, 14);
            Qt::Alignment alignment = Qt::AlignCenter;
            if (i == 0) {
                cell.moveLeft(plot.left());
                alignment = Qt::AlignLeft | Qt::AlignVCenter;
            } else if (i == m_xLabels.size() - 1) {
                cell.moveRight(plot.right());
                alignment = Qt::AlignRight | Qt::AlignVCenter;
            }
            painter.drawText(cell, alignment, m_xLabels.at(i));
        }
    }

    // Legend above the plot.
    if (m_legend) {
        painter.setFont(smallFont(painter));
        const QFontMetrics metrics(painter.font());
        // Each entry gets an equal share of the width, so a narrow chart
        // shortens names instead of running the last one off the edge.
        const double share = (width() - plot.left()) / std::max<int>(1, int(m_series.size()));
        double x = plot.left();
        for (const auto& series : m_series) {
            painter.setPen(QPen(series.color, 1.6));
            painter.drawLine(QPointF(x, 7), QPointF(x + 9, 7));
            painter.setPen(palette::TextSecondary);
            const int room = std::max(0, int(share) - 13 - 10);
            const QString name = metrics.elidedText(series.name, Qt::ElideRight, room);
            const int nameWidth = metrics.horizontalAdvance(name);
            painter.drawText(QRectF(x + 13, 0, nameWidth + 4, 14), Qt::AlignLeft | Qt::AlignVCenter,
                             name);
            x += 13 + nameWidth + 14;
        }
    }

    // Axis captions.
    painter.setPen(palette::TextDim);
    painter.setFont(smallFont(painter));
    if (!m_leftCaption.isEmpty()) {
        painter.save();
        painter.translate(9, plot.center().y());
        painter.rotate(-90);
        painter.drawText(QRectF(-plot.height() / 2, -9, plot.height(), 12), Qt::AlignCenter,
                         m_leftCaption);
        painter.restore();
    }
    if (!m_rightCaption.isEmpty()) {
        painter.save();
        painter.translate(width() - 3, plot.center().y());
        painter.rotate(-90);
        painter.drawText(QRectF(-plot.height() / 2, -9, plot.height(), 12), Qt::AlignCenter,
                         m_rightCaption);
        painter.restore();
    }
    if (!m_bottomCaption.isEmpty())
        painter.drawText(QRectF(plot.left(), height() - 12, plot.width(), 12),
                         Qt::AlignCenter, m_bottomCaption);
}

// --------------------------------------------------------------- DonutChart

DonutChart::DonutChart(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(120);
}

void DonutChart::setSlices(QVector<DonutSlice> slices)
{
    m_slices = std::move(slices);
    update();
}

void DonutChart::setCenterCaption(QString value, QString caption)
{
    m_centerValue = std::move(value);
    m_centerCaption = std::move(caption);
    update();
}

void DonutChart::setEmptyText(const QString& text)
{
    m_emptyText = text;
    update();
}

void DonutChart::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    double total = 0.0;
    for (const auto& slice : m_slices)
        total += slice.value;
    if (total <= 0.0) {
        drawEmpty(painter, rect(), m_emptyText);
        return;
    }

    const int diameter = std::min<int>(height(), int(width() * 0.40)) - 6;
    const QRectF ring(4, (height() - diameter) / 2.0, diameter, diameter);
    const double thickness = diameter * 0.22;

    int startAngle = 90 * 16;
    for (const auto& slice : m_slices) {
        const int span = int(std::round(-360.0 * 16.0 * (slice.value / total)));
        painter.setPen(QPen(slice.color, thickness, Qt::SolidLine, Qt::FlatCap));
        painter.drawArc(ring.adjusted(thickness / 2, thickness / 2, -thickness / 2, -thickness / 2),
                        startAngle, span);
        startAngle += span;
    }

    if (!m_centerValue.isEmpty()) {
        // The figure must fit inside the hole: shrink it, and past a point
        // write it compactly (76,920 -> 76.9k) rather than spill over the ring.
        const double hole = std::max(10.0, diameter - 2 * thickness - 8);
        QString value = m_centerValue;
        bool isNumber = false;
        const double number = m_centerValue.toDouble(&isNumber);
        if (isNumber)
            value = QLocale::system().toString(qlonglong(std::llround(number)));
        QFont font = painter.font();
        font.setBold(true);
        int pixels = 18;
        font.setPixelSize(pixels);
        while (pixels > 10 && QFontMetrics(font).horizontalAdvance(value) > hole) {
            font.setPixelSize(--pixels);
        }
        if (isNumber && QFontMetrics(font).horizontalAdvance(value) > hole && number >= 1000)
            value = number >= 1e6 ? QStringLiteral("%1M").arg(number / 1e6, 0, 'f', 1)
                                  : QStringLiteral("%1k").arg(number / 1e3, 0, 'f', 1);
        painter.setFont(font);
        painter.setPen(palette::TextPrimary);
        const QRectF figure(ring.center().x() - hole / 2, ring.center().y() - pixels, hole, pixels + 2);
        painter.drawText(figure, Qt::AlignHCenter | Qt::AlignBottom, value);
        painter.setFont(smallFont(painter));
        painter.setPen(palette::TextDim);
        painter.drawText(QRectF(ring.center().x() - hole / 2, ring.center().y() + 3, hole, 14),
                         Qt::AlignHCenter | Qt::AlignTop, m_centerCaption);
    }

    // Legend column to the right of the ring.
    painter.setFont(smallFont(painter));
    const QFontMetrics metrics(painter.font());
    const double rowHeight = std::min(20.0, height() / double(std::max(1, int(m_slices.size()))));
    double y = (height() - rowHeight * m_slices.size()) / 2.0;
    const double legendLeft = ring.right() + 14;
    for (const auto& slice : m_slices) {
        painter.setBrush(slice.color);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(QRectF(legendLeft, y + rowHeight / 2 - 4, 8, 8), 2, 2);

        painter.setPen(palette::TextSecondary);
        // The share only: the count of each slice is the chart's total
        // times it, and spelling it out squeezed the names to nothing.
        const double percent = 100.0 * slice.value / total;
        const QString share = percent >= 10 || percent == 0
                                  ? QStringLiteral("%1%").arg(std::llround(percent))
                                  : QStringLiteral("%1%").arg(percent, 0, 'f', 1);
        const double shareWidth = metrics.horizontalAdvance(share) + 4;
        const QRectF nameCell(legendLeft + 14, y, width() - legendLeft - 18 - shareWidth, rowHeight);
        painter.drawText(nameCell, Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(slice.name, Qt::ElideRight, int(nameCell.width())));
        painter.setPen(palette::TextDim);
        painter.drawText(QRectF(width() - shareWidth - 4, y, shareWidth, rowHeight),
                         Qt::AlignRight | Qt::AlignVCenter, share);
        y += rowHeight;
    }
}

// ------------------------------------------------------------------ BarList

BarList::BarList(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(100);
}

void BarList::setRows(QVector<BarRow> rows)
{
    m_rows = std::move(rows);
    update();
}

void BarList::setEmptyText(const QString& text)
{
    m_emptyText = text;
    update();
}

void BarList::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (m_rows.isEmpty()) {
        drawEmpty(painter, rect(), m_emptyText);
        return;
    }

    double maximum = 0.0;
    for (const auto& row : m_rows)
        maximum = std::max(maximum, row.value);
    if (maximum <= 0.0)
        maximum = 1.0;

    painter.setFont(smallFont(painter, true));
    const QFontMetrics metrics(painter.font());
    int badgeWidth = 0;
    int valueWidth = 0;
    for (const auto& row : m_rows) {
        badgeWidth = std::max(badgeWidth, metrics.horizontalAdvance(row.badge) + 12);
        valueWidth = std::max(valueWidth, metrics.horizontalAdvance(row.valueText) + 6);
    }

    const double rowHeight = std::min(22.0, height() / double(m_rows.size()));
    double y = 0;
    for (const auto& row : m_rows) {
        const QRectF badge(0, y + rowHeight / 2 - 8, badgeWidth, 16);
        QColor fill = row.color;
        fill.setAlpha(45);
        painter.setBrush(fill);
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(badge, 4, 4);
        painter.setPen(row.color);
        painter.drawText(badge, Qt::AlignCenter, row.badge);

        const double barLeft = badgeWidth + 8;
        const double barWidth = width() - barLeft - valueWidth - 8;
        if (barWidth > 4) {
            const QRectF track(barLeft, y + rowHeight / 2 - 2.5, barWidth, 5);
            painter.setBrush(palette::LineSoft);
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(track, 2.5, 2.5);
            if (row.value > 0.0) {
                QRectF value = track;
                value.setWidth(std::max(3.0, barWidth * (row.value / maximum)));
                painter.setBrush(row.color);
                painter.drawRoundedRect(value, 2.5, 2.5);
            }
        }

        painter.setPen(palette::TextSecondary);
        painter.drawText(QRectF(width() - valueWidth, y, valueWidth, rowHeight),
                         Qt::AlignRight | Qt::AlignVCenter, row.valueText);
        y += rowHeight;
    }
}

// ---------------------------------------------------------------- Sparkline

Sparkline::Sparkline(QWidget* parent) : QWidget(parent), m_color(palette::Accent)
{
    setMinimumHeight(24);
}

void Sparkline::setValues(QVector<double> values)
{
    m_values = std::move(values);
    update();
}

void Sparkline::setColor(const QColor& color)
{
    m_color = color;
    update();
}

void Sparkline::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    if (m_values.size() < 2)
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto bounds = std::minmax_element(m_values.cbegin(), m_values.cend());
    const double low = *bounds.first;
    const double span = std::max(1e-6, *bounds.second - low);
    const double step = double(width() - 2) / double(m_values.size() - 1);

    QPainterPath path;
    for (int i = 0; i < m_values.size(); ++i) {
        const double x = 1 + i * step;
        const double y = height() - 2 - ((m_values.at(i) - low) / span) * (height() - 4);
        if (i == 0)
            path.moveTo(x, y);
        else
            path.lineTo(x, y);
    }
    painter.setPen(QPen(m_color, 1.4));
    painter.drawPath(path);
}

}  // namespace aquatrack::ui
