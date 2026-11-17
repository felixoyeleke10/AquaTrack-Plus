#include "ui/datasets/DatasetsPage.h"

#include "ui/AppTheme.h"
#include "ui/common/Charts.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"
#include "ui/datasets/DatasetInspector.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <utility>

using namespace aquatrack::ui;
namespace data = aquatrack::data;

namespace {

QColor formatColor(data::DatasetFormat format)
{
    return format == data::DatasetFormat::BrackishMot ? palette::Accent
                                                      : palette::Secondary;
}

// What the source filter sorts by: survey footage, simulated footage, or a
// detector training set.
enum SourceFilter { AllSources = -1, SurveySequences = 0, SyntheticSequences = 1, YoloSets = 2 };

int sourceOf(const data::DatasetDescriptor& dataset)
{
    if (dataset.format == data::DatasetFormat::YoloDetection)
        return YoloSets;
    return dataset.synthetic ? SyntheticSequences : SurveySequences;
}

QString normalisedPath(const QString& path)
{
    // Windows paths compare case-insensitively.
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toCaseFolded();
}

QString grouped(qint64 value)
{
    return QLocale::system().toString(value);
}

// Item data roles shared by the table and the grid.
constexpr int kIndexRole = Qt::UserRole;            // index into m_datasets
constexpr int kSortRole = Qt::UserRole + 1;         // numeric sort key
constexpr int kSubtitleRole = Qt::UserRole + 2;     // grid card second line
constexpr int kTagRole = Qt::UserRole + 3;          // grid card tag text
constexpr int kTagColorRole = Qt::UserRole + 4;     // grid card tag colour
constexpr int kThumbKeyRole = Qt::UserRole + 5;     // dataset id
constexpr int kInProjectRole = Qt::UserRole + 6;    // bool

constexpr QSize kCardSize(220, 186);
constexpr QSize kThumbnailSize(240, 136);

// Sorts by kSortRole when present, so "1,200" sorts after "130".
class SortableItem final : public QTableWidgetItem {
public:
    using QTableWidgetItem::QTableWidgetItem;
    bool operator<(const QTableWidgetItem& other) const override
    {
        const QVariant left = data(kSortRole);
        const QVariant right = other.data(kSortRole);
        if (left.isValid() && right.isValid())
            return left.toLongLong() < right.toLongLong();
        return text().compare(other.text(), Qt::CaseInsensitive) < 0;
    }
};

// A dataset as a card: thumbnail, tag, name and one line of facts.
class DatasetCardDelegate final : public QStyledItemDelegate {
public:
    DatasetCardDelegate(const QHash<QString, QPixmap>* thumbnails, QObject* parent)
        : QStyledItemDelegate(parent), m_thumbnails(thumbnails)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return kCardSize;
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const QRectF card = QRectF(option.rect).adjusted(4, 4, -4, -4);

        painter->setPen(QPen(selected ? palette::Accent : hovered ? QColor(0x2a, 0x34, 0x46)
                                                                  : palette::Line,
                             selected ? 1.6 : 1.0));
        painter->setBrush(selected ? palette::SurfaceRaised : palette::Surface);
        painter->drawRoundedRect(card, 12, 12);

        // Thumbnail, or a placeholder while it loads.
        const QRectF thumb(card.left() + 8, card.top() + 8, card.width() - 16, 112);
        QPainterPath clip;
        clip.addRoundedRect(thumb, 8, 8);
        painter->save();
        painter->setClipPath(clip);
        const QPixmap pixmap = m_thumbnails->value(index.data(kThumbKeyRole).toString());
        if (pixmap.isNull()) {
            painter->fillRect(thumb, palette::Sunken);
            paintGlyph(*painter, Glyph::Datasets,
                       QRectF(thumb.center() - QPointF(14, 14), QSizeF(28, 28)), palette::Line);
        } else {
            QSizeF scaled = pixmap.deviceIndependentSize();
            scaled.scale(thumb.size(), Qt::KeepAspectRatioByExpanding);
            QRectF source(QPointF(), scaled);
            source.moveCenter(thumb.center());
            painter->drawPixmap(source, pixmap, pixmap.rect());
        }
        painter->restore();

        QFont tagFont = option.font;
        tagFont.setPointSizeF(7.4);
        tagFont.setBold(true);
        const QFontMetrics tagMetrics(tagFont);
        painter->setFont(tagFont);
        const auto chip = [&](const QString& text, const QColor& colour, bool right) {
            const qreal width = tagMetrics.horizontalAdvance(text) + 12;
            const QRectF box(right ? thumb.right() - width - 6 : thumb.left() + 6,
                             thumb.top() + 6, width, tagMetrics.height() + 4);
            QColor fill = colour;
            fill.setAlpha(230);
            painter->setPen(Qt::NoPen);
            painter->setBrush(fill);
            painter->drawRoundedRect(box, 5, 5);
            painter->setPen(QColor(6, 8, 12));
            painter->drawText(box, Qt::AlignCenter, text);
        };
        const QString tag = index.data(kTagRole).toString();
        if (!tag.isEmpty())
            chip(tag, index.data(kTagColorRole).value<QColor>(), false);
        if (index.data(kInProjectRole).toBool())
            chip(QStringLiteral("In project"), palette::Success, true);

