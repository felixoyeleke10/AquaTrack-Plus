#include "ui/AppTheme.h"

#include "data/BrackishMotSequence.h"

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
        {QStringLiteral("@surfaceHover"), palette::SurfaceHover},
        {QStringLiteral("@accentTint"), palette::AccentTint},
        {QStringLiteral("@secondary"), palette::Secondary},
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
    for (const auto& name : names) {
        const QColor colour = tokens.value(name);
        // Translucent tokens become rgba(), which Qt style sheets understand.
        sheet.replace(name, colour.alpha() == 255
                                ? colour.name(QColor::HexRgb)
                                : QStringLiteral("rgba(%1, %2, %3, %4)")
                                      .arg(colour.red())
                                      .arg(colour.green())
                                      .arg(colour.blue())
                                      .arg(colour.alpha()));
    }
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

QColor speciesColorForName(const QString& name, int fallbackIndex)
{
    const int classId = aquatrack::BrackishMotSequence::classIdForName(name);
    if (classId > 0 || name == aquatrack::BrackishMotSequence::className(0))
        return speciesColor(classId);
    return trackColor(fallbackIndex);
}

QString applicationStyleSheet()
{
    return resolveTokens(QStringLiteral(R"SHEET(
QMainWindow, QWidget {
    background: @canvas;
    color: @text;
    font-family: "Segoe UI Variable Text", "Segoe UI", "Inter", sans-serif;
    font-size: 13px;
}
QLabel { background: transparent; }
QToolTip {
    background: @surfaceRaised; color: @text;
    border: 1px solid @line; border-radius: 6px; padding: 6px 9px;
}

/* ---- Structure ------------------------------------------------------- */
QFrame[role="chrome"] { background: @chrome; border: none; border-bottom: 1px solid @lineSoft; }
QFrame[role="card"]     { background: @surface; border: 1px solid @line; border-radius: 12px; }
QFrame[role="cardFlat"] { background: @surface; border: 1px solid @lineSoft; border-radius: 10px; }
QFrame[role="inset"]    { background: @sunken; border: 1px solid @lineSoft; border-radius: 10px; }
QFrame[role="alert"]    { background: @surface; border: 1px solid @warning; border-radius: 12px; }
QFrame[role="divider"]  { background: @lineSoft; border: none; max-height: 1px; min-height: 1px; }
/* Layout-only containers inside cards; without this a plain QWidget paints
   the canvas colour and shows up as a dark band. */
QWidget[role="bare"]    { background: transparent; }

/* ---- Type scale ------------------------------------------------------ */
QLabel[type="display"]  { font-size: 26px; font-weight: 700; color: @text; }
QLabel[type="title"]    { font-size: 18px; font-weight: 600; color: @text; }
QLabel[type="subtitle"] { font-size: 15px; font-weight: 600; color: @text; }
QLabel[type="body"]     { font-size: 13px; color: @text; }
QLabel[type="small"]    { font-size: 12px; color: @textSecondary; }
QLabel[type="dim"]      { font-size: 12px; color: @textDim; }
QLabel[type="label"]    { font-size: 11px; font-weight: 700; color: @textDim; letter-spacing: 1.2px; }
QLabel[type="stat"]     { font-size: 22px; font-weight: 700; color: @text; }
QLabel[type="metric"]   { font-size: 30px; font-weight: 700; color: @accentHover; }
QLabel[type="mono"]     { font-family: "Cascadia Mono", "Consolas", monospace; font-size: 12px; color: @textSecondary; }

/* ---- Buttons --------------------------------------------------------- */
QPushButton {
    background: @surfaceRaised; border: 1px solid @line; border-radius: 8px;
    padding: 7px 14px; color: @text; font-size: 13px;
}
QPushButton:hover    { background: @surfaceHover; border-color: #2a3446; }
QPushButton:pressed  { background: @surface; }
QPushButton:disabled { background: @sunken; color: #3a4252; border-color: @lineSoft; }
/* A button that opens a list of choices: the arrow sits centred on the right. */
/* Review header: Detect & track and its detector as one split button. */
QLabel[role="reviewTitle"] { font-size: 13px; font-weight: 600; color: @text; }
#detectCapsule {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @accentHover, stop:1 @accent);
    border: 1px solid @accentHover; border-radius: 8px;
}
#detectCapsule:disabled { background: @sunken; border-color: @lineSoft; }
#detectCapsuleDivider { background: rgba(255, 255, 255, 64); border: none; }
#detectCapsuleDivider:disabled { background: @lineSoft; }
QPushButton#analyzeMedia {
    background: transparent; border: none; border-radius: 0;
    border-top-left-radius: 7px; border-bottom-left-radius: 7px;
    padding: 7px 14px 7px 13px; color: #ffffff; font-weight: 600;
}
QPushButton#analyzeMedia:hover   { background: rgba(255, 255, 255, 24); }
QPushButton#analyzeMedia:pressed { background: rgba(255, 255, 255, 34); }
QPushButton#analyzeMedia:disabled { background: transparent; color: #3a4252; }
QPushButton#detectorPicker {
    background: transparent; border: none; padding: 0;
    font-size: 12px; font-weight: 500;
}

QPushButton[role="primary"] {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @accentHover, stop:1 @accent);
    border: 1px solid @accent; color: #ffffff; font-weight: 600;
}
QPushButton[role="primary"]:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #78abff, stop:1 @accentHover);
    border-color: @accentHover;
}
QPushButton[role="primary"]:pressed  { background: @accentSunken; border-color: @accentSunken; }
QPushButton[role="primary"]:disabled { background: #0f1a33; border-color: #0f1a33; color: #3d5487; }

QPushButton[role="ghost"] {
    background: transparent; border: 1px solid transparent; color: @textSecondary; padding: 6px 10px;
}
QPushButton[role="ghost"]:hover { background: @surfaceRaised; color: @text; }


/* Dock and title-bar icon buttons; the active page is a blue pill. */
QPushButton[role="nav"], QPushButton[role="tool"] {
    background: transparent; border: 1px solid transparent; border-radius: 10px; padding: 0;
}
QPushButton[role="nav"]:hover, QPushButton[role="tool"]:hover {
    background: @surfaceRaised; border-color: @line;
}
QPushButton[role="nav"]:pressed, QPushButton[role="tool"]:pressed { background: @surface; }
QPushButton[role="navActive"], QPushButton[role="toolActive"] {
    background: @accentTint; border: 1px solid #1d3f7a; border-radius: 10px; padding: 0;
}

/* Project chip in the header. */
QPushButton[role="projectChip"] {
    background: @surface; border: 1px solid @line; border-radius: 12px; padding: 0;
}
QPushButton[role="projectChip"]:hover { background: @surfaceRaised; border-color: #2a3446; }
QPushButton[role="projectChip"]:pressed { background: @surface; }

/* Window caption controls: flat, full height, square like Windows 11. */
QPushButton[role="caption"], QPushButton[role="captionClose"] {
    background: transparent; border: none; border-radius: 0; padding: 0; margin: 0;
}
QPushButton[role="caption"]:hover       { background: @surfaceHover; }
QPushButton[role="caption"]:pressed     { background: @surfaceRaised; }
QPushButton[role="captionClose"]:hover   { background: #c42b1c; }
QPushButton[role="captionClose"]:pressed { background: #a3261a; }

QPushButton[role="chip"] {
    background: @sunken; border: 1px solid @lineSoft; border-radius: 10px;
    padding: 8px 10px; color: @textSecondary; font-size: 12px; text-align: left;
}
QPushButton[role="chip"]:hover { background: @surface; border-color: @accent; color: @text; }

/* Segmented switch between sibling pages. */
QFrame[role="segmented"] { background: @sunken; border: 1px solid @line; border-radius: 10px; }
QPushButton[role="segment"] {
    background: transparent; border: 1px solid transparent; border-radius: 7px;
    padding: 5px 14px; color: @textSecondary; font-size: 12px; font-weight: 600;
}
QPushButton[role="segment"]:hover { color: @text; background: @surface; }
QPushButton[role="segmentActive"] {
    background: @surfaceRaised; border: 1px solid @line; border-radius: 7px;
    padding: 5px 14px; color: @text; font-size: 12px; font-weight: 600;
}

/* A whole list row that acts as a button (event rows in the review rail). */
QPushButton[role="row"] {
    background: transparent; border: none; border-radius: 7px; padding: 0; text-align: left;
}
QPushButton[role="row"]:hover { background: @surfaceRaised; }
QPushButton[role="row"]:pressed { background: @surface; }

QPushButton[role="icon"] {
    background: transparent; border: 1px solid transparent; border-radius: 8px;
    padding: 5px; color: @textSecondary; min-width: 28px;
}
QPushButton[role="icon"]:hover { background: @surfaceRaised; border-color: @line; color: @text; }

QPushButton[role="overlay"] {
    background: rgba(8, 10, 15, 210); border: 1px solid @line; border-radius: 8px;
    padding: 5px 10px; color: @textSecondary; font-size: 12px;
}
QPushButton[role="overlay"]:hover { color: @text; border-color: @accent; }
QPushButton[role="overlayActive"] {
    background: @accent; border: 1px solid @accent; border-radius: 8px;
    padding: 5px 10px; color: #ffffff; font-size: 12px; font-weight: 600;
}

/* ---- Inputs ---------------------------------------------------------- */
QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox {
    background: @sunken; border: 1px solid @line; border-radius: 8px;
    padding: 7px 10px; color: @text; selection-background-color: @accent;
}
QLineEdit:hover, QComboBox:hover, QSpinBox:hover, QDoubleSpinBox:hover { border-color: #2a3446; }
QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus { border-color: @accent; }
QComboBox::drop-down { border: none; width: 20px; }
QComboBox QAbstractItemView {
    background: @surfaceRaised; border: 1px solid @line; border-radius: 8px;
    selection-background-color: @accentTint; selection-color: @text; outline: 0;
}

QCheckBox { background: transparent; color: @textSecondary; spacing: 8px; }
QCheckBox::indicator {
    width: 15px; height: 15px; border: 1px solid @line; border-radius: 4px; background: @sunken;
}
QCheckBox::indicator:checked { background: @accent; border-color: @accent; }

/* ---- Review transport ------------------------------------------------ */
#transportBar { background: @chrome; border-top: 1px solid @line; }
QFrame[role="transportDeck"] {
    background: @sunken; border: 1px solid @lineSoft; border-radius: 21px;
}
QPushButton[role="transport"] {
    background: transparent; border: none; border-radius: 15px; padding: 0;
    min-width: 30px; max-width: 30px; min-height: 30px; max-height: 30px;
}
QPushButton[role="transport"]:hover   { background: @surfaceRaised; }
QPushButton[role="transport"]:pressed { background: @surface; }
QPushButton[role="play"] {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 @accentHover, stop:1 @accent);
    border: 1px solid @accentHover; border-radius: 17px; padding: 0 0 0 1px;
    min-width: 34px; max-width: 34px; min-height: 34px; max-height: 34px;
}
QPushButton[role="play"][playing="true"] { padding: 0; }
QPushButton[role="play"]:hover {
    background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #78abff, stop:1 @accentHover);
}
QPushButton[role="play"]:pressed { background: @accent; }
QPushButton[role="play"]:disabled { background: @surfaceRaised; border-color: @line; }
QLabel[role="timecode"] {
    font-family: "Cascadia Mono", "Consolas", monospace; font-size: 14px; font-weight: 600;
    color: @text;
}
QLabel[role="timecode"]:disabled { color: @textDim; }
QLabel[role="timecodeTotal"] {
    font-family: "Cascadia Mono", "Consolas", monospace; font-size: 12px; color: @textDim;
}
QLabel[role="frameCounter"] {
    font-family: "Cascadia Mono", "Consolas", monospace; font-size: 10px; color: @textDim;
}
QFrame[role="transportDivider"] { background: @line; border: none; }
#frameScrubber::groove:horizontal { height: 4px; }
#frameScrubber::handle:horizontal {
    width: 12px; height: 12px; margin: -5px 0; border-radius: 7px;
    background: #ffffff; border: 2px solid @accent;
}
#frameScrubber::handle:horizontal:disabled { background: @surfaceRaised; border-color: @line; }

/* ---- Sliders --------------------------------------------------------- */
QSlider::groove:horizontal { height: 4px; background: @line; border-radius: 2px; }
QSlider::sub-page:horizontal {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 @accentSunken, stop:1 @accentHover);
    border-radius: 2px;
}
QSlider::add-page:horizontal { background: @line; border-radius: 2px; }
QSlider::handle:horizontal {
    width: 13px; height: 13px; margin: -5px 0; border-radius: 7px;
    background: #ffffff; border: 2px solid @accent;
}
QSlider::handle:horizontal:hover { border-color: @accentHover; }

/* ---- Lists and tables ------------------------------------------------ */
QListWidget, QTreeWidget, QTableView {
    background: @sunken; border: 1px solid @lineSoft; border-radius: 10px;
    outline: 0; alternate-background-color: #080a0f;
}
/* No colour on ::item: it would override per-item foregrounds (track
   colours, dimmed missing entries). The list's own colour is the default. */
QListWidget { color: @textSecondary; }
QListWidget::item { border-bottom: 1px solid @lineSoft; padding: 8px; }
QListWidget::item:selected { background: @accentTint; color: @text; }
QListWidget::item:hover { background: @surface; }
QHeaderView::section {
    background: @surface; color: @textDim; border: none;
    border-bottom: 1px solid @line; padding: 7px 8px;
    font-size: 11px; font-weight: 700; letter-spacing: 0.8px;
}
QTableView { gridline-color: @lineSoft; }
QTableView::item { padding: 6px 8px; border: none; }
QTableView::item:selected { background: @accentTint; color: @text; }

/* ---- Tabs ------------------------------------------------------------ */
QTabWidget::pane { background: transparent; border: none; border-top: 1px solid @lineSoft; }
QTabBar::tab {
    background: transparent; color: @textDim; padding: 8px 14px;
    border-bottom: 2px solid transparent; font-size: 12px; font-weight: 600;
}
QTabBar::tab:hover { color: @textSecondary; }
QTabBar::tab:selected { color: @text; border-bottom: 2px solid @accent; }

/* ---- Scrollbars ------------------------------------------------------ */
QScrollBar:vertical { background: transparent; width: 10px; margin: 0; }
QScrollBar::handle:vertical { background: #1f2633; border-radius: 5px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: #2c3648; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 0; }
QScrollBar::handle:horizontal { background: #1f2633; border-radius: 5px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: #2c3648; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QScrollArea { border: none; background: transparent; }

/* ---- Menus ----------------------------------------------------------- */
QMenu {
    background: @surfaceRaised; border: 1px solid @line; border-radius: 10px; padding: 5px;
}
QMenu::item { padding: 7px 22px 7px 12px; border-radius: 6px; color: @text; }
QMenu::item:selected { background: @accentTint; }
QMenu::item:disabled { color: @textDim; }
QMenu::separator { height: 1px; background: @line; margin: 5px 8px; }

/* ---- Dialogs --------------------------------------------------------- */
QMessageBox, QInputDialog, QFileDialog { background: @surface; }

/* ---- Status bar ------------------------------------------------------ */
QStatusBar { background: @chrome; color: @textDim; border-top: 1px solid @lineSoft; }
QStatusBar::item { border: none; }
)SHEET"));
}

}  // namespace aquatrack::ui
