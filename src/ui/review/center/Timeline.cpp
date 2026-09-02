#include "ui/review/center/Timeline.h"

#include "ui/AppTheme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QToolTip>

#include <algorithm>
#include <cmath>
#include <utility>

namespace aquatrack::ui {
namespace {

// A thin rule, not a ribbon: the whole strip is 30px so it reads as a
// companion to the transport above rather than a second panel.
constexpr int kHeight = 30;
constexpr int kLaneY = 20;    // baseline the marks sit on
constexpr int kTickHalf = 6;  // marks extend this far either side
constexpr int kTickWidth = 2;
constexpr int kGutter = 26;   // room for the "ID" caption
constexpr int kHitSlop = 4;
constexpr int kThumbGap = 6;

QString clockLabel(qint64 milliseconds)
{
    const qint64 seconds = std::max<qint64>(0, milliseconds) / 1000;
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace

// ----------------------------------------------------------- EventTimeline

EventTimeline::EventTimeline(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(kHeight);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
}

void EventTimeline::setSequenceLength(int frames, int frameRate)
{
    m_frames = std::max(0, frames);
    m_frameRate = std::max(1, frameRate);
    update();
}

void EventTimeline::setMarkers(QVector<Marker> markers)
{
    m_markers = std::move(markers);
    m_hoveredMarker = -1;
    update();
}

void EventTimeline::setTicks(QVector<Tick> ticks)
{
    m_ticks = std::move(ticks);
    m_hoveredTick = -1;
    update();
}

void EventTimeline::setCurrentFrame(int frame)
{
    if (m_currentFrame == frame)
        return;
    m_currentFrame = frame;
    update();
}

void EventTimeline::clear()
{
    m_markers.clear();
    m_ticks.clear();
    m_frames = 0;
    m_currentFrame = 0;
    m_hoveredMarker = -1;
    m_hoveredTick = -1;
    update();
}

QRectF EventTimeline::lane() const
{
    return QRectF(kGutter, 0, std::max(1.0, double(width() - kGutter - 4)), kHeight);
}

double EventTimeline::xForFrame(int frame) const
{
    const auto strip = lane();
    if (m_frames <= 0)
        return strip.left();
    return strip.left() + std::clamp(double(frame) / m_frames, 0.0, 1.0) * strip.width();
}

int EventTimeline::frameAt(int x) const
{
    const auto strip = lane();
    if (m_frames <= 0 || strip.width() <= 0)
        return 0;
    const double ratio = std::clamp((x - strip.left()) / strip.width(), 0.0, 1.0);
    return std::clamp(int(std::lround(ratio * m_frames)), 1, m_frames);
}

int EventTimeline::markerAt(const QPoint& point) const
{
    // Event pins live above the rule; track marks live on it.
    if (point.y() > kLaneY - kTickHalf)
        return -1;
    for (int index = 0; index < m_markers.size(); ++index) {
        const auto& marker = m_markers.at(index);
        const double left = xForFrame(marker.startFrame);
        const double right = xForFrame(std::max(marker.startFrame, marker.endFrame));
        if (point.x() >= left - kHitSlop && point.x() <= right + kHitSlop)
            return index;
    }
    return -1;
}

int EventTimeline::tickAt(const QPoint& point) const
{
    if (point.y() < kLaneY - kTickHalf - kHitSlop)
        return -1;
    int best = -1;
    double bestDistance = kHitSlop + 1.0;
    for (int index = 0; index < m_ticks.size(); ++index) {
        const double distance = std::abs(xForFrame(m_ticks.at(index).frame) - point.x());
        if (distance < bestDistance) {
            bestDistance = distance;
            best = index;
        }
    }
    return best;
}

void EventTimeline::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QFont caption = painter.font();
    caption.setPointSizeF(7.2);
    painter.setFont(caption);

    if (m_frames <= 0) {
        painter.setPen(palette::TextDim);
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("Track marks appear once a sequence is open"));
        return;
    }

    const auto strip = lane();

