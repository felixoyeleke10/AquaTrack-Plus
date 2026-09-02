#include "ui/review/center/ReviewPanel.h"

#include "ui/AppTheme.h"
#include "ui/common/Charts.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"
#include "ui/review/center/video/SequenceView.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QTabWidget>
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

// One analytics card: a titled panel wrapping a chart.
QFrame* chartCard(const QString& title, QWidget* body, QWidget* parent, QLabel** footer = nullptr)
{
    auto* frame = card(parent);
    auto* column = new QVBoxLayout(frame);
    column->setContentsMargins(12, 10, 12, 10);
    column->setSpacing(7);

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->addWidget(sectionLabel(title, frame));
    head->addStretch();
    if (footer) {
        *footer = typedLabel(QString(), "dim", frame);
        (*footer)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        head->addWidget(*footer);
    }
    column->addLayout(head);

    body->setParent(frame);
    column->addWidget(body, 1);
    return frame;
}

QTableWidget* buildTable(const QStringList& headers, QWidget* parent)
{
    auto* table = new QTableWidget(0, headers.size(), parent);
    table->setHorizontalHeaderLabels(headers);
    table->verticalHeader()->setVisible(false);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setHighlightSections(false);
    return table;
}

}  // namespace

ReviewPanel::ReviewPanel(QWidget* parent) : QWidget(parent)
{
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    // The frame and the material below it compete for the same vertical
    // space, and how much each deserves depends on the task: scrubbing wants
    // a large frame, auditing tracks wants a large table. A splitter lets the
    // reviewer decide, and stops the transport bar being squeezed out when
    // the window is short.
    auto* split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);
    split->setHandleWidth(10);
    column->addWidget(split);

    // ---- Canvas ------------------------------------------------------
    auto* canvasCard = card(split);
    auto* canvasBody = new QVBoxLayout(canvasCard);
    canvasBody->setContentsMargins(1, 1, 1, 1);
    canvasBody->setSpacing(0);
    m_view = new SequenceView(canvasCard);
    canvasBody->addWidget(m_view, 1);
    canvasBody->addWidget(buildTransport());
    canvasCard->setMinimumHeight(240);
    split->addWidget(canvasCard);

    auto* lower = new QWidget(split);
    auto* lowerColumn = new QVBoxLayout(lower);
    lowerColumn->setContentsMargins(0, 0, 0, 0);
    lowerColumn->setSpacing(10);

    // ---- Sequence timeline -------------------------------------------
    auto* timelineCard = card(lower);
    auto* timelineBody = new QVBoxLayout(timelineCard);
    timelineBody->setContentsMargins(12, 6, 12, 8);
    timelineBody->setSpacing(4);

    // No heading: the strip is self-evident next to the transport, and the
    // row it needed cost more than it explained.
    m_timeline = new EventTimeline(timelineCard);
    connect(m_timeline, &EventTimeline::frameRequested, this, &ReviewPanel::frameRequested);
    timelineBody->addWidget(m_timeline);

    m_filmstrip = new Filmstrip(timelineCard);
    connect(m_filmstrip, &Filmstrip::frameRequested, this, &ReviewPanel::frameRequested);
    timelineBody->addWidget(m_filmstrip);
    lowerColumn->addWidget(timelineCard);

    // ---- Tables ------------------------------------------------------
    auto* tabCard = card(lower);
    auto* tabBody = new QVBoxLayout(tabCard);
    tabBody->setContentsMargins(10, 6, 10, 10);
    m_tabs = new QTabWidget(tabCard);
    m_tabs->setDocumentMode(true);
    m_trackTable = buildTable({QStringLiteral("Track"), QStringLiteral("Species"),
                               QStringLiteral("First"), QStringLiteral("Last"),
                               QStringLiteral("Frames"), QStringLiteral("Duration")},
                              m_tabs);
    m_eventTable = buildTable({QStringLiteral("Time"), QStringLiteral("Event")}, m_tabs);
    m_tabs->addTab(m_trackTable, QStringLiteral("TRACKS"));
    m_tabs->addTab(m_eventTable, QStringLiteral("EVENTS"));
    tabBody->addWidget(m_tabs);
    tabCard->setMinimumHeight(120);
    lowerColumn->addWidget(tabCard, 1);

    // ---- Analytics ---------------------------------------------------
    lowerColumn->addWidget(buildAnalyticsRow(lower));

    split->addWidget(lower);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
}

