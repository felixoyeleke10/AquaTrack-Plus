#include "ui/dashboard/DashboardPage.h"

#include "application/ReviewSession.h"
#include "data/ProjectFileStore.h"
#include "data/media/MediaSource.h"
#include "data/media/ThumbnailCache.h"
#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"
#include "ui/dashboard/DashboardCards.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

using namespace aquatrack::ui;
using namespace aquatrack::ui::dashboard;
using aquatrack::application::MediaProgress;
using aquatrack::application::ProjectSummary;

namespace {

constexpr int kPosterWidth = 480;
constexpr int kResumeWidth = 640;
constexpr int kHeroFrames = 5;

// A layout-only container. Plain QWidgets otherwise take the canvas colour
// from the application sheet.
QWidget* bare(QWidget* parent)
{
    auto* widget = new QWidget(parent);
    widget->setProperty("role", "bare");
    return widget;
}

// A page that scrolls when the window is short, with the page margins.
QScrollArea* scrollPage(QWidget* parent, QVBoxLayout*& body, QWidget*& content)
{
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    content = bare(scroll);
    scroll->setWidget(content);
    body = new QVBoxLayout(content);
    body->setContentsMargins(28, 22, 28, 28);
    body->setSpacing(20);
    return scroll;
}

QLabel* eyebrow(const QString& text, QWidget* parent, bool accent = false)
{
    auto* label = sectionLabel(text, parent);
    if (accent)
        label->setStyleSheet(QStringLiteral("color:%1").arg(palette::AccentHover.name()));
    return label;
}

QPushButton* ghost(const QString& text, Glyph glyph, QWidget* parent)
{
    auto* button = textButton(text, parent, "ghost");
    button->setIcon(glyphIcon(glyph, 15, palette::TextSecondary, palette::TextPrimary));
    return button;
}

QPushButton* primary(const QString& text, Glyph glyph, QWidget* parent)
{
    auto* button = textButton(text, parent, "primary");
    button->setIcon(glyphIcon(glyph, 15, Qt::white, Qt::white));
    return button;
}

// "19 h ago", "yesterday", "12 Mar 2026".
QString ago(const QDateTime& when)
{
    const qint64 seconds = when.secsTo(QDateTime::currentDateTime());
    if (seconds < 60)
        return QStringLiteral("just now");
    if (seconds < 3600)
        return QStringLiteral("%1 min ago").arg(seconds / 60);
    if (seconds < 86400)
        return QStringLiteral("%1 h ago").arg(seconds / 3600);
    const qint64 days = when.date().daysTo(QDate::currentDate());
    if (days <= 1)
        return QStringLiteral("yesterday");
    if (days < 14)
        return QStringLiteral("%1 days ago").arg(days);
    return QLocale::system().toString(when.date(), QStringLiteral("d MMM yyyy"));
}

QString duration(qint64 milliseconds)
{
    const qint64 total = milliseconds / 1000;
    const qint64 hours = total / 3600;
    const qint64 minutes = (total / 60) % 60;
    const qint64 seconds = total % 60;
    return hours > 0 ? QStringLiteral("%1:%2:%3")
                           .arg(hours)
                           .arg(minutes, 2, 10, QLatin1Char('0'))
                           .arg(seconds, 2, 10, QLatin1Char('0'))
                     : QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
}

QString count(int value, const QString& one, const QString& many)
{
    return QStringLiteral("%1 %2").arg(QLocale::system().toString(value), value == 1 ? one : many);
}

QString percent(double ratio)
{
    return QStringLiteral("%1%").arg(int(std::round(ratio * 100)));
}

// The last two folders identify a project; the full path is the tooltip.
QString shortLocation(const QFileInfo& info)
{
    const auto segments = info.absolutePath().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (segments.size() <= 2)
        return QDir::toNativeSeparators(info.absolutePath());
    return QDir::toNativeSeparators(QStringLiteral(".../%1").arg(
        QStringList(segments.mid(segments.size() - 2)).join(QLatin1Char('/'))));
}

bool samePath(const QString& left, const QString& right)
{
    if (left.isEmpty() || right.isEmpty())
        return false;
    return QDir::cleanPath(QFileInfo(left).absoluteFilePath())
               .compare(QDir::cleanPath(QFileInfo(right).absoluteFilePath()), Qt::CaseInsensitive) == 0;
}

QListWidgetItem* itemForPath(QListWidget* list, const QString& path)
{
    for (int i = 0; i < list->count(); ++i)
        if (samePath(list->item(i)->data(PathRole).toString(), path))
            return list->item(i);
    return nullptr;
}

QString mediaName(const QString& path)
{
    const QString name = aquatrack::media::MediaSource::reviewIdFor(path);
    return name.isEmpty() ? QFileInfo(path).completeBaseName() : name;
}

// A figure in the progress card: value over caption, no box of its own.
QLabel* figure(QGridLayout* grid, int column, const QString& caption, QWidget* parent)
{
    auto* value = typedLabel(QStringLiteral("\u2014"), "subtitle", parent);
    grid->addWidget(value, 0, column);
    grid->addWidget(typedLabel(caption, "dim", parent), 1, column);
    return value;
}

}  // namespace

// ------------------------------------------------------------------ Page

DashboardPage::DashboardPage(QWidget* parent) : QWidget(parent)
{
    m_pool.setMaxThreadCount(2);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    m_modes = new QStackedWidget(this);
    m_modes->setProperty("role", "bare");
    m_modes->addWidget(buildBrowser());
    m_modes->addWidget(buildOverview());
    outer->addWidget(m_modes);

    setProject(QString(), QString(), {}, QString(), false);
    setRecentProjects({});
}

