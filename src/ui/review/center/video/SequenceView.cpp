#include "ui/review/center/video/SequenceView.h"

#include "ui/AppTheme.h"

#include <QFontMetrics>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QSizePolicy>

#include <algorithm>
#include <optional>
#include <utility>

using namespace aquatrack::ui;

SequenceView::SequenceView(QWidget* parent) : QWidget(parent)
{
    // Small enough that a 720 px tall window fits the canvas card's header
    // and transport around it.
    setMinimumSize(320, 130);
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

    // ---- Boxes -------------------------------------------------------
    // Species on screen and how many of each. Their colour only means
    // something when boxes are coloured by species (track colours off).
    struct LegendEntry {
        QString name;
        QColor colour;
        int count = 0;
    };
    QVector<LegendEntry> legend;
    struct Tag {
        QRectF box;
        QColor colour;
        int trackId = 0;
        float confidence = 0.0F;
    };
    QVector<Tag> tags;
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
            painter.setPen(QPen(colour, 1.5));
            painter.drawRoundedRect(box, 2.5, 2.5);
            // Corner ticks read as a targeting reticle at small box sizes.
            const qreal tick = std::min<qreal>(6.0, std::min(box.width(), box.height()) / 3.0);
            painter.setPen(QPen(colour, 2.4, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(box.topLeft(), box.topLeft() + QPointF(tick, 0));
            painter.drawLine(box.topLeft(), box.topLeft() + QPointF(0, tick));
            painter.drawLine(box.bottomRight(), box.bottomRight() - QPointF(tick, 0));
            painter.drawLine(box.bottomRight(), box.bottomRight() - QPointF(0, tick));
        }
        if (m_overlays.showIds)
            tags.push_back({box, colour, annotation.trackId, annotation.confidence});

        const QString species = aquatrack::BrackishMotSequence::className(annotation.classId);
        const auto known = std::find_if(legend.begin(), legend.end(),
                                        [&](const auto& item) { return item.name == species; });
        if (known == legend.end())
            legend.push_back({species, speciesColor(annotation.classId), 1});
        else
            ++known->count;
    }

    // ---- ID tags ------------------------------------------------------
    // Small tags hugging a corner of their box. Each tries above-left,
    // below-left, above-right and below-right; a tag that would cover
    // another is left out (its box keeps its colour), so a school of fish
    // stays readable instead of turning into a pile of labels. The most
    // confident detections are placed first.
    QFont tagFont = painter.font();
    tagFont.setPointSizeF(7.2);
    tagFont.setWeight(QFont::DemiBold);
    painter.setFont(tagFont);
    const QFontMetricsF tagMetrics(tagFont);
    std::stable_sort(tags.begin(), tags.end(), [](const Tag& left, const Tag& right) {
        return left.confidence > right.confidence;
    });
    QVector<QRectF> placed;
    const QRectF bounds = QRectF(target).adjusted(2, 2, -2, -2);
    for (const Tag& tag : tags) {
        const QString text = QString::number(tag.trackId);
        const QSizeF size(tagMetrics.horizontalAdvance(text) + 8, tagMetrics.height() + 1);
        const QRectF& box = tag.box;
        const QPointF candidates[] = {
            {box.left(), box.top() - size.height() - 2},
            {box.left(), box.bottom() + 2},
            {box.right() - size.width(), box.top() - size.height() - 2},
            {box.right() - size.width(), box.bottom() + 2},
        };
        std::optional<QRectF> spot;
        for (const QPointF& corner : candidates) {
            const QRectF rect(corner, size);
            if (!bounds.contains(rect))
                continue;
            const bool clash = std::any_of(placed.cbegin(), placed.cend(), [&](const QRectF& other) {
                return other.adjusted(-1, -1, 1, 1).intersects(rect);
            });
            if (!clash) {
                spot = rect;
                break;
            }
        }
        if (!spot)
            continue;
        placed.push_back(*spot);
        painter.setPen(Qt::NoPen);
        painter.setBrush(tag.colour);
        painter.drawRoundedRect(*spot, 3, 3);
        // Dark text on light tags, white on dark ones.
        painter.setPen(tag.colour.lightnessF() > 0.55 ? QColor(8, 10, 14) : QColor(Qt::white));
        painter.drawText(*spot, Qt::AlignCenter, text);
    }

    // ---- Species count, top right -------------------------------------
    // One quiet chip per species; the frame number and time are already in
    // the transport bar, so nothing else covers the picture.
    if (!legend.isEmpty()) {
        QFont chipFont = painter.font();
        chipFont.setPointSizeF(8.0);
        chipFont.setWeight(QFont::Medium);
        painter.setFont(chipFont);
        const QFontMetricsF chipMetrics(chipFont);
        const bool dots = !m_overlays.showTracks;
        qreal right = target.right() - 10;
        const qreal top = target.top() + 10;
        const qreal height = chipMetrics.height() + 8;
        for (auto it = legend.crbegin(); it != legend.crend(); ++it) {
            const QString count = QLocale::system().toString(it->count);
            const qreal width = 10 + (dots ? 12 : 0) + chipMetrics.horizontalAdvance(it->name) + 6 +
                                chipMetrics.horizontalAdvance(count) + 10;
            const QRectF chip(right - width, top, width, height);
            if (chip.left() < target.left() + 10)
                break;
            painter.setPen(QPen(QColor(255, 255, 255, 26), 1.0));
            painter.setBrush(QColor(8, 10, 15, 190));
            painter.drawRoundedRect(chip, height / 2, height / 2);
            qreal x = chip.left() + 10;
            if (dots) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(it->colour);
                painter.drawEllipse(QRectF(x, chip.center().y() - 3.5, 7, 7));
                x += 12;
            }
            painter.setPen(palette::TextSecondary);
            const QRectF nameRect(x, chip.top(), chipMetrics.horizontalAdvance(it->name) + 1, height);
            painter.drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, it->name);
            painter.setPen(palette::TextPrimary);
            painter.drawText(QRectF(nameRect.right() + 6, chip.top(),
                                    chipMetrics.horizontalAdvance(count) + 1, height),
                             Qt::AlignLeft | Qt::AlignVCenter, count);
            right = chip.left() - 6;
        }
    }
}
