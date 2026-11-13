#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

class QPainter;

// The application's icon set: Lucide (https://lucide.dev, ISC licence), a
// stroked 24x24 family. The artwork is embedded as SVG text by
// tools/generate_glyph_artwork.py, so the build ships no binary assets and
// the colour and stroke weight are chosen at render time.
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
};

// Paints the glyph centred in the given square, stroked in `color`.
// `strokeWidth` is in device-independent pixels, so the line weight is the
// same at every icon size.
void paintGlyph(QPainter& painter, Glyph glyph, const QRectF& box, const QColor& color,
                qreal strokeWidth = 1.6);

[[nodiscard]] QPixmap glyphPixmap(Glyph glyph, int size, const QColor& color,
                                  qreal strokeWidth = 1.6);

// Builds a two-state icon so buttons dim when inactive and light up on hover.
[[nodiscard]] QIcon glyphIcon(Glyph glyph, int size, const QColor& normal, const QColor& active);

}  // namespace aquatrack::ui