DashboardPage::~DashboardPage()
{
    // Queued thumbnails would only be thrown away; let running ones finish.
    m_pool.clear();
    m_pool.waitForDone();
}

// --------------------------------------------------------------- Browser

QWidget* DashboardPage::buildBrowser()
{
    QVBoxLayout* body = nullptr;
    QWidget* content = nullptr;
    auto* scroll = scrollPage(this, body, content);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(10);
    auto* titles = new QVBoxLayout;
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(3);
    titles->addWidget(eyebrow(QStringLiteral("AquaTrack+"), content, true));
    titles->addWidget(typedLabel(QStringLiteral("Projects"), "display", content));
    m_browserSubtitle = typedLabel(QString(), "small", content);
    titles->addWidget(m_browserSubtitle);
    header->addLayout(titles, 1);

    m_backToProject = ghost(QStringLiteral("Back to project"), Glyph::Dashboard, content);
    connect(m_backToProject, &QPushButton::clicked, this, [this] {
        m_browsing = false;
        showMode();
    });
    auto* open = textButton(QStringLiteral("Open project..."), content);
    connect(open, &QPushButton::clicked, this, &DashboardPage::openProjectRequested);
    auto* create = primary(QStringLiteral("New project"), Glyph::Files, content);
    connect(create, &QPushButton::clicked, this, &DashboardPage::newProjectRequested);
    header->addWidget(m_backToProject, 0, Qt::AlignBottom);
    header->addWidget(open, 0, Qt::AlignBottom);
    header->addWidget(create, 0, Qt::AlignBottom);
    body->addLayout(header);

    // Quieter ways in that do not need a project.
    auto* tools = new QHBoxLayout;
    tools->setContentsMargins(0, 0, 0, 0);
    tools->setSpacing(2);
    auto* openMedia = ghost(QStringLiteral("Open media..."), Glyph::Video, content);
    openMedia->setToolTip(QStringLiteral("Review a video or sequence folder without a project"));
    connect(openMedia, &QPushButton::clicked, this, &DashboardPage::openSequenceRequested);
    auto* scanDatasets = ghost(QStringLiteral("Scan datasets..."), Glyph::Datasets, content);
    connect(scanDatasets, &QPushButton::clicked, this, &DashboardPage::scanDatasetsRequested);
    auto* scanModels = ghost(QStringLiteral("Scan model registry..."), Glyph::Models, content);
    connect(scanModels, &QPushButton::clicked, this, &DashboardPage::scanModelsRequested);
    tools->addWidget(openMedia);
    tools->addWidget(scanDatasets);
    tools->addWidget(scanModels);
    tools->addStretch();
    m_clearMissing = ghost(QStringLiteral("Clear missing"), Glyph::Alert, content);
    m_clearMissing->setObjectName(QStringLiteral("clearMissingRecent"));
    m_clearMissing->setToolTip(QStringLiteral("Remove projects that can no longer be found"));
    connect(m_clearMissing, &QPushButton::clicked, this,
            &DashboardPage::clearMissingRecentRequested);
    tools->addWidget(m_clearMissing);
    body->addLayout(tools);

    m_recent = new CardGrid(260, ProjectCardDelegate::TextHeight, content);
    m_recent->setObjectName(QStringLiteral("recentProjects"));
    m_recent->setItemDelegate(new ProjectCardDelegate(m_recent));
    // One click opens, like any launcher. Opening moves the project to the
    // front and rebuilds the grid, so the mouse's double-click activation is
    // ignored: it would land on whichever card slid under the pointer.
    // Enter still activates from the keyboard.
    connect(m_recent, &QListWidget::itemClicked, this, &DashboardPage::openRecentItem);
    connect(m_recent, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        if (QGuiApplication::mouseButtons() == Qt::NoButton)
            openRecentItem(item);
    });
    m_recent->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_recent, &QListWidget::customContextMenuRequested, this, [this](const QPoint& at) {
        QListWidgetItem* item = m_recent->itemAt(at);
        if (!item)
            return;
        const QString path = item->data(PathRole).toString();
        const bool exists = QFileInfo::exists(path);
        QMenu menu(m_recent);
        QAction* open = menu.addAction(exists ? QStringLiteral("Open")
                                              : QStringLiteral("Locate..."));
        QAction* reveal = menu.addAction(QStringLiteral("Show in folder"));
        reveal->setEnabled(exists);
        menu.addSeparator();
        QAction* remove = menu.addAction(QStringLiteral("Remove from list"));
        QAction* chosen = menu.exec(m_recent->viewport()->mapToGlobal(at));
        if (chosen == open)
            emit recentProjectActivated(path);
        else if (chosen == reveal)
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        else if (chosen == remove)
            emit recentProjectRemoveRequested(path);
    });
    // The grid's cards carry their own inset; line its edges up with the
    // header above.
    auto* gridRow = new QHBoxLayout;
    gridRow->setContentsMargins(-8, -8, -8, 0);
    gridRow->addWidget(m_recent);
    body->addLayout(gridRow);

    // First run: nothing to list yet.
    m_recentEmpty = card(content);
    auto* empty = new QVBoxLayout(m_recentEmpty);
    empty->setContentsMargins(32, 56, 32, 56);
    empty->setSpacing(8);
    auto* mark = new QLabel(m_recentEmpty);
    mark->setFixedSize(56, 56);
    mark->setAlignment(Qt::AlignCenter);
    mark->setPixmap(glyphPixmap(Glyph::Drop, 28, Qt::white));
    mark->setStyleSheet(QStringLiteral("background:qlineargradient(x1:0, y1:0, x2:1, y2:1,"
                                       " stop:0 %1, stop:1 %2);border-radius:16px")
                            .arg(palette::AccentHover.name(), palette::AccentSunken.name()));
    empty->addWidget(mark, 0, Qt::AlignHCenter);
    empty->addSpacing(8);
    auto* emptyTitle = typedLabel(QStringLiteral("Start your first survey"), "title", m_recentEmpty);
    emptyTitle->setAlignment(Qt::AlignCenter);
    empty->addWidget(emptyTitle);
    auto* emptyBody = typedLabel(
        QStringLiteral("A project keeps a survey's footage, annotations and exports together "
                       "in one folder. Create one, or open an existing .aqp file."),
        "small", m_recentEmpty);
    emptyBody->setAlignment(Qt::AlignCenter);
    emptyBody->setWordWrap(true);
    empty->addWidget(emptyBody);
    body->addWidget(m_recentEmpty);
    body->addStretch();
    return scroll;
}

