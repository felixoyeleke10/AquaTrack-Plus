#include "ui/common/WindowChrome.h"

#include <QWidget>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

namespace aquatrack::ui {

void useDarkTitleBar(QWidget* window)
{
#ifdef Q_OS_WIN
    if (!window)
        return;
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    constexpr DWORD kImmersiveDarkMode = 20;  // DWMWA_USE_IMMERSIVE_DARK_MODE
    const BOOL enabled = TRUE;
    DwmSetWindowAttribute(hwnd, kImmersiveDarkMode, &enabled, sizeof(enabled));
#else
    Q_UNUSED(window);
#endif
}

}  // namespace aquatrack::ui
