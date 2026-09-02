#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

class QPainter;

// A small stroked icon set drawn with QPainter on a 24x24 grid. The project
// ships no icon assets and pulling in an icon font would mean shipping a
// binary the build cannot verify, so the handful of glyphs the shell needs
// are drawn in code.
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
};

// Paints the glyph centred in the given square, stroked in `color`.
void paintGlyph(QPainter& painter, Glyph glyph, const QRectF& box, const QColor& color,
                qreal strokeWidth = 1.6);

[[nodiscard]] QPixmap glyphPixmap(Glyph glyph, int size, const QColor& color,
                                  qreal strokeWidth = 1.6);

// Builds a two-state icon so buttons dim when inactive and light up on hover.
[[nodiscard]] QIcon glyphIcon(Glyph glyph, int size, const QColor& normal, const QColor& active);

}  // namespace aquatrack::ui
