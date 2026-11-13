#include "ui/videos/VideosPage.h"

#include "data/BrackishMotSequence.h"
#include "data/media/FrameDecoder.h"
#include "data/media/VideoProbe.h"
#include "data/media/VideoSequenceManifest.h"
#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <utility>

using namespace aquatrack::ui;
namespace media = aquatrack::media;

namespace {

constexpr int kThumbnailWidth = 176;
constexpr int kThumbnailHeight = 99;

enum Column { NameColumn, KindColumn, FramesColumn, DurationColumn, SizeColumn, LocationColumn };

QString formatBytes(qint64 bytes)
{
    constexpr double kMegabyte = 1024.0 * 1024.0;
    if (bytes <= 0)
        return QStringLiteral("—");
    if (bytes < kMegabyte)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 0);
    if (bytes < kMegabyte * 1024.0)
        return QStringLiteral("%1 MB").arg(bytes / kMegabyte, 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(bytes / (kMegabyte * 1024.0), 0, 'f', 2);
}

QString frameRateText(double rate)
{
    if (rate <= 0.0)
        return QStringLiteral("—");
    return std::abs(rate - std::round(rate)) < 0.01
               ? QStringLiteral("%1 fps").arg(int(std::lround(rate)))
               : QStringLiteral("%1 fps").arg(rate, 0, 'f', 2);
}

// What a sequence is made of, for the Kind column and detail card.
QString sequenceMakeup(media::MediaKind kind, int clips)
{
    if (kind == media::MediaKind::VideoSequence)
        return clips > 0 ? QStringLiteral("%1 video%2").arg(clips).arg(clips == 1 ? "" : "s")
                         : QStringLiteral("videos");
    if (kind == media::MediaKind::ImageSequence)
        return QStringLiteral("image frames");
    return {};
}

bool isSequence(media::MediaKind kind)
{
    return kind == media::MediaKind::ImageSequence || kind == media::MediaKind::VideoSequence;
}

constexpr int kSequencesFilter = 101;
constexpr int kAttentionFilter = 100;

QColor kindColor(media::MediaKind kind)
{
    switch (kind) {
    case media::MediaKind::ImageSequence:
    case media::MediaKind::VideoSequence:
        return palette::Accent;
    case media::MediaKind::VideoFile:
        return palette::Secondary;
    case media::MediaKind::Folder:
    case media::MediaKind::Unsupported:
        return palette::Warning;
    case media::MediaKind::Missing:
        return palette::Danger;
    }
    return palette::TextSecondary;
}

QWidget* detailRow(const QString& caption, QLabel*& value, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(typedLabel(caption, "dim", row));
    layout->addStretch();
    value = typedLabel(QStringLiteral("—"), "small", row);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(value);
    return row;
}

// A dark tile with the media kind's glyph, shown until a thumbnail arrives.
QIcon placeholderIcon(media::MediaKind kind)
{
    QPixmap tile(kThumbnailWidth, kThumbnailHeight);
    tile.fill(QColor(0x10, 0x14, 0x1c));
    QPainter painter(&tile);
    const Glyph glyph = kind == media::MediaKind::ImageSequence ? Glyph::Review
                        : kind == media::MediaKind::Missing     ? Glyph::Alert
                        : kind == media::MediaKind::Folder || kind == media::MediaKind::Unsupported
                            ? Glyph::Files
                            : Glyph::Video;
    const QPixmap mark = glyphPixmap(glyph, 30, kindColor(kind));
    painter.drawPixmap((tile.width() - mark.width() / mark.devicePixelRatio()) / 2,
                       (tile.height() - mark.height() / mark.devicePixelRatio()) / 2, mark);
    return QIcon(tile);
}

QIcon thumbnailIcon(const QImage& image)
{
    QPixmap tile(kThumbnailWidth, kThumbnailHeight);
    tile.fill(Qt::black);
    QPainter painter(&tile);
    const QImage scaled =
        image.scaled(tile.size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    painter.drawImage(QPoint((tile.width() - scaled.width()) / 2, (tile.height() - scaled.height()) / 2),
                      scaled);
    return QIcon(tile);
}

}  // namespace

VideosPage::VideosPage(QWidget* parent) : QWidget(parent)
{
    m_pool.setMaxThreadCount(3);

    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(24, 20, 24, 20);
    page->setSpacing(12);

    // ---- Heading -------------------------------------------------------
    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(10);

    auto* titles = new QVBoxLayout;
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(1);
    titles->addWidget(sectionLabel(QStringLiteral("Video library"), this));
    titles->addWidget(typedLabel(QStringLiteral("Survey media"), "display", this));
    m_projectLabel =
        typedLabel(QStringLiteral("Create or open a project to manage media."), "dim", this);
    titles->addWidget(m_projectLabel);
    head->addLayout(titles);
    head->addStretch();

    m_addVideo = textButton(QStringLiteral("Add videos"), this);
    m_addVideo->setObjectName(QStringLiteral("addVideos"));
    m_addVideo->setIcon(glyphIcon(Glyph::Video, 15, palette::TextSecondary, palette::TextPrimary));
    auto* videoMenu = new QMenu(m_addVideo);
    connect(videoMenu->addAction(QStringLiteral("Add video files...")), &QAction::triggered, this,
            &VideosPage::addMediaRequested);
    connect(videoMenu->addAction(QStringLiteral("Add every video in a folder...")),
            &QAction::triggered, this, &VideosPage::addVideoFolderRequested);
    m_addVideo->setMenu(videoMenu);
    head->addWidget(m_addVideo, 0, Qt::AlignBottom);

    m_addSequence = textButton(QStringLiteral("Add existing sequence"), this);
    m_addSequence->setObjectName(QStringLiteral("addExistingSequence"));
    m_addSequence->setIcon(glyphIcon(Glyph::Review, 15, palette::TextSecondary, palette::TextPrimary));
    m_addSequence->setToolTip(QStringLiteral(
        "Add a sequence folder made earlier: an AquaTrack sequence of videos, or a BrackishMOT / "
        "MOTChallenge folder of frames (seqinfo.ini, img1/, gt/gt.txt)"));
    connect(m_addSequence, &QPushButton::clicked, this, &VideosPage::addSequenceRequested);
    head->addWidget(m_addSequence, 0, Qt::AlignBottom);

    m_createSequence = textButton(QStringLiteral("New sequence"), this, "primary");
    m_createSequence->setObjectName(QStringLiteral("newSequence"));
    m_createSequence->setIcon(glyphIcon(Glyph::Merge, 15, Qt::white, Qt::white));
    m_createSequence->setToolTip(QStringLiteral(
        "Make a sequence from the selected videos (or choose videos): they play back to back on "
        "one timeline. The videos are referenced, never copied or converted."));
    connect(m_createSequence, &QPushButton::clicked, this,
            [this] { emit createSequenceRequested(selectedVideoPaths()); });
    head->addWidget(m_createSequence, 0, Qt::AlignBottom);
    page->addLayout(head);

    // ---- Filter --------------------------------------------------------
    auto* filters = new QHBoxLayout;
    filters->setContentsMargins(0, 0, 0, 0);
    filters->setSpacing(8);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("Search media by name or path"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(360);
    connect(m_search, &QLineEdit::textChanged, this, &VideosPage::applyFilter);
    filters->addWidget(m_search);
    filters->addStretch();
    m_summary = typedLabel(QString(), "dim", this);
    filters->addWidget(m_summary);
    m_typeFilter = new QComboBox(this);
    m_typeFilter->setObjectName(QStringLiteral("typeFilter"));
    m_typeFilter->addItem(QStringLiteral("All types"), -1);
    m_typeFilter->addItem(QStringLiteral("Videos"), int(media::MediaKind::VideoFile));
    m_typeFilter->addItem(QStringLiteral("Sequences"), kSequencesFilter);
    m_typeFilter->addItem(QStringLiteral("Needs attention"), kAttentionFilter);
    m_typeFilter->setToolTip(QStringLiteral("Filter the media library by type"));
    connect(m_typeFilter, &QComboBox::currentIndexChanged, this, &VideosPage::applyFilter);
    filters->addWidget(m_typeFilter);
    m_listView = textButton(QStringLiteral("List"), this);
    m_gridView = textButton(QStringLiteral("Grid"), this);
    m_listView->setObjectName(QStringLiteral("mediaListView"));
    m_gridView->setObjectName(QStringLiteral("mediaGridView"));
    for (auto* button : {m_listView, m_gridView})
        button->setCheckable(true);
    m_listView->setChecked(true);
    m_listView->setToolTip(QStringLiteral("Show media as a list"));
    m_gridView->setToolTip(QStringLiteral("Show media as thumbnails"));
    filters->addWidget(m_listView);
    filters->addWidget(m_gridView);
    page->addLayout(filters);

    // ---- Library views and detail ----------------------------------------
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(12);

    m_table = new QTableWidget(0, 6, this);
    m_table->setObjectName(QStringLiteral("mediaTable"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("Media"), QStringLiteral("Kind"),
                                        QStringLiteral("Frames"), QStringLiteral("Duration"),
                                        QStringLiteral("Size"), QStringLiteral("Location")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    for (int column = KindColumn; column <= LocationColumn; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    connect(m_table, &QTableWidget::currentCellChanged, this,
            [this](int, int, int, int) { showDetail(); });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &VideosPage::updateSelectionActions);
    connect(m_table, &QTableWidget::customContextMenuRequested, this,
            [this](const QPoint& position) { showContextMenu(position, m_table); });

    m_grid = new QListWidget(this);
    m_grid->setObjectName(QStringLiteral("mediaGrid"));
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setWrapping(true);
    m_grid->setSpacing(10);
    m_grid->setIconSize(QSize(kThumbnailWidth, kThumbnailHeight));
    m_grid->setGridSize(QSize(kThumbnailWidth + 20, kThumbnailHeight + 52));
    m_grid->setWordWrap(true);
    m_grid->setUniformItemSizes(true);
    m_grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_grid->setContextMenuPolicy(Qt::CustomContextMenu);
    // The grid mirrors the table's selection, so actions read one place.
    connect(m_grid, &QListWidget::itemSelectionChanged, this, [this] {
        const QSignalBlocker blocker(m_table);
        m_table->clearSelection();
        QListWidgetItem* current = m_grid->currentItem();
        for (auto* item : m_grid->selectedItems()) {
            const int row = m_grid->row(item);
            if (row >= 0 && row < m_table->rowCount())
                m_table->selectionModel()->select(
                    m_table->model()->index(row, 0),
                    QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        if (current) {
            const int row = m_grid->row(current);
            m_table->setCurrentCell(row, 0, QItemSelectionModel::NoUpdate);
        }
        showDetail();
        updateSelectionActions();
    });
    connect(m_grid, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint& position) { showContextMenu(position, m_grid); });

    const auto activate = [this] {
        const auto* entry = selectedEntry();
        if (!entry)
            return;
        // Sequences open like folders; a single video opens in Review.
        if (isSequence(entry->kind))
            openFolder(entry->path);
        else if (entry->kind == media::MediaKind::VideoFile && entry->metadata.error.isEmpty())
            emit reviewRequested(entry->path);
    };
    connect(m_table, &QTableWidget::itemDoubleClicked, this, activate);
    connect(m_grid, &QListWidget::itemDoubleClicked, this, activate);

    // Sequence contents ("folder") view.
    auto* folderPage = new QWidget(this);
    auto* folderColumn = new QVBoxLayout(folderPage);
    folderColumn->setContentsMargins(0, 0, 0, 0);
    folderColumn->setSpacing(8);
    m_folderBar = new QWidget(folderPage);
    auto* folderBar = new QHBoxLayout(m_folderBar);
    folderBar->setContentsMargins(0, 0, 0, 0);
    folderBar->setSpacing(8);
    auto* back = textButton(QStringLiteral("Library"), m_folderBar);
    back->setObjectName(QStringLiteral("folderBack"));
    back->setIcon(glyphIcon(Glyph::Previous, 14, palette::TextSecondary, palette::TextPrimary));
    back->setToolTip(QStringLiteral("Back to the media library"));
    connect(back, &QPushButton::clicked, this, &VideosPage::closeFolder);
    folderBar->addWidget(back);
    folderBar->addWidget(typedLabel(QStringLiteral("›"), "dim", m_folderBar));
    m_folderTitle = typedLabel(QString(), "small", m_folderBar);
    m_folderTitle->setStyleSheet(QStringLiteral("font-weight:600;color:%1").arg(palette::TextPrimary.name()));
    folderBar->addWidget(m_folderTitle, 1);
    folderColumn->addWidget(m_folderBar);
    m_contents = new QTableWidget(0, 4, folderPage);
    m_contents->setObjectName(QStringLiteral("sequenceContents"));
    m_contents->setHorizontalHeaderLabels({QStringLiteral("File"), QStringLiteral("Frames"),
                                           QStringLiteral("Duration"), QStringLiteral("Starts at")});
    m_contents->verticalHeader()->setVisible(false);
    m_contents->setShowGrid(false);
    m_contents->setAlternatingRowColors(true);
    m_contents->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_contents->setSelectionMode(QAbstractItemView::SingleSelection);
    m_contents->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_contents->horizontalHeader()->setHighlightSections(false);
    m_contents->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 4; ++column)
        m_contents->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    m_contents->setToolTip(QStringLiteral("Double-click a clip or frame to open it in Review"));
    connect(m_contents, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem* item) {
        const int frame = m_contents->item(item->row(), 0)->data(Qt::UserRole).toInt();
        if (frame > 0)
            emit reviewAtFrameRequested(m_folderPath, frame);
    });
    folderColumn->addWidget(m_contents, 1);

    m_views = new QStackedWidget(this);
    m_views->addWidget(m_table);
    m_views->addWidget(m_grid);
    m_views->addWidget(folderPage);
    body->addWidget(m_views, 1);
    connect(m_listView, &QPushButton::clicked, this, [this] {
        m_listView->setChecked(true);
        m_gridView->setChecked(false);
        if (m_folderPath.isEmpty())
            m_views->setCurrentWidget(m_table);
    });
    connect(m_gridView, &QPushButton::clicked, this, [this] {
        m_gridView->setChecked(true);
        m_listView->setChecked(false);
        if (m_folderPath.isEmpty())
            m_views->setCurrentWidget(m_grid);
    });

    // ---- Detail card ---------------------------------------------------
    auto* detailCard = card(this);
    detailCard->setFixedWidth(330);
    auto* detailOuter = new QVBoxLayout(detailCard);
    detailOuter->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(detailCard);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* detailHost = new QWidget(scroll);
    auto* detailColumn = new QVBoxLayout(detailHost);
    detailColumn->setContentsMargins(14, 13, 14, 14);
    detailColumn->setSpacing(10);

    m_detailEmpty = typedLabel(
        QStringLiteral("Select an item to see what it is and where it lives.\n\n"
                       "A sequence is frames reviewed on one timeline. Select several videos "
                       "and choose New sequence to review them back to back; double-click a "
                       "sequence to see what is in it.\n\n"
                       "Everything plays from the original files: nothing is converted."),
        "dim", detailHost);
    m_detailEmpty->setWordWrap(true);
    detailColumn->addWidget(m_detailEmpty);

    m_detailBody = new QWidget(detailHost);
    auto* detail = new QVBoxLayout(m_detailBody);
    detail->setContentsMargins(0, 0, 0, 0);
    detail->setSpacing(9);

    m_detailName = typedLabel(QStringLiteral("—"), "subtitle", m_detailBody);
    m_detailName->setWordWrap(true);
    detail->addWidget(m_detailName);
    m_detailPath = typedLabel(QString(), "mono", m_detailBody);
    m_detailPath->setWordWrap(true);
    m_detailPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detail->addWidget(m_detailPath);

    auto* badges = new QHBoxLayout;
    badges->setContentsMargins(0, 0, 0, 0);
    badges->setSpacing(6);
    m_detailKind = new Badge(m_detailBody);
    m_detailState = new Badge(m_detailBody);
    badges->addWidget(m_detailKind);
    badges->addWidget(m_detailState);
    badges->addStretch();
    detail->addLayout(badges);

    detail->addWidget(divider(m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Frames"), m_detailFrames, m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Duration"), m_detailDuration, m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Resolution"), m_detailResolution, m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Frame rate"), m_detailRate, m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Size on disk"), m_detailSize, m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Location"), m_detailLocation, m_detailBody));