        QFont nameFont = option.font;
        nameFont.setPointSizeF(9.6);
        nameFont.setBold(true);
        painter->setFont(nameFont);
        painter->setPen(palette::TextPrimary);
        const QRectF nameRect(card.left() + 12, thumb.bottom() + 8, card.width() - 24, 20);
        painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(nameFont).elidedText(index.data(Qt::DisplayRole).toString(),
                                                            Qt::ElideMiddle, int(nameRect.width())));
        QFont subFont = option.font;
        subFont.setPointSizeF(8.2);
        painter->setFont(subFont);
        painter->setPen(palette::TextDim);
        const QRectF subRect(card.left() + 12, nameRect.bottom(), card.width() - 24, 18);
        painter->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(subFont).elidedText(index.data(kSubtitleRole).toString(),
                                                           Qt::ElideRight, int(subRect.width())));
        painter->restore();
    }

private:
    const QHash<QString, QPixmap>* m_thumbnails;
};

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
    value->setWordWrap(true);
    layout->addWidget(value, 1);
    return row;
}

}  // namespace

DatasetsPage::DatasetsPage(QWidget* parent) : QWidget(parent)
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
    // Datasets live beside the models trained on them.
    titles->addWidget(segmentedSwitch({QStringLiteral("Models"), QStringLiteral("Training data")}, 1,
                                      this, [this](int) { emit modelsRequested(); }));
    titles->addSpacing(8);
    titles->addWidget(typedLabel(QStringLiteral("Training data"), "display", this));
    m_rootLabel = typedLabel(QStringLiteral("No catalog scanned"), "dim", this);
    titles->addWidget(m_rootLabel);
    head->addLayout(titles);
    head->addStretch();

    m_refresh = textButton(QStringLiteral("Refresh"), this);
    m_refresh->setIcon(glyphIcon(Glyph::Refresh, 15, palette::TextSecondary, palette::TextPrimary));
    m_refresh->setEnabled(false);
    connect(m_refresh, &QPushButton::clicked, this, &DatasetsPage::refreshRequested);
    head->addWidget(m_refresh, 0, Qt::AlignBottom);

    auto* scan = textButton(QStringLiteral("Scan datasets"), this, "primary");
    scan->setIcon(glyphIcon(Glyph::Datasets, 15, Qt::white, Qt::white));
    connect(scan, &QPushButton::clicked, this, &DatasetsPage::scanRequested);
    head->addWidget(scan, 0, Qt::AlignBottom);
    page->addLayout(head);

    // ---- Filters -------------------------------------------------------
    auto* filters = new QHBoxLayout;
    filters->setContentsMargins(0, 0, 0, 0);
    filters->setSpacing(8);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("Search name, path or class"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(360);
    connect(m_search, &QLineEdit::textChanged, this, &DatasetsPage::applyFilter);
    filters->addWidget(m_search);

    m_formatFilter = new QComboBox(this);
    m_formatFilter->setObjectName(QStringLiteral("datasetSourceFilter"));
    m_formatFilter->addItem(QStringLiteral("All datasets"), int(AllSources));
    m_formatFilter->addItem(QStringLiteral("Survey sequences"), int(SurveySequences));
    m_formatFilter->addItem(QStringLiteral("Synthetic sequences"), int(SyntheticSequences));
    m_formatFilter->addItem(QStringLiteral("YOLO detection sets"), int(YoloSets));
    m_formatFilter->setMaximumWidth(200);
    connect(m_formatFilter, &QComboBox::currentIndexChanged, this, &DatasetsPage::applyFilter);
    filters->addWidget(m_formatFilter);

    filters->addStretch();
    m_summary = typedLabel(QString(), "dim", this);
    filters->addWidget(m_summary);
    filters->addSpacing(8);
    // List / grid toggle.
    m_listViewButton = iconButton(Glyph::ListView, QStringLiteral("List view"), this);
    m_listViewButton->setObjectName(QStringLiteral("listView"));
    m_gridViewButton = iconButton(Glyph::GridView, QStringLiteral("Grid view"), this);
    m_gridViewButton->setObjectName(QStringLiteral("gridView"));
    for (auto* button : {m_listViewButton, m_gridViewButton}) {
        button->setCheckable(true);
        button->setFixedSize(36, 32);
        filters->addWidget(button);
    }
    connect(m_listViewButton, &QPushButton::clicked, this, [this] { setGridView(false); });
    connect(m_gridViewButton, &QPushButton::clicked, this, [this] { setGridView(true); });
    page->addLayout(filters);

    // ---- Table and detail ----------------------------------------------
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(12);

    auto* listColumn = new QVBoxLayout;
    listColumn->setContentsMargins(0, 0, 0, 0);
    listColumn->setSpacing(10);

    m_table = new QTableWidget(0, 6, this);
    m_table->setObjectName(QStringLiteral("datasetTable"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("Dataset"), QStringLiteral("Format"),
                                        QStringLiteral("Frames"), QStringLiteral("Annotations"),
                                        QStringLiteral("Media"), QStringLiteral("Project")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    // Ctrl/Shift-click to pick several sequences and add them in one go.
    m_table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSortIndicatorShown(true);
    // Qt's default indicator is descending; start A to Z by name.
    m_table->horizontalHeader()->setSortIndicator(0, Qt::AscendingOrder);
    m_table->horizontalHeader()->setToolTip(QStringLiteral("Click a column to sort by it"));
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 6; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int, int, int, int) {
        showDetail();
    });
    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this] {
        updateProjectAction();
        syncGridFromTable();
    });
    // Re-sorting the list re-orders the grid to match.
    connect(m_table->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this,
            [this] { QTimer::singleShot(0, this, &DatasetsPage::rebuildGrid); });
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& at) {
        if (m_table->itemAt(at))
            showContextMenu(m_table->viewport()->mapToGlobal(at));
    });
    connect(m_table, &QTableWidget::itemDoubleClicked, this, &DatasetsPage::activateCurrent);

    m_grid = new QListWidget(this);
    m_grid->setObjectName(QStringLiteral("datasetGrid"));
    m_grid->setViewMode(QListView::IconMode);
    m_grid->setResizeMode(QListView::Adjust);
    m_grid->setMovement(QListView::Static);
    m_grid->setUniformItemSizes(true);
    m_grid->setGridSize(kCardSize);
    m_grid->setSpacing(0);
    m_grid->setWrapping(true);
    m_grid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_grid->setMouseTracking(true);
    m_grid->setStyleSheet(QStringLiteral(
        "QListWidget#datasetGrid { background: transparent; border: none; }"
        "QListWidget#datasetGrid::item { border: none; padding: 0; }"));
    m_grid->setItemDelegate(new DatasetCardDelegate(&m_thumbnails, m_grid));
    connect(m_grid, &QListWidget::itemSelectionChanged, this, &DatasetsPage::syncTableFromGrid);
    connect(m_grid, &QListWidget::itemDoubleClicked, this, &DatasetsPage::activateCurrent);
    m_grid->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_grid, &QListWidget::customContextMenuRequested, this, [this](const QPoint& at) {
        if (m_grid->itemAt(at))
            showContextMenu(m_grid->viewport()->mapToGlobal(at));
    });
    connect(m_grid->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &DatasetsPage::queueThumbnails);

    m_thumbnailTimer = new QTimer(this);
    m_thumbnailTimer->setInterval(0);
    connect(m_thumbnailTimer, &QTimer::timeout, this, &DatasetsPage::loadSomeThumbnails);

    m_views = new QStackedWidget(this);
    m_views->addWidget(m_table);
    m_views->addWidget(m_grid);
    listColumn->addWidget(m_views, 1);

    m_warningCard = card(this);
    auto* warningBody = new QVBoxLayout(m_warningCard);
    warningBody->setContentsMargins(12, 10, 12, 11);
    warningBody->setSpacing(7);
    auto* warningHead = new QHBoxLayout;
    warningHead->setContentsMargins(0, 0, 0, 0);
    auto* warningIcon = new QLabel(m_warningCard);
    warningIcon->setPixmap(glyphPixmap(Glyph::Alert, 15, palette::Warning));
    warningHead->addWidget(warningIcon);
    warningHead->addWidget(sectionLabel(QStringLiteral("Catalog warnings"), m_warningCard));
    warningHead->addStretch();
    warningBody->addLayout(warningHead);
    m_warningList = new QListWidget(m_warningCard);
    m_warningList->setMaximumHeight(96);
    m_warningList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_warningList->setWordWrap(true);
    warningBody->addWidget(m_warningList);
    m_warningCard->hide();
    listColumn->addWidget(m_warningCard);
    body->addLayout(listColumn, 1);

    // ---- Detail panel ---------------------------------------------------
    auto* detailCard = card(this);
    detailCard->setFixedWidth(340);
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
        QStringLiteral("Select a dataset to see its frames, annotations and splits.\n\n"
                       "A catalog root is any folder tree containing BrackishMOT sequences "
                       "(seqinfo.ini plus gt/gt.txt) or YOLO manifests (data.yaml or "
                       "dataset.yaml)."),
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
    m_detailFormat = new Badge(m_detailBody);
    m_detailState = new Badge(m_detailBody);
    m_detailOrigin = new Badge(m_detailBody);
    m_detailOrigin->setText(QStringLiteral("Synthetic"));
    m_detailOrigin->setAccent(palette::Warning);
    m_detailOrigin->setToolTip(
        QStringLiteral("Rendered imagery for training and stress tests, not survey footage"));
    badges->addWidget(m_detailFormat);
    badges->addWidget(m_detailState);
    badges->addWidget(m_detailOrigin);
    badges->addStretch();
    detail->addLayout(badges);

    detail->addWidget(divider(m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Declared frames"), m_detailFrames, m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Images on disk"), m_detailImages, m_detailBody));
    detail->addWidget(
        detailRow(QStringLiteral("Annotations"), m_detailAnnotations, m_detailBody));

    detail->addWidget(divider(m_detailBody));
    detail->addWidget(sectionLabel(QStringLiteral("Classes"), m_detailBody));
    m_detailClasses = typedLabel(QStringLiteral("—"), "small", m_detailBody);
    m_detailClasses->setWordWrap(true);
    detail->addWidget(m_detailClasses);

    m_splitsCaption = sectionLabel(QStringLiteral("Splits"), m_detailBody);
    detail->addWidget(m_splitsCaption);
    m_detailSplits = new BarList(m_detailBody);
    m_detailSplits->setEmptyText(QStringLiteral("No splits declared"));
    m_detailSplits->setMinimumHeight(66);
    detail->addWidget(m_detailSplits);

    detail->addWidget(divider(m_detailBody));
    m_review = textButton(QStringLiteral("Open in review"), m_detailBody, "primary");
    m_review->setObjectName(QStringLiteral("openInReview"));
    m_review->setIcon(glyphIcon(Glyph::Review, 15, Qt::white, Qt::white));
    connect(m_review, &QPushButton::clicked, this, [this] {
        if (const auto* dataset = selectedDataset(); dataset && dataset->isReviewable())
            emit reviewRequested(dataset->rootPath);
    });
    detail->addWidget(m_review);

    m_inspect = textButton(QStringLiteral("Inspect images"), m_detailBody);
    m_inspect->setObjectName(QStringLiteral("inspectDataset"));
    m_inspect->setIcon(glyphIcon(Glyph::Inspect, 15, palette::AccentHover, palette::AccentHover));
    m_inspect->setToolTip(QStringLiteral("Browse the images with their annotation boxes"));
    connect(m_inspect, &QPushButton::clicked, this, &DatasetsPage::inspectCurrent);
    detail->addWidget(m_inspect);

    m_addToProject = textButton(QStringLiteral("Add to project"), m_detailBody);
    m_addToProject->setObjectName(QStringLiteral("addToProject"));
    m_addToProject->setIcon(glyphIcon(Glyph::Files, 15, palette::AccentHover, palette::AccentHover));
    connect(m_addToProject, &QPushButton::clicked, this, &DatasetsPage::requestAddSelected);
    detail->addWidget(m_addToProject);
    m_selectionNote = typedLabel(QString(), "dim", m_detailBody);
    m_selectionNote->setWordWrap(true);
    detail->addWidget(m_selectionNote);

    m_reveal = textButton(QStringLiteral("Show in folder"), m_detailBody);
    m_reveal->setIcon(glyphIcon(Glyph::Files, 15, palette::TextSecondary, palette::TextPrimary));
    connect(m_reveal, &QPushButton::clicked, this, [this] {
        if (const auto* dataset = selectedDataset())
            QDesktopServices::openUrl(QUrl::fromLocalFile(dataset->rootPath));
    });
    detail->addWidget(m_reveal);

    m_detailBody->hide();
    detailColumn->addWidget(m_detailBody);
    detailColumn->addStretch();
    scroll->setWidget(detailHost);
    detailOuter->addWidget(scroll);
    body->addWidget(detailCard);

    page->addLayout(body, 1);
    setCatalog(QString(), {});
    setGridView(false);
}