QWidget* ReviewPanel::buildTransport()
{
    auto* bar = new QWidget(this);
    bar->setFixedHeight(46);
    bar->setStyleSheet(QStringLiteral("background:%1;border-top:1px solid %2")
                           .arg(palette::Chrome.name(), palette::Line.name()));
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(12, 0, 12, 0);
    row->setSpacing(6);

    auto* back = iconButton(Glyph::Undo, QStringLiteral("Previous frame"), bar);
    connect(back, &QPushButton::clicked, this, [this] { emit stepRequested(-1); });
    row->addWidget(back);

    m_play = iconButton(Glyph::Play, QStringLiteral("Play or pause"), bar, "icon", 19);
    m_play->setEnabled(false);
    connect(m_play, &QPushButton::clicked, this, &ReviewPanel::playToggled);
    row->addWidget(m_play);

    auto* forward = iconButton(Glyph::Next, QStringLiteral("Next frame"), bar);
    connect(forward, &QPushButton::clicked, this, [this] { emit stepRequested(1); });
    row->addWidget(forward);
    row->addSpacing(8);

    m_clock = typedLabel(QStringLiteral("00:00:00 / 00:00:00"), "mono", bar);
    m_clock->setMinimumWidth(140);
    row->addWidget(m_clock);

    m_frameSlider = new QSlider(Qt::Horizontal, bar);
    m_frameSlider->setEnabled(false);
    connect(m_frameSlider, &QSlider::valueChanged, this, &ReviewPanel::frameRequested);
    row->addWidget(m_frameSlider, 1);

    row->addWidget(iconButton(Glyph::Camera, QStringLiteral("Snapshot (not yet wired)"), bar));
    row->addWidget(iconButton(Glyph::Fullscreen, QStringLiteral("Fullscreen (not yet wired)"), bar));
    return bar;
}

QWidget* ReviewPanel::buildAnalyticsRow(QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* grid = new QGridLayout(row);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);

    m_countChart = new LineChart(row);
    m_countChart->setEmptyText(QStringLiteral("Open a sequence"));
    m_countChart->setPeakCallout(true);
    m_countChart->setAxisCaptions(QStringLiteral("Tracks"));
    grid->addWidget(chartCard(QStringLiteral("Count over time"), m_countChart, row, &m_countTotal),
                    0, 0);

    m_speciesChart = new DonutChart(row);
    grid->addWidget(chartCard(QStringLiteral("Species breakdown"), m_speciesChart, row), 0, 1);

    m_activityChart = new LineChart(row);
    m_activityChart->setEmptyText(QStringLiteral("Open a sequence"));
    m_activityChart->setLegendVisible(true);
    grid->addWidget(chartCard(QStringLiteral("Observations vs tracks"), m_activityChart, row), 0, 2);

    m_topTracks = new BarList(row);
    grid->addWidget(chartCard(QStringLiteral("Longest tracks"), m_topTracks, row), 0, 3);

    for (int index = 0; index < 4; ++index)
        grid->setColumnStretch(index, 1);
    row->setFixedHeight(146);
    return row;
}

void ReviewPanel::configureSequence(int frames, int frameRate)
{
    m_totalFrames = frames;
    m_frameRate = std::max(1, frameRate);
    m_play->setEnabled(true);
    m_frameSlider->setEnabled(true);
    const QSignalBlocker blocker(m_frameSlider);
    m_frameSlider->setRange(1, std::max(1, frames));
    m_timeline->setSequenceLength(frames, m_frameRate);
}

void ReviewPanel::showFrame(const QImage& image, const QVector<aquatrack::GroundTruth>& annotations,
                            int frame, int total, qint64 timeMs)
{
    m_view->setFrame(image, annotations);
    m_view->setFrameInfo(frame, total, timeMs);
    {
        const QSignalBlocker blocker(m_frameSlider);
        m_frameSlider->setValue(frame);
    }
    m_timeline->setCurrentFrame(frame);
    m_filmstrip->setCurrentFrame(frame);
    m_clock->setText(QStringLiteral("%1 / %2").arg(
        clock(timeMs), clock(qint64(total) * 1000 / m_frameRate)));
}

void ReviewPanel::setPlaying(bool playing)
{
    m_play->setIcon(glyphIcon(playing ? Glyph::Pause : Glyph::Play, 19, palette::TextSecondary,
                              palette::Accent));
}

