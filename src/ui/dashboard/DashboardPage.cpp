#include "ui/dashboard/DashboardPage.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QFileInfo>
#include <QStringList>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

using namespace aquatrack::ui;

namespace {

// A quick action: icon, title, one line of why you would press it.
QPushButton* actionCard(Glyph glyph, const QString& title, const QString& caption, QWidget* parent)
{
    auto* button = new QPushButton(parent);
    button->setProperty("role", "chip");
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(62);

    auto* row = new QHBoxLayout(button);
    row->setContentsMargins(12, 9, 12, 9);
    row->setSpacing(11);

    auto* icon = new QLabel(button);
    icon->setPixmap(glyphPixmap(glyph, 19, palette::Accent));
    icon->setFixedWidth(19);
    row->addWidget(icon);

    auto* column = new QVBoxLayout;
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(1);
    auto* heading = typedLabel(title, "body", button);
    heading->setStyleSheet(QStringLiteral("font-weight:600;color:%1")
                               .arg(palette::TextPrimary.name()));
    auto* description = typedLabel(caption, "dim", button);
    description->setWordWrap(true);
    column->addWidget(heading);
    column->addWidget(description);
    row->addLayout(column, 1);
    return button;
}

QWidget* step(int number, const QString& text, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 2);
    layout->setSpacing(10);

    auto* badge = new QLabel(QString::number(number), row);
    badge->setFixedSize(20, 20);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QStringLiteral("background:%1;color:white;border-radius:10px;"
                                        "font-size:11px;font-weight:700")
                             .arg(palette::Accent.name()));
    layout->addWidget(badge, 0, Qt::AlignTop);

    auto* label = typedLabel(text, "small", row);
    label->setWordWrap(true);
    layout->addWidget(label, 1);
    return row;
}

}  // namespace

