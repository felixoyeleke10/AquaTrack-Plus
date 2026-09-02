#include "ui/models/ModelsPage.h"

#include "ui/AppTheme.h"
#include "ui/common/Charts.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

using namespace aquatrack::ui;
namespace models = aquatrack::models;

namespace {

QString typeLabel(models::ModelType type)
{
    switch (type) {
    case models::ModelType::Detector:
        return QStringLiteral("Detector");
    case models::ModelType::Tracker:
        return QStringLiteral("Tracker");
    case models::ModelType::ReIdentification:
        return QStringLiteral("Re-ID");
    case models::ModelType::Species:
        return QStringLiteral("Species");
    case models::ModelType::Behavior:
        return QStringLiteral("Behaviour");
    case models::ModelType::Unknown:
        break;
    }
    return QStringLiteral("Unknown");
}

QColor typeColor(models::ModelType type)
{
    switch (type) {
    case models::ModelType::Detector:
        return palette::Accent;
    case models::ModelType::Tracker:
        return QColor(0x7e, 0xd9, 0x57);
    case models::ModelType::ReIdentification:
        return QColor(0xa7, 0x8b, 0xfa);
    case models::ModelType::Species:
        return QColor(0xf5, 0xd9, 0x49);
    case models::ModelType::Behavior:
        return QColor(0xff, 0x9f, 0x45);
    case models::ModelType::Unknown:
        break;
    }
    return palette::TextDim;
}

QString formatBytes(qint64 bytes)
{
    constexpr double kMegabyte = 1024.0 * 1024.0;
    if (bytes <= 0)
        return QStringLiteral("—");
    if (bytes < kMegabyte)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 0);
    return QStringLiteral("%1 MB").arg(bytes / kMegabyte, 0, 'f', 1);
}

