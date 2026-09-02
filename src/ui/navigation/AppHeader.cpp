#include "ui/navigation/AppHeader.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

using aquatrack::ui::Badge;
using aquatrack::ui::Glyph;
using aquatrack::ui::glyphIcon;
using aquatrack::ui::glyphPixmap;
using aquatrack::ui::iconButton;
using aquatrack::ui::typedLabel;
namespace palette = aquatrack::ui::palette;

namespace {

struct NavEntry {
    Glyph glyph;
    const char* label;
};

// Order matches application::AppState::Page.
const QVector<NavEntry> kNavigation{
    {Glyph::Dashboard, "Dashboard"}, {Glyph::Video, "Videos"},
    {Glyph::Review, "Review"},       {Glyph::Analytics, "Analytics"},
    {Glyph::Datasets, "Datasets"},   {Glyph::Models, "Models"},
};

constexpr int kSettingsPage = 6;

}  // namespace

AppHeader::AppHeader(QWidget* parent) : QFrame(parent)
{
    setProperty("role", "chrome");
    setFixedHeight(62);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(16, 0, 16, 0);
    row->setSpacing(0);

    // ---- Brand -------------------------------------------------------
    auto* mark = new QLabel(this);
    mark->setFixedSize(32, 32);
    mark->setPixmap(glyphPixmap(Glyph::Drop, 19, Qt::white));
    mark->setAlignment(Qt::AlignCenter);
    mark->setStyleSheet(QStringLiteral("background:%1;border-radius:8px")
                            .arg(palette::Accent.name()));
    row->addWidget(mark);
    row->addSpacing(10);

    auto* brand = new QVBoxLayout;
    brand->setContentsMargins(0, 0, 0, 0);
    brand->setSpacing(0);
    auto* wordmark = typedLabel(QStringLiteral("AquaTrack+"), "title", this);
    auto* tagline =
        typedLabel(QStringLiteral("Underwater detection, tracking & survey analysis"), "dim", this);
    brand->addWidget(wordmark);
    brand->addWidget(tagline);
    row->addLayout(brand);
    row->addSpacing(34);

    // ---- Primary navigation ------------------------------------------
    for (int index = 0; index < kNavigation.size(); ++index) {
        const auto& entry = kNavigation.at(index);
        auto* button = new QPushButton(QString::fromLatin1(entry.label), this);
        button->setProperty("role", "nav");
        button->setCursor(Qt::PointingHandCursor);
        button->setIcon(glyphIcon(entry.glyph, 16, palette::TextSecondary, palette::Accent));
        button->setIconSize(QSize(16, 16));
        connect(button, &QPushButton::clicked, this, [this, index] { emit pageRequested(index); });
        row->addWidget(button);
        m_navButtons.push_back(button);
    }
    row->addStretch();

    // ---- Utilities ---------------------------------------------------
    auto* settings = new QPushButton(QStringLiteral("Settings"), this);
    settings->setProperty("role", "nav");
    settings->setCursor(Qt::PointingHandCursor);
    settings->setIcon(glyphIcon(Glyph::Settings, 16, palette::TextSecondary, palette::Accent));
    settings->setIconSize(QSize(16, 16));
    connect(settings, &QPushButton::clicked, this,
            [this] { emit pageRequested(kSettingsPage); });
    row->addWidget(settings);
    m_navButtons.push_back(settings);

    auto* help = iconButton(Glyph::Help, QStringLiteral("Keyboard shortcuts and help"), this);
    connect(help, &QPushButton::clicked, this, &AppHeader::helpRequested);
    row->addWidget(help);

    auto* bellHolder = new QWidget(this);
    auto* bellRow = new QHBoxLayout(bellHolder);
    bellRow->setContentsMargins(0, 0, 0, 0);
    bellRow->setSpacing(3);
    bellRow->addWidget(
        iconButton(Glyph::Bell, QStringLiteral("Validation warnings"), bellHolder));
    m_notifications = new Badge(bellHolder);
    m_notifications->setAccent(palette::Warning);
    m_notifications->hide();
    bellRow->addWidget(m_notifications);
    row->addWidget(bellHolder);
    row->addSpacing(10);

    // ---- Workspace chip ----------------------------------------------
    m_avatar = new QLabel(this);
    m_avatar->setFixedSize(30, 30);
    m_avatar->setAlignment(Qt::AlignCenter);
    m_avatar->setPixmap(glyphPixmap(Glyph::User, 17, palette::TextSecondary));
    m_avatar->setStyleSheet(QStringLiteral("background:%1;border-radius:15px")
                                .arg(palette::SurfaceRaised.name()));
    row->addWidget(m_avatar);
    row->addSpacing(8);

    auto* workspace = new QVBoxLayout;
    workspace->setContentsMargins(0, 0, 0, 0);
    workspace->setSpacing(0);
    m_projectLabel = typedLabel(QStringLiteral("No project open"), "small", this);
    m_projectLabel->setStyleSheet(QStringLiteral("font-weight:600;color:%1")
                                      .arg(palette::TextPrimary.name()));
    m_projectCaption = typedLabel(QStringLiteral("AquaTrack workspace"), "dim", this);
    workspace->addWidget(m_projectLabel);
    workspace->addWidget(m_projectCaption);
    row->addLayout(workspace);

    setCurrentPage(0);
}

void AppHeader::setCurrentPage(int page)
{
    for (int index = 0; index < m_navButtons.size(); ++index) {
        auto* button = m_navButtons.at(index);
        const int pageForButton = index < kNavigation.size() ? index : kSettingsPage;
        const bool active = pageForButton == page;
        button->setProperty("role", active ? "navActive" : "nav");
        const Glyph glyph =
            index < kNavigation.size() ? kNavigation.at(index).glyph : Glyph::Settings;
        button->setIcon(glyphIcon(glyph, 16, active ? palette::Accent : palette::TextSecondary,
                                  palette::Accent));
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

void AppHeader::setProjectName(const QString& name)
{
    m_projectLabel->setText(name.isEmpty() ? QStringLiteral("No project open") : name);
    m_projectCaption->setText(name.isEmpty() ? QStringLiteral("AquaTrack workspace")
                                             : QStringLiteral("Active project"));
}

void AppHeader::setNotificationCount(int count)
{
    if (count <= 0) {
        m_notifications->hide();
        return;
    }
    m_notifications->setText(QString::number(count));
    m_notifications->show();
}
