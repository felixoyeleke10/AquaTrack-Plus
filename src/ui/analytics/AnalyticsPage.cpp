#include "ui/analytics/AnalyticsPage.h"

#include "analytics/SurveyAnalytics.h"
#include "ui/AppTheme.h"
#include "ui/common/Charts.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

using namespace aquatrack::ui;
namespace analytics = aquatrack::analytics;

namespace {

QString clock(qint64 milliseconds)
{
    const qint64 seconds = std::max<qint64>(0, milliseconds) / 1000;
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

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

QFrame* chartCard(const QString& title, QWidget* body, QWidget* parent)
{
    auto* frame = card(parent);
    auto* column = new QVBoxLayout(frame);
    column->setContentsMargins(14, 12, 14, 12);
    column->setSpacing(8);
    column->addWidget(sectionLabel(title, frame));
    body->setParent(frame);
    column->addWidget(body, 1);
    return frame;
}

}  // namespace

AnalyticsPage::AnalyticsPage(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* host = new QWidget(scroll);
    auto* page = new QVBoxLayout(host);
    page->setContentsMargins(24, 20, 24, 20);
    page->setSpacing(12);

    // ---- Heading -------------------------------------------------------
    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    auto* titles = new QVBoxLayout;
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(1);
    titles->addWidget(sectionLabel(QStringLiteral("Survey analytics"), host));
    titles->addWidget(typedLabel(QStringLiteral("Observation summary"), "display", host));
    m_source = typedLabel(QStringLiteral("No observations loaded"), "dim", host);
    titles->addWidget(m_source);
    head->addLayout(titles);
    head->addStretch();

    m_export = textButton(QStringLiteral("Export summary CSV"), host, "primary");
    m_export->setIcon(glyphIcon(Glyph::Export, 15, Qt::white, Qt::white));
    m_export->setEnabled(false);
    connect(m_export, &QPushButton::clicked, this, &AnalyticsPage::exportSummaryRequested);
    head->addWidget(m_export, 0, Qt::AlignBottom);
    page->addLayout(head);

    m_status = typedLabel(
        QStringLiteral("Open a sequence in Review to populate these analytics."), "small", host);
    m_status->setWordWrap(true);
    page->addWidget(m_status);

    // ---- Metric tiles ---------------------------------------------------
    auto* tileCard = card(host);
    auto* tiles = new QHBoxLayout(tileCard);
    tiles->setContentsMargins(16, 14, 16, 14);
    tiles->setSpacing(10);
    m_totalObservations = new MetricTile(QStringLiteral("Observations"), tileCard);
    m_uniqueTracks = new MetricTile(QStringLiteral("Unique tracks"), tileCard);
    m_speciesCount = new MetricTile(QStringLiteral("Species"), tileCard);
    m_peakInFrame = new MetricTile(QStringLiteral("Peak in frame"), tileCard);
    m_meanConfidence = new MetricTile(QStringLiteral("Mean confidence"), tileCard);
    m_duration = new MetricTile(QStringLiteral("Survey span"), tileCard);
    for (auto* tile : {m_totalObservations, m_uniqueTracks, m_speciesCount, m_peakInFrame,
                       m_meanConfidence, m_duration})
        tiles->addWidget(tile);
    page->addWidget(tileCard);

    // ---- Charts ----------------------------------------------------------
    auto* charts = new QGridLayout;
    charts->setContentsMargins(0, 0, 0, 0);
    charts->setSpacing(12);

    m_abundance = new LineChart(host);
    m_abundance->setEmptyText(QStringLiteral("No observations"));
    m_abundance->setLegendVisible(true);
    m_abundance->setPeakCallout(true);
    m_abundance->setMinimumHeight(170);
    charts->addWidget(chartCard(QStringLiteral("Abundance over time"), m_abundance, host), 0, 0);

    m_composition = new DonutChart(host);
    m_composition->setMinimumHeight(170);
    charts->addWidget(chartCard(QStringLiteral("Species composition"), m_composition, host), 0, 1);

    m_durations = new BarList(host);
    m_durations->setEmptyText(QStringLiteral("No tracks"));
    m_durations->setMinimumHeight(150);
    charts->addWidget(
        chartCard(QStringLiteral("Track duration distribution"), m_durations, host), 1, 0);

    m_confidence = new LineChart(host);
    m_confidence->setEmptyText(QStringLiteral("No observations"));
    m_confidence->setMinimumHeight(150);
    charts->addWidget(chartCard(QStringLiteral("Mean confidence over time"), m_confidence, host),
                      1, 1);
    charts->setColumnStretch(0, 1);
    charts->setColumnStretch(1, 1);
    page->addLayout(charts);

    // ---- Species table and events ----------------------------------------
    auto* lower = new QHBoxLayout;
    lower->setContentsMargins(0, 0, 0, 0);
    lower->setSpacing(12);

    auto* speciesCard = card(host);
    auto* speciesBody = new QVBoxLayout(speciesCard);
    speciesBody->setContentsMargins(14, 12, 14, 13);
    speciesBody->setSpacing(8);
    speciesBody->addWidget(sectionLabel(QStringLiteral("Species breakdown"), speciesCard));
    m_speciesTable = new QTableWidget(0, 5, speciesCard);
    m_speciesTable->setHorizontalHeaderLabels(
        {QStringLiteral("Species"), QStringLiteral("Observations"), QStringLiteral("Share"),
         QStringLiteral("Tracks"), QStringLiteral("Mean conf.")});
    m_speciesTable->verticalHeader()->setVisible(false);
    m_speciesTable->setShowGrid(false);
    m_speciesTable->setAlternatingRowColors(true);
    m_speciesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_speciesTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_speciesTable->horizontalHeader()->setHighlightSections(false);
    m_speciesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 5; ++column)
        m_speciesTable->horizontalHeader()->setSectionResizeMode(column,
                                                                 QHeaderView::ResizeToContents);
    m_speciesTable->setMinimumHeight(180);
    speciesBody->addWidget(m_speciesTable);
    lower->addWidget(speciesCard, 3);

