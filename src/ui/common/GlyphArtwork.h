#pragma once

#include "ui/common/Glyphs.h"

#include <QByteArray>

namespace aquatrack::ui {

// The drawing elements (paths, circles, ...) of a glyph on a 24x24 grid,
// without the surrounding <svg> element. Generated from Lucide by
// tools/generate_glyph_artwork.py.
[[nodiscard]] QByteArray glyphArtwork(Glyph glyph);

}  // namespace aquatrack::ui
