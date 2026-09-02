#include "ui/common/Glyphs.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace aquatrack::ui {
namespace {

// Every glyph is authored on a 24x24 grid and scaled to the target box, so
// stroke weights stay consistent across icon sizes.
constexpr qreal kGrid = 24.0;

void roundedRect(QPainterPath& path, qreal x, qreal y, qreal w, qreal h, qreal r)
{
    path.addRoundedRect(QRectF(x, y, w, h), r, r);
}

void buildPath(Glyph glyph, QPainterPath& stroke, QPainterPath& fill)
{
    switch (glyph) {
    case Glyph::Dashboard:
        roundedRect(stroke, 3, 3, 8, 8, 1.5);
        roundedRect(stroke, 13, 3, 8, 5, 1.5);
        roundedRect(stroke, 13, 10, 8, 11, 1.5);
        roundedRect(stroke, 3, 13, 8, 8, 1.5);
        break;
    case Glyph::Video:
        roundedRect(stroke, 2.5, 5, 14, 14, 2.5);
        stroke.moveTo(16.5, 10);
        stroke.lineTo(21.5, 7);
        stroke.lineTo(21.5, 17);
        stroke.lineTo(16.5, 14);
        stroke.closeSubpath();
        break;
    case Glyph::Review:
        roundedRect(stroke, 2.5, 4, 19, 16, 2.5);
        stroke.moveTo(2.5, 8.5);
        stroke.lineTo(21.5, 8.5);
        fill.addRect(QRectF(9.5, 12, 5, 4));
        break;
    case Glyph::Analytics:
        stroke.moveTo(3, 21);
        stroke.lineTo(21, 21);
        fill.addRect(QRectF(5, 12, 3.4, 7));
        fill.addRect(QRectF(10.3, 7, 3.4, 12));
        fill.addRect(QRectF(15.6, 14, 3.4, 5));
        break;
    case Glyph::Datasets:
        stroke.addEllipse(QRectF(3.5, 3, 17, 5));
        stroke.moveTo(3.5, 5.5);
        stroke.lineTo(3.5, 18.5);
        stroke.moveTo(20.5, 5.5);
        stroke.lineTo(20.5, 18.5);
        stroke.arcMoveTo(QRectF(3.5, 16, 17, 5), 180);
        stroke.arcTo(QRectF(3.5, 16, 17, 5), 180, 180);
        stroke.arcMoveTo(QRectF(3.5, 9.5, 17, 5), 180);
        stroke.arcTo(QRectF(3.5, 9.5, 17, 5), 180, 180);
        break;
    case Glyph::Models:
        stroke.moveTo(12, 2.5);
        stroke.lineTo(21, 7.5);
        stroke.lineTo(21, 16.5);
        stroke.lineTo(12, 21.5);
        stroke.lineTo(3, 16.5);
        stroke.lineTo(3, 7.5);
        stroke.closeSubpath();
        stroke.moveTo(3, 7.5);
        stroke.lineTo(12, 12.5);
        stroke.lineTo(21, 7.5);
        stroke.moveTo(12, 12.5);
        stroke.lineTo(12, 21.5);
        break;
    case Glyph::Settings:
        stroke.addEllipse(QRectF(9, 9, 6, 6));
        stroke.addEllipse(QRectF(3.5, 3.5, 17, 17));
        stroke.moveTo(12, 2);
        stroke.lineTo(12, 5);
        stroke.moveTo(12, 19);
        stroke.lineTo(12, 22);
        stroke.moveTo(2, 12);
        stroke.lineTo(5, 12);
        stroke.moveTo(19, 12);
        stroke.lineTo(22, 12);
        break;
    case Glyph::Help:
        stroke.addEllipse(QRectF(3, 3, 18, 18));
        stroke.moveTo(9.2, 9.2);
        stroke.cubicTo(9.2, 6.4, 14.8, 6.4, 14.8, 9.8);
        stroke.cubicTo(14.8, 12.2, 12, 12.2, 12, 14.6);
        fill.addEllipse(QRectF(11.1, 16.6, 1.9, 1.9));
        break;
    case Glyph::Theme:
        stroke.addEllipse(QRectF(8, 8, 8, 8));
        for (int i = 0; i < 8; ++i) {
            const qreal angle = i * 3.14159265 / 4.0;
            stroke.moveTo(12 + 7.2 * std::cos(angle), 12 + 7.2 * std::sin(angle));
            stroke.lineTo(12 + 9.8 * std::cos(angle), 12 + 9.8 * std::sin(angle));
        }
        break;
    case Glyph::Bell:
        stroke.moveTo(6, 17);
        stroke.lineTo(6, 11);
        stroke.cubicTo(6, 6.4, 18, 6.4, 18, 11);
        stroke.lineTo(18, 17);
        stroke.lineTo(20, 17);
        stroke.lineTo(4, 17);
        stroke.moveTo(10, 19.5);
        stroke.cubicTo(10.6, 21.6, 13.4, 21.6, 14, 19.5);
        break;
    case Glyph::User:
        stroke.addEllipse(QRectF(8, 3.5, 8, 8));
        stroke.moveTo(3.8, 21);
        stroke.cubicTo(4.6, 15, 19.4, 15, 20.2, 21);
        break;
    case Glyph::Search:
        stroke.addEllipse(QRectF(4, 4, 12, 12));
        stroke.moveTo(15, 15);
        stroke.lineTo(20.5, 20.5);
        break;
    case Glyph::Filter:
        stroke.moveTo(3.5, 5.5);
        stroke.lineTo(20.5, 5.5);
        stroke.lineTo(14, 13);
        stroke.lineTo(14, 20);
        stroke.lineTo(10, 17.5);
        stroke.lineTo(10, 13);
        stroke.closeSubpath();
        break;
    case Glyph::Camera:
        roundedRect(stroke, 2.5, 6.5, 19, 13, 2.5);
        stroke.addEllipse(QRectF(9, 9.5, 6, 6));
        stroke.moveTo(8.5, 6.5);
        stroke.lineTo(10, 3.8);
        stroke.lineTo(14, 3.8);
        stroke.lineTo(15.5, 6.5);
        break;
    case Glyph::Expand:
        stroke.moveTo(3.5, 9);
        stroke.lineTo(3.5, 3.5);
        stroke.lineTo(9, 3.5);
        stroke.moveTo(15, 3.5);
        stroke.lineTo(20.5, 3.5);
        stroke.lineTo(20.5, 9);
        stroke.moveTo(20.5, 15);
        stroke.lineTo(20.5, 20.5);
        stroke.lineTo(15, 20.5);
        stroke.moveTo(9, 20.5);
        stroke.lineTo(3.5, 20.5);
        stroke.lineTo(3.5, 15);
        break;
    case Glyph::More:
        fill.addEllipse(QRectF(3.4, 10.6, 2.8, 2.8));
        fill.addEllipse(QRectF(10.6, 10.6, 2.8, 2.8));
        fill.addEllipse(QRectF(17.8, 10.6, 2.8, 2.8));
        break;
    case Glyph::Play:
        fill.moveTo(7, 4.5);
        fill.lineTo(19.5, 12);
        fill.lineTo(7, 19.5);
        fill.closeSubpath();
        break;
    case Glyph::Pause:
        roundedRect(fill, 6.5, 4.5, 3.8, 15, 1.2);
        roundedRect(fill, 13.7, 4.5, 3.8, 15, 1.2);
        break;
    case Glyph::Next:
        fill.moveTo(5.5, 4.5);
        fill.lineTo(16, 12);
        fill.lineTo(5.5, 19.5);
        fill.closeSubpath();
        roundedRect(fill, 17, 4.5, 2.8, 15, 1.0);
        break;
    case Glyph::Volume:
        fill.moveTo(4, 9.5);
        fill.lineTo(8, 9.5);
        fill.lineTo(12.5, 5);
        fill.lineTo(12.5, 19);
        fill.lineTo(8, 14.5);
        fill.lineTo(4, 14.5);
        fill.closeSubpath();
        stroke.arcMoveTo(QRectF(11, 7, 8, 10), 60);
        stroke.arcTo(QRectF(11, 7, 8, 10), 60, -120);
        stroke.arcMoveTo(QRectF(13, 4.5, 10, 15), 55);
        stroke.arcTo(QRectF(13, 4.5, 10, 15), 55, -110);
        break;
    case Glyph::Fullscreen:
        stroke.moveTo(3.5, 8.5);
        stroke.lineTo(3.5, 3.5);
        stroke.lineTo(8.5, 3.5);
        stroke.moveTo(15.5, 3.5);
        stroke.lineTo(20.5, 3.5);
        stroke.lineTo(20.5, 8.5);
        stroke.moveTo(20.5, 15.5);
        stroke.lineTo(20.5, 20.5);
        stroke.lineTo(15.5, 20.5);
        stroke.moveTo(8.5, 20.5);
        stroke.lineTo(3.5, 20.5);
        stroke.lineTo(3.5, 15.5);
        break;
    case Glyph::ChevronRight:
        stroke.moveTo(9.5, 5);
        stroke.lineTo(16, 12);
        stroke.lineTo(9.5, 19);
        break;
    case Glyph::Annotations:
        roundedRect(stroke, 3.5, 3.5, 17, 17, 2.5);
        stroke.moveTo(7.5, 12);
        stroke.lineTo(10.5, 15);
        stroke.lineTo(16.5, 9);
        break;
    case Glyph::Tracks:
        stroke.moveTo(3, 18);
        stroke.cubicTo(7, 18, 7, 7, 11, 7);
        stroke.cubicTo(15, 7, 15, 17, 19, 17);
        fill.addEllipse(QRectF(1.6, 16.6, 2.8, 2.8));
        fill.addEllipse(QRectF(17.6, 15.6, 2.8, 2.8));
        break;
    case Glyph::Species:
        stroke.moveTo(3.5, 12);
        stroke.cubicTo(7, 6.5, 14, 6.5, 18, 12);
        stroke.cubicTo(14, 17.5, 7, 17.5, 3.5, 12);
        stroke.moveTo(18, 12);
        stroke.lineTo(21.5, 8.5);
        stroke.lineTo(21.5, 15.5);
        stroke.closeSubpath();
        fill.addEllipse(QRectF(7.6, 10.6, 2.4, 2.4));
        break;
    case Glyph::Events:
        stroke.moveTo(12, 3);
        stroke.lineTo(21, 19.5);
        stroke.lineTo(3, 19.5);
        stroke.closeSubpath();
        stroke.moveTo(12, 9.5);
        stroke.lineTo(12, 14);
        fill.addEllipse(QRectF(11.1, 15.8, 1.8, 1.8));
        break;
    case Glyph::Environment:
        stroke.moveTo(12, 3);
        stroke.cubicTo(17.5, 9.5, 19, 12.5, 19, 15);
        stroke.cubicTo(19, 19, 15.8, 21, 12, 21);
        stroke.cubicTo(8.2, 21, 5, 19, 5, 15);
        stroke.cubicTo(5, 12.5, 6.5, 9.5, 12, 3);
        stroke.closeSubpath();
        break;
    case Glyph::Files:
        stroke.moveTo(6, 3.5);
        stroke.lineTo(14, 3.5);
        stroke.lineTo(19, 8.5);
        stroke.lineTo(19, 20.5);
        stroke.lineTo(6, 20.5);
        stroke.closeSubpath();
        stroke.moveTo(14, 3.5);
        stroke.lineTo(14, 8.5);
        stroke.lineTo(19, 8.5);
        break;
    case Glyph::Clock:
        stroke.addEllipse(QRectF(3, 3, 18, 18));
        stroke.moveTo(12, 7);
        stroke.lineTo(12, 12.4);
        stroke.lineTo(15.8, 14.6);
        break;
    case Glyph::Alert:
        stroke.addEllipse(QRectF(3, 3, 18, 18));
        stroke.moveTo(12, 7);
        stroke.lineTo(12, 13);
        fill.addEllipse(QRectF(11.1, 15.4, 1.8, 1.8));
        break;
    case Glyph::Refresh:
        stroke.arcMoveTo(QRectF(4, 4, 16, 16), 60);
        stroke.arcTo(QRectF(4, 4, 16, 16), 60, 260);
        stroke.moveTo(16, 3);
        stroke.lineTo(16.6, 8.4);
        stroke.lineTo(11.4, 7.4);
        break;
    case Glyph::Send:
        stroke.moveTo(21, 3.5);
        stroke.lineTo(3, 10.5);
        stroke.lineTo(10.5, 13.5);
        stroke.lineTo(13.5, 21);
        stroke.closeSubpath();
        stroke.moveTo(10.5, 13.5);
        stroke.lineTo(21, 3.5);
        break;
    case Glyph::Sparkle:
        stroke.moveTo(9, 3);
        stroke.lineTo(10.6, 7.4);
        stroke.lineTo(15, 9);
        stroke.lineTo(10.6, 10.6);
        stroke.lineTo(9, 15);
        stroke.lineTo(7.4, 10.6);
        stroke.lineTo(3, 9);
        stroke.lineTo(7.4, 7.4);
        stroke.closeSubpath();
        stroke.moveTo(17, 13);
        stroke.lineTo(18, 16);
        stroke.lineTo(21, 17);
        stroke.lineTo(18, 18);
        stroke.lineTo(17, 21);
        stroke.lineTo(16, 18);
        stroke.lineTo(13, 17);
        stroke.lineTo(16, 16);
        stroke.closeSubpath();
        break;
    case Glyph::Merge:
        stroke.moveTo(4, 5);
        stroke.cubicTo(10, 5, 10, 12, 15, 12);
        stroke.moveTo(4, 19);
        stroke.cubicTo(10, 19, 10, 12, 15, 12);
        stroke.lineTo(20, 12);
        stroke.moveTo(16.5, 8.8);
        stroke.lineTo(20.5, 12);
        stroke.lineTo(16.5, 15.2);
        break;
    case Glyph::Split:
        stroke.moveTo(20, 5);
        stroke.cubicTo(14, 5, 14, 12, 9, 12);
        stroke.moveTo(20, 19);
        stroke.cubicTo(14, 19, 14, 12, 9, 12);
        stroke.lineTo(4, 12);
        stroke.moveTo(7.5, 8.8);
        stroke.lineTo(3.5, 12);
        stroke.lineTo(7.5, 15.2);
        break;
    case Glyph::Undo:
        stroke.moveTo(8, 7);
        stroke.lineTo(3.5, 11);
        stroke.lineTo(8, 15);
        stroke.moveTo(3.5, 11);
        stroke.lineTo(14, 11);
        stroke.cubicTo(19.5, 11, 19.5, 20, 14, 20);
        stroke.lineTo(9, 20);
        break;
    case Glyph::Redo:
        stroke.moveTo(16, 7);
        stroke.lineTo(20.5, 11);
        stroke.lineTo(16, 15);
        stroke.moveTo(20.5, 11);
        stroke.lineTo(10, 11);
        stroke.cubicTo(4.5, 11, 4.5, 20, 10, 20);
        stroke.lineTo(15, 20);
        break;
    case Glyph::Save:
        stroke.moveTo(4, 4.5);
        stroke.lineTo(16.5, 4.5);
        stroke.lineTo(20, 8);
        stroke.lineTo(20, 19.5);
        stroke.lineTo(4, 19.5);
        stroke.closeSubpath();
        stroke.moveTo(7.5, 4.5);
        stroke.lineTo(7.5, 10);
        stroke.lineTo(15.5, 10);
        stroke.lineTo(15.5, 4.5);
        roundedRect(stroke, 7.5, 13, 9, 6.5, 1.0);
        break;
    case Glyph::Export:
        stroke.moveTo(12, 3);
        stroke.lineTo(12, 14.5);
        stroke.moveTo(8, 7);
        stroke.lineTo(12, 3);
        stroke.lineTo(16, 7);
        stroke.moveTo(4, 14);
        stroke.lineTo(4, 20.5);
        stroke.lineTo(20, 20.5);
        stroke.lineTo(20, 14);
        break;
    case Glyph::Drop:
        stroke.moveTo(12, 2.5);
        stroke.cubicTo(18.5, 10, 20, 12.8, 20, 15.2);
        stroke.cubicTo(20, 19.4, 16.4, 21.5, 12, 21.5);
        stroke.cubicTo(7.6, 21.5, 4, 19.4, 4, 15.2);
        stroke.cubicTo(4, 12.8, 5.5, 10, 12, 2.5);
        stroke.closeSubpath();
        break;
    }
}

}  // namespace