void DatasetsPage::setGridView(bool grid)
{
    m_views->setCurrentWidget(grid ? static_cast<QWidget*>(m_grid) : m_table);
    m_listViewButton->setChecked(!grid);
    m_gridViewButton->setChecked(grid);
    for (auto* button : {m_listViewButton, m_gridViewButton}) {
        const bool active = button->isChecked();
        button->setProperty("role", active ? "toolActive" : "tool");
        button->setIcon(glyphIcon(button == m_gridViewButton ? Glyph::GridView : Glyph::ListView,
                                  17, active ? palette::AccentHover : palette::TextSecondary,
                                  active ? palette::AccentHover : palette::TextPrimary));
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
    if (grid) {
        syncGridFromTable();
        queueThumbnails();
    }
}

bool DatasetsPage::isGridView() const
{
    return m_views->currentWidget() == m_grid;
}

int DatasetsPage::datasetIndexAtRow(int row) const
{
    const auto* item = m_table->item(row, 0);
    return item ? item->data(kIndexRole).toInt() : -1;
}

int DatasetsPage::rowForDatasetIndex(int index) const
{
    for (int row = 0; row < m_table->rowCount(); ++row)
        if (datasetIndexAtRow(row) == index)
            return row;
    return -1;
}

void DatasetsPage::rebuildGrid()
{
    // Cards in the list's current order, so sorting applies to both views.
    const QSignalBlocker blocker(m_grid);
    m_grid->clear();
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const int index = datasetIndexAtRow(row);
        if (index < 0)
            continue;
        const auto& dataset = m_datasets.at(index);
        auto* item = new QListWidgetItem(dataset.name);
        item->setData(kIndexRole, index);
        item->setData(kThumbKeyRole, dataset.id);
        item->setData(kInProjectRole, inProject(dataset.rootPath));
        item->setToolTip(QDir::toNativeSeparators(dataset.rootPath));
        QString subtitle = dataset.format == data::DatasetFormat::YoloDetection
                               ? QStringLiteral("YOLO  ·  %1 images  ·  %2 splits")
                                     .arg(grouped(dataset.imageCount))
                                     .arg(dataset.splits.size())
                               : QStringLiteral("%1 frames  ·  %2 annotations")
                                     .arg(grouped(dataset.frameCount),
                                          grouped(dataset.annotationCount));
        item->setData(kSubtitleRole, subtitle);
        if (!dataset.mediaPresent) {
            item->setData(kTagRole, QStringLiteral("Images missing"));
            item->setData(kTagColorRole, palette::Danger);
        } else if (dataset.format == data::DatasetFormat::BrackishMot
                   && dataset.imageCount < dataset.frameCount) {
            item->setData(kTagRole, QStringLiteral("Incomplete"));
            item->setData(kTagColorRole, palette::Danger);
        } else if (dataset.synthetic) {
            item->setData(kTagRole, QStringLiteral("Synthetic"));
            item->setData(kTagColorRole, palette::Warning);
        } else if (dataset.format == data::DatasetFormat::YoloDetection) {
            item->setData(kTagRole, QStringLiteral("YOLO"));
            item->setData(kTagColorRole, palette::Secondary);
        }
        m_grid->addItem(item);
    }
    syncGridFromTable();
    queueThumbnails();
}