    // ---- Caption and baseline -----------------------------------------
    painter.setPen(palette::TextDim);
    painter.drawText(QRectF(0, kLaneY - 7, kGutter - 6, 14), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("ID"));
    painter.setPen(QPen(palette::LineSoft, 1.0));
    painter.drawLine(QPointF(strip.left(), kLaneY), QPointF(strip.right(), kLaneY));

    // ---- Event intervals, behind the marks ------------------------------
    for (int index = 0; index < m_markers.size(); ++index) {
        const auto& marker = m_markers.at(index);
        const double left = xForFrame(marker.startFrame);
        const double right = xForFrame(std::max(marker.startFrame, marker.endFrame));
        if (right - left <= 2.0)
            continue;
        QColor band = marker.color;
        band.setAlpha(index == m_hoveredMarker ? 70 : 38);
        painter.setPen(Qt::NoPen);
        painter.setBrush(band);
        painter.drawRoundedRect(
            QRectF(left, kLaneY - kTickHalf - 1, right - left, kTickHalf * 2 + 2), 3, 3);
    }

    // ---- Track marks -----------------------------------------------------
    // One mark per entry or exit, coloured by track. Exits are drawn shorter
    // and dimmer so a track leaving does not look like another arriving.
    for (int index = 0; index < m_ticks.size(); ++index) {
        const auto& tick = m_ticks.at(index);
        const double x = xForFrame(tick.frame);
        const bool hovered = index == m_hoveredTick;
        QColor colour = trackColor(tick.trackId);
        if (!tick.entering)
            colour.setAlpha(hovered ? 255 : 150);
        const double half = tick.entering ? kTickHalf : kTickHalf - 2;
        const double thickness = hovered ? kTickWidth + 1.0 : kTickWidth;
        painter.setPen(Qt::NoPen);
        painter.setBrush(colour);
        painter.drawRoundedRect(
            QRectF(x - thickness / 2.0, kLaneY - half, thickness, half * 2), 1, 1);
    }

    // ---- Event pins, above the rule --------------------------------------
    for (int index = 0; index < m_markers.size(); ++index) {
        const auto& marker = m_markers.at(index);
        const double left = xForFrame(marker.startFrame);
        const double right = xForFrame(std::max(marker.startFrame, marker.endFrame));
        const double x = (left + right) / 2.0;
        QPainterPath pin;
        pin.moveTo(x, kLaneY - kTickHalf - 1);
        pin.lineTo(x - 3.5, kLaneY - kTickHalf - 7);
        pin.lineTo(x + 3.5, kLaneY - kTickHalf - 7);
        pin.closeSubpath();
        painter.setPen(Qt::NoPen);
        painter.setBrush(marker.color);
        painter.drawPath(pin);
        if (index == m_hoveredMarker) {
            painter.setPen(QPen(Qt::white, 1.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(pin);
        }
    }

    // ---- Playhead ---------------------------------------------------------
    if (m_currentFrame > 0) {
        const double x = xForFrame(m_currentFrame);
        painter.setPen(QPen(palette::Accent, 1.2));
        painter.drawLine(QPointF(x, kLaneY - kTickHalf - 2), QPointF(x, kLaneY + kTickHalf + 2));
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette::Accent);
        painter.drawEllipse(QPointF(x, kLaneY - kTickHalf - 5), 3.4, 3.4);
    }
}

void EventTimeline::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_frames <= 0)
        return;
    const auto point = event->position().toPoint();
    const int marker = markerAt(point);
    if (marker >= 0) {
        emit frameRequested(m_markers.at(marker).startFrame);
        return;
    }
    // Snapping to a nearby mark makes these small targets clickable.
    const int tick = tickAt(point);
    emit frameRequested(tick >= 0 ? m_ticks.at(tick).frame : frameAt(point.x()));
}

