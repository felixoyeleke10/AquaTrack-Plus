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

// ---- Track lanes geometry ----------------------------------------------
constexpr int kGutter = 60;      // track labels
constexpr int kPinRow = 16;      // event pins above the lanes
constexpr int kAxis = 16;        // time labels below the lanes
constexpr int kRightPad = 6;
constexpr int kEmptyHeight = 44;
constexpr int kHitSlop = 4;
constexpr int kAxisDivisions = 4;
// More tracks than this and tracks that never overlap share rows.
constexpr int kMaxDedicatedRows = 12;

// ---- Filmstrip ----------------------------------------------------------
constexpr int kThumbGap = 6;

}  // namespace

// -------------------------------------------------------------- TrackLanes

TrackLanes::TrackLanes(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("trackLanes"));
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
}

void TrackLanes::setSequenceLength(int frames, int frameRate)
{
    m_frames = std::max(0, frames);
    m_frameRate = std::max(1, frameRate);
    update();
}

void TrackLanes::setLanes(QVector<Lane> lanes)
{
    // Earliest first, so the chart reads left to right as animals arrive.
    std::sort(lanes.begin(), lanes.end(), [](const Lane& left, const Lane& right) {
        const int leftStart = left.segments.isEmpty() ? 0 : left.segments.first().first;
        const int rightStart = right.segments.isEmpty() ? 0 : right.segments.first().first;
        if (leftStart != rightStart)
            return leftStart < rightStart;
        return left.trackId < right.trackId;
    });
    m_lanes = std::move(lanes);
    m_hoveredLane = -1;

    // A dedicated, labelled row per track while that stays readable;
    // beyond that, pack: each track goes in the first row that is free by
    // the time it starts, with a little air between neighbours.
    m_rowOfLane.clear();
    m_packed = m_lanes.size() > kMaxDedicatedRows;
    if (!m_packed) {
        for (int index = 0; index < m_lanes.size(); ++index)
            m_rowOfLane.push_back(index);
        m_rows = int(m_lanes.size());
    } else {
        const int air = std::max(1, m_frames / 150);
        QVector<int> rowEnds;
        for (const auto& lane : m_lanes) {
            const int first = lane.segments.isEmpty() ? 0 : lane.segments.first().first;
            const int last = lane.segments.isEmpty() ? 0 : lane.segments.last().second;
            int row = 0;
            while (row < rowEnds.size() && rowEnds[row] + air >= first)
                ++row;
            if (row == rowEnds.size())
                rowEnds.push_back(last);
            else
                rowEnds[row] = last;
            m_rowOfLane.push_back(row);
        }
        m_rows = int(rowEnds.size());
    }
    updateGeometry();
    update();
}

void TrackLanes::setMarkers(QVector<Marker> markers)
{
    m_markers = std::move(markers);
    m_hoveredMarker = -1;
    update();
}

void TrackLanes::setLinks(QVector<Link> links)
{
    m_links = std::move(links);
    update();
}

void TrackLanes::setCurrentFrame(int frame)
{
    if (m_currentFrame == frame)
        return;
    m_currentFrame = frame;
    update();
}

void TrackLanes::clear()
{
    m_lanes.clear();
    m_rowOfLane.clear();
    m_rows = 0;
    m_packed = false;
    m_markers.clear();
    m_links.clear();
    m_frames = 0;
    m_currentFrame = 0;
    m_hoveredLane = -1;
    m_hoveredMarker = -1;
    updateGeometry();
    update();
}

qreal TrackLanes::idealPitch() const
{
    // Comfortable lanes for a handful of tracks; thinner ones for crowded
    // sequences so the chart never pushes the frame view off screen.
    const int count = m_rows;
    if (count <= 6)
        return 18.0;
    if (count <= 12)
        return 14.0;
    return std::max(4.0, 168.0 / count);
}

qreal TrackLanes::lanePitch() const
{
    if (m_lanes.isEmpty())
        return idealPitch();
    // Whatever height the pane has, shared between lanes: every row always
    // fits, hairline-thin for a crowded sequence (an overview; make the
    // pane taller for detail) rather than running off the bottom.
    const qreal available = std::max(0, height() - kPinRow - kAxis);
    return std::max(0.5, std::min(idealPitch(), available / std::max(1, m_rows)));
}

QSize TrackLanes::sizeHint() const
{
    if (m_lanes.isEmpty())
        return {400, kEmptyHeight};
    return {400, kPinRow + int(std::ceil(m_rows * idealPitch())) + kAxis};
}