    m_detailNote = typedLabel(QString(), "dim", m_detailBody);
    m_detailNote->setWordWrap(true);
    detail->addWidget(m_detailNote);

    detail->addWidget(divider(m_detailBody));
    m_review = textButton(QStringLiteral("Open in review"), m_detailBody, "primary");
    m_review->setObjectName(QStringLiteral("openInReview"));
    m_review->setIcon(glyphIcon(Glyph::Review, 15, Qt::white, Qt::white));
    connect(m_review, &QPushButton::clicked, this, [this] {
        if (const auto* entry = selectedEntry(); entry && media::isReviewable(entry->kind))
            emit reviewRequested(entry->path);
    });
    detail->addWidget(m_review);

    m_analyze = textButton(QStringLiteral("Detect && track fish"), m_detailBody);
    m_analyze->setObjectName(QStringLiteral("analyzeFromLibrary"));
    m_analyze->setIcon(glyphIcon(Glyph::Sparkle, 15, palette::AccentHover, palette::AccentHover));
    m_analyze->setToolTip(QStringLiteral("Open in Review and run the active detector and tracker"));
    connect(m_analyze, &QPushButton::clicked, this, [this] {
        if (const auto* entry = selectedEntry(); entry && media::isReviewable(entry->kind))
            emit analyzeRequested(entry->path);
    });
    detail->addWidget(m_analyze);