// -------------------------------------------------------------- Overview

QWidget* DashboardPage::buildOverview()
{
    QVBoxLayout* body = nullptr;
    QWidget* content = nullptr;
    auto* scroll = scrollPage(this, body, content);
    body->setSpacing(18);

    // ---- Top bar: back to the browser, project state and file actions.
    auto* top = new QHBoxLayout;
    top->setContentsMargins(0, 0, 0, 0);
    top->setSpacing(8);
    auto* allProjects = ghost(QStringLiteral("All projects"), Glyph::GridView, content);
    allProjects->setObjectName(QStringLiteral("allProjects"));
    connect(allProjects, &QPushButton::clicked, this, [this] {
        m_browsing = true;
        showMode();
    });
    top->addWidget(allProjects);
    top->addStretch();
    m_projectState = new Badge(content);
    top->addWidget(m_projectState, 0, Qt::AlignVCenter);
    auto* openFolder = ghost(QStringLiteral("Open folder"), Glyph::Files, content);
    openFolder->setObjectName(QStringLiteral("openProjectFolder"));
    openFolder->setToolTip(QStringLiteral("Show the project folder in Explorer"));
    connect(openFolder, &QPushButton::clicked, this, &DashboardPage::openProjectFolderRequested);
    m_save = textButton(QStringLiteral("Save"), content);
    m_save->setObjectName(QStringLiteral("saveProject"));
    m_save->setIcon(glyphIcon(Glyph::Save, 15, palette::TextSecondary, palette::TextPrimary));
    connect(m_save, &QPushButton::clicked, this, &DashboardPage::saveProjectRequested);
    top->addWidget(openFolder);
    top->addWidget(m_save);
    body->addLayout(top);

    m_hero = new HeroBanner(content);
    body->addWidget(m_hero);

    // ---- Next action | survey progress.
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(18);

    auto* continueCard = card(content);
    auto* continueBody = new QVBoxLayout(continueCard);
    continueBody->setContentsMargins(20, 20, 20, 20);
    m_continue = new QStackedWidget(continueCard);
    m_continue->setProperty("role", "bare");

    // Resume or start: a frame, what it is, and one button.
    auto* resume = bare(m_continue);
    auto* resumeRow = new QHBoxLayout(resume);
    resumeRow->setContentsMargins(0, 0, 0, 0);
    resumeRow->setSpacing(22);
    m_continueFrame = new FramePreview(resume);
    m_continueFrame->setFixedSize(352, 198);
    resumeRow->addWidget(m_continueFrame, 0, Qt::AlignTop);
    auto* resumeText = new QVBoxLayout;
    resumeText->setContentsMargins(0, 2, 0, 0);
    resumeText->setSpacing(6);
    m_continueEyebrow = eyebrow(QString(), resume, true);
    m_continueTitle = typedLabel(QString(), "title", resume);
    m_continueTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_continueDetail = typedLabel(QString(), "small", resume);
    m_continueDetail->setWordWrap(true);
    m_continuePosition = new RatioBar(resume, 4);
    resumeText->addWidget(m_continueEyebrow);
    resumeText->addWidget(m_continueTitle);
    resumeText->addWidget(m_continueDetail);
    resumeText->addSpacing(4);
    resumeText->addWidget(m_continuePosition);
    resumeText->addStretch();
    auto* resumeButtons = new QHBoxLayout;
    resumeButtons->setContentsMargins(0, 0, 0, 0);
    resumeButtons->setSpacing(8);
    m_continueButton = primary(QStringLiteral("Resume"), Glyph::Play, resume);
    m_continueButton->setObjectName(QStringLiteral("continueReview"));
    connect(m_continueButton, &QPushButton::clicked, this, [this] {
        if (!m_continuePath.isEmpty())
            emit mediaReviewRequested(m_continuePath, m_continueFrameNumber);
    });
    auto* manage = ghost(QStringLiteral("All media"), Glyph::ListView, resume);
    connect(manage, &QPushButton::clicked, this, &DashboardPage::manageMediaRequested);
    resumeButtons->addWidget(m_continueButton);
    resumeButtons->addWidget(manage);
    resumeButtons->addStretch();
    resumeText->addLayout(resumeButtons);
    resumeRow->addLayout(resumeText, 1);
    m_continue->addWidget(resume);

    // An empty project: where footage comes from.
    auto* start = bare(m_continue);
    auto* startBody = new QVBoxLayout(start);
    startBody->setContentsMargins(4, 4, 4, 4);
    startBody->setSpacing(6);
    startBody->addWidget(eyebrow(QStringLiteral("Get started"), start, true));
    startBody->addWidget(typedLabel(QStringLiteral("Add footage to this project"), "title", start));
    auto* startText = typedLabel(
        QStringLiteral("Pick sequences from a dataset folder, add a BrackishMOT sequence folder, "
                       "or bring in survey videos. Anything you open in Review joins the project "
                       "automatically."),
        "small", start);
    startText->setWordWrap(true);
    startBody->addWidget(startText);
    startBody->addSpacing(10);
    auto* startButtons = new QHBoxLayout;
    startButtons->setContentsMargins(0, 0, 0, 0);
    startButtons->setSpacing(8);
    auto* browse = primary(QStringLiteral("Browse datasets"), Glyph::Datasets, start);
    browse->setObjectName(QStringLiteral("browseDatasets"));
    connect(browse, &QPushButton::clicked, this, &DashboardPage::browseDatasetsRequested);
    auto* addFolder = textButton(QStringLiteral("Add sequence folder..."), start);
    addFolder->setObjectName(QStringLiteral("addSequenceFolder"));
    connect(addFolder, &QPushButton::clicked, this, &DashboardPage::addSequenceFolderRequested);
    auto* addVideos = textButton(QStringLiteral("Add videos..."), start);
    connect(addVideos, &QPushButton::clicked, this, &DashboardPage::addVideosRequested);
    auto* newSequence = textButton(QStringLiteral("New sequence..."), start);
    newSequence->setToolTip(QStringLiteral("Join several videos into one timeline, in an order you choose"));
    connect(newSequence, &QPushButton::clicked, this, &DashboardPage::createSequenceRequested);
    startButtons->addWidget(browse);
    startButtons->addWidget(addFolder);
    startButtons->addWidget(addVideos);
    startButtons->addWidget(newSequence);
    startButtons->addStretch();
    startBody->addLayout(startButtons);
    startBody->addStretch();
    m_continue->addWidget(start);
    continueBody->addWidget(m_continue);
    row->addWidget(continueCard, 3);

    // Survey progress: how much is verified, and what has been found.
    auto* progressCard = card(content);
    auto* progress = new QVBoxLayout(progressCard);
    progress->setContentsMargins(20, 16, 20, 20);
    progress->setSpacing(10);
    auto* progressHead = new QHBoxLayout;
    progressHead->setContentsMargins(0, 0, 0, 0);
    progressHead->addWidget(eyebrow(QStringLiteral("Survey progress"), progressCard));
    progressHead->addStretch();
    m_export = ghost(QStringLiteral("Export"), Glyph::Export, progressCard);
    m_export->setToolTip(QStringLiteral("Export observations as CSV or MOTChallenge"));
    connect(m_export, &QPushButton::clicked, this, &DashboardPage::exportRequested);
    progressHead->addWidget(m_export);
    progress->addLayout(progressHead);

    auto* headline = new QHBoxLayout;
    headline->setContentsMargins(0, 0, 0, 0);
    headline->setSpacing(12);
    m_verifiedPercent = typedLabel(QStringLiteral("\u2014"), "display", progressCard);
    m_verifiedPercent->setStyleSheet(QStringLiteral("font-size:34px"));
    headline->addWidget(m_verifiedPercent, 0, Qt::AlignBottom);
    m_verifiedCaption = typedLabel(QString(), "small", progressCard);
    m_verifiedCaption->setWordWrap(true);
    headline->addWidget(m_verifiedCaption, 1, Qt::AlignBottom);
    progress->addLayout(headline);
    m_verifiedBar = new RatioBar(progressCard, 6);
    progress->addWidget(m_verifiedBar);
    progress->addSpacing(4);

    auto* figures = new QGridLayout;
    figures->setContentsMargins(0, 0, 0, 0);
    figures->setHorizontalSpacing(16);
    figures->setVerticalSpacing(0);
    m_figureDetections = figure(figures, 0, QStringLiteral("Detections"), progressCard);
    m_figureTracks = figure(figures, 1, QStringLiteral("Tracks"), progressCard);
    m_figureSpecies = figure(figures, 2, QStringLiteral("Species"), progressCard);
    m_figureFrames = figure(figures, 3, QStringLiteral("Frames annotated"), progressCard);
    progress->addLayout(figures);
    progress->addSpacing(6);
    progress->addWidget(divider(progressCard));
    progress->addSpacing(2);
    progress->addWidget(eyebrow(QStringLiteral("Species \u00b7 individuals"), progressCard));
    m_species = new SpeciesChart(progressCard);
    progress->addWidget(m_species);
    m_speciesEmpty = typedLabel(
        QStringLiteral("Nothing detected yet. Run detection in Review, or import annotations."),
        "dim", progressCard);
    m_speciesEmpty->setWordWrap(true);
    progress->addWidget(m_speciesEmpty);
    progress->addStretch();
    row->addWidget(progressCard, 2);
    body->addLayout(row);

    // ---- Media.
    auto* mediaHead = new QHBoxLayout;
    mediaHead->setContentsMargins(0, 6, 0, 0);
    mediaHead->setSpacing(4);
    mediaHead->addWidget(eyebrow(QStringLiteral("Media"), content));
    m_mediaCount = typedLabel(QString(), "dim", content);
    mediaHead->addWidget(m_mediaCount);
    mediaHead->addStretch();
    auto* createSequence = ghost(QStringLiteral("New sequence..."), Glyph::Merge, content);
    createSequence->setObjectName(QStringLiteral("dashboardNewSequence"));
    createSequence->setToolTip(QStringLiteral("Join several videos into one timeline, in an order you choose"));
    connect(createSequence, &QPushButton::clicked, this, &DashboardPage::createSequenceRequested);
    auto* addSequence = ghost(QStringLiteral("Add sequence folder..."), Glyph::Files, content);
    connect(addSequence, &QPushButton::clicked, this, &DashboardPage::addSequenceFolderRequested);
    auto* addVideoFiles = ghost(QStringLiteral("Add videos..."), Glyph::Video, content);
    connect(addVideoFiles, &QPushButton::clicked, this, &DashboardPage::addVideosRequested);
    auto* manageMedia = ghost(QStringLiteral("Manage"), Glyph::ListView, content);
    manageMedia->setToolTip(QStringLiteral("Open the Videos page"));
    connect(manageMedia, &QPushButton::clicked, this, &DashboardPage::manageMediaRequested);
    mediaHead->addWidget(createSequence);
    mediaHead->addWidget(addSequence);
    mediaHead->addWidget(addVideoFiles);
    mediaHead->addWidget(manageMedia);
    body->addLayout(mediaHead);

    m_media = new CardGrid(230, MediaCardDelegate::TextHeight, content);
    m_media->setObjectName(QStringLiteral("projectMedia"));
    m_media->setItemDelegate(new MediaCardDelegate(m_media));
    connect(m_media, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (item->data(ExistsRole).toBool())
            emit mediaReviewRequested(item->data(PathRole).toString(), 0);
    });
    m_media->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_media, &QListWidget::customContextMenuRequested, this, [this](const QPoint& at) {
        QListWidgetItem* item = m_media->itemAt(at);
        if (!item)
            return;
        const QString path = item->data(PathRole).toString();
        const bool exists = item->data(ExistsRole).toBool();
        QMenu menu(m_media);
        QAction* review = menu.addAction(QStringLiteral("Open in Review"));
        review->setEnabled(exists);
        QAction* edit = nullptr;
        if (exists && aquatrack::media::classifyMedia(path) == aquatrack::media::MediaKind::VideoSequence)
            edit = menu.addAction(QStringLiteral("Edit sequence..."));
        QAction* reveal = menu.addAction(QStringLiteral("Show in folder"));
        reveal->setEnabled(exists);
        QAction* chosen = menu.exec(m_media->viewport()->mapToGlobal(at));
        if (chosen == review)
            emit mediaReviewRequested(path, 0);
        else if (edit && chosen == edit)
            emit editSequenceRequested(path);
        else if (chosen == reveal)
            QDesktopServices::openUrl(QUrl::fromLocalFile(
                QFileInfo(path).isDir() ? path : QFileInfo(path).absolutePath()));
    });
    auto* mediaRow = new QHBoxLayout;
    mediaRow->setContentsMargins(-8, -8, -8, 0);
    mediaRow->addWidget(m_media);
    body->addLayout(mediaRow);
    body->addStretch();
    return scroll;
}