DashboardPage::DashboardPage(QWidget* parent) : QWidget(parent)
{
    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(24, 20, 24, 20);
    page->setSpacing(12);

    auto* titles = new QVBoxLayout;
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(1);
    titles->addWidget(sectionLabel(QStringLiteral("Workspace"), this));
    titles->addWidget(typedLabel(QStringLiteral("AquaTrack+"), "display", this));
    page->addLayout(titles);

    // ---- Current project --------------------------------------------------
    auto* projectCard = card(this);
    auto* projectBody = new QVBoxLayout(projectCard);
    projectBody->setContentsMargins(16, 14, 16, 15);
    projectBody->setSpacing(10);

    auto* projectHead = new QHBoxLayout;
    projectHead->setContentsMargins(0, 0, 0, 0);
    projectHead->setSpacing(8);
    auto* projectTitles = new QVBoxLayout;
    projectTitles->setContentsMargins(0, 0, 0, 0);
    projectTitles->setSpacing(1);
    m_projectName = typedLabel(QStringLiteral("No project open"), "title", projectCard);
    projectTitles->addWidget(m_projectName);
    m_projectPath = typedLabel(QString(), "mono", projectCard);
    m_projectPath->setWordWrap(true);
    projectTitles->addWidget(m_projectPath);
    projectHead->addLayout(projectTitles, 1);
    m_projectState = new Badge(projectCard);
    projectHead->addWidget(m_projectState, 0, Qt::AlignTop);
    projectBody->addLayout(projectHead);

    // Shown when a project is open.
    m_projectDetail = new QWidget(projectCard);
    auto* detail = new QVBoxLayout(m_projectDetail);
    detail->setContentsMargins(0, 0, 0, 0);
    detail->setSpacing(9);
    m_projectMedia = typedLabel(QString(), "small", m_projectDetail);
    detail->addWidget(m_projectMedia);
    auto* projectActions = new QHBoxLayout;
    projectActions->setContentsMargins(0, 0, 0, 0);
    projectActions->setSpacing(8);
    m_review = textButton(QStringLiteral("Go to review"), m_projectDetail, "primary");
    m_review->setIcon(glyphIcon(Glyph::Review, 15, Qt::white, Qt::white));
    connect(m_review, &QPushButton::clicked, this, &DashboardPage::reviewRequested);
    m_save = textButton(QStringLiteral("Save project"), m_projectDetail);
    m_save->setObjectName(QStringLiteral("saveProject"));
    m_save->setIcon(glyphIcon(Glyph::Save, 15, palette::TextSecondary, palette::TextPrimary));
    connect(m_save, &QPushButton::clicked, this, &DashboardPage::saveProjectRequested);
    projectActions->addWidget(m_review);
    projectActions->addWidget(m_save);
    projectActions->addStretch();
    detail->addLayout(projectActions);
    projectBody->addWidget(m_projectDetail);

    // Shown instead when there is no project: what to do, in order.
    m_gettingStarted = new QWidget(projectCard);
    auto* started = new QVBoxLayout(m_gettingStarted);
    started->setContentsMargins(0, 0, 0, 0);
    started->setSpacing(5);
    started->addWidget(step(1, QStringLiteral("Create a project. It stores your observations "
                                              "and the media you reference."),
                            m_gettingStarted));
    started->addWidget(step(2, QStringLiteral("Add a BrackishMOT sequence, or scan a folder of "
                                              "them from the Datasets page."),
                            m_gettingStarted));
    started->addWidget(step(3, QStringLiteral("Review frames and tracks, correct annotations, "
                                              "then export CSV or MOTChallenge."),
                            m_gettingStarted));
    auto* startActions = new QHBoxLayout;
    startActions->setContentsMargins(0, 6, 0, 0);
    startActions->setSpacing(8);
    auto* create = textButton(QStringLiteral("New project"), m_gettingStarted, "primary");
    create->setIcon(glyphIcon(Glyph::Files, 15, Qt::white, Qt::white));
    connect(create, &QPushButton::clicked, this, &DashboardPage::newProjectRequested);
    auto* open = textButton(QStringLiteral("Open project"), m_gettingStarted);
    connect(open, &QPushButton::clicked, this, &DashboardPage::openProjectRequested);
    startActions->addWidget(create);
    startActions->addWidget(open);
    startActions->addStretch();
    started->addLayout(startActions);
    projectBody->addWidget(m_gettingStarted);
    page->addWidget(projectCard);

    // ---- Workspace totals --------------------------------------------------
    auto* totalsCard = card(this);
    auto* totals = new QHBoxLayout(totalsCard);
    totals->setContentsMargins(16, 14, 16, 14);
    totals->setSpacing(10);
    m_datasets = new MetricTile(QStringLiteral("Datasets catalogued"), totalsCard);
    m_models = new MetricTile(QStringLiteral("Models registered"), totalsCard);
    m_observations = new MetricTile(QStringLiteral("Observations loaded"), totalsCard);
    m_tracks = new MetricTile(QStringLiteral("Unique tracks"), totalsCard);
    for (auto* tile : {m_datasets, m_models, m_observations, m_tracks})
        totals->addWidget(tile);
    page->addWidget(totalsCard);

    // ---- Recent projects and quick actions ----------------------------------
    auto* lower = new QHBoxLayout;
    lower->setContentsMargins(0, 0, 0, 0);
    lower->setSpacing(12);

    auto* recentCard = card(this);
    auto* recentBody = new QVBoxLayout(recentCard);
    recentBody->setContentsMargins(16, 14, 16, 15);
    recentBody->setSpacing(8);
    recentBody->addWidget(sectionLabel(QStringLiteral("Recent projects"), recentCard));
    m_recentEmpty = typedLabel(
        QStringLiteral("Projects you create or open will be listed here."), "dim", recentCard);
    m_recentEmpty->setWordWrap(true);
    recentBody->addWidget(m_recentEmpty);
    m_recent = new QListWidget(recentCard);
    m_recent->setObjectName(QStringLiteral("recentProjects"));
    m_recent->setMinimumHeight(150);
    m_recent->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(m_recent, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        emit recentProjectActivated(item->data(Qt::UserRole).toString());
    });
    connect(m_recent, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) {
        emit recentProjectActivated(item->data(Qt::UserRole).toString());
    });
    recentBody->addWidget(m_recent, 1);
    lower->addWidget(recentCard, 3);

    auto* actionsCard = card(this);
    auto* actionsBody = new QVBoxLayout(actionsCard);
    actionsBody->setContentsMargins(16, 14, 16, 15);
    actionsBody->setSpacing(8);
    actionsBody->addWidget(sectionLabel(QStringLiteral("Quick actions"), actionsCard));

    auto* openSequence =
        actionCard(Glyph::Review, QStringLiteral("Open a sequence"),
                   QStringLiteral("Review a BrackishMOT folder directly"), actionsCard);
    connect(openSequence, &QPushButton::clicked, this, &DashboardPage::openSequenceRequested);
    auto* scanDatasets =
        actionCard(Glyph::Datasets, QStringLiteral("Scan datasets"),
                   QStringLiteral("Catalogue a folder of sequences or YOLO data"), actionsCard);
    connect(scanDatasets, &QPushButton::clicked, this, &DashboardPage::scanDatasetsRequested);
    auto* scanModels =
        actionCard(Glyph::Models, QStringLiteral("Scan model registry"),
                   QStringLiteral("Find model.json manifests and their weights"), actionsCard);
    connect(scanModels, &QPushButton::clicked, this, &DashboardPage::scanModelsRequested);
    auto* newProject =
        actionCard(Glyph::Files, QStringLiteral("New project"),
                   QStringLiteral("Start a fresh observation store"), actionsCard);
    connect(newProject, &QPushButton::clicked, this, &DashboardPage::newProjectRequested);

    for (auto* action : {openSequence, scanDatasets, scanModels, newProject})
        actionsBody->addWidget(action);
    actionsBody->addStretch();
    lower->addWidget(actionsCard, 2);
    page->addLayout(lower, 1);

    setProject(QString(), QString(), 0, false);
    setRecentProjects({});
    setWorkspaceTotals(0, 0, 0, 0);
}