    m_browse = textButton(QStringLiteral("Browse contents"), m_detailBody);
    m_browse->setObjectName(QStringLiteral("browseSequence"));
    m_browse->setIcon(glyphIcon(Glyph::Files, 15, palette::TextSecondary, palette::TextPrimary));
    connect(m_browse, &QPushButton::clicked, this, [this] {
        if (const auto* entry = selectedEntry())
            openFolder(entry->path);
    });
    detail->addWidget(m_browse);

    m_reveal = textButton(QStringLiteral("Show in folder"), m_detailBody);
    m_reveal->setIcon(glyphIcon(Glyph::Files, 15, palette::TextSecondary, palette::TextPrimary));
    connect(m_reveal, &QPushButton::clicked, this, [this] {
        const auto* entry = selectedEntry();
        if (!entry)
            return;
        const QFileInfo info(entry->path);
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(info.isDir() ? info.absoluteFilePath() : info.absolutePath()));
    });
    detail->addWidget(m_reveal);

    m_copyIn = textButton(QStringLiteral("Copy into project folder"), m_detailBody);
    m_copyIn->setObjectName(QStringLiteral("copyIntoProject"));
    m_copyIn->setIcon(glyphIcon(Glyph::Export, 15, palette::AccentHover, palette::AccentHover));
    m_copyIn->setToolTip(QStringLiteral(
        "Copy this media into the project's media folder so the project is self-contained"));
    connect(m_copyIn, &QPushButton::clicked, this, [this] {
        if (const auto* entry = selectedEntry())
            emit copyIntoProjectRequested(entry->path);
    });
    detail->addWidget(m_copyIn);

    m_remove = textButton(QStringLiteral("Remove from project"), m_detailBody);
    m_remove->setObjectName(QStringLiteral("removeMedia"));
    m_remove->setToolTip(QStringLiteral("Removes the reference only; no file is deleted"));
    connect(m_remove, &QPushButton::clicked, this, [this] {
        if (const auto* entry = selectedEntry())
            emit removeMediaRequested(entry->path);
    });
    detail->addWidget(m_remove);

    m_detailBody->hide();
    detailColumn->addWidget(m_detailBody);
    detailColumn->addStretch();
    scroll->setWidget(detailHost);
    detailOuter->addWidget(scroll);
    body->addWidget(detailCard);
    page->addLayout(body, 1);

    setProject(nullptr);
}