void paintGlyph(QPainter& painter, Glyph glyph, const QRectF& box, const QColor& color,
                qreal strokeWidth)
{
    QPainterPath stroke;
    QPainterPath fill;
    buildPath(glyph, stroke, fill);

    const qreal side = std::min(box.width(), box.height());
    const qreal scale = side / kGrid;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(box.center().x() - side / 2.0, box.center().y() - side / 2.0);
    painter.scale(scale, scale);

    if (!fill.isEmpty()) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPath(fill);
    }
    if (!stroke.isEmpty()) {
        // The painter is scaled to the target box, so undo that scale on the
        // pen to keep the stroke a constant weight at every icon size.
        QPen pen(color);
        pen.setWidthF(strokeWidth / std::max(scale, 0.0001));
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(stroke);
    }
    painter.restore();
}

QPixmap glyphPixmap(Glyph glyph, int size, const QColor& color, qreal strokeWidth)
{
    const qreal ratio = 2.0;  // render at 2x so the strokes stay crisp when scaled
    QPixmap pixmap(int(size * ratio), int(size * ratio));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    paintGlyph(painter, glyph, QRectF(0, 0, size, size), color, strokeWidth);
    return pixmap;
}

QIcon glyphIcon(Glyph glyph, int size, const QColor& normal, const QColor& active)
{
    QIcon icon;
    icon.addPixmap(glyphPixmap(glyph, size, normal), QIcon::Normal);
    icon.addPixmap(glyphPixmap(glyph, size, active), QIcon::Active);
    icon.addPixmap(glyphPixmap(glyph, size, active), QIcon::Selected);
    QColor disabled = normal;
    disabled.setAlpha(90);
    icon.addPixmap(glyphPixmap(glyph, size, disabled), QIcon::Disabled);
    return icon;
}

}  // namespace aquatrack::ui