// The metric a reviewer scans a detector list for, if the manifest has one.
QString headlineMetric(const std::map<std::string, double>& metrics)
{
    for (const auto* preferred : {"map50_95", "map50", "map", "f1", "recall", "precision"}) {
        const auto found = metrics.find(preferred);
        if (found != metrics.end())
            return QStringLiteral("%1 %2")
                .arg(QString::fromStdString(found->first))
                .arg(found->second, 0, 'f', 3);
    }
    if (metrics.empty())
        return QStringLiteral("—");
    const auto& first = *metrics.begin();
    return QStringLiteral("%1 %2")
        .arg(QString::fromStdString(first.first))
        .arg(first.second, 0, 'f', 3);
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

ModelsPage::ModelsPage(QWidget* parent) : QWidget(parent)
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
    titles->addWidget(sectionLabel(QStringLiteral("Model registry"), this));
    titles->addWidget(typedLabel(QStringLiteral("Models"), "display", this));
    m_rootLabel = typedLabel(QStringLiteral("No registry open"), "dim", this);
    titles->addWidget(m_rootLabel);
    head->addLayout(titles);
    head->addStretch();

    m_refresh = textButton(QStringLiteral("Refresh"), this);
    m_refresh->setIcon(glyphIcon(Glyph::Refresh, 15, palette::TextSecondary, palette::TextPrimary));
    m_refresh->setEnabled(false);
    connect(m_refresh, &QPushButton::clicked, this, &ModelsPage::refreshRequested);
    head->addWidget(m_refresh, 0, Qt::AlignBottom);

    auto* scan = textButton(QStringLiteral("Scan registry"), this, "primary");
    scan->setIcon(glyphIcon(Glyph::Datasets, 15, Qt::white, Qt::white));
    connect(scan, &QPushButton::clicked, this, &ModelsPage::scanRequested);
    head->addWidget(scan, 0, Qt::AlignBottom);
    page->addLayout(head);

    // ---- Filters -------------------------------------------------------
    auto* filters = new QHBoxLayout;
    filters->setContentsMargins(0, 0, 0, 0);
    filters->setSpacing(8);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(QStringLiteral("Search name, id, backend or class"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(360);
    connect(m_search, &QLineEdit::textChanged, this, &ModelsPage::applyFilter);
    filters->addWidget(m_search);

    m_typeFilter = new QComboBox(this);
    m_typeFilter->addItem(QStringLiteral("All types"), -1);
    for (const auto type : {models::ModelType::Detector, models::ModelType::Tracker,
                            models::ModelType::ReIdentification, models::ModelType::Species,
                            models::ModelType::Behavior})
        m_typeFilter->addItem(typeLabel(type), int(type));
    m_typeFilter->setMaximumWidth(160);
    connect(m_typeFilter, &QComboBox::currentIndexChanged, this, &ModelsPage::applyFilter);
    filters->addWidget(m_typeFilter);

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

    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Model"), QStringLiteral("Type"),
                                        QStringLiteral("Backend"), QStringLiteral("Input"),
                                        QStringLiteral("Headline metric"),
                                        QStringLiteral("Artifact")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 6; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    connect(m_table, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) { showDetail(row); });
    listColumn->addWidget(m_table, 1);

    // Warnings live in their own card rather than being appended to the list
    // of models, so a broken manifest cannot be mistaken for a usable one.
    m_warningCard = card(this);
    auto* warningBody = new QVBoxLayout(m_warningCard);
    warningBody->setContentsMargins(12, 10, 12, 11);
    warningBody->setSpacing(7);
    auto* warningHead = new QHBoxLayout;
    warningHead->setContentsMargins(0, 0, 0, 0);
    auto* warningIcon = new QLabel(m_warningCard);
    warningIcon->setPixmap(glyphPixmap(Glyph::Alert, 15, palette::Warning));
    warningHead->addWidget(warningIcon);
    warningHead->addWidget(sectionLabel(QStringLiteral("Registry warnings"), m_warningCard));
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
        QStringLiteral("Select a model to see its backend, input size, classes, metrics and "
                       "artifact.\n\nA registry is any folder tree containing model.json files. "
                       "Each one needs id, name, type, backend, input and artifact."),
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

    m_detailId = typedLabel(QString(), "mono", m_detailBody);
    m_detailId->setWordWrap(true);
    detail->addWidget(m_detailId);

    auto* badges = new QHBoxLayout;
    badges->setContentsMargins(0, 0, 0, 0);
    badges->setSpacing(6);
    m_detailType = new Badge(m_detailBody);
    m_detailState = new Badge(m_detailBody);
    badges->addWidget(m_detailType);
    badges->addWidget(m_detailState);
    badges->addStretch();
    detail->addLayout(badges);

    detail->addWidget(divider(m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Backend"), m_detailBackend, m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Precision"), m_detailPrecision, m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Input"), m_detailInput, m_detailBody));
    detail->addWidget(detailRow(QStringLiteral("Artifact"), m_detailArtifact, m_detailBody));

    detail->addWidget(divider(m_detailBody));
    detail->addWidget(sectionLabel(QStringLiteral("Classes"), m_detailBody));
    m_detailClasses = typedLabel(QStringLiteral("—"), "small", m_detailBody);
    m_detailClasses->setWordWrap(true);
    detail->addWidget(m_detailClasses);

    detail->addWidget(divider(m_detailBody));
    detail->addWidget(sectionLabel(QStringLiteral("Reported metrics"), m_detailBody));
    m_detailMetrics = new BarList(m_detailBody);
    m_detailMetrics->setEmptyText(QStringLiteral("No metrics in this manifest"));
    m_detailMetrics->setMinimumHeight(74);
    detail->addWidget(m_detailMetrics);

    detail->addWidget(divider(m_detailBody));
    m_activate = textButton(QStringLiteral("Set as active detector"), m_detailBody, "primary");
    // Its label changes with the selection, so give it a stable handle.
    m_activate->setObjectName(QStringLiteral("activateDetector"));
    m_activate->setIcon(glyphIcon(Glyph::Models, 15, Qt::white, Qt::white));
    connect(m_activate, &QPushButton::clicked, this, [this] {
        if (const auto* entry = selectedEntry())
            emit activeDetectorRequested(QString::fromStdString(entry->manifest.id));
    });
    detail->addWidget(m_activate);

    m_reveal = textButton(QStringLiteral("Show in folder"), m_detailBody);
    m_reveal->setIcon(glyphIcon(Glyph::Files, 15, palette::TextSecondary, palette::TextPrimary));
    connect(m_reveal, &QPushButton::clicked, this, [this] {
        const auto* entry = selectedEntry();
        if (!entry)
            return;
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(QFileInfo(entry->manifestPath).absolutePath()));
    });
    detail->addWidget(m_reveal);

    m_detailBody->hide();
    detailColumn->addWidget(m_detailBody);
    detailColumn->addStretch();
    scroll->setWidget(detailHost);
    detailOuter->addWidget(scroll);
    body->addWidget(detailCard);

    page->addLayout(body, 1);
    setRegistry(QString(), {}, {});
}

void ModelsPage::setRegistry(const QString& root, const QVector<aquatrack::data::ModelEntry>& entries,
                             const QStringList& warnings)
{
    m_entries = entries;
    m_rootLabel->setText(root.isEmpty() ? QStringLiteral("No registry open") : root);
    m_refresh->setEnabled(!root.isEmpty());

    m_warningList->clear();
    const QString prefix = QDir::fromNativeSeparators(root) + QLatin1Char('/');
    for (const auto& warning : warnings) {
        // Warnings carry absolute manifest paths, which are mostly the registry
        // root repeated. Show what distinguishes them instead.
        QString text = QDir::fromNativeSeparators(warning);
        if (!root.isEmpty())
            text.replace(prefix, QString());
        m_warningList->addItem(text);
    }
    m_warningCard->setVisible(!warnings.isEmpty());

    applyFilter();
}

void ModelsPage::setActiveDetector(const QString& modelId)
{
    m_activeDetector = modelId;
    applyFilter();
}

void ModelsPage::applyFilter()
{
    const QString needle = m_search->text().trimmed();
    const int wantedType = m_typeFilter->currentData().toInt();

    // Refreshing or filtering should not throw away what the operator was
    // looking at, so remember it by id and restore it afterwards.
    QString previousId;
    if (const auto* current = selectedEntry())
        previousId = QString::fromStdString(current->manifest.id);

    m_visible.clear();
    for (int index = 0; index < m_entries.size(); ++index) {
        const auto& manifest = m_entries.at(index).manifest;
        if (wantedType >= 0 && int(manifest.type) != wantedType)
            continue;
        if (!needle.isEmpty()) {
            QString haystack = QString::fromStdString(manifest.displayName) + ' '
                + QString::fromStdString(manifest.id) + ' '
                + QString::fromStdString(manifest.backend);
            for (const auto& className : manifest.classes)
                haystack += ' ' + QString::fromStdString(className);
            if (!haystack.contains(needle, Qt::CaseInsensitive))
                continue;
        }
        m_visible.push_back(index);
    }

    m_table->setRowCount(m_visible.size());
    for (int row = 0; row < m_visible.size(); ++row) {
        const auto& entry = m_entries.at(m_visible.at(row));
        const auto& manifest = entry.manifest;
        const bool active = QString::fromStdString(manifest.id) == m_activeDetector;

        auto* name = new QTableWidgetItem(QString::fromStdString(manifest.displayName)
                                          + (active ? QStringLiteral("   ● active") : QString()));
        if (active)
            name->setForeground(palette::Accent);
        m_table->setItem(row, 0, name);

        auto* type = new QTableWidgetItem(typeLabel(manifest.type));
        type->setForeground(typeColor(manifest.type));
        m_table->setItem(row, 1, type);

        m_table->setItem(row, 2,
                         new QTableWidgetItem(QString::fromStdString(manifest.backend)));
        m_table->setItem(row, 3,
                         new QTableWidgetItem(QStringLiteral("%1 x %2")
                                                  .arg(manifest.inputWidth)
                                                  .arg(manifest.inputHeight)));
        m_table->setItem(row, 4, new QTableWidgetItem(headlineMetric(manifest.metrics)));

        auto* artifact = new QTableWidgetItem(
            entry.isUsable() ? formatBytes(entry.artifactBytes) : QStringLiteral("missing"));
        artifact->setForeground(entry.isUsable() ? palette::TextSecondary : palette::Danger);
        m_table->setItem(row, 5, artifact);
    }

    if (m_entries.isEmpty())
        m_summary->setText(QStringLiteral("No models found"));
    else
        m_summary->setText(QStringLiteral("%1 of %2 model(s)")
                               .arg(m_visible.size())
                               .arg(m_entries.size()));

    if (m_visible.isEmpty()) {
        showDetail(-1);
        return;
    }
    int restored = 0;
    for (int row = 0; row < m_visible.size(); ++row) {
        if (QString::fromStdString(m_entries.at(m_visible.at(row)).manifest.id) == previousId) {
            restored = row;
            break;
        }
    }
    m_table->setCurrentCell(restored, 0);
    // setCurrentCell is a no-op when the row is unchanged, so refresh the
    // detail explicitly: the entry behind that row may have new contents.
    showDetail(restored);
}

const aquatrack::data::ModelEntry* ModelsPage::selectedEntry() const
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_visible.size())
        return nullptr;
    return &m_entries.at(m_visible.at(row));
}