void ReviewPanel::setAnalytics(const std::vector<analytics::TimeBin>& bins,
                               const analytics::SurveySummary& summary,
                               const std::vector<analytics::TrackSpan>& spans, int frameRate)
{
    m_frameRate = std::max(1, frameRate);

    // Count over time.
    ChartSeries tracks;
    tracks.name = QStringLiteral("Unique tracks");
    tracks.color = palette::Accent;
    tracks.filled = true;
    ChartSeries observations;
    observations.name = QStringLiteral("Observations");
    observations.color = QColor(0xa7, 0x8b, 0xfa);
    for (const auto& bin : bins) {
        tracks.values.push_back(double(bin.uniqueTracks));
        observations.values.push_back(double(bin.observations));
    }
    m_countChart->setSeries({tracks});
    if (!bins.empty()) {
        m_countChart->setAxisLabels({shortClock(bins.front().startMs), shortClock(bins.back().endMs)});
        m_countTotal->setText(QStringLiteral("%1 bins").arg(bins.size()));
    }

    ChartSeries tracksCopy = tracks;
    tracksCopy.filled = false;
    m_activityChart->setSeries({observations, tracksCopy});
    if (!bins.empty())
        m_activityChart->setAxisLabels(
            {shortClock(bins.front().startMs), shortClock(bins.back().endMs)});

    // Species breakdown.
    QVector<DonutSlice> slices;
    for (const auto& [name, count] : summary.observationsByClass) {
        DonutSlice slice;
        slice.name = QString::fromStdString(name);
        slice.value = double(count);
        slices.push_back(slice);
    }
    std::sort(slices.begin(), slices.end(),
              [](const DonutSlice& left, const DonutSlice& right) { return left.value > right.value; });
    // Colour by rank so the largest slice is always the first palette entry
    // and the legend reads top-down in the same order as the ring.
    for (int index = 0; index < slices.size(); ++index)
        slices[index].color = speciesColor((index % 6) + 1);
    m_speciesChart->setSlices(slices);
    m_speciesChart->setCenterCaption(QString::number(summary.totalObservations),
                                     QStringLiteral("total"));

    // Longest tracks.
    QVector<BarRow> rows;
    for (const auto& span : spans) {
        if (rows.size() >= 5)
            break;
        BarRow row;
        row.badge = QStringLiteral("ID %1").arg(span.trackId);
        row.color = trackColor(int(span.trackId));
        row.value = double(span.durationMs());
        row.valueText = shortClock(span.durationMs());
        rows.push_back(row);
    }
    m_topTracks->setRows(rows);
}

void ReviewPanel::setTracksTable(const std::vector<analytics::TrackSpan>& spans)
{
    m_trackTable->setRowCount(int(spans.size()));
    for (int row = 0; row < int(spans.size()); ++row) {
        const auto& span = spans.at(std::size_t(row));
        auto* badge = new QTableWidgetItem(QStringLiteral("ID %1").arg(span.trackId));
        badge->setForeground(trackColor(int(span.trackId)));
        m_trackTable->setItem(row, 0, badge);
        m_trackTable->setItem(row, 1,
                              new QTableWidgetItem(QString::fromStdString(span.className)));
        m_trackTable->setItem(row, 2, new QTableWidgetItem(QString::number(span.firstFrame)));
        m_trackTable->setItem(row, 3, new QTableWidgetItem(QString::number(span.lastFrame)));
        m_trackTable->setItem(row, 4, new QTableWidgetItem(QString::number(span.observations)));
        m_trackTable->setItem(row, 5, new QTableWidgetItem(shortClock(span.durationMs())));
    }
    m_tabs->setTabText(0, QStringLiteral("TRACKS (%1)").arg(spans.size()));
}

void ReviewPanel::setEventsTable(const std::vector<analytics::SurveyEvent>& events)
{
    m_eventTable->setRowCount(int(events.size()));
    for (int row = 0; row < int(events.size()); ++row) {
        const auto& event = events.at(std::size_t(row));
        m_eventTable->setItem(row, 0, new QTableWidgetItem(shortClock(event.atMs)));
        auto* description = new QTableWidgetItem(QString::fromStdString(event.description));
        switch (event.severity) {
        case analytics::EventSeverity::Alert:
            description->setForeground(palette::Danger);
            break;
        case analytics::EventSeverity::Warning:
            description->setForeground(palette::Warning);
            break;
        case analytics::EventSeverity::Info:
            description->setForeground(palette::TextSecondary);
            break;
        }
        m_eventTable->setItem(row, 1, description);
    }
    m_tabs->setTabText(1, QStringLiteral("EVENTS (%1)").arg(events.size()));
}

void ReviewPanel::clearAnalytics()
{
    m_countChart->setSeries({});
    m_activityChart->setSeries({});
    m_speciesChart->setSlices({});
    m_topTracks->setRows({});
    m_trackTable->setRowCount(0);
    m_eventTable->setRowCount(0);
    m_countTotal->clear();
}
