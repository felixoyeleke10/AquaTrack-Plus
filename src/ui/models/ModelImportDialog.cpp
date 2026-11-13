#include "ui/models/ModelImportDialog.h"

#include "ui/AppTheme.h"
#include "ui/common/Widgets.h"
#include "ui/common/WindowChrome.h"

#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

using namespace aquatrack::ui;
using aquatrack::application::ModelCandidate;

namespace {

QString percent(const ModelCandidate& candidate, const char* key)
{
    const auto found = candidate.metrics.constFind(QLatin1String(key));
    return found == candidate.metrics.cend() ? QStringLiteral("—")
                                             : QString::number(found.value() * 100.0, 'f', 1) +
                                                   QLatin1Char('%');
}

}  // namespace

ModelImportDialog::ModelImportDialog(QVector<ModelCandidate> candidates, const QString& destination,
                                     QWidget* parent)
    : QDialog(parent), m_candidates(std::move(candidates))
{
    setObjectName(QStringLiteral("modelImportDialog"));
    setWindowTitle(QStringLiteral("Import models"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(940, 520);

    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(18, 16, 18, 16);
    page->setSpacing(12);
    page->addWidget(typedLabel(QStringLiteral("Import trained models"), "title", this));
    auto* intro = typedLabel(
        QStringLiteral("Each selected model's weights are copied into %1 with a model.json "
                       "describing it, so the project carries its own detectors. Scores are "
                       "from each run's benchmark, where one was recorded.")
            .arg(QDir::toNativeSeparators(destination)),
        "small", this);
    intro->setWordWrap(true);
    page->addWidget(intro);

    m_table = new QTableWidget(int(m_candidates.size()), 7, this);
    m_table->setObjectName(QStringLiteral("modelCandidates"));
    m_table->setHorizontalHeaderLabels({QStringLiteral("Model"), QStringLiteral("Backend"),
                                        QStringLiteral("mAP@50"), QStringLiteral("mAP@50-95"),
                                        QStringLiteral("Classes"), QStringLiteral("Input"),
                                        QStringLiteral("Size")});
    m_table->verticalHeader()->setVisible(false);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 7; ++column)
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);

    const bool anyBenchmarked = std::any_of(m_candidates.cbegin(), m_candidates.cend(),
                                            [](const auto& item) { return !item.metrics.isEmpty(); });
    for (int row = 0; row < m_candidates.size(); ++row) {
        const auto& candidate = m_candidates.at(row);
        auto* name = new QTableWidgetItem(candidate.name);
        name->setFlags(name->flags() | Qt::ItemIsUserCheckable);
        // Preselect the runs worth having: benchmarked (if any are), not pilots.
        const bool wanted = !candidate.pilot && (!anyBenchmarked || !candidate.metrics.isEmpty());
        name->setCheckState(wanted ? Qt::Checked : Qt::Unchecked);
        name->setToolTip(QDir::toNativeSeparators(candidate.weights));
        if (candidate.pilot) {
            name->setText(candidate.name + QStringLiteral("  ·  pilot run"));
            name->setForeground(palette::TextDim);
        }
        m_table->setItem(row, 0, name);
        m_table->setItem(row, 1, new QTableWidgetItem(candidate.backend));
        auto* map50 = new QTableWidgetItem(percent(candidate, "map50"));
        map50->setForeground(candidate.metrics.isEmpty() ? palette::TextDim : palette::AccentHover);
        m_table->setItem(row, 2, map50);
        m_table->setItem(row, 3, new QTableWidgetItem(percent(candidate, "map50_95")));
        m_table->setItem(row, 4,
                         new QTableWidgetItem(candidate.classes.isEmpty()
                                                  ? QStringLiteral("unknown")
                                                  : candidate.classes.join(QStringLiteral(", "))));
        m_table->setItem(row, 5, new QTableWidgetItem(QStringLiteral("%1 px").arg(candidate.inputSize)));
        m_table->setItem(row, 6,
                         new QTableWidgetItem(QLocale::system().formattedDataSize(candidate.bytes)));
    }
    connect(m_table, &QTableWidget::itemChanged, this, &ModelImportDialog::updateSummary);
    page->addWidget(m_table, 1);

    auto* footer = new QHBoxLayout;
    m_summary = typedLabel(QString(), "dim", this);
    footer->addWidget(m_summary, 1);
    auto* cancel = textButton(QStringLiteral("Cancel"), this);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    m_import = textButton(QStringLiteral("Import"), this, "primary");
    m_import->setObjectName(QStringLiteral("importModels"));
    connect(m_import, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(cancel);
    footer->addWidget(m_import);
    page->addLayout(footer);

    updateSummary();
    useDarkTitleBar(this);
}

void ModelImportDialog::setChecked(int row, bool checked)
{
    if (auto* item = m_table->item(row, 0))
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

QVector<ModelCandidate> ModelImportDialog::selected() const
{
    QVector<ModelCandidate> chosen;
    for (int row = 0; row < m_candidates.size(); ++row)
        if (m_table->item(row, 0)->checkState() == Qt::Checked)
            chosen.push_back(m_candidates.at(row));
    return chosen;
}

void ModelImportDialog::updateSummary()
{
    const auto chosen = selected();
    qint64 bytes = 0;
    for (const auto& candidate : chosen)
        bytes += candidate.bytes;
    m_summary->setText(chosen.isEmpty()
                           ? QStringLiteral("Nothing selected")
                           : QStringLiteral("%1 model(s), %2 to copy")
                                 .arg(chosen.size())
                                 .arg(QLocale::system().formattedDataSize(bytes)));
    m_import->setEnabled(!chosen.isEmpty());
    m_import->setText(chosen.size() > 1 ? QStringLiteral("Import %1 models").arg(chosen.size())
                                        : QStringLiteral("Import"));
}
