#include "ui/navigation/AppHeader.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/HoverHint.h"
#include "ui/common/Widgets.h"

#include <QApplication>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>

using aquatrack::ui::Badge;
using aquatrack::ui::Glyph;
using aquatrack::ui::glyphIcon;
using aquatrack::ui::glyphPixmap;
using aquatrack::ui::typedLabel;
namespace palette = aquatrack::ui::palette;

namespace {

constexpr int kBarHeight = 54;
constexpr int kTitleMaxText = 260;  // px of project name before it is elided

}  // namespace

AppHeader::AppHeader(QWidget* parent) : QFrame(parent)
{
    setProperty("role", "chrome");
    setFixedHeight(kBarHeight);

    auto* row = new QHBoxLayout(this);
    // No right margin: the caption buttons run flush to the window edge.
    row->setContentsMargins(16, 0, 0, 0);
    row->setSpacing(0);

    // ---- Brand -------------------------------------------------------
    auto* mark = new QLabel(this);
    mark->setFixedSize(30, 30);
    mark->setPixmap(glyphPixmap(Glyph::Drop, 18, Qt::white));
    mark->setAlignment(Qt::AlignCenter);
    mark->setStyleSheet(QStringLiteral("background:qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                                       " stop:0 %1, stop:1 %2);border-radius:9px")
                            .arg(palette::AccentHover.name(), palette::AccentSunken.name()));
    row->addWidget(mark);
    row->addSpacing(10);

    auto* brand = new QVBoxLayout;
    brand->setContentsMargins(0, 0, 0, 0);
    brand->setSpacing(0);
    auto* wordmark = typedLabel(QStringLiteral("AquaTrack+"), "subtitle", this);
    auto* tagline = typedLabel(QStringLiteral("Underwater survey analysis"), "dim", this);
    brand->addStretch();
    brand->addWidget(wordmark);
    brand->addWidget(tagline);
    brand->addStretch();
    row->addLayout(brand);
    row->addStretch(1);

    // ---- Tools -------------------------------------------------------
    const auto tool = [this, row](Glyph glyph, const QString& label) {
        auto* button = new QPushButton(this);
        button->setProperty("role", "tool");
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedSize(38, 34);
        button->setIcon(glyphIcon(glyph, 17, palette::TextSecondary, palette::TextPrimary));
        button->setIconSize(QSize(17, 17));
        aquatrack::ui::attachHoverHint(button, label, aquatrack::ui::HintPlacement::Below);
        row->addWidget(button, 0, Qt::AlignVCenter);
        return button;
    };
    m_settings = tool(Glyph::Settings, QStringLiteral("Settings"));
    m_settings->setObjectName(QStringLiteral("settingsButton"));
    connect(m_settings, &QPushButton::clicked, this, &AppHeader::settingsRequested);
    row->addSpacing(2);
    auto* help = tool(Glyph::Help, QStringLiteral("Help"));
    help->setObjectName(QStringLiteral("helpButton"));
    connect(help, &QPushButton::clicked, this, &AppHeader::helpRequested);
    row->addSpacing(2);
    auto* bell = tool(Glyph::Bell, QStringLiteral("Validation warnings"));
    bell->setObjectName(QStringLiteral("warningsButton"));
    connect(bell, &QPushButton::clicked, this, &AppHeader::warningsRequested);
    // The count rides on the bell's corner instead of widening the row.
    m_notifications = new Badge(bell);
    m_notifications->setAccent(palette::Warning);
    m_notifications->hide();
    row->addSpacing(10);

    // ---- Window caption buttons --------------------------------------
    const auto caption = [this, row](Glyph glyph, const QString& tip, const char* role,
                                     const QColor& hoverColour) {
        auto* button = new QPushButton(this);
        button->setProperty("role", role);
        button->setToolTip(tip);
        button->setFocusPolicy(Qt::NoFocus);
        button->setFixedSize(46, kBarHeight);
        button->setIcon(glyphIcon(glyph, 14, palette::TextSecondary, hoverColour));
        button->setIconSize(QSize(14, 14));
        row->addWidget(button);
        return button;
    };
    auto* minimize = caption(Glyph::Minimize, QStringLiteral("Minimize"), "caption",
                             palette::TextPrimary);
    minimize->setObjectName(QStringLiteral("captionMinimize"));
    connect(minimize, &QPushButton::clicked, this, [this] { window()->showMinimized(); });
    m_maximize = caption(Glyph::Maximize, QStringLiteral("Maximize"), "caption",
                         palette::TextPrimary);
    m_maximize->setObjectName(QStringLiteral("captionMaximize"));
    connect(m_maximize, &QPushButton::clicked, this, &AppHeader::toggleMaximized);
    auto* close = caption(Glyph::Close, QStringLiteral("Close"), "captionClose", Qt::white);
    close->setObjectName(QStringLiteral("captionClose"));
    connect(close, &QPushButton::clicked, this, [this] { window()->close(); });

    // ---- Project title -----------------------------------------------
    // Positioned by hand (centreProjectTitle) rather than by the layout, so
    // it sits on the window's centre line like a title.
    m_projectChip = new QPushButton(this);
    m_projectChip->setObjectName(QStringLiteral("projectChip"));
    m_projectChip->setProperty("role", "projectChip");
    m_projectChip->setCursor(Qt::PointingHandCursor);
    m_projectChip->setFixedHeight(40);
    aquatrack::ui::attachHoverHint(m_projectChip, QStringLiteral("Project home"),
                                   aquatrack::ui::HintPlacement::Below);
    connect(m_projectChip, &QPushButton::clicked, this, &AppHeader::projectRequested);
    auto* chipRow = new QHBoxLayout(m_projectChip);
    chipRow->setContentsMargins(5, 5, 14, 5);
    chipRow->setSpacing(10);
    m_avatar = new QLabel(m_projectChip);
    m_avatar->setFixedSize(30, 30);
    m_avatar->setAlignment(Qt::AlignCenter);
    m_avatar->setAttribute(Qt::WA_TransparentForMouseEvents);
    chipRow->addWidget(m_avatar);
    auto* chipText = new QVBoxLayout;
    chipText->setContentsMargins(0, 0, 0, 0);
    chipText->setSpacing(0);
    m_projectLabel = new QLabel(m_projectChip);
    m_projectLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_projectLabel->setStyleSheet(
        QStringLiteral("font-size:13px;font-weight:600;color:%1").arg(palette::TextPrimary.name()));
    m_projectCaption = new QLabel(m_projectChip);
    m_projectCaption->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_projectCaption->setStyleSheet(
        QStringLiteral("font-size:11px;color:%1").arg(palette::TextDim.name()));
    chipText->addStretch();
    chipText->addWidget(m_projectLabel);
    chipText->addWidget(m_projectCaption);
    chipText->addStretch();
    chipRow->addLayout(chipText, 1);
    setProjectName(QString());
}

