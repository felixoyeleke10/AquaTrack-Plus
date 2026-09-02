#pragma once

#include <QColor>
#include <QString>

namespace aquatrack::ui {

// Design tokens. The stylesheet below is generated from these, and the
// hand-painted widgets in ui/common read them directly, so a colour is
// defined once and every surface that uses it stays in step.
namespace palette {
inline const QColor Canvas{0x0a, 0x17, 0x27};
inline const QColor Chrome{0x0b, 0x1a, 0x2c};
inline const QColor Surface{0x0e, 0x21, 0x35};
inline const QColor SurfaceRaised{0x12, 0x28, 0x3f};
inline const QColor Sunken{0x08, 0x16, 0x26};
inline const QColor Line{0x1b, 0x3b, 0x58};
inline const QColor LineSoft{0x14, 0x2c, 0x44};

inline const QColor TextPrimary{0xe8, 0xf1, 0xfa};
inline const QColor TextSecondary{0x8f, 0xa8, 0xbf};
inline const QColor TextDim{0x64, 0x80, 0x9a};

inline const QColor Accent{0x2e, 0x9b, 0xf0};
inline const QColor AccentHover{0x4f, 0xb0, 0xff};
inline const QColor AccentSunken{0x1d, 0x7a, 0xc4};
inline const QColor Success{0x35, 0xd0, 0x7f};
inline const QColor Warning{0xf5, 0xa6, 0x23};
inline const QColor Danger{0xff, 0x6b, 0x6b};
}  // namespace palette

// Stable per-track colours. Index by track id; the sequence repeats so a
// long survey never runs out and the same id always paints the same colour.
[[nodiscard]] QColor trackColor(int trackId);

// Per-species colours, indexed by BrackishMOT class id (1..6).
[[nodiscard]] QColor speciesColor(int classId);

[[nodiscard]] QString applicationStyleSheet();

}  // namespace aquatrack::ui