VideosPage::~VideosPage()
{
    m_pool.clear();
    m_pool.waitForDone();
}

void VideosPage::setProject(const aquatrack::domain::ProjectManifest* project,
                            const QString& projectFolder)
{
    m_hasProject = project != nullptr;
    m_projectFolder = project ? QDir::cleanPath(projectFolder) : QString();
    m_addVideo->setEnabled(m_hasProject);
    m_addSequence->setEnabled(m_hasProject);

    m_mediaPaths.clear();
    if (project) {
        m_projectLabel->setText(QStringLiteral("Project: %1")
                                    .arg(QString::fromStdString(project->name)));
        for (const auto& path : project->mediaPaths)
            m_mediaPaths << QString::fromStdString(path);
    } else {
        m_projectLabel->setText(QStringLiteral("Create or open a project to manage media."));
    }
    if (!m_folderPath.isEmpty() && !m_mediaPaths.contains(m_folderPath))
        closeFolder();
    rebuild();
}

QString VideosPage::signatureOf(const QString& path)
{
    QFileInfo info(path);
    if (info.isDir())
        info = QFileInfo(QDir(path).filePath(QString::fromLatin1(media::VideoSequenceManifestName)));
    return QStringLiteral("%1:%2").arg(info.size()).arg(info.lastModified().toMSecsSinceEpoch());
}