void EventTimeline::mouseMoveEvent(QMouseEvent* event)
{
    const auto point = event->position().toPoint();
    if (event->buttons() & Qt::LeftButton) {
        if (m_frames > 0)
            emit frameRequested(frameAt(point.x()));
        return;
    }

    const int marker = markerAt(point);
    const int tick = marker >= 0 ? -1 : tickAt(point);
    if (marker != m_hoveredMarker || tick != m_hoveredTick) {
        m_hoveredMarker = marker;
        m_hoveredTick = tick;
        update();
    }

    if (marker >= 0) {
        QToolTip::showText(event->globalPosition().toPoint(), m_markers.at(marker).label, this);
    } else if (tick >= 0) {
        const auto& value = m_ticks.at(tick);
        QToolTip::showText(
            event->globalPosition().toPoint(),
            QStringLiteral("Track %1 %2 at %3")
                .arg(value.trackId)
                .arg(value.entering ? QStringLiteral("enters") : QStringLiteral("leaves"))
                .arg(clockLabel(qint64(std::max(0, value.frame - 1)) * 1000 / m_frameRate)),
            this);
    } else {
        QToolTip::hideText();
    }
}

void EventTimeline::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    if (m_hoveredMarker != -1 || m_hoveredTick != -1) {
        m_hoveredMarker = -1;
        m_hoveredTick = -1;
        update();
    }
}

// --------------------------------------------------------------- Filmstrip

Filmstrip::Filmstrip(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(62);
    setCursor(Qt::PointingHandCursor);
}

void Filmstrip::setThumbnails(QVector<Thumbnail> thumbnails)
{
    m_thumbnails = std::move(thumbnails);
    update();
}

void Filmstrip::setCurrentFrame(int frame)
{
    if (m_currentFrame == frame)
        return;
    m_currentFrame = frame;
    update();
}

void Filmstrip::clear()
{
    m_thumbnails.clear();
    m_currentFrame = 0;
    update();
}

int Filmstrip::indexAt(const QPoint& point) const
{
    if (m_thumbnails.isEmpty())
        return -1;
    const double cellWidth =
        (width() - kThumbGap * (m_thumbnails.size() - 1)) / double(m_thumbnails.size());
    const int index = int(point.x() / (cellWidth + kThumbGap));
    return index >= 0 && index < m_thumbnails.size() ? index : -1;
}

void Filmstrip::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (m_thumbnails.isEmpty()) {
        painter.setPen(palette::TextDim);
        QFont font = painter.font();
        font.setPointSizeF(8.0);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("Preview frames appear once a sequence is open"));
        return;
    }

    const double cellWidth =
        (width() - kThumbGap * (m_thumbnails.size() - 1)) / double(m_thumbnails.size());
    QFont caption = painter.font();
    caption.setPointSizeF(7.2);
    painter.setFont(caption);

    int activeIndex = 0;
    for (int i = 0; i < m_thumbnails.size(); ++i)
        if (m_thumbnails.at(i).frame <= m_currentFrame)
            activeIndex = i;

    for (int i = 0; i < m_thumbnails.size(); ++i) {
        const auto& thumbnail = m_thumbnails.at(i);
        const QRectF cell(i * (cellWidth + kThumbGap), 0, cellWidth, height() - 13);
        painter.save();
        QPainterPath clip;
        clip.addRoundedRect(cell, 6, 6);
        painter.setClipPath(clip);
        if (thumbnail.image.isNull()) {
            painter.fillRect(cell, palette::Sunken);
        } else {
            const QSize scaled =
                thumbnail.image.size().scaled(cell.size().toSize(), Qt::KeepAspectRatioByExpanding);
            QRectF source(0, 0, scaled.width(), scaled.height());
            source.moveCenter(cell.center());
            painter.drawImage(source, thumbnail.image);
        }
        painter.restore();

        const bool active = i == activeIndex;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(active ? palette::Accent : palette::Line, active ? 2.0 : 1.0));
        painter.drawRoundedRect(cell.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);

        painter.setPen(active ? palette::Accent : palette::TextDim);
        painter.drawText(QRectF(cell.left(), height() - 12, cell.width(), 12), Qt::AlignCenter,
                         thumbnail.caption);
    }
}

void Filmstrip::mousePressEvent(QMouseEvent* event)
{
    const int index = indexAt(event->position().toPoint());
    if (index >= 0)
        emit frameRequested(m_thumbnails.at(index).frame);
}

}  // namespace aquatrack::ui