// ----------------------------------------------------------------- State

void DashboardPage::setProject(const QString& name, const QString& path,
                               const QStringList& mediaPaths, const QString& observationStore,
                               bool unsaved)
{
    const bool open = !name.isEmpty();
    const bool otherProject = path != m_projectPath;
    m_projectName = name;
    m_projectPath = path;
    m_mediaPaths = mediaPaths;
    m_store = observationStore;
    if (otherProject) {
        m_browsing = false;
        m_haveSummary = false;
        m_summary = {};
        m_resumeKey.clear();
        m_resumePixmap = {};
    }

    m_projectState->setText(unsaved ? QStringLiteral("Unsaved changes") : QStringLiteral("Saved"));
    m_projectState->setAccent(unsaved ? palette::Warning : palette::Success);
    m_save->setEnabled(unsaved);
    m_backToProject->setVisible(open);
    if (open) {
        m_backToProject->setText(QStringLiteral("Back to %1").arg(name));
        const QString signature =
            path + QLatin1Char('|') + mediaPaths.join(QLatin1Char('|')) + QLatin1Char('|') + m_store;
        if (signature != m_overviewSignature) {
            m_overviewSignature = signature;
            rebuildMedia();
            refreshOverview();
        } else {
            updateHero();
        }
    } else {
        m_overviewSignature.clear();
    }
    if (otherProject)
        rebuildRecent();
    showMode();
}

