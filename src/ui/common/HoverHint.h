#pragma once

#include <QString>

class QWidget;

namespace aquatrack::ui {

enum class HintPlacement { Above, Below };

// Shows `text` in a small themed label next to `target` the moment the
// pointer enters it, and hides it on leave or click. Used for icon-only
// controls, where the standard tooltip's delay makes the name feel hidden.
// The hint is owned by `target` and goes away with it.
void attachHoverHint(QWidget* target, const QString& text, HintPlacement placement);

}  // namespace aquatrack::ui