void DatasetsPage::syncGridFromTable()
{
    if (m_syncingSelection || !m_grid)
        return;
    m_syncingSelection = true;
    QSet<int> selected;
    for (const auto& index : m_table->selectionModel()->selectedRows())
        selected.insert(datasetIndexAtRow(index.row()));
    const int current = datasetIndexAtRow(m_table->currentRow());
    for (int position = 0; position < m_grid->count(); ++position) {
        auto* item = m_grid->item(position);
        const int index = item->data(kIndexRole).toInt();
        item->setSelected(selected.contains(index));
        if (index == current) {
            m_grid->setCurrentItem(item, QItemSelectionModel::NoUpdate);
            m_grid->scrollToItem(item);
        }
    }
    m_syncingSelection = false;
}

void DatasetsPage::syncTableFromGrid()
{
    if (m_syncingSelection)
        return;
    m_syncingSelection = true;
    m_table->clearSelection();
    const auto items = m_grid->selectedItems();
    for (auto* item : items) {
        const int row = rowForDatasetIndex(item->data(kIndexRole).toInt());
        if (row >= 0)
            m_table->selectionModel()->select(
                m_table->model()->index(row, 0),
                QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
    if (auto* current = m_grid->currentItem()) {
        const int row = rowForDatasetIndex(current->data(kIndexRole).toInt());
        if (row >= 0)
            m_table->setCurrentCell(row, 0, QItemSelectionModel::NoUpdate);
    }
    m_syncingSelection = false;
    showDetail();
}

void DatasetsPage::queueThumbnails()
{
    if (!isGridView())
        return;
    // Visible cards first, then the rest, skipping ones already decoded.
    QStringList queue;
    const QRect viewport = m_grid->viewport()->rect();
    QStringList later;
    for (int position = 0; position < m_grid->count(); ++position) {
        auto* item = m_grid->item(position);
        const QString id = item->data(kThumbKeyRole).toString();
        if (m_thumbnails.contains(id))
            continue;
        (m_grid->visualItemRect(item).intersects(viewport) ? queue : later) << id;
    }
    m_thumbnailQueue = queue + later;
    if (!m_thumbnailQueue.isEmpty())
        m_thumbnailTimer->start();
}

void DatasetsPage::loadSomeThumbnails()
{
    // A handful per turn of the event loop keeps scrolling smooth.
    for (int count = 0; count < 4 && !m_thumbnailQueue.isEmpty(); ++count) {
        const QString id = m_thumbnailQueue.takeFirst();
        const auto found = std::find_if(m_datasets.cbegin(), m_datasets.cend(),
                                        [&id](const auto& item) { return item.id == id; });
        QPixmap pixmap;
        if (found != m_datasets.cend() && !found->previewImage.isEmpty()) {
            QImageReader reader(found->previewImage);
            QSize size = reader.size();
            if (size.isValid()) {
                size.scale(kThumbnailSize, Qt::KeepAspectRatioByExpanding);
                reader.setScaledSize(size);  // JPEG decodes straight to the small size
            }
            pixmap = QPixmap::fromImage(reader.read());
        }
        // Cache failures too (as null), so a broken file is not retried.
        m_thumbnails.insert(id, pixmap);
    }
    m_grid->viewport()->update();
    if (m_thumbnailQueue.isEmpty())
        m_thumbnailTimer->stop();
}

void DatasetsPage::activateCurrent()
{
    // Sequences open in Review; anything else with images opens the inspector.
    const auto* dataset = selectedDataset();
    if (!dataset)
        return;
    if (dataset->isReviewable())
        emit reviewRequested(dataset->rootPath);
    else if (dataset->mediaPresent)
        inspectCurrent();
}

void DatasetsPage::inspectCurrent()
{
    const auto* dataset = selectedDataset();
    if (!dataset || !dataset->mediaPresent)
        return;
    auto* inspector = new DatasetInspector(*dataset, window());
    connect(inspector, &DatasetInspector::reviewRequested, this, &DatasetsPage::reviewRequested);
    inspector->show();
    inspector->raise();
}

void DatasetsPage::showContextMenu(const QPoint& globalPosition)
{
    const auto* current = selectedDataset();
    QMenu menu(this);
    QAction* review = menu.addAction(QStringLiteral("Open in review"));
    review->setEnabled(current && current->isReviewable());
    QAction* inspect = menu.addAction(QStringLiteral("Inspect images"));
    inspect->setEnabled(current && current->mediaPresent);
    QAction* add = menu.addAction(m_addToProject->text());
    add->setEnabled(m_addToProject->isEnabled());
    menu.addSeparator();
    QAction* reveal = menu.addAction(QStringLiteral("Show in folder"));
    QAction* chosen = menu.exec(globalPosition);
    if (chosen == review && current)
        emit reviewRequested(current->rootPath);
    else if (chosen == inspect)
        inspectCurrent();
    else if (chosen == add)
        requestAddSelected();
    else if (chosen == reveal && current)
        QDesktopServices::openUrl(QUrl::fromLocalFile(current->rootPath));
}

void DatasetsPage::setCatalog(const QString& root, const data::CatalogScanResult& result)
{
    if (const auto* current = selectedDataset())
        m_pendingSelection = current->id;
    m_visible.clear();
    m_datasets = result.datasets;
    m_rootLabel->setText(root.isEmpty() ? QStringLiteral("No catalog scanned") : root);
    m_refresh->setEnabled(!root.isEmpty());

    m_warningList->clear();
    const QString prefix = QDir::fromNativeSeparators(root) + QLatin1Char('/');
    for (const auto& warning : result.warnings) {
        QString text = QDir::fromNativeSeparators(warning);
        if (!root.isEmpty())
            text.replace(prefix, QString());
        m_warningList->addItem(text);
    }
    m_warningCard->setVisible(!result.warnings.isEmpty());

    applyFilter();
}

void DatasetsPage::applyFilter()
{
    const QString needle = m_search->text().trimmed();
    const int wantedSource = m_formatFilter->currentData().toInt();

    QString previousId = std::exchange(m_pendingSelection, QString());
    if (const auto* current = selectedDataset(); previousId.isEmpty() && current)
        previousId = current->id;

    m_visible.clear();
    qint64 totalFrames = 0;
    qint64 totalAnnotations = 0;
    for (int index = 0; index < m_datasets.size(); ++index) {
        const auto& dataset = m_datasets.at(index);
        if (wantedSource != AllSources && sourceOf(dataset) != wantedSource)
            continue;
        if (!needle.isEmpty()) {
            QString haystack = dataset.name + ' ' + dataset.rootPath + ' '
                + dataset.classNames.join(QLatin1Char(' '));
            if (!haystack.contains(needle, Qt::CaseInsensitive))
                continue;
        }
        m_visible.push_back(index);
        totalFrames += dataset.frameCount;
        totalAnnotations += dataset.annotationCount;
    }

    // Sorting is suspended while rows are filled, then re-applied.
    m_table->setSortingEnabled(false);
    m_table->clearContents();
    m_table->setRowCount(m_visible.size());
    for (int row = 0; row < m_visible.size(); ++row) {
        const auto& dataset = m_datasets.at(m_visible.at(row));

        auto* name = new SortableItem(dataset.name);
        name->setData(kIndexRole, m_visible.at(row));
        m_table->setItem(row, 0, name);

        auto* format = new SortableItem(data::datasetFormatName(dataset.format));
        format->setForeground(formatColor(dataset.format));
        if (dataset.synthetic) {
            // Simulated footage must not be mistaken for a survey.
            format->setText(format->text() + QStringLiteral("  \u00b7  synthetic"));
            format->setForeground(palette::Warning);
            format->setIcon(glyphIcon(Glyph::Sparkle, 14, palette::Warning, palette::Warning));
            format->setToolTip(
                QStringLiteral("Rendered imagery for training and stress tests, not survey footage"));
        }
        m_table->setItem(row, 1, format);

        auto* frames = new SortableItem(grouped(dataset.frameCount));
        frames->setData(kSortRole, dataset.frameCount);
        m_table->setItem(row, 2, frames);
        auto* annotations = new SortableItem(grouped(dataset.annotationCount));
        annotations->setData(kSortRole, dataset.annotationCount);
        m_table->setItem(row, 3, annotations);

        auto* media = new SortableItem(dataset.mediaPresent
                                               ? QStringLiteral("%1 images")
                                                     .arg(grouped(dataset.imageCount))
                                               : QStringLiteral("missing"));
        media->setForeground(dataset.mediaPresent ? palette::TextSecondary : palette::Danger);
        media->setData(kSortRole, dataset.mediaPresent ? dataset.imageCount : -1);
        m_table->setItem(row, 4, media);

        auto* project = new SortableItem(inProject(dataset.rootPath) ? QStringLiteral("Added")
                                                                         : QString());
        project->setForeground(palette::Success);
        m_table->setItem(row, 5, project);
    }
    m_table->setSortingEnabled(true);

    if (m_datasets.isEmpty()) {
        m_summary->setText(QStringLiteral("No datasets found"));
    } else {
        m_summary->setText(QStringLiteral("%1 of %2 dataset(s)  ·  %3 frames  ·  %4 annotations")
                               .arg(m_visible.size())
                               .arg(m_datasets.size())
                               .arg(grouped(totalFrames), grouped(totalAnnotations)));
    }

    if (m_visible.isEmpty()) {
        rebuildGrid();
        showDetail();
        return;
    }
    int restored = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const int index = datasetIndexAtRow(row);
        if (index >= 0 && m_datasets.at(index).id == previousId) {
            restored = row;
            break;
        }
    }
    m_table->setCurrentCell(restored, 0);
    rebuildGrid();
    showDetail();
}

void DatasetsPage::setProjectMedia(bool projectOpen, const QStringList& mediaPaths)
{
    m_projectOpen = projectOpen;
    m_projectMedia.clear();
    for (const auto& path : mediaPaths)
        m_projectMedia.insert(normalisedPath(path));
    // Refresh the "Project" column and card chips without disturbing the
    // selection.
    for (int row = 0; row < m_table->rowCount(); ++row) {
        const int index = datasetIndexAtRow(row);
        if (auto* item = m_table->item(row, 5); item && index >= 0)
            item->setText(inProject(m_datasets.at(index).rootPath) ? QStringLiteral("Added")
                                                                   : QString());
    }
    for (int position = 0; position < m_grid->count(); ++position) {
        auto* item = m_grid->item(position);
        item->setData(kInProjectRole,
                      inProject(m_datasets.at(item->data(kIndexRole).toInt()).rootPath));
    }
    updateProjectAction();
}

bool DatasetsPage::inProject(const QString& path) const
{
    return m_projectMedia.contains(normalisedPath(path));
}

QVector<const data::DatasetDescriptor*> DatasetsPage::selectedDatasets() const
{
    QVector<const data::DatasetDescriptor*> selected;
    const auto rows = m_table->selectionModel()->selectedRows();
    for (const auto& index : rows) {
        const int dataset = datasetIndexAtRow(index.row());
        if (dataset >= 0)
            selected.push_back(&m_datasets.at(dataset));
    }
    if (selected.isEmpty())
        if (const auto* current = selectedDataset())
            selected.push_back(current);
    return selected;
}

QStringList DatasetsPage::selectedSequencePaths() const
{
    QStringList paths;
    for (const auto* dataset : selectedDatasets())
        if (dataset->isReviewable())
            paths << dataset->rootPath;
    return paths;
}

void DatasetsPage::updateProjectAction()
{
    const auto selected = selectedDatasets();
    int sequences = 0;
    int fresh = 0;
    int synthetic = 0;
    for (const auto* dataset : selected) {
        if (!dataset->isReviewable())
            continue;
        ++sequences;
        if (!inProject(dataset->rootPath))
            ++fresh;
        if (dataset->synthetic)
            ++synthetic;
    }

    if (sequences == 0) {
        m_addToProject->setEnabled(false);
        m_addToProject->setText(QStringLiteral("Add to project"));
        m_addToProject->setToolTip(
            QStringLiteral("Only BrackishMOT sequences with images and ground truth can be added"));
    } else if (fresh == 0) {
        m_addToProject->setEnabled(false);
        m_addToProject->setText(sequences == 1 ? QStringLiteral("Already in project")
                                               : QStringLiteral("All already in project"));
        m_addToProject->setToolTip(QString());
    } else {
        m_addToProject->setEnabled(true);
        const QString what = fresh == 1 ? QStringLiteral("sequence")
                                        : QStringLiteral("%1 sequences").arg(fresh);
        m_addToProject->setText(m_projectOpen ? QStringLiteral("Add %1 to project").arg(what)
                                              : QStringLiteral("Add %1 to a new project...").arg(what));
        m_addToProject->setToolTip(QString());
    }

    QStringList notes;
    if (selected.size() > 1)
        notes << QStringLiteral("%1 selected. Ctrl or Shift-click to change the selection.")
                     .arg(selected.size());
    if (synthetic > 0 && sequences == 1)
        notes << QStringLiteral("Synthetic footage: useful for training and stress tests, but keep "
                                "it out of survey counts.");
    else if (synthetic > 0)
        notes << QStringLiteral("%1 of the %2 sequences are synthetic.").arg(synthetic).arg(sequences);
    m_selectionNote->setText(notes.join(QLatin1Char(' ')));
    m_selectionNote->setVisible(!notes.isEmpty());
}

void DatasetsPage::requestAddSelected()
{
    QStringList fresh;
    for (const auto& path : selectedSequencePaths())
        if (!inProject(path))
            fresh << path;
    if (!fresh.isEmpty())
        emit addToProjectRequested(fresh);
}

const data::DatasetDescriptor* DatasetsPage::selectedDataset() const
{
    const int index = datasetIndexAtRow(m_table->currentRow());
    if (index < 0 || index >= m_datasets.size())
        return nullptr;
    return &m_datasets.at(index);
}

void DatasetsPage::showDetail()
{
    const auto* dataset = selectedDataset();
    m_detailBody->setVisible(dataset != nullptr);
    m_detailEmpty->setVisible(dataset == nullptr);
    updateProjectAction();
    if (!dataset)
        return;
    m_detailOrigin->setVisible(dataset->synthetic);

    m_detailName->setText(dataset->name);
    m_detailPath->setText(dataset->rootPath);

    m_detailFormat->setText(data::datasetFormatName(dataset->format));
    m_detailFormat->setAccent(formatColor(dataset->format));

    if (!dataset->mediaPresent) {
        m_detailState->setText(QStringLiteral("Images missing"));
        m_detailState->setAccent(palette::Danger);
    } else if (!dataset->hasAnnotations) {
        m_detailState->setText(QStringLiteral("No annotations"));
        m_detailState->setAccent(palette::Warning);
    } else if (dataset->format == data::DatasetFormat::BrackishMot
               && dataset->imageCount < dataset->frameCount) {
        m_detailState->setText(QStringLiteral("Incomplete"));
        m_detailState->setAccent(palette::Warning);
    } else {
        m_detailState->setText(QStringLiteral("Ready"));
        m_detailState->setAccent(palette::Success);
    }

    m_detailFrames->setText(dataset->frameCount > 0 ? grouped(dataset->frameCount)
                                                    : QStringLiteral("—"));
    m_detailImages->setText(grouped(dataset->imageCount));
    m_detailAnnotations->setText(dataset->annotationCount > 0
                                     ? grouped(dataset->annotationCount)
                                     : QStringLiteral("none"));

    m_detailClasses->setText(dataset->classNames.isEmpty()
                                 ? QStringLiteral("Not declared in the manifest")
                                 : QStringLiteral("%1  (%2)")
                                       .arg(dataset->classNames.join(QStringLiteral(", ")))
                                       .arg(dataset->classNames.size()));

    QVector<BarRow> splits;
    for (const auto& split : dataset->splits) {
        BarRow row;
        row.badge = split.name;
        row.color = formatColor(dataset->format);
        row.value = double(split.imageCount);
        row.valueText = grouped(split.imageCount);
        splits.push_back(std::move(row));
    }
    m_detailSplits->setRows(splits);
    const bool hasSplits = dataset->format == data::DatasetFormat::YoloDetection;
    m_splitsCaption->setVisible(hasSplits);
    m_detailSplits->setVisible(hasSplits);

    // Only a BrackishMOT sequence with imagery and ground truth can be opened
    // in the review workspace; say why when it cannot.
    m_inspect->setEnabled(dataset->mediaPresent);
    m_review->setEnabled(dataset->isReviewable());
    if (dataset->isReviewable())
        m_review->setText(QStringLiteral("Open in review"));
    else if (dataset->format != data::DatasetFormat::BrackishMot)
        m_review->setText(QStringLiteral("Review needs a BrackishMOT sequence"));
    else if (!dataset->mediaPresent)
        m_review->setText(QStringLiteral("Images missing"));
    else
        m_review->setText(QStringLiteral("Annotations missing"));
}