void DashboardPage::showMode()
{
    m_modes->setCurrentIndex(!m_projectName.isEmpty() && !m_browsing ? 1 : 0);
}

void DashboardPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // Coming back from Review: counts and where review stopped have moved.
    if (!m_projectName.isEmpty())
        refreshOverview();
    rebuildRecent();
}

// -------------------------------------------------------------- Projects

void DashboardPage::setRecentProjects(const QStringList& paths)
{
    m_recentPaths = paths;
    rebuildRecent();
}

void DashboardPage::rebuildRecent()
{
    m_recent->clear();
    int missing = 0;
    for (const auto& path : m_recentPaths) {
        const QFileInfo info(path);
        const bool exists = info.exists();
        auto* item = new QListWidgetItem(info.completeBaseName(), m_recent);
        item->setData(PathRole, path);
        item->setData(ExistsRole, exists);
        item->setData(CurrentRole, samePath(path, m_projectPath));
        item->setData(DetailRole, shortLocation(info));
        item->setData(PosterRole, m_posters.value(path));
        if (exists) {
            const QString edited = QStringLiteral("Edited %1").arg(ago(info.lastModified()));
            const auto media = m_projectMedia.constFind(path);
            item->setData(MetaRole, media == m_projectMedia.constEnd()
                                        ? edited
                                        : count(*media, QStringLiteral("media"),
                                                QStringLiteral("media")) +
                                              QStringLiteral("  \u00b7  ") + edited);
            item->setToolTip(QDir::toNativeSeparators(info.absoluteFilePath()));
            requestPoster(path);
        } else {
            ++missing;
            item->setData(MetaRole, QStringLiteral("Click to locate it, or right-click to remove"));
            item->setToolTip(QStringLiteral("Missing: %1")
                                 .arg(QDir::toNativeSeparators(info.absoluteFilePath())));
        }
    }
    m_recent->relayout();
    const bool any = !m_recentPaths.isEmpty();
    m_recent->setVisible(any);
    m_recentEmpty->setVisible(!any);
    m_browserSubtitle->setText(any ? QStringLiteral("Pick up a survey where you left it, or start a new one.")
                                   : QStringLiteral("No projects yet."));
    m_clearMissing->setVisible(missing > 0);
    m_clearMissing->setText(missing > 1 ? QStringLiteral("Clear %1 missing").arg(missing)
                                        : QStringLiteral("Clear missing"));
}

