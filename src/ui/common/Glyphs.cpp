#include "ui/common/Glyphs.h"

#include "ui/common/GlyphArtwork.h"

#include <QGuiApplication>
#include <QHash>
#include <QPainter>
#include <QSvgRenderer>

#include <algorithm>
#include <memory>

namespace aquatrack::ui {
namespace {

// Phosphor icons are authored on a 256x256 grid, as filled outlines: the
// line weight is part of the artwork, not a render setting.
constexpr int kGrid = 256;

QByteArray svgDocument(Glyph glyph, GlyphStyle style, const QColor& color)
{
    QByteArray body = glyphArtwork(glyph, style);
    const QByteArray colour = color.name(QColor::HexRgb).toLatin1();
    body.replace("currentColor", colour);
    return QByteArrayLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 ")
        + QByteArray::number(kGrid) + " " + QByteArray::number(kGrid) + "\" fill=\"" + colour
        + "\">" + body + "</svg>";
}

// Parsing SVG on every paint is wasteful (icons repaint on every hover and
// frame), so renderers are kept per glyph, style and colour. The set is
// small and bounded by the palette.
QSvgRenderer* rendererFor(Glyph glyph, GlyphStyle style, const QColor& color)
{
    static QHash<quint64, std::shared_ptr<QSvgRenderer>> cache;
    const quint64 key = (quint64(glyph) << 40) | (quint64(style) << 32) | quint64(color.rgb() & 0xffffff);
    auto found = cache.constFind(key);
    if (found != cache.constEnd())
        return found->get();
    auto renderer = std::make_shared<QSvgRenderer>(svgDocument(glyph, style, color));
    QSvgRenderer* raw = renderer.get();
    cache.insert(key, std::move(renderer));
    return raw;
}

}  // namespace

void paintGlyph(QPainter& painter, Glyph glyph, const QRectF& box, const QColor& color,
                GlyphStyle style)
{
    const qreal side = std::min(box.width(), box.height());
    if (side <= 0.0)
        return;
    QSvgRenderer* renderer = rendererFor(glyph, style, color);
    if (!renderer || !renderer->isValid())
        return;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setOpacity(painter.opacity() * color.alphaF());
    renderer->render(&painter, QRectF(box.center().x() - side / 2.0,
                                      box.center().y() - side / 2.0, side, side));
    painter.restore();
}

QPixmap glyphPixmap(Glyph glyph, int size, const QColor& color, GlyphStyle style)
{
    // The screen's own density (1.25, 1.5, 2 ...), not a fixed 2x that is
    // then resampled and softened.
    const qreal ratio = std::max<qreal>(1.0, qGuiApp ? qGuiApp->devicePixelRatio() : 1.0);
    QPixmap pixmap(qRound(size * ratio), qRound(size * ratio));
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    paintGlyph(painter, glyph, QRectF(0, 0, size, size), color, style);
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
