#include "ui/common/Glyphs.h"

#include "ui/common/GlyphArtwork.h"

#include <QPainter>
#include <QSvgRenderer>

#include <algorithm>

namespace aquatrack::ui {
namespace {

// Lucide icons are authored on a 24x24 grid.
constexpr qreal kGrid = 24.0;

// Transport controls read better as solid shapes than as outlines.
bool isFilled(Glyph glyph)
{
    return glyph == Glyph::Play || glyph == Glyph::Pause;
}

QByteArray svgDocument(Glyph glyph, const QColor& color, qreal gridStroke)
{
    const QByteArray colour = color.name(QColor::HexRgb).toLatin1();
    QByteArray body = glyphArtwork(glyph);
    body.replace("currentColor", colour);
    return QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\"")
        + " fill=\"" + (isFilled(glyph) ? colour : QByteArrayLiteral("none")) + "\""
        + " stroke=\"" + colour + "\""
        + " stroke-width=\"" + QByteArray::number(gridStroke, 'f', 3) + "\""
        + " stroke-linecap=\"round\" stroke-linejoin=\"round\">" + body + "</svg>";
}

}  // namespace

void paintGlyph(QPainter& painter, Glyph glyph, const QRectF& box, const QColor& color,
                qreal strokeWidth)
{
    const qreal side = std::min(box.width(), box.height());
    if (side <= 0.0)
        return;
    // Convert the requested pixel weight into grid units for this size.
    const qreal gridStroke = strokeWidth * kGrid / side;
    QSvgRenderer renderer(svgDocument(glyph, color, gridStroke));
    if (!renderer.isValid())
        return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(painter.opacity() * color.alphaF());
    renderer.render(&painter, QRectF(box.center().x() - side / 2.0,
                                     box.center().y() - side / 2.0, side, side));
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