void DashboardPage::openRecentItem(QListWidgetItem* item)
{
    if (item)
        emit recentProjectActivated(item->data(PathRole).toString());
}

void DashboardPage::requestPoster(const QString& projectFile)
{
    if (m_posters.contains(projectFile) || m_postersPending.contains(projectFile))
        return;
    m_postersPending.insert(projectFile);
    QPointer<DashboardPage> guard(this);
    m_pool.start([guard, projectFile] {
        aquatrack::domain::ProjectManifest manifest;
        int mediaCount = -1;
        QImage poster;
        if (aquatrack::data::ProjectFileStore().load(projectFile, manifest)) {
            mediaCount = int(manifest.mediaPaths.size());
            for (const auto& media : manifest.mediaPaths) {
                poster = aquatrack::media::cachedMediaThumbnail(QString::fromStdString(media),
                                                                kPosterWidth);
                if (!poster.isNull())
                    break;
            }
        }
        if (!guard)
            return;
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, projectFile, mediaCount, poster] {
                if (guard)
                    guard->posterReady(projectFile, mediaCount, poster);
            },
            Qt::QueuedConnection);
    });
}

void DashboardPage::posterReady(const QString& projectFile, int mediaCount, const QImage& image)
{
    m_postersPending.remove(projectFile);
    m_posters.insert(projectFile, image.isNull() ? QPixmap() : QPixmap::fromImage(image));
    if (mediaCount >= 0)
        m_projectMedia.insert(projectFile, mediaCount);
    if (auto* item = itemForPath(m_recent, projectFile)) {
        item->setData(PosterRole, m_posters.value(projectFile));
        if (mediaCount >= 0)
            item->setData(MetaRole,
                          count(mediaCount, QStringLiteral("media"), QStringLiteral("media")) +
                              QStringLiteral("  \u00b7  Edited ") +
                              ago(QFileInfo(projectFile).lastModified()));
    }
}

// ----------------------------------------------------------------- Media

void DashboardPage::invalidateMedia(const QString& path)
{
    for (auto it = m_thumbnails.begin(); it != m_thumbnails.end();)
        it = samePath(it.key(), path) ? m_thumbnails.erase(it) : std::next(it);
    // The project poster may be this media's frame.
    if (!m_projectPath.isEmpty())
        m_posters.remove(m_projectPath);
    m_resumeKey.clear();
    m_resumePixmap = {};
    m_overviewSignature.clear();
}

void DashboardPage::rebuildMedia()
{
    m_media->clear();
    for (int i = 0; i < m_mediaPaths.size(); ++i) {
        const QString& path = m_mediaPaths.at(i);
        const MediaProgress* progress = nullptr;
        if (m_haveSummary)
            for (const auto& entry : m_summary.media)
                if (samePath(entry.path, path))
                    progress = &entry;
        const bool exists = QFileInfo::exists(path);
        auto* item = new QListWidgetItem(progress ? progress->name : mediaName(path), m_media);
        item->setData(PathRole, path);
        item->setData(ExistsRole, exists);
        item->setData(PosterRole, m_thumbnails.value(path));
        item->setToolTip(QDir::toNativeSeparators(path));
        if (!exists) {
            item->setData(MetaRole, QStringLiteral("Not found at its saved location"));
            item->setData(DetailRole, QDir::toNativeSeparators(QFileInfo(path).absolutePath()));
            item->setData(ProgressRole, 0.0);
            continue;
        }
        requestThumbnail(path);
        if (!progress) {
            item->setData(MetaRole, QStringLiteral("Reading..."));
            item->setData(ProgressRole, 0.0);
            continue;
        }
        if (!progress->available) {
            item->setData(MetaRole, QStringLiteral("Could not read this media"));
            item->setData(ProgressRole, 0.0);
            continue;
        }
        QString meta = count(progress->frames, QStringLiteral("frame"), QStringLiteral("frames"));
        if (progress->clips > 1)
            meta += QStringLiteral("  \u00b7  ") +
                    count(progress->clips, QStringLiteral("clip"), QStringLiteral("clips"));
        item->setData(MetaRole, meta);
        if (progress->durationMs > 0)
            item->setData(BadgeRole, duration(progress->durationMs));
        const double ratio =
            progress->observations > 0 ? double(progress->verified) / progress->observations : 0.0;
        item->setData(ProgressRole, ratio);
        item->setData(DetailRole,
                      progress->observations == 0
                          ? QStringLiteral("Not reviewed yet")
                          : (progress->verified == progress->observations
                                 ? QStringLiteral("All %1 verified")
                                       .arg(count(progress->observations, QStringLiteral("detection"),
                                                  QStringLiteral("detections")))
                                 : QStringLiteral("%1  \u00b7  %2 verified")
                                       .arg(count(progress->observations, QStringLiteral("detection"),
                                                  QStringLiteral("detections")),
                                            percent(ratio))));
    }
    m_media->relayout();
    m_mediaCount->setText(m_mediaPaths.isEmpty() ? QString()
                                                 : QStringLiteral("\u00b7  %1").arg(m_mediaPaths.size()));
    m_media->setVisible(!m_mediaPaths.isEmpty());
    m_continue->setCurrentIndex(m_mediaPaths.isEmpty() ? 1 : 0);
    updateHero();
    updateContinue();
}

