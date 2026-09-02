#include "ui/videos/VideosPage.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

using namespace aquatrack::ui;

namespace {

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

}  // namespace

VideosPage::VideosPage(QWidget* parent) : QWidget(parent)
{
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

    m_addVideo = textButton(QStringLiteral("Add video files"), this);
    m_addVideo->setIcon(glyphIcon(Glyph::Video, 15, palette::TextSecondary, palette::TextPrimary));
    connect(m_addVideo, &QPushButton::clicked, this, &VideosPage::addMediaRequested);
    head->addWidget(m_addVideo, 0, Qt::AlignBottom);

    m_addSequence = textButton(QStringLiteral("Add sequence"), this, "primary");
    m_addSequence->setIcon(glyphIcon(Glyph::Review, 15, Qt::white, Qt::white));
    connect(m_addSequence, &QPushButton::clicked, this, &VideosPage::addSequenceRequested);
    head->addWidget(m_addSequence, 0, Qt::AlignBottom);
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
    page->addLayout(filters);

    // ---- Table and detail ------------------------------------------------
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(12);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Media"), QStringLiteral("Kind"),
                                        QStringLiteral("Frames"), QStringLiteral("Size")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 4; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    connect(m_table, &QTableWidget::currentCellChanged, this,
            [this](int, int, int, int) { showDetail(); });
    connect(m_table, &QTableWidget::itemDoubleClicked, this, [this] {
        const auto* entry = selectedEntry();
        if (entry && entry->kind == MediaKind::Sequence)
            emit reviewRequested(entry->path);
    });
    body->addWidget(m_table, 1);

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
                       "Sequences are BrackishMOT folders this build can review. Video files "
                       "are recorded as references only."),
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
    detail->addWidget(detailRow(QStringLiteral("Size"), m_detailSize, m_detailBody));

    m_detailNote = typedLabel(QString(), "dim", m_detailBody);
    m_detailNote->setWordWrap(true);
    detail->addWidget(m_detailNote);

    detail->addWidget(divider(m_detailBody));
    m_review = textButton(QStringLiteral("Open in review"), m_detailBody, "primary");
    m_review->setObjectName(QStringLiteral("openInReview"));
    m_review->setIcon(glyphIcon(Glyph::Review, 15, Qt::white, Qt::white));
    connect(m_review, &QPushButton::clicked, this, [this] {
        if (const auto* entry = selectedEntry(); entry && entry->kind == MediaKind::Sequence)
            emit reviewRequested(entry->path);
    });
    detail->addWidget(m_review);

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

    m_remove = textButton(QStringLiteral("Remove from project"), m_detailBody);
    m_remove->setObjectName(QStringLiteral("removeMedia"));
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

void VideosPage::setProject(const aquatrack::domain::ProjectManifest* project)
{
    m_hasProject = project != nullptr;
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
    rebuild();
}

void VideosPage::rebuild()
{
    m_entries.clear();
    for (const auto& path : m_mediaPaths) {
        MediaEntry entry;
        entry.path = path;
        const QFileInfo info(path);
        if (info.isDir()) {
            // A BrackishMOT sequence identifies itself by its seqinfo.ini.
            const QString seqInfo = QDir(path).filePath(QStringLiteral("seqinfo.ini"));
            if (QFileInfo::exists(seqInfo)) {
                entry.kind = MediaKind::Sequence;
                QSettings settings(seqInfo, QSettings::IniFormat);
                settings.beginGroup(QStringLiteral("Sequence"));
                entry.frameCount = settings.value(QStringLiteral("seqLength"), 0).toInt();
                settings.endGroup();
            } else {
                entry.kind = MediaKind::Missing;
            }
        } else if (info.isFile()) {
            entry.kind = MediaKind::VideoFile;
            entry.sizeBytes = info.size();
        } else {
            entry.kind = MediaKind::Missing;
        }
        m_entries.push_back(std::move(entry));
    }
    applyFilter();
}