    auto* eventCard = card(host);
    eventCard->setMinimumWidth(320);
    m_eventList = new QVBoxLayout(eventCard);
    m_eventList->setContentsMargins(14, 12, 14, 13);
    m_eventList->setSpacing(3);
    m_eventList->addWidget(sectionLabel(QStringLiteral("Notable events"), eventCard));
    m_eventEmpty =
        typedLabel(QStringLiteral("Derived events appear once observations are loaded."), "dim",
                   eventCard);
    m_eventEmpty->setWordWrap(true);
    m_eventList->addWidget(m_eventEmpty);
    m_eventList->addStretch();
    lower->addWidget(eventCard, 2);
    page->addLayout(lower);

    page->addStretch();
    scroll->setWidget(host);
    outer->addWidget(scroll);

    clearAll();
}

void AnalyticsPage::clearAll()
{
    for (auto* tile : {m_totalObservations, m_uniqueTracks, m_speciesCount, m_peakInFrame,
                       m_meanConfidence, m_duration})
        tile->setValue(QStringLiteral("—"));
    m_abundance->setSeries({});
    m_composition->setSlices({});
    m_durations->setRows({});
    m_confidence->setSeries({});
    m_confidence->setValueBadge(QString());
    m_speciesTable->setRowCount(0);
    m_export->setEnabled(false);
}

