#include "TestSupport.h"

#include "ui/common/GlyphArtwork.h"
#include "ui/common/Glyphs.h"

#include <QApplication>
#include <QImage>

namespace {

int inkedPixels(const QPixmap& pixmap)
{
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    int inked = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qAlpha(image.pixel(x, y)) > 0)
                ++inked;
    return inked;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    using aquatrack::ui::Glyph;
    // Every enumerator, first to last: a glyph whose artwork is missing or
    // that Qt's SVG renderer rejects would otherwise paint nothing silently.
    for (int value = int(Glyph::Dashboard); value <= int(Glyph::Inspect); ++value) {
        const auto glyph = static_cast<Glyph>(value);
        requireTest(!aquatrack::ui::glyphArtwork(glyph).isEmpty(), "glyph has artwork");
        const QPixmap pixmap = aquatrack::ui::glyphPixmap(glyph, 24, Qt::white);
        requireTest(inkedPixels(pixmap) > 20, "glyph renders visible strokes");
    }
    // Colour and transparency are applied, so disabled icons really dim.
    const QImage solid =
        aquatrack::ui::glyphPixmap(Glyph::Play, 24, QColor(255, 0, 0)).toImage();
    const QImage faded =
        aquatrack::ui::glyphPixmap(Glyph::Play, 24, QColor(255, 0, 0, 90)).toImage();
    const QPoint centre(solid.width() / 2, solid.height() / 2);
    requireTest(qRed(solid.pixel(centre)) > 200 && qGreen(solid.pixel(centre)) < 40,
                "glyph takes the requested colour");
    requireTest(qAlpha(faded.pixel(centre)) < qAlpha(solid.pixel(centre)),
                "translucent colour renders translucent");
    return 0;
}
