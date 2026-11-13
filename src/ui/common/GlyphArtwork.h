#pragma once

#include "ui/common/Glyphs.h"

#include <QByteArray>

namespace aquatrack::ui {

// The drawing elements (paths) of a glyph on a 256x256 grid, without the
// surrounding <svg> element. Generated from Phosphor by
// tools/generate_glyph_artwork.py.
[[nodiscard]] QByteArray glyphArtwork(Glyph glyph, GlyphStyle style = GlyphStyle::Regular);

}  // namespace aquatrack::ui
