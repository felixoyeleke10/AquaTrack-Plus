#include "ui/review/center/video/SequenceView.h"

#include "ui/AppTheme.h"

#include <QFontMetrics>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QSizePolicy>

#include <algorithm>
#include <utility>

using namespace aquatrack::ui;

namespace {

QString formatClock(qint64 milliseconds)
{
    const qint64 seconds = milliseconds / 1000;
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

// Rounded translucent plate behind canvas read-outs, so text stays legible
// over bright water as well as dark seabed.
void drawPlate(QPainter& painter, const QRectF& box)
{
    painter.setPen(QPen(QColor(255, 255, 255, 28), 1.0));
    painter.setBrush(QColor(8, 10, 15, 214));
    painter.drawRoundedRect(box, 7, 7);
}

}  // namespace

SequenceView::SequenceView(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(420, 190);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SequenceView::setFrame(QImage image, QVector<aquatrack::GroundTruth> annotations,
                            QSize sourceSize)
{
    m_sourceSize = sourceSize.isValid() ? sourceSize : image.size();
    m_image = std::move(image);
    m_annotations = std::move(annotations);
    update();
}

void SequenceView::setOverlays(const Overlays& overlays)
{
    m_overlays = overlays;
    update();
}

void SequenceView::setFrameInfo(int frame, int total, qint64 timeMs)
{
    m_frame = frame;
    m_total = total;
    m_timeMs = timeMs;
    update();
}

void SequenceView::setTrails(QHash<int, QVector<QPointF>> trails)
{
    m_trails = std::move(trails);
    update();
}

void SequenceView::clear()
{
    m_image = QImage();
    m_sourceSize = QSize();
    m_annotations.clear();
    m_trails.clear();
    m_frame = m_total = 0;
    m_timeMs = 0;
    update();
}

bool SequenceView::passes(const aquatrack::GroundTruth& annotation) const
{
    if (m_overlays.speciesClassId >= 0 && annotation.classId != m_overlays.speciesClassId)
        return false;
    return double(annotation.confidence) >= m_overlays.minConfidence;
}

QVector<aquatrack::GroundTruth> SequenceView::visibleAnnotations() const
{
    QVector<aquatrack::GroundTruth> visible;
    for (const auto& annotation : m_annotations)
        if (passes(annotation))
            visible.push_back(annotation);
    return visible;
}

void SequenceView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // Letterbox background.
    painter.fillRect(rect(), QColor(0x02, 0x03, 0x05));

    if (m_image.isNull()) {
        painter.setPen(palette::TextDim);
        QFont font = painter.font();
        font.setPointSizeF(10.0);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("Open a sequence or video to begin review"));
        return;
    }

    const QSize scaled = (m_sourceSize.isValid() ? m_sourceSize : m_image.size())
                             .scaled(size(), Qt::KeepAspectRatio);
    const QRect target((width() - scaled.width()) / 2, (height() - scaled.height()) / 2,
                       scaled.width(), scaled.height());
    painter.drawImage(target, m_image);

    // Annotations are in source pixels, which differ from the decoded image's
    // when a large video was scaled down for display.
    const QSize space = m_sourceSize.isValid() ? m_sourceSize : m_image.size();
    const qreal scaleX = qreal(target.width()) / space.width();
    const qreal scaleY = qreal(target.height()) / space.height();
    const auto toView = [&](const QPointF& point) {
        return QPointF(target.x() + point.x() * scaleX, target.y() + point.y() * scaleY);
    };

    // ---- Track trails, drawn under the boxes --------------------------
    if (m_overlays.showTracks) {
        for (auto it = m_trails.cbegin(); it != m_trails.cend(); ++it) {
            const auto& points = it.value();
            if (points.size() < 2)
                continue;
            const QColor colour = trackColor(it.key());
            QPainterPath path;
            path.moveTo(toView(points.first()));
            for (int i = 1; i < points.size(); ++i)
                path.lineTo(toView(points.at(i)));
            // Fade the tail so the newest motion reads strongest.
            QPen pen(colour, 1.8, Qt::DashLine, Qt::RoundCap);
            QColor faded = colour;
            faded.setAlpha(150);
            pen.setColor(faded);
            pen.setDashPattern({4, 3});
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }
    }

    // ---- Boxes and labels --------------------------------------------
    QFont labelFont = painter.font();
    labelFont.setPointSizeF(8.0);
    labelFont.setBold(true);
    painter.setFont(labelFont);
    const QFontMetrics metrics(labelFont);

