#include "ui/AppTheme.h"

#include <QHash>

#include <algorithm>

namespace aquatrack::ui {
namespace {

// The sheet is authored with @token names rather than literal hex so the
// palette stays the single source of truth for both the stylesheet and the
// hand-painted widgets in ui/common.
QString resolveTokens(QString sheet)
{
    static const QHash<QString, QColor> tokens{
        {QStringLiteral("@canvas"), palette::Canvas},
        {QStringLiteral("@chrome"), palette::Chrome},
        {QStringLiteral("@surfaceRaised"), palette::SurfaceRaised},
        {QStringLiteral("@surface"), palette::Surface},
        {QStringLiteral("@sunken"), palette::Sunken},
        {QStringLiteral("@lineSoft"), palette::LineSoft},
        {QStringLiteral("@line"), palette::Line},
        {QStringLiteral("@textSecondary"), palette::TextSecondary},
        {QStringLiteral("@textDim"), palette::TextDim},
        {QStringLiteral("@text"), palette::TextPrimary},
        {QStringLiteral("@accentHover"), palette::AccentHover},
        {QStringLiteral("@accentSunken"), palette::AccentSunken},
        {QStringLiteral("@accent"), palette::Accent},
        {QStringLiteral("@success"), palette::Success},
        {QStringLiteral("@warning"), palette::Warning},
        {QStringLiteral("@danger"), palette::Danger},
    };
    // Longest name first, so @surfaceRaised is not clipped by @surface.
    auto names = tokens.keys();
    std::sort(names.begin(), names.end(),
              [](const QString& left, const QString& right) { return left.size() > right.size(); });
    for (const auto& name : names)
        sheet.replace(name, tokens.value(name).name(QColor::HexRgb));
    return sheet;
}

}  // namespace

QColor trackColor(int trackId)
{
    static const QVector<QColor> wheel{
        QColor(0xf0, 0x50, 0x9b), QColor(0x4f, 0xd8, 0xf0), QColor(0xf5, 0xd9, 0x49),
        QColor(0x7e, 0xd9, 0x57), QColor(0xff, 0x9f, 0x45), QColor(0xa7, 0x8b, 0xfa),
        QColor(0x2e, 0x9b, 0xf0), QColor(0xff, 0x7a, 0x7a),
    };
    if (trackId < 0)
        trackId = -trackId;
    return wheel.at(trackId % wheel.size());
}

QColor speciesColor(int classId)
{
    static const QVector<QColor> byClass{
        palette::TextDim,          // 0 unknown
        QColor(0xf5, 0xd9, 0x49),  // 1 fish
        QColor(0xff, 0x9f, 0x45),  // 2 crab
        QColor(0xa7, 0x8b, 0xfa),  // 3 shrimp
        QColor(0x7e, 0xd9, 0x57),  // 4 starfish
        QColor(0x4f, 0xd8, 0xf0),  // 5 small fish
        QColor(0xf0, 0x50, 0x9b),  // 6 jellyfish
    };
    return classId >= 0 && classId < byClass.size() ? byClass.at(classId) : palette::TextDim;
}

QString applicationStyleSheet()
{
    return resolveTokens(QStringLiteral(R"SHEET(
QMainWindow, QWidget {
    background: @canvas;
    color: @text;
    font-family: "Segoe UI", "Inter", sans-serif;
    font-size: 13px;
}
QLabel { background: transparent; }
QToolTip {
    background: @surfaceRaised; color: @text;
    border: 1px solid @line; border-radius: 4px; padding: 5px 8px;
}

/* ---- Structure ------------------------------------------------------- */
QFrame[role="chrome"]   { background: @chrome; border: none; border-bottom: 1px solid @line; }
QFrame[role="card"]     { background: @surface; border: 1px solid @line; border-radius: 10px; }
QFrame[role="cardFlat"] { background: @surface; border: 1px solid @lineSoft; border-radius: 8px; }
QFrame[role="inset"]    { background: @sunken; border: 1px solid @lineSoft; border-radius: 8px; }
QFrame[role="alert"]    { background: @surface; border: 1px solid @warning; border-radius: 10px; }
QFrame[role="divider"]  { background: @lineSoft; border: none; max-height: 1px; min-height: 1px; }

/* ---- Type scale ------------------------------------------------------ */
QLabel[type="display"]  { font-size: 26px; font-weight: 700; color: @text; }
QLabel[type="title"]    { font-size: 18px; font-weight: 600; color: @text; }
QLabel[type="subtitle"] { font-size: 15px; font-weight: 600; color: @text; }
QLabel[type="body"]     { font-size: 13px; color: @text; }
QLabel[type="small"]    { font-size: 12px; color: @textSecondary; }
QLabel[type="dim"]      { font-size: 12px; color: @textDim; }
QLabel[type="label"]    { font-size: 11px; font-weight: 700; color: @textDim; letter-spacing: 1px; }
QLabel[type="metric"]   { font-size: 30px; font-weight: 700; color: @accent; }
QLabel[type="mono"]     { font-family: "Cascadia Mono", "Consolas", monospace; font-size: 12px; color: @textSecondary; }

/* ---- Buttons --------------------------------------------------------- */
QPushButton {
    background: @surfaceRaised; border: 1px solid @line; border-radius: 6px;
    padding: 7px 13px; color: @text; font-size: 13px;
}
QPushButton:hover    { background: #17304a; border-color: #24557c; }
QPushButton:pressed  { background: #101f33; }
QPushButton:disabled { background: @sunken; color: #3f556b; border-color: @lineSoft; }

QPushButton[role="primary"] {
    background: @accent; border: 1px solid @accent; color: #ffffff; font-weight: 600;
}
QPushButton[role="primary"]:hover    { background: @accentHover; border-color: @accentHover; }
QPushButton[role="primary"]:pressed  { background: @accentSunken; }
QPushButton[role="primary"]:disabled { background: #1b3550; border-color: #1b3550; color: #4a6480; }

QPushButton[role="ghost"] {
    background: transparent; border: 1px solid transparent; color: @textSecondary; padding: 6px 10px;
}
QPushButton[role="ghost"]:hover { background: @surfaceRaised; color: @text; }

QPushButton[role="nav"] {
    background: transparent; border: none; border-bottom: 2px solid transparent;
    border-radius: 0; padding: 8px 12px; color: @textSecondary; font-size: 13px;
}
QPushButton[role="nav"]:hover { color: @text; }
QPushButton[role="navActive"] {
    background: transparent; border: none; border-bottom: 2px solid @accent;
    border-radius: 0; padding: 8px 12px; color: @accent; font-size: 13px; font-weight: 600;
}

QPushButton[role="chip"] {
    background: @sunken; border: 1px solid @lineSoft; border-radius: 6px;
    padding: 8px 10px; color: @textSecondary; font-size: 12px; text-align: left;
}
QPushButton[role="chip"]:hover { border-color: @accent; color: @text; }

QPushButton[role="icon"] {
    background: transparent; border: 1px solid transparent; border-radius: 6px;
    padding: 5px; color: @textSecondary; min-width: 28px;
}
QPushButton[role="icon"]:hover { background: @surfaceRaised; color: @text; }

QPushButton[role="overlay"] {
    background: rgba(9, 22, 38, 200); border: 1px solid @line; border-radius: 6px;
    padding: 5px 10px; color: @textSecondary; font-size: 12px;
}
QPushButton[role="overlay"]:hover { color: @text; border-color: @accent; }
QPushButton[role="overlayActive"] {
    background: @accent; border: 1px solid @accent; border-radius: 6px;
    padding: 5px 10px; color: #ffffff; font-size: 12px; font-weight: 600;
}

/* ---- Inputs ---------------------------------------------------------- */
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background: @sunken; border: 1px solid @line; border-radius: 6px;
    padding: 7px 10px; color: @text; selection-background-color: @accent;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: @accent; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background: @surfaceRaised; border: 1px solid @line;
    selection-background-color: @accent; outline: 0;
}

QCheckBox { background: transparent; color: @textSecondary; spacing: 8px; }
QCheckBox::indicator {
    width: 15px; height: 15px; border: 1px solid @line; border-radius: 4px; background: @sunken;
}
QCheckBox::indicator:checked { background: @accent; border-color: @accent; }

/* ---- Sliders --------------------------------------------------------- */
QSlider::groove:horizontal { height: 4px; background: #1e3a55; border-radius: 2px; }
QSlider::sub-page:horizontal { background: @accent; border-radius: 2px; }
QSlider::add-page:horizontal { background: #1e3a55; border-radius: 2px; }
QSlider::handle:horizontal {
    width: 13px; height: 13px; margin: -5px 0; border-radius: 7px;
    background: #ffffff; border: 2px solid @accent;
}
QSlider::handle:horizontal:hover { border-color: @accentHover; }

/* ---- Lists and tables ------------------------------------------------ */
QListWidget, QTreeWidget, QTableView {
    background: @sunken; border: 1px solid @lineSoft; border-radius: 8px;
    outline: 0; alternate-background-color: #0c1d30;
}
QListWidget::item { border-bottom: 1px solid @lineSoft; padding: 8px; color: @textSecondary; }
QListWidget::item:selected { background: #16354f; color: @text; }
QListWidget::item:hover { background: #122b42; }
QHeaderView::section {
    background: @surface; color: @textDim; border: none;
    border-bottom: 1px solid @line; padding: 7px 8px;
    font-size: 11px; font-weight: 700; letter-spacing: 0.6px;
}
QTableView { gridline-color: @lineSoft; }
QTableView::item { padding: 6px 8px; border: none; }
QTableView::item:selected { background: #16354f; color: @text; }

/* ---- Tabs ------------------------------------------------------------ */
QTabWidget::pane { background: transparent; border: none; border-top: 1px solid @lineSoft; }
QTabBar::tab {
    background: transparent; color: @textDim; padding: 8px 14px;
    border-bottom: 2px solid transparent; font-size: 12px; font-weight: 600;
}
QTabBar::tab:hover { color: @textSecondary; }
QTabBar::tab:selected { color: @accent; border-bottom: 2px solid @accent; }

/* ---- Scrollbars ------------------------------------------------------ */
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #1f3f5d; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #2a557c; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #1f3f5d; border-radius: 5px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: #2a557c; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QScrollArea { border: none; background: transparent; }

/* ---- Status bar ------------------------------------------------------ */
QStatusBar { background: @chrome; color: @textDim; border-top: 1px solid @line; }
QStatusBar::item { border: none; }
)SHEET"));
}

}  // namespace aquatrack::ui