void AnalyticsPage::setObservations(
    const std::vector<aquatrack::domain::Observation>& observations, const QString& sourceName)
{
    m_observations = observations;
    m_source->setText(sourceName.isEmpty() ? QStringLiteral("No observations loaded")
                                           : sourceName);

    // Rebuild the event rows, keeping the placeholder for the empty case.
    while (m_eventList->count() > 1) {
        auto* item = m_eventList->takeAt(1);
        if (item->widget() && item->widget() != m_eventEmpty)
            item->widget()->deleteLater();
        delete item;
    }

    if (observations.empty()) {
        clearAll();
        m_status->setText(
            QStringLiteral("Open a sequence in Review to populate these analytics."));
        m_eventEmpty->show();
        m_eventList->addWidget(m_eventEmpty);
        m_eventList->addStretch();
        return;
    }

    const auto summary = analytics::summarize(observations);
    const auto species = analytics::speciesBreakdown(observations);
    const auto peak = analytics::peakConcurrentTracks(observations);
    const auto spanMs = summary.lastObservationMs - summary.firstObservationMs;
    const auto binWidth = std::max<aquatrack::domain::TimestampMs>(1, spanMs / 40);
    const auto bins = analytics::binObservations(observations, binWidth);
    const auto events = analytics::notableEvents(observations, binWidth);
    const auto buckets = analytics::trackDurationHistogram(observations);

    m_status->setText(QStringLiteral(
        "Derived from %1 observations. Counts are of visual detections, not absolute abundance.")
                          .arg(QLocale::system().toString(qlonglong(summary.totalObservations))));
    m_export->setEnabled(true);

    m_totalObservations->setValue(
        QLocale::system().toString(qlonglong(summary.totalObservations)));
    m_uniqueTracks->setValue(QString::number(summary.uniqueLocalTracks));
    m_speciesCount->setValue(QString::number(species.size()));
    m_peakInFrame->setValue(QString::number(peak));
    m_meanConfidence->setValue(QString::number(summary.meanConfidence, 'f', 2));
    m_duration->setValue(clock(spanMs));

    // ---- Abundance ------------------------------------------------------
    ChartSeries tracks;
    tracks.name = QStringLiteral("Unique tracks");
    tracks.color = palette::Accent;
    tracks.filled = true;
    ChartSeries detections;
    detections.name = QStringLiteral("Observations");
    detections.color = QColor(0xa7, 0x8b, 0xfa);
    for (const auto& bin : bins) {
        tracks.values.push_back(double(bin.uniqueTracks));
        detections.values.push_back(double(bin.observations));
    }
    m_abundance->setSeries({detections, tracks});
    if (!bins.empty())
        m_abundance->setAxisLabels(
            {shortClock(bins.front().startMs), shortClock(bins.back().endMs)});

    // ---- Composition -----------------------------------------------------
    QVector<DonutSlice> slices;
    for (int index = 0; index < int(species.size()); ++index) {
        DonutSlice slice;
        slice.name = QString::fromStdString(species.at(std::size_t(index)).className);
        slice.value = double(species.at(std::size_t(index)).observations);
        slice.color = speciesColor((index % 6) + 1);
        slices.push_back(std::move(slice));
    }
    m_composition->setSlices(slices);
    m_composition->setCenterCaption(QString::number(summary.totalObservations),
                                    QStringLiteral("total"));

    // ---- Track durations --------------------------------------------------
    QVector<BarRow> durationRows;
    for (const auto& bucket : buckets) {
        BarRow row;
        // Sub-second precision, otherwise adjacent buckets round to
        // overlapping ranges such as "1-3s" followed by "2-5s".
        row.badge = QStringLiteral("%1-%2s")
                        .arg(bucket.lowerMs / 1000.0, 0, 'f', 1)
                        .arg(bucket.upperMs / 1000.0, 0, 'f', 1);
        row.color = palette::Accent;
        row.value = double(bucket.tracks);
        row.valueText = QStringLiteral("%1 track(s)").arg(bucket.tracks);
        durationRows.push_back(std::move(row));
    }
    m_durations->setRows(durationRows);

    // ---- Confidence -------------------------------------------------------
    std::vector<double> confidenceSum(bins.size(), 0.0);
    std::vector<std::size_t> confidenceCount(bins.size(), 0);
    for (const auto& observation : observations) {
        const auto index = std::size_t((observation.timestampMs - summary.firstObservationMs)
                                       / std::max<aquatrack::domain::TimestampMs>(1, binWidth));
        if (index < confidenceSum.size()) {
            confidenceSum[index] += observation.confidence;
            ++confidenceCount[index];
        }
    }
    ChartSeries confidence;
    confidence.name = QStringLiteral("Mean confidence");
    confidence.color = palette::Success;
    confidence.filled = true;
    for (std::size_t index = 0; index < confidenceSum.size(); ++index)
        confidence.values.push_back(
            confidenceCount[index] == 0 ? 0.0 : confidenceSum[index] / double(confidenceCount[index]));
    m_confidence->setSeries({confidence});
    m_confidence->setValueBadge(QString::number(summary.meanConfidence, 'f', 2));
    if (!bins.empty())
        m_confidence->setAxisLabels(
            {shortClock(bins.front().startMs), shortClock(bins.back().endMs)});

    // ---- Species table -----------------------------------------------------
    m_speciesTable->setRowCount(int(species.size()));
    for (int row = 0; row < int(species.size()); ++row) {
        const auto& stats = species.at(std::size_t(row));
        auto* name = new QTableWidgetItem(QString::fromStdString(stats.className));
        name->setForeground(speciesColor((row % 6) + 1));
        m_speciesTable->setItem(row, 0, name);
        m_speciesTable->setItem(
            row, 1, new QTableWidgetItem(QLocale::system().toString(qlonglong(stats.observations))));
        m_speciesTable->setItem(
            row, 2,
            new QTableWidgetItem(QStringLiteral("%1%").arg(
                100.0 * double(stats.observations) / double(summary.totalObservations), 0, 'f', 1)));
        m_speciesTable->setItem(row, 3,
                                new QTableWidgetItem(QString::number(stats.uniqueTracks)));
        m_speciesTable->setItem(
            row, 4, new QTableWidgetItem(QString::number(stats.meanConfidence, 'f', 3)));
    }

    // ---- Events -------------------------------------------------------------
    if (events.empty()) {
        m_eventEmpty->setText(QStringLiteral("Nothing unusual was derived from this survey."));
        m_eventEmpty->show();
        m_eventList->addWidget(m_eventEmpty);
    } else {
        m_eventEmpty->hide();
        for (const auto& event : events)
            m_eventList->addWidget(new EventRow(severityColor(event.severity),
                                                QString::fromStdString(event.description),
                                                shortClock(event.atMs), m_eventList->parentWidget()));
    }
    m_eventList->addStretch();
}
