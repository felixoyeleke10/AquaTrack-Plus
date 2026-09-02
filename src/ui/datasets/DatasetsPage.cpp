#include "ui/datasets/DatasetsPage.h"

#include "ui/AppTheme.h"
#include "ui/common/Charts.h"
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
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

using namespace aquatrack::ui;
namespace data = aquatrack::data;

namespace {

QColor formatColor(data::DatasetFormat format)
{
    return format == data::DatasetFormat::BrackishMot ? palette::Accent
                                                      : QColor(0xa7, 0x8b, 0xfa);
}

QString grouped(qint64 value)
{
    return QLocale::system().toString(value);
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
    titles->addWidget(sectionLabel(QStringLiteral("Data catalog"), this));
    titles->addWidget(typedLabel(QStringLiteral("Datasets"), "display", this));
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
    m_formatFilter->addItem(QStringLiteral("All formats"), -1);
    m_formatFilter->addItem(QStringLiteral("BrackishMOT"), int(data::DatasetFormat::BrackishMot));
    m_formatFilter->addItem(QStringLiteral("YOLO detection"),
                            int(data::DatasetFormat::YoloDetection));
    m_formatFilter->setMaximumWidth(170);
    connect(m_formatFilter, &QComboBox::currentIndexChanged, this, &DatasetsPage::applyFilter);
    filters->addWidget(m_formatFilter);

    filters->addStretch();
    m_summary = typedLabel(QString(), "dim", this);
    filters->addWidget(m_summary);
    page->addLayout(filters);

    // ---- Table and detail ----------------------------------------------
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(12);

    auto* listColumn = new QVBoxLayout;
    listColumn->setContentsMargins(0, 0, 0, 0);
    listColumn->setSpacing(10);

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Dataset"), QStringLiteral("Format"),
                                        QStringLiteral("Frames"), QStringLiteral("Annotations"),
                                        QStringLiteral("Media")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 5; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    connect(m_table, &QTableWidget::currentCellChanged, this, [this](int, int, int, int) {
        showDetail();
    });
    connect(m_table, &QTableWidget::itemDoubleClicked, this, [this] {
        const auto* dataset = selectedDataset();
        if (dataset && dataset->isReviewable())
            emit reviewRequested(dataset->rootPath);
    });
    listColumn->addWidget(m_table, 1);

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
    badges->addWidget(m_detailFormat);
    badges->addWidget(m_detailState);
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
}

void DatasetsPage::setCatalog(const QString& root, const data::CatalogScanResult& result)
{
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
    const int wantedFormat = m_formatFilter->currentData().toInt();

    QString previousId;
    if (const auto* current = selectedDataset())
        previousId = current->id;

    m_visible.clear();
    qint64 totalFrames = 0;
    qint64 totalAnnotations = 0;
    for (int index = 0; index < m_datasets.size(); ++index) {
        const auto& dataset = m_datasets.at(index);
        if (wantedFormat >= 0 && int(dataset.format) != wantedFormat)
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

    m_table->setRowCount(m_visible.size());
    for (int row = 0; row < m_visible.size(); ++row) {
        const auto& dataset = m_datasets.at(m_visible.at(row));

        m_table->setItem(row, 0, new QTableWidgetItem(dataset.name));

        auto* format = new QTableWidgetItem(data::datasetFormatName(dataset.format));
        format->setForeground(formatColor(dataset.format));
        m_table->setItem(row, 1, format);

        m_table->setItem(row, 2, new QTableWidgetItem(grouped(dataset.frameCount)));
        m_table->setItem(row, 3, new QTableWidgetItem(grouped(dataset.annotationCount)));

        auto* media = new QTableWidgetItem(dataset.mediaPresent
                                               ? QStringLiteral("%1 images")
                                                     .arg(grouped(dataset.imageCount))
                                               : QStringLiteral("missing"));
        media->setForeground(dataset.mediaPresent ? palette::TextSecondary : palette::Danger);
        m_table->setItem(row, 4, media);
    }

    if (m_datasets.isEmpty()) {
        m_summary->setText(QStringLiteral("No datasets found"));
    } else {
        m_summary->setText(QStringLiteral("%1 of %2 dataset(s)  ·  %3 frames  ·  %4 annotations")
                               .arg(m_visible.size())
                               .arg(m_datasets.size())
                               .arg(grouped(totalFrames), grouped(totalAnnotations)));
    }

    if (m_visible.isEmpty()) {
        showDetail();
        return;
    }
    int restored = 0;
    for (int row = 0; row < m_visible.size(); ++row) {
        if (m_datasets.at(m_visible.at(row)).id == previousId) {
            restored = row;
            break;
        }
    }
    m_table->setCurrentCell(restored, 0);
    showDetail();
}

const data::DatasetDescriptor* DatasetsPage::selectedDataset() const
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_visible.size())
        return nullptr;
    return &m_datasets.at(m_visible.at(row));
}

void DatasetsPage::showDetail()
{
    const auto* dataset = selectedDataset();
    m_detailBody->setVisible(dataset != nullptr);
    m_detailEmpty->setVisible(dataset == nullptr);
    if (!dataset)
        return;

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