void DashboardPage::requestThumbnail(const QString& mediaPath)
{
    if (m_thumbnails.contains(mediaPath) || m_thumbnailsPending.contains(mediaPath))
        return;
    m_thumbnailsPending.insert(mediaPath);
    QPointer<DashboardPage> guard(this);
    m_pool.start([guard, mediaPath] {
        const QImage image = aquatrack::media::cachedMediaThumbnail(mediaPath, kPosterWidth);
        if (!guard)
            return;
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, mediaPath, image] {
                if (guard)
                    guard->thumbnailReady(mediaPath, image);
            },
            Qt::QueuedConnection);
    });
}

void DashboardPage::thumbnailReady(const QString& mediaPath, const QImage& image)
{
    m_thumbnailsPending.remove(mediaPath);
    const QPixmap pixmap = image.isNull() ? QPixmap() : QPixmap::fromImage(image);
    m_thumbnails.insert(mediaPath, pixmap);
    if (auto* item = itemForPath(m_media, mediaPath))
        item->setData(PosterRole, pixmap);
    updateHero();
    if (samePath(m_continuePath, mediaPath) && m_resumePixmap.isNull())
        m_continueFrame->setPixmap(pixmap);
}

// --------------------------------------------------------------- Summary

void DashboardPage::refreshOverview()
{
    if (m_projectName.isEmpty())
        return;
    const quint64 generation = ++m_generation;
    const QStringList media = m_mediaPaths;
    const QString store = m_store;
    QPointer<DashboardPage> guard(this);
    m_pool.start([guard, generation, media, store] {
        const ProjectSummary summary = aquatrack::application::summarizeProject(media, store);
        if (!guard)
            return;
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, generation, summary] {
                if (guard && guard->m_generation == generation)
                    guard->applySummary(summary);
            },
            Qt::QueuedConnection);
    });
    updateContinue();
}

void DashboardPage::applySummary(const ProjectSummary& summary)
{
    m_summary = summary;
    m_haveSummary = true;
    rebuildMedia();

    const QLocale locale = QLocale::system();
    const bool any = summary.observations > 0;
    m_verifiedPercent->setText(any ? percent(summary.verifiedRatio()) : QStringLiteral("\u2014"));
    m_verifiedCaption->setText(
        any ? QStringLiteral("of %1 verified by a reviewer")
                  .arg(count(summary.observations, QStringLiteral("detection"),
                             QStringLiteral("detections")))
            : QStringLiteral("No detections yet"));
    m_verifiedBar->setRatio(summary.verifiedRatio(),
                            summary.verifiedRatio() >= 0.999 ? palette::Success : palette::Accent);
    m_figureDetections->setText(locale.toString(summary.observations));
    m_figureTracks->setText(locale.toString(summary.tracks));
    m_figureSpecies->setText(locale.toString(int(summary.species.size())));
    m_figureFrames->setText(summary.frames > 0
                                ? QStringLiteral("%1 of %2").arg(locale.toString(summary.annotatedFrames),
                                                                 locale.toString(summary.frames))
                                : locale.toString(summary.annotatedFrames));
    m_species->setSpecies(summary.species, 5);
    m_species->setVisible(!summary.species.isEmpty());
    m_speciesEmpty->setVisible(summary.species.isEmpty());
    m_export->setEnabled(any);
}

void DashboardPage::updateHero()
{
    QStringList facts;
    facts << count(int(m_mediaPaths.size()), QStringLiteral("media item"), QStringLiteral("media items"));
    if (m_haveSummary) {
        qint64 milliseconds = 0;
        for (const auto& item : m_summary.media)
            milliseconds += item.durationMs;
        if (m_summary.frames > 0)
            facts << count(m_summary.frames, QStringLiteral("frame"), QStringLiteral("frames"));
        if (milliseconds > 0)
            facts << duration(milliseconds);
        if (m_summary.observations > 0)
            facts << count(m_summary.observations, QStringLiteral("detection"),
                           QStringLiteral("detections"));
        if (!m_summary.species.isEmpty())
            facts << count(int(m_summary.species.size()), QStringLiteral("species"),
                           QStringLiteral("species"));
    }
    m_hero->setText(QStringLiteral("Project"), m_projectName,
                    facts.join(QStringLiteral("   \u00b7   ")));
    m_hero->setToolTip(QDir::toNativeSeparators(m_projectPath));

    QVector<QPixmap> frames;
    for (const QString& path : m_mediaPaths) {
        const QPixmap pixmap = m_thumbnails.value(path);
        if (!pixmap.isNull())
            frames << pixmap;
        if (frames.size() == kHeroFrames)
            break;
    }
    m_hero->setImages(frames);
}

