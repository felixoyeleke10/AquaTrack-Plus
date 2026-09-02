#include "ui/review/right/InsightPanel.h"

#include "ui/AppTheme.h"
#include "ui/common/Charts.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>

using namespace aquatrack::ui;
namespace analytics = aquatrack::analytics;

namespace {

QString shortClock(qint64 milliseconds)
{
    const qint64 seconds = std::max<qint64>(0, milliseconds) / 1000;
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QColor severityColor(analytics::EventSeverity severity)
{
    switch (severity) {
    case analytics::EventSeverity::Alert:
        return palette::Danger;
    case analytics::EventSeverity::Warning:
        return palette::Warning;
    case analytics::EventSeverity::Info:
        break;
    }
    return palette::Accent;
}

}  // namespace

InsightPanel::InsightPanel(QWidget* parent) : QFrame(parent)
{
    setFixedWidth(352);
    setStyleSheet(QStringLiteral("background:transparent"));

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    auto* column = new QVBoxLayout(content);
    column->setContentsMargins(0, 0, 6, 0);
    column->setSpacing(10);

    // ---- Survey summary ----------------------------------------------
    column->addWidget(sectionLabel(QStringLiteral("Survey summary"), content));
    auto* summaryCard = card(content);
    auto* summaryBody = new QVBoxLayout(summaryCard);
    summaryBody->setContentsMargins(12, 12, 12, 12);
    summaryBody->setSpacing(9);

    auto* tiles = new QHBoxLayout;
    tiles->setContentsMargins(0, 0, 0, 0);
    tiles->setSpacing(6);
    m_observationTile = new MetricTile(QStringLiteral("Observations"), summaryCard);
    m_trackTile = new MetricTile(QStringLiteral("Unique tracks"), summaryCard);
    m_peakTile = new MetricTile(QStringLiteral("Peak in frame"), summaryCard);
    tiles->addWidget(m_observationTile);
    tiles->addWidget(m_trackTile);
    tiles->addWidget(m_peakTile);
    summaryBody->addLayout(tiles);

    summaryBody->addWidget(divider(summaryCard));
    auto* dominantRow = new QHBoxLayout;
    dominantRow->setContentsMargins(0, 0, 0, 0);
    dominantRow->addWidget(typedLabel(QStringLiteral("Dominant species"), "dim", summaryCard));
    dominantRow->addStretch();
    m_dominantSpecies = typedLabel(QStringLiteral("—"), "small", summaryCard);
    m_dominantSpecies->setStyleSheet(
        QStringLiteral("font-weight:600;color:%1").arg(palette::TextPrimary.name()));
    dominantRow->addWidget(m_dominantSpecies);
    summaryBody->addLayout(dominantRow);
    column->addWidget(summaryCard);

    // ---- Notable events ----------------------------------------------
    column->addWidget(sectionLabel(QStringLiteral("Notable events"), content));
    auto* eventCard = card(content);
    m_eventList = new QVBoxLayout(eventCard);
    m_eventList->setContentsMargins(12, 10, 12, 10);
    m_eventList->setSpacing(2);
    m_eventEmpty = typedLabel(
        QStringLiteral("Derived events appear once a sequence is open."), "dim", eventCard);
    m_eventEmpty->setWordWrap(true);
    m_eventList->addWidget(m_eventEmpty);
    column->addWidget(eventCard);

    // ---- Identity switch candidate ------------------------------------
    m_switchCard = card(content, "alert");
    auto* switchBody = new QVBoxLayout(m_switchCard);
    switchBody->setContentsMargins(12, 10, 12, 11);
    switchBody->setSpacing(8);

    auto* switchHead = new QHBoxLayout;
    switchHead->setContentsMargins(0, 0, 0, 0);
    switchHead->addWidget(sectionLabel(QStringLiteral("Possible ID switch"), m_switchCard));
    switchHead->addStretch();
    m_switchBadge = new Badge(m_switchCard);
    m_switchBadge->setAccent(palette::Warning);
    switchHead->addWidget(m_switchBadge);
    switchBody->addLayout(switchHead);

    m_switchSummary = typedLabel(QStringLiteral("—"), "subtitle", m_switchCard);
    switchBody->addWidget(m_switchSummary);

    auto* stats = new QHBoxLayout;
    stats->setContentsMargins(0, 0, 0, 0);
    stats->setSpacing(14);
    auto* gapColumn = new QVBoxLayout;
    gapColumn->setSpacing(0);
    gapColumn->addWidget(typedLabel(QStringLiteral("Time gap"), "dim", m_switchCard));
    m_switchGap = typedLabel(QStringLiteral("—"), "small", m_switchCard);
    gapColumn->addWidget(m_switchGap);
    auto* similarityColumn = new QVBoxLayout;
    similarityColumn->setSpacing(0);
    similarityColumn->addWidget(typedLabel(QStringLiteral("Similarity"), "dim", m_switchCard));
    m_switchSimilarity = typedLabel(QStringLiteral("—"), "small", m_switchCard);
    similarityColumn->addWidget(m_switchSimilarity);
    stats->addLayout(gapColumn);
    stats->addLayout(similarityColumn);
    m_switchSparkline = new Sparkline(m_switchCard);
    m_switchSparkline->setColor(palette::Warning);
    m_switchSparkline->setMinimumWidth(80);
    stats->addWidget(m_switchSparkline, 1);
    switchBody->addLayout(stats);

    auto* actions = new QHBoxLayout;
    actions->setContentsMargins(0, 0, 0, 0);
    actions->setSpacing(6);
    m_merge = textButton(QStringLiteral("Merge tracks"), m_switchCard, "primary");
    m_merge->setIcon(glyphIcon(Glyph::Merge, 15, Qt::white, Qt::white));
    m_reviewClip = textButton(QStringLiteral("Go to frame"), m_switchCard);
    actions->addWidget(m_merge);
    actions->addWidget(m_reviewClip);
    switchBody->addLayout(actions);

    m_switchRemaining = typedLabel(QString(), "dim", m_switchCard);
    switchBody->addWidget(m_switchRemaining);

    connect(m_merge, &QPushButton::clicked, this, [this] {
        if (m_candidate)
            emit mergeTracksRequested(m_candidate->endingTrack, m_candidate->startingTrack);
    });
    connect(m_reviewClip, &QPushButton::clicked, this, [this] {
        if (m_candidate)
            emit frameRequested(int(m_candidate->atMs));  // translated by the page
    });
    m_switchCard->hide();
    column->addWidget(m_switchCard);

    // ---- Confidence trend ---------------------------------------------
    column->addWidget(sectionLabel(QStringLiteral("Confidence over time"), content));
    auto* confidenceCard = card(content);
    auto* confidenceBody = new QVBoxLayout(confidenceCard);
    confidenceBody->setContentsMargins(12, 10, 12, 10);
    m_confidenceChart = new LineChart(confidenceCard);
    m_confidenceChart->setEmptyText(QStringLiteral("Open a sequence"));
    m_confidenceChart->setMinimumHeight(92);
    confidenceBody->addWidget(m_confidenceChart);
    column->addWidget(confidenceCard);

    // ---- Frame annotations --------------------------------------------
    column->addWidget(sectionLabel(QStringLiteral("Annotations in this frame"), content));
    auto* editCard = card(content);
    auto* editBody = new QVBoxLayout(editCard);
    editBody->setContentsMargins(12, 10, 12, 11);
    editBody->setSpacing(8);

    m_editHint = typedLabel(
        QStringLiteral("Create or open a project so edits have somewhere durable to go."),
        "dim", editCard);
    m_editHint->setWordWrap(true);
    editBody->addWidget(m_editHint);

    m_observations = new QListWidget(editCard);
    m_observations->setMinimumHeight(110);
    editBody->addWidget(m_observations);

    auto* editActions = new QGridLayout;
    editActions->setContentsMargins(0, 0, 0, 0);
    editActions->setSpacing(6);
    m_relabel = textButton(QStringLiteral("Relabel"), editCard);
    m_delete = textButton(QStringLiteral("Delete"), editCard);
    m_undo = textButton(QStringLiteral("Undo"), editCard);
    m_redo = textButton(QStringLiteral("Redo"), editCard);
    m_save = textButton(QStringLiteral("Save annotations"), editCard, "primary");
    m_undo->setIcon(glyphIcon(Glyph::Undo, 15, palette::TextSecondary, palette::TextPrimary));
    m_redo->setIcon(glyphIcon(Glyph::Redo, 15, palette::TextSecondary, palette::TextPrimary));
    m_save->setIcon(glyphIcon(Glyph::Save, 15, Qt::white, Qt::white));
    editActions->addWidget(m_relabel, 0, 0);
    editActions->addWidget(m_delete, 0, 1);
    editActions->addWidget(m_undo, 1, 0);
    editActions->addWidget(m_redo, 1, 1);
    editActions->addWidget(m_save, 2, 0, 1, 2);
    editBody->addLayout(editActions);
    column->addWidget(editCard);

    connect(m_relabel, &QPushButton::clicked, this, [this] {
        const auto id = selectedObservationId();
        if (!id.isEmpty())
            emit relabelRequested(id);
    });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        const auto id = selectedObservationId();
        if (!id.isEmpty())
            emit deleteRequested(id);
    });
    connect(m_undo, &QPushButton::clicked, this, &InsightPanel::undoRequested);
    connect(m_redo, &QPushButton::clicked, this, &InsightPanel::redoRequested);
    connect(m_save, &QPushButton::clicked, this, &InsightPanel::saveRequested);

    setEditingEnabled(false);
    column->addStretch();
    scroll->setWidget(content);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
}