QSize TrackLanes::minimumSizeHint() const
{
    if (m_lanes.isEmpty())
        return {120, kEmptyHeight};
    // Readable bars for up to six lanes even in a short window; the
    // filmstrip below gives way first (see Filmstrip).
    const int lanes = int(std::ceil(std::min(m_rows, 6) * 8.0));
    return {120, kPinRow + lanes + kAxis};
}

QRectF TrackLanes::plot() const
{
    return QRectF(kGutter, kPinRow, std::max(1.0, double(width() - kGutter - kRightPad)),
                  m_rows * lanePitch());
}

QRectF TrackLanes::laneRect(int index) const
{
    const auto area = plot();
    const qreal pitch = lanePitch();
    const qreal bar = pitch >= 10.0 ? pitch - 6.0
                      : pitch >= 3.0  ? pitch - 1.0
                                      : std::max(0.8, pitch);  // packed rows touch
    const int row = index >= 0 && index < m_rowOfLane.size() ? m_rowOfLane[index] : index;
    const qreal top = area.top() + row * pitch + (pitch - bar) / 2.0;
    return QRectF(area.left(), top, area.width(), bar);
}

double TrackLanes::xForFrame(int frame) const
{
    // Frame f (1-based) occupies [(f-1)/N, f/N] of the plot width.
    const auto area = plot();
    if (m_frames <= 0)
        return area.left();
    return area.left() + std::clamp(double(frame - 1) / m_frames, 0.0, 1.0) * area.width();
}

int TrackLanes::frameAt(double x) const
{
    const auto area = plot();
    if (m_frames <= 0)
        return 0;
    const double ratio = std::clamp((x - area.left()) / area.width(), 0.0, 1.0);
    return std::clamp(int(ratio * m_frames) + 1, 1, m_frames);
}

int TrackLanes::laneAt(const QPoint& point) const
{
    const auto area = plot();
    if (point.y() < area.top() || point.y() >= area.bottom())
        return -1;
    const int row = int((point.y() - area.top()) / lanePitch());
    if (row < 0 || row >= m_rows)
        return -1;
    if (!m_packed)
        return row < m_lanes.size() ? row : -1;
    // Several tracks share a packed row: the one under the pointer, or the
    // nearest within a few pixels.
    const int frame = frameAt(point.x());
    const int slop =
        std::max(1, int(std::ceil(kHitSlop * double(m_frames) / std::max(1.0, area.width()))));
    int best = -1;
    int bestDistance = slop + 1;
    for (int index = 0; index < m_lanes.size(); ++index) {
        if (m_rowOfLane[index] != row || m_lanes[index].segments.isEmpty())
            continue;
        const int first = m_lanes[index].segments.first().first;
        const int last = m_lanes[index].segments.last().second;
        const int distance = frame < first ? first - frame : frame > last ? frame - last : 0;
        if (distance < bestDistance) {
            best = index;
            bestDistance = distance;
        }
    }
    return best;
}

int TrackLanes::markerAt(const QPoint& point) const
{
    if (point.y() > kPinRow + 2)
        return -1;
    for (int index = 0; index < m_markers.size(); ++index) {
        const auto& marker = m_markers.at(index);
        const double left = xForFrame(marker.startFrame);
        const double right = xForFrame(std::max(marker.startFrame, marker.endFrame) + 1);
        if (point.x() >= left - kHitSlop && point.x() <= right + kHitSlop)
            return index;
    }
    return -1;
}

int TrackLanes::laneIndexOf(int trackId) const
{
    for (int index = 0; index < m_lanes.size(); ++index)
        if (m_lanes.at(index).trackId == trackId)
            return index;
    return -1;
}

bool TrackLanes::visibleAt(const Lane& lane, int frame) const
{
    return std::any_of(lane.segments.cbegin(), lane.segments.cend(), [frame](const auto& run) {
        return frame >= run.first && frame <= run.second;
    });
}