void AppHeader::setProjectName(const QString& name)
{
    const bool open = !name.isEmpty();
    const QString title = open ? name : QStringLiteral("No project open");
    const QString caption = open ? QStringLiteral("Active project")
                                 : QStringLiteral("Create or open a project");
    // Long names are elided so the title never runs into the tools.
    const QFontMetrics titleMetrics(m_projectLabel->font());
    const QString shown = titleMetrics.elidedText(title, Qt::ElideRight, kTitleMaxText);
    m_projectLabel->setText(shown);
    m_projectCaption->setText(caption);
    m_projectChip->setAccessibleName(open ? QStringLiteral("Project %1").arg(name) : title);

    if (open) {
        // The project's initial on the brand gradient.
        m_avatar->setPixmap({});
        m_avatar->setText(name.trimmed().left(1).toUpper());
        m_avatar->setStyleSheet(
            QStringLiteral("background:qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:1 %2);"
                           "border-radius:15px;color:white;font-weight:700;font-size:13px")
                .arg(palette::AccentHover.name(), palette::AccentSunken.name()));
    } else {
        m_avatar->setText(QString());
        m_avatar->setPixmap(glyphPixmap(Glyph::User, 16, palette::TextSecondary));
        m_avatar->setStyleSheet(QStringLiteral("background:%1;border:1px solid %2;border-radius:15px")
                                    .arg(palette::SurfaceHover.name(), palette::Line.name()));
    }

    // Size the title to its text: avatar, gap, the wider of the two lines.
    const int textWidth =
        std::max(titleMetrics.horizontalAdvance(shown),
                 QFontMetrics(m_projectCaption->font()).horizontalAdvance(caption));
    m_projectChip->setFixedWidth(5 + 30 + 10 + textWidth + 14 + 4);
    centreProjectTitle();
}

void AppHeader::centreProjectTitle()
{
    m_projectChip->move((width() - m_projectChip->width()) / 2,
                        (height() - m_projectChip->height()) / 2);
    m_projectChip->raise();
}

void AppHeader::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    centreProjectTitle();
}

void AppHeader::setWindowMaximized(bool maximized)
{
    m_maximize->setIcon(glyphIcon(maximized ? Glyph::Restore : Glyph::Maximize, 14,
                                  palette::TextSecondary, palette::TextPrimary));
    m_maximize->setToolTip(maximized ? QStringLiteral("Restore down") : QStringLiteral("Maximize"));
}

void AppHeader::setSettingsActive(bool active)
{
    m_settings->setProperty("role", active ? "toolActive" : "tool");
    m_settings->setIcon(glyphIcon(Glyph::Settings, 17,
                                  active ? palette::AccentHover : palette::TextSecondary,
                                  active ? palette::AccentHover : palette::TextPrimary));
    m_settings->style()->unpolish(m_settings);
    m_settings->style()->polish(m_settings);
}

void AppHeader::toggleMaximized()
{
    QWidget* top = window();
    if (top->isMaximized())
        top->showNormal();
    else
        top->showMaximized();
}

void AppHeader::mousePressEvent(QMouseEvent* event)
{
    // Buttons consume their own presses, so anything arriving here is a
    // press on empty chrome or on a label: the window's drag handle.
    if (event->button() == Qt::LeftButton) {
        m_dragArmed = true;
        m_pressPosition = event->position().toPoint();
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void AppHeader::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragArmed && (event->buttons() & Qt::LeftButton)
        && (event->position().toPoint() - m_pressPosition).manhattanLength()
               >= QApplication::startDragDistance()) {
        m_dragArmed = false;
        // The system move loop gives Aero Snap, drag-to-restore from
        // maximized, and multi-monitor moves for free.
        if (QWindow* handle = window()->windowHandle())
            handle->startSystemMove();
        event->accept();
        return;
    }
    QFrame::mouseMoveEvent(event);
}

void AppHeader::mouseReleaseEvent(QMouseEvent* event)
{
    m_dragArmed = false;
    QFrame::mouseReleaseEvent(event);
}

void AppHeader::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragArmed = false;
        toggleMaximized();
        event->accept();
        return;
    }
    QFrame::mouseDoubleClickEvent(event);
}

void AppHeader::setNotificationCount(int count)
{
    if (count <= 0) {
        m_notifications->hide();
        return;
    }
    m_notifications->setText(count > 99 ? QStringLiteral("99+") : QString::number(count));
    m_notifications->adjustSize();
    auto* bell = m_notifications->parentWidget();
    m_notifications->move(bell->width() - m_notifications->width(), 0);
    m_notifications->show();
    m_notifications->raise();
}