QString InsightPanel::selectedObservationId() const
{
    auto* item = m_observations->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void InsightPanel::setSummary(const analytics::SurveySummary& summary, std::size_t peakConcurrent)
{
    m_observationTile->setValue(QString::number(summary.totalObservations));
    m_trackTile->setValue(QString::number(summary.uniqueLocalTracks));
    m_peakTile->setValue(QString::number(peakConcurrent));

    QString dominant = QStringLiteral("—");
    std::size_t best = 0;
    for (const auto& [name, count] : summary.observationsByClass) {
        if (count > best) {
            best = count;
            dominant = QString::fromStdString(name);
        }
    }
    m_dominantSpecies->setText(dominant);
}

void InsightPanel::setEvents(const std::vector<analytics::SurveyEvent>& events)
{
    // Drop the previous rows, keeping the placeholder for the empty case.
    while (m_eventList->count() > 0) {
        auto* item = m_eventList->takeAt(0);
        if (item->widget() && item->widget() != m_eventEmpty)
            item->widget()->deleteLater();
        delete item;
    }
    if (events.empty()) {
        m_eventEmpty->show();
        m_eventList->addWidget(m_eventEmpty);
        return;
    }
    m_eventEmpty->hide();
    auto* container = qobject_cast<QWidget*>(m_eventList->parentWidget());
    for (const auto& event : events) {
        m_eventList->addWidget(new EventRow(severityColor(event.severity),
                                            QString::fromStdString(event.description),
                                            shortClock(event.atMs), container));
    }
}

void InsightPanel::setIdSwitch(const std::optional<analytics::IdSwitchCandidate>& candidate,
                               int remaining)
{
    m_candidate = candidate;
    if (!candidate) {
        m_switchCard->hide();
        return;
    }
    m_switchCard->show();
    m_switchSummary->setText(QStringLiteral("Track %1  →  Track %2")
                                 .arg(candidate->endingTrack)
                                 .arg(candidate->startingTrack));
    m_switchGap->setText(QStringLiteral("%1 s").arg(candidate->gapMs / 1000.0, 0, 'f', 1));
    m_switchSimilarity->setText(QStringLiteral("%1%").arg(candidate->similarity * 100.0, 0, 'f', 0));
    m_switchBadge->setText(candidate->similarity > 0.8 ? QStringLiteral("High confidence")
                                                       : QStringLiteral("Worth a look"));
    m_switchBadge->setAccent(candidate->similarity > 0.8 ? palette::Warning : palette::TextDim);
    m_switchRemaining->setText(remaining > 1
                                   ? QStringLiteral("%1 more candidate(s) in this sequence")
                                         .arg(remaining - 1)
                                   : QStringLiteral("No other candidates in this sequence"));

    // A short synthetic ramp of the similarity, purely as a visual cue that
    // the two tracks converge; there is no per-frame similarity series yet.
    QVector<double> shape;
    for (int i = 0; i < 24; ++i)
        shape.push_back(candidate->similarity * (0.55 + 0.45 * i / 23.0));
    m_switchSparkline->setValues(shape);
}

void InsightPanel::setConfidenceSeries(const QVector<double>& values, double latest)
{
    ChartSeries series;
    series.name = QStringLiteral("Mean confidence");
    series.color = palette::Accent;
    series.filled = true;
    series.values = values;
    m_confidenceChart->setSeries({series});
    m_confidenceChart->setValueBadge(values.isEmpty() ? QString()
                                                      : QString::number(latest, 'f', 2));
}

void InsightPanel::setCurrentObservations(
    const std::vector<aquatrack::domain::Observation>& observations)
{
    const auto previous = selectedObservationId();
    m_observations->clear();
    for (const auto& observation : observations) {
        auto* item = new QListWidgetItem(
            QStringLiteral("ID %1   %2   %3")
                .arg(observation.localTrackId)
                .arg(QString::fromStdString(observation.className))
                .arg(observation.confidence, 0, 'f', 2));
        item->setData(Qt::UserRole, QString::fromStdString(observation.id));
        item->setForeground(trackColor(int(observation.localTrackId)));
        m_observations->addItem(item);
        if (QString::fromStdString(observation.id) == previous)
            m_observations->setCurrentItem(item);
    }
}

void InsightPanel::setEditingEnabled(bool enabled)
{
    m_editingEnabled = enabled;
    for (auto* button : {m_relabel, m_delete, m_save})
        button->setEnabled(enabled);
    m_observations->setEnabled(enabled);
    m_editHint->setVisible(!enabled);
}

void InsightPanel::setHistoryState(bool canUndo, bool canRedo, bool dirty)
{
    m_undo->setEnabled(m_editingEnabled && canUndo);
    m_redo->setEnabled(m_editingEnabled && canRedo);
    m_save->setEnabled(m_editingEnabled && dirty);
    m_save->setText(dirty ? QStringLiteral("Save annotations •")
                          : QStringLiteral("Save annotations"));
}