QString TrackLanes::clock(int frame) const
{
    const qint64 seconds = qint64(std::max(0, frame - 1)) / m_frameRate;
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

void TrackLanes::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QFont small = painter.font();
    small.setPointSizeF(7.4);
    painter.setFont(small);

    if (m_frames <= 0 || m_lanes.isEmpty()) {
        painter.setPen(palette::TextDim);
        painter.drawText(rect(), Qt::AlignCenter,
                         m_frames <= 0 ? QStringLiteral("Track lanes appear once a sequence is open")
                                       : QStringLiteral("No tracks in this sequence"));
        return;
    }

    const auto area = plot();
    const qreal pitch = lanePitch();
    const bool labelled = !m_packed && pitch >= 11.0;

    // ---- Time grid and axis --------------------------------------------
    for (int step = 0; step <= kAxisDivisions; ++step) {
        const int frame = 1 + int(std::lround(double(m_frames - 1) * step / kAxisDivisions));
        const double x = area.left() + area.width() * step / kAxisDivisions;
        painter.setPen(QPen(palette::LineSoft, 1.0));
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        painter.setPen(palette::TextDim);
        const Qt::Alignment align = step == 0                ? Qt::AlignLeft
                                    : step == kAxisDivisions ? Qt::AlignRight
                                                             : Qt::AlignHCenter;
        const QRectF label = step == 0 ? QRectF(x, area.bottom() + 2, 60, kAxis - 2)
                             : step == kAxisDivisions
                                 ? QRectF(x - 60, area.bottom() + 2, 60, kAxis - 2)
                                 : QRectF(x - 30, area.bottom() + 2, 60, kAxis - 2);
        painter.drawText(label, align | Qt::AlignVCenter, clock(frame));
    }

    // ---- Interval events shade the whole chart ---------------------------
    for (const auto& marker : m_markers) {
        if (marker.endFrame <= marker.startFrame)
            continue;
        QColor band = marker.color;
        band.setAlpha(26);
        const double left = xForFrame(marker.startFrame);
        const double right = xForFrame(marker.endFrame + 1);
        painter.fillRect(QRectF(left, area.top(), right - left, area.height()), band);
    }

    // ---- Lanes ----------------------------------------------------------
    if (!labelled) {
        painter.setPen(palette::TextDim);
        painter.drawText(QRectF(0, area.top(), kGutter - 8, 14), Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("%1 tracks").arg(m_lanes.size()));
        if (m_packed && area.height() >= 30)
            painter.drawText(QRectF(0, area.top() + 13, kGutter - 8, 14),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QStringLiteral("%1 rows").arg(m_rows));
    }
    // Packed rows: one faint rail per row, drawn once, rather than one per
    // track stacked on top of each other.
    if (m_packed) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette::LineSoft);
        const qreal barHeight = pitch >= 10.0 ? pitch - 6.0 : std::max(2.0, pitch - 1.0);
        const qreal rail = std::min<qreal>(2.0, barHeight);
        for (int row = 0; row < m_rows; ++row) {
            const qreal middle = area.top() + row * pitch + pitch / 2.0;
            painter.drawRoundedRect(QRectF(area.left(), middle - rail / 2, area.width(), rail), 1, 1);
        }
    }
    for (int index = 0; index < m_lanes.size(); ++index) {
        const auto& lane = m_lanes.at(index);
        const QRectF bar = laneRect(index);
        const bool onScreen = visibleAt(lane, m_currentFrame);
        const bool hovered = index == m_hoveredLane;

        // Faint rail: the whole sequence, so gaps read as gaps.
        painter.setPen(Qt::NoPen);
        if (!m_packed) {
            painter.setBrush(palette::LineSoft);
            const qreal rail = std::min<qreal>(2.0, bar.height());
            painter.drawRoundedRect(
                QRectF(bar.left(), bar.center().y() - rail / 2, bar.width(), rail), 1, 1);
        }

        QColor fill = lane.color;
        fill.setAlpha(hovered ? 255 : onScreen ? 230 : 120);
        painter.setBrush(fill);
        const qreal radius = std::min<qreal>(3.0, bar.height() / 2.0);
        for (const auto& run : lane.segments) {
            const double left = xForFrame(run.first);
            const double right = std::max(left + 2.0, xForFrame(run.second + 1));
            painter.drawRoundedRect(QRectF(left, bar.top(), right - left, bar.height()), radius,
                                    radius);
        }

        if (labelled) {
            QFont labelFont = small;
            labelFont.setBold(onScreen || hovered);
            painter.setFont(labelFont);
            painter.setPen(onScreen || hovered ? lane.color : palette::TextDim);
            painter.drawText(QRectF(0, bar.center().y() - pitch / 2, kGutter - 8, pitch),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             QStringLiteral("ID %1").arg(lane.trackId));
            painter.setFont(small);
        }
    }

    // ---- Likely identity switches ----------------------------------------
    QPen linkPen(palette::Danger, 1.4, Qt::DashLine, Qt::RoundCap);
    linkPen.setDashPattern({3, 2.5});
    for (const auto& link : m_packed ? QVector<Link>() : m_links) {
        const int from = laneIndexOf(link.fromTrack);
        const int to = laneIndexOf(link.toTrack);
        if (from < 0 || to < 0)
            continue;
        const QPointF start(xForFrame(link.fromFrame + 1), laneRect(from).center().y());
        const QPointF end(xForFrame(link.toFrame), laneRect(to).center().y());
        QPainterPath path(start);
        const double bend = std::max(8.0, (end.x() - start.x()) / 2.0);
        path.cubicTo(start + QPointF(bend, 0), end - QPointF(bend, 0), end);
        painter.setPen(linkPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette::Danger);
        painter.drawEllipse(start, 2.6, 2.6);
        painter.drawEllipse(end, 2.6, 2.6);
    }

    // ---- Event pins -------------------------------------------------------
    for (int index = 0; index < m_markers.size(); ++index) {
        const auto& marker = m_markers.at(index);
        const double left = xForFrame(marker.startFrame);
        const double right = xForFrame(std::max(marker.startFrame, marker.endFrame) + 1);
        const double x = (left + right) / 2.0;
        QPainterPath pin;
        pin.moveTo(x, kPinRow - 3);
        pin.lineTo(x - 4.0, kPinRow - 10);
        pin.lineTo(x + 4.0, kPinRow - 10);
        pin.closeSubpath();
        painter.setPen(index == m_hoveredMarker ? QPen(Qt::white, 1.0) : Qt::NoPen);
        painter.setBrush(marker.color);
        painter.drawPath(pin);
    }

    // ---- Playhead -----------------------------------------------------------
    if (m_currentFrame > 0) {
        const double x = (xForFrame(m_currentFrame) + xForFrame(m_currentFrame + 1)) / 2.0;
        painter.setPen(QPen(palette::Accent, 1.4));
        painter.drawLine(QPointF(x, kPinRow - 2), QPointF(x, area.bottom() + 1));
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette::AccentHover);
        painter.drawEllipse(QPointF(x, kPinRow - 3), 3.2, 3.2);
    }
}