void VideosPage::rebuild()
{
    if (const auto* current = selectedEntry())
        m_pendingSelection = current->path;
    m_visible.clear();
    m_entries.clear();
    for (const auto& path : m_mediaPaths) {
        MediaEntry entry;
        entry.path = path;
        entry.kind = media::classifyMedia(path);
        const QFileInfo info(path);
        entry.insideProject =
            !m_projectFolder.isEmpty() &&
            QDir::cleanPath(info.absoluteFilePath())
                .startsWith(m_projectFolder + QLatin1Char('/'), Qt::CaseInsensitive);
        if (entry.kind == media::MediaKind::ImageSequence) {
            // Cheap and synchronous: a few lines of seqinfo.ini.
            QSettings settings(QDir(path).filePath(QStringLiteral("seqinfo.ini")), QSettings::IniFormat);
            settings.beginGroup(QStringLiteral("Sequence"));
            entry.metadata.known = true;
            entry.metadata.frames = settings.value(QStringLiteral("seqLength"), 0).toInt();
            entry.metadata.frameRate = settings.value(QStringLiteral("frameRate"), 0).toDouble();
            entry.metadata.size = QSize(settings.value(QStringLiteral("imWidth"), 0).toInt(),
                                        settings.value(QStringLiteral("imHeight"), 0).toInt());
            if (entry.metadata.frameRate > 0.0)
                entry.metadata.durationMs =
                    qint64(std::llround(entry.metadata.frames * 1000.0 / entry.metadata.frameRate));
            settings.endGroup();
        } else if (entry.kind == media::MediaKind::VideoFile) {
            entry.sizeBytes = info.size();
        }
        if (entry.kind == media::MediaKind::VideoFile || entry.kind == media::MediaKind::VideoSequence) {
            const QString signature = signatureOf(path);
            if (m_signatures.value(path) == signature && m_metadata.contains(path))
                entry.metadata = m_metadata.value(path);
            else
                requestMetadata(entry);
        }
        if (entry.kind == media::MediaKind::ImageSequence && !m_thumbnails.contains(path) &&
            !m_requested.contains(path))
            requestMetadata(entry);
        if (entry.kind == media::MediaKind::VideoSequence && entry.metadata.known)
            entry.sizeBytes = entry.metadata.bytes;
        m_entries.push_back(std::move(entry));
    }
    applyFilter();
}

void VideosPage::requestMetadata(const MediaEntry& entry)
{
    const QString signature = signatureOf(entry.path);
    if (m_requested.value(entry.path) == signature)
        return;
    m_requested.insert(entry.path, signature);
    ++m_probing;
    QPointer<VideosPage> guard(this);
    const QString path = entry.path;
    const media::MediaKind kind = entry.kind;
    m_pool.start([guard, path, kind, signature] {
        Metadata metadata;
        metadata.known = true;
        if (kind == media::MediaKind::VideoFile) {
            media::VideoInfo info;
            QString error;
            if (media::probeVideo(path, info, &error)) {
                metadata.frames = info.frameCount;
                metadata.durationMs = info.durationMs;
                metadata.frameRate = info.frameRate;
                metadata.size = QSize(info.width, info.height);
                metadata.bytes = info.bytes;
                metadata.clips = 1;
            } else {
                metadata.error = error;
            }
        } else if (kind == media::MediaKind::VideoSequence) {
            media::VideoSequenceManifest manifest;
            QString error;
            bool changed = false;
            if (manifest.load(path, &error) && manifest.refresh(&error, &changed)) {
                if (changed)
                    (void)manifest.save(path);
                metadata.frames = manifest.totalFrames();
                metadata.durationMs = manifest.totalDurationMs();
                metadata.frameRate = manifest.clips.first().frameRate;
                metadata.size = QSize(manifest.clips.first().width, manifest.clips.first().height);
                metadata.clips = int(manifest.clips.size());
                for (const auto& clip : manifest.clips)
                    metadata.bytes += clip.bytes;
            } else {
                metadata.error = error;
            }
        }
        const QImage thumbnail =
            metadata.error.isEmpty() ? media::mediaThumbnail(path, kThumbnailWidth * 2) : QImage();
        if (!guard)
            return;
        QMetaObject::invokeMethod(
            guard.data(),
            [guard, path, signature, metadata, thumbnail] {
                if (guard)
                    guard->metadataReady(path, signature, metadata, thumbnail);
            },
            Qt::QueuedConnection);
    });
}

void VideosPage::metadataReady(const QString& path, const QString& signature,
                               const Metadata& metadata, const QImage& thumbnail)
{
    m_probing = std::max(0, m_probing - 1);
    if (m_requested.value(path) == signature)
        m_requested.remove(path);
    if (!thumbnail.isNull())
        m_thumbnails.insert(path, thumbnail);
    MediaEntry* entry = entryForPath(path);
    if (entry && entry->kind != media::MediaKind::ImageSequence) {
        m_metadata.insert(path, metadata);
        m_signatures.insert(path, signature);
        entry->metadata = metadata;
        if (entry->kind == media::MediaKind::VideoSequence)
            entry->sizeBytes = metadata.bytes;
    }
    if (!entry)
        return;
    const int index = int(entry - m_entries.data());
    const int row = int(m_visible.indexOf(index));
    if (row >= 0) {
        fillRow(row, *entry);
        if (row == m_table->currentRow())
            showDetail();
    }
    if (m_probing == 0)
        applyFilter();  // summary counts change once everything is known
}

VideosPage::MediaEntry* VideosPage::entryForPath(const QString& path)
{
    for (auto& entry : m_entries)
        if (entry.path == path)
            return &entry;
    return nullptr;
}

QString VideosPage::stateText(const MediaEntry& entry) const
{
    if (entry.kind == media::MediaKind::Missing)
        return QStringLiteral("Not found");
    if (!media::isReviewable(entry.kind))
        return QStringLiteral("Not reviewable");
    if (!entry.metadata.known)
        return QStringLiteral("Reading...");
    if (!entry.metadata.error.isEmpty())
        return QStringLiteral("Cannot open");
    return QStringLiteral("Reviewable");
}