void DashboardPage::setProject(const QString& name, const QString& path, int mediaCount,
                               bool unsaved)
{
    const bool open = !name.isEmpty();
    m_projectDetail->setVisible(open);
    m_gettingStarted->setVisible(!open);

    if (!open) {
        m_projectName->setText(QStringLiteral("No project open"));
        m_projectPath->clear();
        m_projectState->setText(QStringLiteral("Getting started"));
        m_projectState->setAccent(palette::TextDim);
        return;
    }

    m_projectName->setText(name);
    m_projectPath->setText(path);
    m_projectState->setText(unsaved ? QStringLiteral("Unsaved changes")
                                    : QStringLiteral("Saved"));
    m_projectState->setAccent(unsaved ? palette::Warning : palette::Success);
    m_save->setEnabled(unsaved);
    m_projectMedia->setText(
        mediaCount == 0
            ? QStringLiteral("No media referenced yet. Add a sequence from the Videos page.")
            : QStringLiteral("%1 media item(s) referenced.").arg(mediaCount));
}

void DashboardPage::setRecentProjects(const QStringList& paths)
{
    m_recent->clear();
    for (const auto& path : paths) {
        const QFileInfo info(path);
        // Full paths force a scrollbar and are mostly shared prefix; the last
        // couple of segments identify a project at a glance. The whole path
        // stays available as the tooltip.
        const auto segments = info.absolutePath().split(QLatin1Char('/'), Qt::SkipEmptyParts);
        const QString location =
            segments.size() <= 2
                ? info.absolutePath()
                : QStringLiteral(".../%1").arg(
                      QStringList(segments.mid(segments.size() - 2)).join(QLatin1Char('/')));
        auto* item = new QListWidgetItem(
            QStringLiteral("%1\n%2").arg(info.completeBaseName(), location));
        item->setData(Qt::UserRole, path);
        item->setToolTip(info.absoluteFilePath());
        if (!info.exists()) {
            // A project that has been moved or deleted should not look live.
            item->setForeground(palette::TextDim);
            item->setText(item->text() + QStringLiteral("   (missing)"));
        }
        m_recent->addItem(item);
    }
    m_recent->setVisible(!paths.isEmpty());
    m_recentEmpty->setVisible(paths.isEmpty());
}

void DashboardPage::setWorkspaceTotals(int datasets, int models, int observations, int tracks)
{
    const QLocale locale = QLocale::system();
    m_datasets->setValue(datasets > 0 ? locale.toString(datasets) : QStringLiteral("—"));
    m_models->setValue(models > 0 ? locale.toString(models) : QStringLiteral("—"));
    m_observations->setValue(observations > 0 ? locale.toString(observations)
                                              : QStringLiteral("—"));
    m_tracks->setValue(tracks > 0 ? locale.toString(tracks) : QStringLiteral("—"));
}
