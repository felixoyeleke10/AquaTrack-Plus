#pragma once

#include <QColor>
#include <QString>

namespace aquatrack::ui {

// Design tokens. The stylesheet below is generated from these, and the
// hand-painted widgets in ui/common read them directly, so a colour is
// defined once and every surface that uses it stays in step.
//
// The theme is black and electric blue: near-black neutral surfaces that
// step up in small increments, hairline borders, and a single saturated blue
// that carries every interactive and selected state.
namespace palette {
inline const QColor Canvas{0x05, 0x06, 0x08};
inline const QColor Chrome{0x07, 0x08, 0x0c};
inline const QColor Surface{0x0b, 0x0d, 0x13};
inline const QColor SurfaceRaised{0x12, 0x16, 0x1f};
inline const QColor SurfaceHover{0x18, 0x1e, 0x2b};
inline const QColor Sunken{0x06, 0x08, 0x0c};
inline const QColor Line{0x1b, 0x21, 0x2e};
inline const QColor LineSoft{0x12, 0x17, 0x21};

inline const QColor TextPrimary{0xf1, 0xf5, 0xfb};
inline const QColor TextSecondary{0x98, 0xa3, 0xb6};
inline const QColor TextDim{0x5c, 0x66, 0x79};

inline const QColor Accent{0x2f, 0x7b, 0xff};
inline const QColor AccentHover{0x5a, 0x97, 0xff};
inline const QColor AccentSunken{0x1c, 0x5c, 0xe6};
// Translucent blue for selected rows, the active nav pill and focus glow.
inline const QColor AccentTint{0x2f, 0x7b, 0xff, 0x2e};
// Second series colour for charts: cyan sits beside the blue without
// competing with it.
inline const QColor Secondary{0x22, 0xd3, 0xee};
inline const QColor Success{0x34, 0xd3, 0x99};
inline const QColor Warning{0xfb, 0xbf, 0x24};
inline const QColor Danger{0xf8, 0x71, 0x71};
}  // namespace palette

// Stable per-track colours. Index by track id; the sequence repeats so a
// long survey never runs out and the same id always paints the same colour.
[[nodiscard]] QColor trackColor(int trackId);

// Per-species colours, indexed by BrackishMOT class id (1..6).
[[nodiscard]] QColor speciesColor(int classId);

// Colour for a species by name, matching the review canvas legend. Names
// outside the BrackishMOT classes fall back to a colour picked by
// `fallbackIndex` so that several such species stay distinguishable.
[[nodiscard]] QColor speciesColorForName(const QString& name, int fallbackIndex);

[[nodiscard]] QString applicationStyleSheet();

}  // namespace aquatrack::ui