void VideosPage::fillRow(int row, const MediaEntry& entry)
{
    const QLocale locale = QLocale::system();
    auto* name = new QTableWidgetItem(QFileInfo(entry.path).fileName());
    name->setToolTip(QDir::toNativeSeparators(entry.path));
    m_table->setItem(row, NameColumn, name);

    QString kindText = media::kindLabel(entry.kind);
    if (isSequence(entry.kind))
        kindText += QStringLiteral(" \u00b7 ") + sequenceMakeup(entry.kind, entry.metadata.clips);
    auto* kind = new QTableWidgetItem(kindText);
    kind->setForeground(kindColor(entry.kind));
    if (!entry.metadata.error.isEmpty()) {
        kind->setText(kind->text() + QStringLiteral(" · cannot open"));
        kind->setForeground(palette::Danger);
        kind->setToolTip(entry.metadata.error);
    }
    m_table->setItem(row, KindColumn, kind);

    QString frames = QStringLiteral("—");
    if (entry.metadata.frames > 0)
        frames = locale.toString(entry.metadata.frames);
    else if (media::isReviewable(entry.kind) && !entry.metadata.known)
        frames = QStringLiteral("…");
    m_table->setItem(row, FramesColumn, new QTableWidgetItem(frames));
    m_table->setItem(row, DurationColumn,
                     new QTableWidgetItem(entry.metadata.durationMs > 0
                                              ? media::formatDuration(entry.metadata.durationMs)
                                              : QStringLiteral("—")));
    m_table->setItem(row, SizeColumn, new QTableWidgetItem(formatBytes(entry.sizeBytes)));

    auto* location = new QTableWidgetItem(entry.insideProject ? QStringLiteral("In project")
                                                              : QStringLiteral("Linked"));
    location->setForeground(entry.insideProject ? palette::Success : palette::TextSecondary);
    location->setToolTip(entry.insideProject
                             ? QStringLiteral("Stored in this project's folder")
                             : QStringLiteral("Stays where it is; the project breaks if it "
                                              "moves. Copy it in to make the project "
                                              "self-contained."));
    m_table->setItem(row, LocationColumn, location);

    if (row < m_grid->count()) {
        auto* tile = m_grid->item(row);
        QString caption = QFileInfo(entry.path).fileName();
        QString second = media::kindLabel(entry.kind);
        if (entry.metadata.durationMs > 0)
            second += QStringLiteral(" · ") + media::formatDuration(entry.metadata.durationMs);
        tile->setText(caption + QLatin1Char('\n') + second);
        const auto thumbnail = m_thumbnails.constFind(entry.path);
        tile->setIcon(thumbnail != m_thumbnails.constEnd() ? thumbnailIcon(*thumbnail)
                                                           : placeholderIcon(entry.kind));
        tile->setForeground(entry.metadata.error.isEmpty() ? palette::TextPrimary : palette::Danger);
    }
}

void VideosPage::applyFilter()
{
    const QString needle = m_search->text().trimmed();
    const int kindFilter = m_typeFilter ? m_typeFilter->currentData().toInt() : -1;
    QString previousPath = std::exchange(m_pendingSelection, QString());
    if (const auto* current = selectedEntry(); previousPath.isEmpty() && current)
        previousPath = current->path;

    m_visible.clear();
    int reviewable = 0;
    int videos = 0;
    int problems = 0;
    for (int index = 0; index < m_entries.size(); ++index) {
        const auto& entry = m_entries.at(index);
        const bool problem = !media::isReviewable(entry.kind) || !entry.metadata.error.isEmpty();
        if (problem)
            ++problems;
        else
            ++reviewable;
        if (entry.kind == media::MediaKind::VideoFile)
            ++videos;
        if (!needle.isEmpty() && !entry.path.contains(needle, Qt::CaseInsensitive))
            continue;
        if (kindFilter == kAttentionFilter && !problem)
            continue;
        if (kindFilter == kSequencesFilter && !isSequence(entry.kind))
            continue;
        if (kindFilter >= 0 && kindFilter < kAttentionFilter && int(entry.kind) != kindFilter)
            continue;
        m_visible.push_back(index);
    }

    {
        const QSignalBlocker tableBlocker(m_table);
        const QSignalBlocker gridBlocker(m_grid);
        m_table->setRowCount(int(m_visible.size()));
        m_grid->clear();
        for (int row = 0; row < m_visible.size(); ++row) {
            const auto& entry = m_entries.at(m_visible.at(row));
            auto* tile = new QListWidgetItem(m_grid);
            tile->setData(Qt::UserRole, entry.path);
            tile->setToolTip(QDir::toNativeSeparators(entry.path));
            tile->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            fillRow(row, entry);
        }
    }

    if (!m_hasProject)
        m_summary->setText(QString());
    else if (m_entries.isEmpty())
        m_summary->setText(QStringLiteral("No media yet: add videos or a sequence"));
    else
        m_summary->setText(QStringLiteral("%1 item(s)  ·  %2 reviewable  ·  %3 video(s)%4%5")
                               .arg(m_entries.size())
                               .arg(reviewable)
                               .arg(videos)
                               .arg(problems > 0 ? QStringLiteral("  ·  %1 need attention").arg(problems)
                                                 : QString())
                               .arg(m_probing > 0 ? QStringLiteral("  ·  reading metadata...")
                                                  : QString()));

    if (m_visible.isEmpty()) {
        showDetail();
        updateSelectionActions();
        return;
    }
    int restored = 0;
    for (int row = 0; row < m_visible.size(); ++row) {
        if (m_entries.at(m_visible.at(row)).path == previousPath) {
            restored = row;
            break;
        }
    }
    {
        const QSignalBlocker gridBlocker(m_grid);
        m_grid->setCurrentRow(restored);
    }
    m_table->setCurrentCell(restored, 0);
    showDetail();
    updateSelectionActions();
}

const VideosPage::MediaEntry* VideosPage::selectedEntry() const
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_visible.size())
        return nullptr;
    return &m_entries.at(m_visible.at(row));
}

