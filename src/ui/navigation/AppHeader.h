#pragma once

#include <QFrame>
#include <QPoint>

class QLabel;
class QPushButton;

namespace aquatrack::ui {
class Badge;
}

// The frameless window's title bar. The brand sits on the left, the open
// project is centred where a window title would be, and help, warnings and
// the caption buttons are on the right. Page navigation lives in the
// NavigationDock at the bottom of the window.
//
// Dragging an empty part of the bar moves the window (with Windows snap) and
// double-clicking it maximizes.
class AppHeader final : public QFrame {
    Q_OBJECT

public:
    explicit AppHeader(QWidget* parent = nullptr);

    void setProjectName(const QString& name);
    void setNotificationCount(int count);
    // Swaps the maximize glyph for "restore" while the window is maximized.
    void setWindowMaximized(bool maximized);
    // Lights the settings button while the settings page is showing.
    void setSettingsActive(bool active);

signals:
    void settingsRequested();
    void helpRequested();
    void warningsRequested();
    // The project title was clicked: show the project's home (the dashboard).
    void projectRequested();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void toggleMaximized();
    // Keeps the project title centred on the window, not on the space left
    // between the brand and the tools.
    void centreProjectTitle();

    QPushButton* m_maximize = nullptr;
    QPushButton* m_settings = nullptr;
    QPushButton* m_projectChip = nullptr;
    QLabel* m_projectLabel = nullptr;
    QLabel* m_projectCaption = nullptr;
    QLabel* m_avatar = nullptr;
    aquatrack::ui::Badge* m_notifications = nullptr;
    // A press on empty chrome arms a drag; it becomes a system move once the
    // pointer travels, so a double-click still reaches mouseDoubleClickEvent.
    bool m_dragArmed = false;
    QPoint m_pressPosition;
};