void TrackLanes::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_frames <= 0)
        return;
    const auto point = event->position().toPoint();
    const int marker = markerAt(point);
    if (marker >= 0) {
        emit frameRequested(m_markers.at(marker).startFrame);
        return;
    }
    // A click on a track's label jumps to where that track begins.
    if (point.x() < kGutter) {
        const int lane = laneAt(point);
        if (lane >= 0 && !m_lanes.at(lane).segments.isEmpty())
            emit frameRequested(m_lanes.at(lane).segments.first().first);
        return;
    }
    emit frameRequested(frameAt(point.x()));
}

void TrackLanes::mouseMoveEvent(QMouseEvent* event)
{
    const auto point = event->position().toPoint();
    if (event->buttons() & Qt::LeftButton) {
        if (m_frames > 0 && point.x() >= kGutter)
            emit frameRequested(frameAt(point.x()));
        return;
    }

    const int marker = markerAt(point);
    const int lane = marker >= 0 ? -1 : laneAt(point);
    if (marker != m_hoveredMarker || lane != m_hoveredLane) {
        m_hoveredMarker = marker;
        m_hoveredLane = lane;
        update();
    }

    if (marker >= 0) {
        QToolTip::showText(event->globalPosition().toPoint(), m_markers.at(marker).label, this);
    } else if (lane >= 0) {
        const auto& value = m_lanes.at(lane);
        const int first = value.segments.isEmpty() ? 0 : value.segments.first().first;
        const int last = value.segments.isEmpty() ? 0 : value.segments.last().second;
        const int gaps = std::max(0, int(value.segments.size()) - 1);
        QString text = QStringLiteral("Track %1  ·  %2\nFrames %3–%4 (%5–%6)  ·  "
                                      "%7 observation(s)\n%8")
                           .arg(value.trackId)
                           .arg(value.species.isEmpty() ? QStringLiteral("Unknown") : value.species)
                           .arg(first)
                           .arg(last)
                           .arg(clock(first), clock(last))
                           .arg(value.observations)
                           .arg(gaps == 0 ? QStringLiteral("Continuous, no gaps")
                                          : QStringLiteral("Lost %1 time(s) along the way").arg(gaps));
        QToolTip::showText(event->globalPosition().toPoint(), text, this);
    } else {
        QToolTip::hideText();
    }
}

void TrackLanes::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    if (m_hoveredMarker != -1 || m_hoveredLane != -1) {
        m_hoveredMarker = -1;
        m_hoveredLane = -1;
        update();
    }
}

// --------------------------------------------------------------- Filmstrip

Filmstrip::Filmstrip(QWidget* parent) : QWidget(parent)
{
    // Lowest priority in the timeline card: it shrinks before the track
    // lanes do, and steps aside entirely when there is no room for it.
    setMinimumHeight(0);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    setCursor(Qt::PointingHandCursor);
}

QSize Filmstrip::sizeHint() const
{
    return {400, 62};
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
    if (height() < 34)
        return;  // too short to show a recognisable frame
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
