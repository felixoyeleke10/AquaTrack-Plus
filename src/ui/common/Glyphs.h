#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

class QPainter;

// The application's icon set: Phosphor (https://phosphoricons.com, MIT
// licence) on a 256x256 grid, in two weights: Regular for everyday icons and
// Duotone (an outline over a soft tint) for the active or featured one, such
// as the current page or the open inspector tab. The artwork is embedded as
// SVG text by tools/generate_glyph_artwork.py, so the build ships no binary
// assets, and colour is chosen at render time.
namespace aquatrack::ui {

enum class Glyph {
    Dashboard,
    Video,
    Review,
    Analytics,
    Datasets,
    Models,
    Settings,
    Help,
    Theme,
    Bell,
    User,
    Search,
    Filter,
    Camera,
    Expand,
    More,
    Play,
    Pause,
    Previous,
    Next,
    Volume,
    Fullscreen,
    ChevronRight,
    Annotations,
    Tracks,
    Species,
    Events,
    Environment,
    Files,
    Clock,
    Alert,
    Refresh,
    Send,
    Sparkle,
    Merge,
    Split,
    Undo,
    Redo,
    Save,
    Export,
    Drop,
    // Window caption controls.
    Minimize,
    Maximize,
    Restore,
    Close,
    // View modes and inspection.
    ListView,
    GridView,
    Inspect,
    // Transport: jump to either end, step one frame.
    JumpStart,
    JumpEnd,
    StepBack,
    StepForward,
};

enum class GlyphStyle {
    Regular,  // outline
    Duotone,  // outline over a soft tint of the same colour: active / featured
};

// Paints the glyph centred in the given square, in `color` (its alpha is
// honoured, so translucent colours dim the icon).
void paintGlyph(QPainter& painter, Glyph glyph, const QRectF& box, const QColor& color,
                GlyphStyle style = GlyphStyle::Regular);

// A pixmap rendered at the screen's own pixel density, so icons stay sharp
// at every Windows scaling setting.
[[nodiscard]] QPixmap glyphPixmap(Glyph glyph, int size, const QColor& color,
                                  GlyphStyle style = GlyphStyle::Regular);

// Builds a two-state icon so buttons dim when inactive and light up on hover.
[[nodiscard]] QIcon glyphIcon(Glyph glyph, int size, const QColor& normal, const QColor& active);

}  // namespace aquatrack::ui
