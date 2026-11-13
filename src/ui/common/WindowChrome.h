#pragma once

class QWidget;

namespace aquatrack::ui {

// Gives a top-level window a dark native title bar on Windows 10/11, so
// secondary windows match the dark application instead of flashing white.
// Does nothing elsewhere or on older Windows.
void useDarkTitleBar(QWidget* window);

}  // namespace aquatrack::ui