QStringList VideosPage::selectedVideoPaths() const
{
    QStringList paths;
    QList<int> rows;
    for (const auto& range : m_table->selectedRanges())
        for (int row = range.topRow(); row <= range.bottomRow(); ++row)
            rows << row;
    std::sort(rows.begin(), rows.end());
    for (const int row : rows) {
        if (row < 0 || row >= m_visible.size())
            continue;
        const auto& entry = m_entries.at(m_visible.at(row));
        if (entry.kind == media::MediaKind::VideoFile && entry.metadata.error.isEmpty())
            paths << entry.path;
    }
    paths.removeDuplicates();
    return paths;
}

void VideosPage::updateSelectionActions()
{
    // With videos selected it makes the sequence from them; without, it
    // asks which videos to use.
    const int count = int(selectedVideoPaths().size());
    m_createSequence->setEnabled(m_hasProject);
    m_createSequence->setText(count >= 2 ? QStringLiteral("New sequence from %1 videos").arg(count)
                                         : QStringLiteral("New sequence"));
}

void VideosPage::showContextMenu(const QPoint& position, QWidget* view)
{
    const auto* entry = selectedEntry();
    if (!entry)
        return;
    QMenu menu(this);
    const bool reviewable = media::isReviewable(entry->kind) && entry->metadata.error.isEmpty();
    const QString path = entry->path;
    menu.addAction(QStringLiteral("Open in review"), this, [this, path] { emit reviewRequested(path); })
        ->setEnabled(reviewable);
    menu.addAction(QStringLiteral("Detect && track fish"), this, [this, path] { emit analyzeRequested(path); })
        ->setEnabled(reviewable);
    if (isSequence(entry->kind))
        menu.addAction(QStringLiteral("Browse contents"), this, [this, path] { openFolder(path); });
    if (entry->kind == media::MediaKind::VideoSequence)
        menu.addAction(QStringLiteral("Edit sequence..."), this,
                       [this, path] { emit editSequenceRequested(path); });
    const QStringList videos = selectedVideoPaths();
    if (!videos.isEmpty())
        menu.addAction(QStringLiteral("New sequence from %1 video(s)...").arg(videos.size()),
                       this, [this, videos] { emit createSequenceRequested(videos); });
    menu.addSeparator();
    menu.addAction(QStringLiteral("Show in folder"), m_reveal, &QPushButton::click)
        ->setEnabled(entry->kind != media::MediaKind::Missing);
    if (!entry->insideProject && entry->kind != media::MediaKind::Missing)
        menu.addAction(QStringLiteral("Copy into project folder"), this,
                       [this, path] { emit copyIntoProjectRequested(path); });
    menu.addAction(QStringLiteral("Remove from project"), this,
                   [this, path] { emit removeMediaRequested(path); });
    QAbstractScrollArea* area = qobject_cast<QAbstractScrollArea*>(view);
    menu.exec((area ? area->viewport() : view)->mapToGlobal(position));
}

void VideosPage::openFolder(const QString& sequencePath)
{
    if (!isSequence(media::classifyMedia(sequencePath)))
        return;
    m_folderPath = sequencePath;
    // The detail card describes the sequence being browsed.
    for (int row = 0; row < m_visible.size(); ++row) {
        if (m_entries.at(m_visible.at(row)).path == sequencePath) {
            m_table->setCurrentCell(row, 0);
            break;
        }
    }
    populateFolder();
    m_views->setCurrentIndex(2);
}

void VideosPage::closeFolder()
{
    m_folderPath.clear();
    m_contents->setRowCount(0);
    m_views->setCurrentWidget(m_gridView->isChecked() ? static_cast<QWidget*>(m_grid) : m_table);
}

void VideosPage::populateFolder()
{
    const QLocale locale = QLocale::system();
    m_contents->setRowCount(0);
    const auto addRow = [this](const QString& file, const QString& frames, const QString& duration,
                               const QString& start, int frame, const QColor& colour) {
        const int row = m_contents->rowCount();
        m_contents->insertRow(row);
        auto* name = new QTableWidgetItem(file);
        name->setData(Qt::UserRole, frame);
        name->setToolTip(frame > 0 ? QStringLiteral("Double-click to open in Review at frame %1").arg(frame)
                                   : file);
        name->setForeground(colour);
        m_contents->setItem(row, 0, name);
        m_contents->setItem(row, 1, new QTableWidgetItem(frames));
        m_contents->setItem(row, 2, new QTableWidgetItem(duration));
        m_contents->setItem(row, 3, new QTableWidgetItem(start));
    };

    m_contents->setUpdatesEnabled(false);
    if (media::classifyMedia(m_folderPath) == media::MediaKind::VideoSequence) {
        media::VideoSequenceManifest manifest;
        QString error;
        if (!manifest.load(m_folderPath, &error)) {
            addRow(error, QString(), QString(), QString(), 0, palette::Danger);
        } else {
            m_folderTitle->setText(QStringLiteral("%1   ·   sequence of %2 videos")
                                       .arg(manifest.name)
                                       .arg(manifest.clips.size()));
            int first = 1;
            qint64 start = 0;
            for (int index = 0; index < manifest.clips.size(); ++index) {
                const auto& clip = manifest.clips.at(index);
                const bool exists = QFileInfo(clip.path).isFile();
                addRow(QStringLiteral("%1.  %2%3")
                           .arg(index + 1)
                           .arg(QFileInfo(clip.path).fileName(),
                                exists ? QString() : QStringLiteral("   (missing)")),
                       clip.frameCount > 0 ? locale.toString(clip.frameCount) : QStringLiteral("—"),
                       clip.durationMs > 0 ? media::formatDuration(clip.durationMs) : QStringLiteral("—"),
                       QStringLiteral("frame %1  ·  %2")
                           .arg(locale.toString(first), media::formatDuration(start)),
                       exists ? first : 0, exists ? palette::TextPrimary : palette::Danger);
                first += std::max(0, clip.frameCount);
                start += std::max<qint64>(0, clip.durationMs);
            }
        }
    } else {
        aquatrack::BrackishMotSequence sequence;
        QString error;
        if (!sequence.load(m_folderPath, &error)) {
            addRow(error, QString(), QString(), QString(), 0, palette::Danger);
        } else {
            m_folderTitle->setText(QStringLiteral("%1   ·   sequence of %2 image frames")
                                       .arg(sequence.name(), locale.toString(sequence.frameCount())));
            addRow(QStringLiteral("seqinfo.ini"), QString(), QString(), QString(), 0,
                   palette::TextSecondary);
            addRow(QStringLiteral("gt/gt.txt"), QString(), QString(), QString(), 0, palette::TextSecondary);
            const QDir root(m_folderPath);
            for (int frame = 1; frame <= sequence.frameCount(); ++frame) {
                const qint64 milliseconds =
                    qint64(std::llround((frame - 1) * 1000.0 / std::max(1, sequence.frameRate())));
                addRow(root.relativeFilePath(sequence.imagePath(frame)), QStringLiteral("1"), QString(),
                       QStringLiteral("frame %1  ·  %2")
                           .arg(locale.toString(frame), media::formatDuration(milliseconds)),
                       frame, palette::TextPrimary);
            }
        }
    }
    m_contents->setUpdatesEnabled(true);
}