    // Species on screen and how many of each. Their colour only means
    // something when boxes are coloured by species (track colours off).
    struct LegendEntry {
        QString name;
        QColor colour;
        int count = 0;
    };
    QVector<LegendEntry> legend;
    for (const auto& annotation : m_annotations) {
        if (!passes(annotation))
            continue;
        const QColor colour = m_overlays.showTracks ? trackColor(annotation.trackId)
                                                    : speciesColor(annotation.classId);
        const QRectF box(target.x() + annotation.box.x() * scaleX,
                         target.y() + annotation.box.y() * scaleY,
                         annotation.box.width() * scaleX, annotation.box.height() * scaleY);

        if (m_overlays.showDetections) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(colour, 1.8));
            painter.drawRoundedRect(box, 3, 3);

            // Corner ticks read as a targeting reticle at small box sizes.
            const qreal tick = std::min<qreal>(7.0, std::min(box.width(), box.height()) / 3.0);
            painter.setPen(QPen(colour, 2.6));
            painter.drawLine(box.topLeft(), box.topLeft() + QPointF(tick, 0));
            painter.drawLine(box.topLeft(), box.topLeft() + QPointF(0, tick));
            painter.drawLine(box.bottomRight(), box.bottomRight() - QPointF(tick, 0));
            painter.drawLine(box.bottomRight(), box.bottomRight() - QPointF(0, tick));
        }

        if (m_overlays.showIds) {
            const QString text = QStringLiteral("ID %1").arg(annotation.trackId);
            QRectF plate(box.left(), box.top() - metrics.height() - 5,
                         metrics.horizontalAdvance(text) + 12, metrics.height() + 4);
            if (plate.top() < target.top())
                plate.moveTop(box.top() + 3);
            painter.setPen(Qt::NoPen);
            painter.setBrush(colour);
            painter.drawRoundedRect(plate, 3, 3);
            painter.setPen(QColor(6, 8, 12));
            painter.drawText(plate, Qt::AlignCenter, text);
        }

        const QString species = aquatrack::BrackishMotSequence::className(annotation.classId);
        const auto known = std::find_if(legend.begin(), legend.end(),
                                        [&](const auto& item) { return item.name == species; });
        if (known == legend.end())
            legend.push_back({species, speciesColor(annotation.classId), 1});
        else
            ++known->count;
    }

    // ---- Frame and time plate, top left -------------------------------
    QFont plateFont = painter.font();
    plateFont.setPointSizeF(8.2);
    plateFont.setBold(false);
    painter.setFont(plateFont);
    const QFontMetrics plateMetrics(plateFont);

    if (m_total > 0) {
        const QLocale locale = QLocale::system();
        const QString frameText = QStringLiteral("Frame %1 of %2")
                                      .arg(locale.toString(m_frame), locale.toString(m_total));
        const QString timeText = formatClock(m_timeMs);
        const int textWidth = std::max(plateMetrics.horizontalAdvance(frameText),
                                       plateMetrics.horizontalAdvance(timeText));
        const QRectF plate(target.left() + 10, target.top() + 10, textWidth + 20,
                           plateMetrics.height() * 2 + 14);
        drawPlate(painter, plate);
        painter.setPen(palette::TextPrimary);
        painter.drawText(plate.adjusted(10, 6, -10, 0), Qt::AlignLeft | Qt::AlignTop, frameText);
        painter.setPen(palette::TextSecondary);
        painter.drawText(plate.adjusted(10, 0, -10, -6), Qt::AlignLeft | Qt::AlignBottom, timeText);
    }

    // ---- Species legend, top right ------------------------------------
    if (!legend.isEmpty()) {
        const QString heading = QStringLiteral("Species in frame");
        const bool dots = !m_overlays.showTracks;
        int widest = plateMetrics.horizontalAdvance(heading);
        for (const auto& entry : legend)
            widest = std::max(widest, plateMetrics.horizontalAdvance(entry.name) + (dots ? 16 : 0) +
                                          plateMetrics.horizontalAdvance(QString::number(entry.count)) + 14);
        const qreal rowHeight = plateMetrics.height() + 3;
        const QRectF plate(target.right() - widest - 26, target.top() + 10, widest + 24,
                           rowHeight * (legend.size() + 1) + 12);
        drawPlate(painter, plate);

        painter.setPen(palette::TextDim);
        painter.drawText(QRectF(plate.left() + 11, plate.top() + 5, plate.width() - 22, rowHeight),
                         Qt::AlignLeft | Qt::AlignVCenter, heading);
        qreal y = plate.top() + 5 + rowHeight;
        for (const auto& entry : legend) {
            qreal left = plate.left() + 11;
            if (dots) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(entry.colour);
                painter.drawEllipse(QRectF(left, y + rowHeight / 2 - 3.5, 7, 7));
                left += 14;
            }
            const QRectF row(left, y, plate.right() - 11 - left, rowHeight);
            painter.setPen(palette::TextPrimary);
            painter.drawText(row, Qt::AlignLeft | Qt::AlignVCenter, entry.name);
            painter.setPen(palette::TextSecondary);
            painter.drawText(row, Qt::AlignRight | Qt::AlignVCenter, QString::number(entry.count));
            y += rowHeight;
        }
    }
}