// ------------------------------------------------------------- Continue

void DashboardPage::updateContinue()
{
    if (m_mediaPaths.isEmpty() || m_projectPath.isEmpty())
        return;
    const QLocale locale = QLocale::system();
    const MediaProgress* progress = nullptr;
    const auto progressFor = [this](const QString& path) -> const MediaProgress* {
        if (m_haveSummary)
            for (const auto& entry : m_summary.media)
                if (samePath(entry.path, path))
                    return &entry;
        return nullptr;
    };

    // Where review last stopped, if that media is still part of the project.
    const auto session = aquatrack::application::loadReviewSession(m_projectPath);
    const bool resumable =
        session && QFileInfo::exists(session->mediaPath) &&
        std::any_of(m_mediaPaths.cbegin(), m_mediaPaths.cend(),
                    [&](const QString& path) { return samePath(path, session->mediaPath); });

    if (resumable) {
        m_continuePath = session->mediaPath;
        m_continueFrameNumber = session->frame;
        progress = progressFor(m_continuePath);
        m_continueEyebrow->setText(QStringLiteral("CONTINUE REVIEWING"));
        m_continueTitle->setText(progress ? progress->name : mediaName(m_continuePath));
        QString detail = progress && progress->frames > 0
                             ? QStringLiteral("Frame %1 of %2").arg(locale.toString(session->frame),
                                                                    locale.toString(progress->frames))
                             : QStringLiteral("Frame %1").arg(locale.toString(session->frame));
        if (session->savedAtMs > 0)
            detail += QStringLiteral("  \u00b7  last opened %1")
                          .arg(ago(QDateTime::fromMSecsSinceEpoch(session->savedAtMs)));
        if (progress && progress->observations > 0)
            detail += QStringLiteral("\n%1 verified of %2")
                          .arg(locale.toString(progress->verified),
                               count(progress->observations, QStringLiteral("detection"),
                                     QStringLiteral("detections")));
        m_continueDetail->setText(detail);
        m_continuePosition->setVisible(true);
        m_continuePosition->setRatio(progress && progress->frames > 0
                                         ? double(session->frame) / progress->frames
                                         : 0.0,
                                     palette::Accent);
        m_continueButton->setText(QStringLiteral("Resume"));
        requestResumeFrame(m_continuePath, session->frame);
        // The exact frame once decoded; the media's poster until then.
        m_continueFrame->setPixmap(m_resumePixmap.isNull() ? m_thumbnails.value(m_continuePath)
                                                           : m_resumePixmap);
        return;
    }

    // Otherwise: the first media nobody has reviewed yet, or the first one.
    QString next;
    for (const QString& path : m_mediaPaths) {
        if (!QFileInfo::exists(path))
            continue;
        const MediaProgress* candidate = progressFor(path);
        if (next.isEmpty())
            next = path;
        if (candidate && candidate->observations == 0) {
            next = path;
            break;
        }
    }
    if (next.isEmpty())
        next = m_mediaPaths.first();
    progress = progressFor(next);
    m_continuePath = next;
    m_continueFrameNumber = 0;
    m_resumeKey.clear();
    m_resumePixmap = {};
    const bool reviewed = progress && progress->observations > 0;
    m_continueEyebrow->setText(reviewed ? QStringLiteral("REVIEW") : QStringLiteral("NEXT UP"));
    m_continueTitle->setText(progress ? progress->name : mediaName(next));
    if (!progress)
        m_continueDetail->setText(QString());
    else if (!reviewed)
        m_continueDetail->setText(
            QStringLiteral("Not reviewed yet  \u00b7  %1")
                .arg(count(progress->frames, QStringLiteral("frame"), QStringLiteral("frames"))));
    else
        m_continueDetail->setText(
            QStringLiteral("%1  \u00b7  %2 verified")
                .arg(count(progress->observations, QStringLiteral("detection"),
                           QStringLiteral("detections")),
                     percent(double(progress->verified) / progress->observations)));
    m_continuePosition->setVisible(false);
    m_continueButton->setText(reviewed ? QStringLiteral("Open in Review")
                                       : QStringLiteral("Start review"));
    m_continueFrame->setPixmap(m_thumbnails.value(next));
}

void DashboardPage::requestResumeFrame(const QString& mediaPath, int frame)
{
    const QString key = mediaPath + QLatin1Char('#') + QString::number(frame);
    if (m_resumeKey == key)
        return;
    m_resumeKey = key;
    m_resumePixmap = {};
    QPointer<DashboardPage> guard(this);
    m_pool.start([guard, mediaPath, frame, key] {
        const QImage image = aquatrack::media::cachedFrameThumbnail(mediaPath, frame, kResumeWidth);
        if (!guard)
            return;
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, key, image] {
                if (!guard || guard->m_resumeKey != key || image.isNull())
                    return;
                guard->m_resumePixmap = QPixmap::fromImage(image);
                guard->m_continueFrame->setPixmap(guard->m_resumePixmap);
            },
            Qt::QueuedConnection);
    });
}