void VideosPage::showDetail()
{
    const auto* entry = selectedEntry();
    m_detailBody->setVisible(entry != nullptr);
    m_detailEmpty->setVisible(entry == nullptr);
    if (!entry)
        return;
    const QLocale locale = QLocale::system();
    const QFileInfo info(entry->path);
    m_detailName->setText(info.fileName());
    m_detailPath->setText(QDir::toNativeSeparators(entry->path));

    m_detailKind->setText(isSequence(entry->kind)
                              ? media::kindLabel(entry->kind) + QStringLiteral(" \u00b7 ") +
                                    sequenceMakeup(entry->kind, entry->metadata.clips)
                              : media::kindLabel(entry->kind));
    m_detailKind->setAccent(kindColor(entry->kind));
    const QString state = stateText(*entry);
    m_detailState->setText(state);
    m_detailState->setAccent(state == QLatin1String("Reviewable")   ? palette::Success
                             : state == QLatin1String("Reading...") ? palette::TextDim
                                                                    : palette::Danger);

    const auto& metadata = entry->metadata;
    m_detailFrames->setText(metadata.frames > 0 ? locale.toString(metadata.frames)
                                                : QStringLiteral("—"));
    m_detailDuration->setText(metadata.durationMs > 0 ? media::formatDuration(metadata.durationMs)
                                                      : QStringLiteral("—"));
    m_detailResolution->setText(metadata.size.isValid() && metadata.size.width() > 0
                                    ? QStringLiteral("%1 × %2")
                                          .arg(metadata.size.width())
                                          .arg(metadata.size.height())
                                    : QStringLiteral("—"));
    m_detailRate->setText(frameRateText(metadata.frameRate));
    m_detailSize->setText(formatBytes(entry->sizeBytes));
    m_detailLocation->setText(entry->insideProject ? QStringLiteral("In project")
                                                   : QStringLiteral("Linked"));

    QString note;
    switch (entry->kind) {
    case media::MediaKind::ImageSequence:
        note = QStringLiteral("A sequence whose frames are stored as images, with the ground truth "
                              "that came with them (BrackishMOT). Open it in Review, or Browse "
                              "contents to pick a frame.");
        break;
    case media::MediaKind::VideoFile:
        note = metadata.error.isEmpty()
                   ? QStringLiteral("Plays directly from the original file, frame by frame. Run "
                                    "Detect & track for fish tracks, or import ground truth in Review.")
                   : metadata.error;
        break;
    case media::MediaKind::VideoSequence:
        note = metadata.error.isEmpty()
                   ? QStringLiteral("A sequence of %1 videos played back to back on one timeline, "
                                    "straight from the video files. Browse contents to see them and "
                                    "open any one.")
                         .arg(metadata.clips > 0 ? QString::number(metadata.clips) : QStringLiteral("Several"))
                   : metadata.error;
        break;
    case media::MediaKind::Folder:
        note = QStringLiteral("This folder is not a sequence: it has neither a sequence.json (a "
                              "sequence of videos) nor a seqinfo.ini (a sequence of image frames). "
                              "Add its videos, or the sequence folder itself.");
        break;
    case media::MediaKind::Unsupported:
        note = QStringLiteral("This file is not a video AquaTrack can open (%1). Remove it from the "
                              "project.")
                   .arg(media::videoExtensions().join(QStringLiteral(", ")));
        break;
    case media::MediaKind::Missing:
        note = QStringLiteral("Nothing exists at this path any more. It may have been moved, or the "
                              "drive may not be connected.");
        break;
    }
    m_detailNote->setText(note);

    const bool reviewable = media::isReviewable(entry->kind) && metadata.error.isEmpty();
    m_review->setEnabled(reviewable);
    m_review->setText(reviewable ? QStringLiteral("Open in review")
                                 : (media::isReviewable(entry->kind) ? QStringLiteral("Cannot open")
                                                                     : QStringLiteral("Not reviewable")));
    m_analyze->setEnabled(reviewable);
    m_browse->setVisible(isSequence(entry->kind));
    m_reveal->setEnabled(entry->kind != media::MediaKind::Missing);
    m_copyIn->setVisible(!entry->insideProject && entry->kind != media::MediaKind::Missing &&
                         entry->kind != media::MediaKind::VideoSequence);
}