void VideosPage::applyFilter()
{
    const QString needle = m_search->text().trimmed();
    QString previousPath;
    if (const auto* current = selectedEntry())
        previousPath = current->path;

    m_visible.clear();
    int sequences = 0;
    int missing = 0;
    for (int index = 0; index < m_entries.size(); ++index) {
        const auto& entry = m_entries.at(index);
        if (entry.kind == MediaKind::Sequence)
            ++sequences;
        if (entry.kind == MediaKind::Missing)
            ++missing;
        if (!needle.isEmpty() && !entry.path.contains(needle, Qt::CaseInsensitive))
            continue;
        m_visible.push_back(index);
    }

    m_table->setRowCount(m_visible.size());
    for (int row = 0; row < m_visible.size(); ++row) {
        const auto& entry = m_entries.at(m_visible.at(row));
        m_table->setItem(row, 0, new QTableWidgetItem(QFileInfo(entry.path).fileName()));

        QString kindText;
        QColor kindColor = palette::TextSecondary;
        switch (entry.kind) {
        case MediaKind::Sequence:
            kindText = QStringLiteral("Sequence");
            kindColor = palette::Accent;
            break;
        case MediaKind::VideoFile:
            kindText = QStringLiteral("Video file");
            kindColor = QColor(0xa7, 0x8b, 0xfa);
            break;
        case MediaKind::Missing:
            kindText = QStringLiteral("Missing");
            kindColor = palette::Danger;
            break;
        }
        auto* kind = new QTableWidgetItem(kindText);
        kind->setForeground(kindColor);
        m_table->setItem(row, 1, kind);

        m_table->setItem(row, 2,
                         new QTableWidgetItem(entry.frameCount > 0
                                                  ? QLocale::system().toString(entry.frameCount)
                                                  : QStringLiteral("—")));
        m_table->setItem(row, 3, new QTableWidgetItem(formatBytes(entry.sizeBytes)));
    }

    if (!m_hasProject)
        m_summary->setText(QString());
    else if (m_entries.isEmpty())
        m_summary->setText(QStringLiteral("No media referenced yet"));
    else
        m_summary->setText(QStringLiteral("%1 item(s)  ·  %2 reviewable sequence(s)%3")
                               .arg(m_entries.size())
                               .arg(sequences)
                               .arg(missing > 0 ? QStringLiteral("  ·  %1 missing").arg(missing)
                                                : QString()));

    if (m_visible.isEmpty()) {
        showDetail();
        return;
    }
    int restored = 0;
    for (int row = 0; row < m_visible.size(); ++row) {
        if (m_entries.at(m_visible.at(row)).path == previousPath) {
            restored = row;
            break;
        }
    }
    m_table->setCurrentCell(restored, 0);
    showDetail();
}

const VideosPage::MediaEntry* VideosPage::selectedEntry() const
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_visible.size())
        return nullptr;
    return &m_entries.at(m_visible.at(row));
}

void VideosPage::showDetail()
{
    const auto* entry = selectedEntry();
    m_detailBody->setVisible(entry != nullptr);
    m_detailEmpty->setVisible(entry == nullptr);
    if (!entry)
        return;

    const QFileInfo info(entry->path);
    m_detailName->setText(info.fileName());
    m_detailPath->setText(entry->path);

    switch (entry->kind) {
    case MediaKind::Sequence:
        m_detailKind->setText(QStringLiteral("Sequence"));
        m_detailKind->setAccent(palette::Accent);
        m_detailState->setText(QStringLiteral("Reviewable"));
        m_detailState->setAccent(palette::Success);
        m_detailNote->setText(QStringLiteral(
            "A BrackishMOT image sequence. Open it in Review to inspect frames and tracks."));
        break;
    case MediaKind::VideoFile:
        m_detailKind->setText(QStringLiteral("Video file"));
        m_detailKind->setAccent(QColor(0xa7, 0x8b, 0xfa));
        m_detailState->setText(QStringLiteral("Reference only"));
        m_detailState->setAccent(palette::Warning);
        // Being explicit beats a button that silently does nothing.
        m_detailNote->setText(QStringLiteral(
            "This build has no video decoder, so the file is recorded as a project reference "
            "but cannot be opened in Review. Convert it to an image sequence, or add the "
            "sequence folder instead."));
        break;
    case MediaKind::Missing:
        m_detailKind->setText(QStringLiteral("Unknown"));
        m_detailKind->setAccent(palette::TextDim);
        m_detailState->setText(QStringLiteral("Not found"));
        m_detailState->setAccent(palette::Danger);
        m_detailNote->setText(QStringLiteral(
            "Nothing exists at this path any more. It may have been moved, or the drive may "
            "not be mounted."));
        break;
    }

    m_detailFrames->setText(entry->frameCount > 0
                                ? QLocale::system().toString(entry->frameCount)
                                : QStringLiteral("—"));
    m_detailSize->setText(formatBytes(entry->sizeBytes));

    m_review->setEnabled(entry->kind == MediaKind::Sequence);
    m_review->setText(entry->kind == MediaKind::Sequence
                          ? QStringLiteral("Open in review")
                          : QStringLiteral("Not reviewable in this build"));
    m_reveal->setEnabled(entry->kind != MediaKind::Missing);
}
