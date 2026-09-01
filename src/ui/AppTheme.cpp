#include "ui/AppTheme.h"

QString aquatrack::ui::applicationStyleSheet()
{
    return QStringLiteral(R"(
        QMainWindow, QWidget { background:#061729; color:#dbe8f5; font-family:"Segoe UI"; font-size:13px; }
        QFrame[role="header"] { background:#061323; border-bottom:1px solid #17334d; }
        QFrame[role="panel"], QFrame[role="card"] { background:#0a2035; border:1px solid #143650; border-radius:7px; }
        QLabel[role="brand"] { font-size:21px; font-weight:700; color:#f5f9fd; }
        QLabel[role="section"] { color:#829bb2; font-size:11px; font-weight:600; }
        QLabel[role="muted"] { color:#8fa6bb; font-size:11px; }
        QLabel[role="metric"] { color:#46a6ff; font-size:24px; font-weight:600; }
        QPushButton { background:transparent; border:1px solid transparent; border-radius:5px; padding:8px 11px; color:#afc2d4; }
        QPushButton:hover { background:#102b44; color:white; }
        QPushButton[role="primary"] { background:#147ce5; color:white; font-weight:600; }
        QPushButton[role="navActive"] { color:#43a5ff; border-bottom:2px solid #1688f4; border-radius:0; }
        QSlider::groove:horizontal { height:5px; background:#29445c; border-radius:2px; }
        QSlider::sub-page:horizontal { background:#258cf1; }
        QSlider::handle:horizontal { width:14px; margin:-5px 0; background:#e8f4ff; border:2px solid #258cf1; border-radius:7px; }
        QListWidget { background:#081c30; border:1px solid #143650; border-radius:5px; outline:0; }
        QListWidget::item { border-bottom:1px solid #153047; padding:10px 8px; }
        QListWidget::item:selected { background:#11385a; color:white; }
        QTabWidget::pane { background:#081c30; border:1px solid #143650; border-radius:6px; }
        QTabBar::tab { background:#071a2d; color:#91a9bd; padding:9px 16px; border-bottom:2px solid transparent; }
        QTabBar::tab:selected { color:#48a8ff; border-bottom:2px solid #258cf1; }
        QCheckBox { color:#b9cad9; spacing:8px; }
        QStatusBar { background:#061323; color:#7993aa; border-top:1px solid #17334d; }
    )");
}