void ModelsPage::showDetail(int entryIndex)
{
    Q_UNUSED(entryIndex);
    const auto* entry = selectedEntry();
    m_detailBody->setVisible(entry != nullptr);
    m_detailEmpty->setVisible(entry == nullptr);
    if (!entry)
        return;

    const auto& manifest = entry->manifest;
    m_detailName->setText(QString::fromStdString(manifest.displayName));
    m_detailId->setText(QString::fromStdString(manifest.id));

    m_detailType->setText(typeLabel(manifest.type));
    m_detailType->setAccent(typeColor(manifest.type));

    const bool active = QString::fromStdString(manifest.id) == m_activeDetector;
    if (!entry->isUsable()) {
        m_detailState->setText(QStringLiteral("Artifact missing"));
        m_detailState->setAccent(palette::Danger);
    } else if (active) {
        m_detailState->setText(QStringLiteral("Active detector"));
        m_detailState->setAccent(palette::Accent);
    } else {
        m_detailState->setText(QStringLiteral("Ready"));
        m_detailState->setAccent(palette::Success);
    }

    m_detailBackend->setText(QString::fromStdString(manifest.backend));
    m_detailPrecision->setText(manifest.precision.empty()
                                   ? QStringLiteral("—")
                                   : QString::fromStdString(manifest.precision));
    m_detailInput->setText(
        QStringLiteral("%1 x %2").arg(manifest.inputWidth).arg(manifest.inputHeight));
    m_detailArtifact->setText(entry->isUsable()
                                  ? QStringLiteral("%1  ·  %2")
                                        .arg(QFileInfo(entry->artifactPath).fileName(),
                                             formatBytes(entry->artifactBytes))
                                  : QStringLiteral("not found"));
    m_detailArtifact->setToolTip(entry->artifactPath);

    if (manifest.classes.empty()) {
        m_detailClasses->setText(QStringLiteral("None declared"));
    } else {
        QStringList classes;
        for (const auto& className : manifest.classes)
            classes << QString::fromStdString(className);
        m_detailClasses->setText(QStringLiteral("%1  (%2)")
                                     .arg(classes.join(QStringLiteral(", ")))
                                     .arg(classes.size()));
    }

    QVector<BarRow> metrics;
    for (const auto& [name, value] : manifest.metrics) {
        BarRow row;
        row.badge = QString::fromStdString(name);
        row.color = typeColor(manifest.type);
        // Manifest metrics are conventionally 0..1 scores; anything larger is
        // still drawn, scaled against the largest value present.
        row.value = value;
        row.valueText = QString::number(value, 'f', 3);
        metrics.push_back(std::move(row));
    }
    m_detailMetrics->setRows(std::move(metrics));

    // Only a detector with a real artifact can be made active, and there is no
    // point offering to re-select the one already in use.
    const bool activatable =
        manifest.type == models::ModelType::Detector && entry->isUsable() && !active;
    m_activate->setEnabled(activatable);
    if (active)
        m_activate->setText(QStringLiteral("Already the active detector"));
    else if (manifest.type != models::ModelType::Detector)
        m_activate->setText(QStringLiteral("Only detectors can be activated"));
    else if (!entry->isUsable())
        m_activate->setText(QStringLiteral("Artifact missing"));
    else
        m_activate->setText(QStringLiteral("Set as active detector"));
}
