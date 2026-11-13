#include "ui/review/right/InsightPanel.h"

#include "ui/AppTheme.h"
#include "ui/common/Charts.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>

using namespace aquatrack::ui;
namespace analytics = aquatrack::analytics;

namespace {

constexpr int kEventsShown = 6;
constexpr int kObservationRow = 30;
constexpr int kColourRole = Qt::UserRole + 1;
constexpr int kTrackRole = Qt::UserRole + 2;
constexpr int kSpeciesRole = Qt::UserRole + 3;
constexpr int kConfidenceRole = Qt::UserRole + 4;
constexpr int kVerifiedRole = Qt::UserRole + 5;

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

QWidget* bare(QWidget* parent)
{
    auto* widget = new QWidget(parent);
    widget->setProperty("role", "bare");
    return widget;
}

// A heading over a card, with an optional figure at the right.
QHBoxLayout* heading(const QString& text, QWidget* parent, QLabel** figure = nullptr)
{
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 2, 0);
    row->addWidget(sectionLabel(text, parent));
    row->addStretch();
    if (figure) {
        *figure = typedLabel(QString(), "dim", parent);
        row->addWidget(*figure);
    }
    return row;
}

// A small caption over a value, as used in the identity-switch card.
QLabel* stat(QGridLayout* grid, int column, const QString& caption, QWidget* parent)
{
    grid->addWidget(typedLabel(caption, "dim", parent), 0, column);
    auto* value = typedLabel(QStringLiteral("—"), "body", parent);
    value->setStyleSheet(QStringLiteral("font-weight:600"));
    grid->addWidget(value, 1, column);
    return value;
}

// One event: severity dot, what happened (shortened to one line; the full
// text is the tooltip), when. The whole row is a button that moves the
// playhead there. Painted, so its height never depends on wrapping.
class EventButton final : public QPushButton {
public:
    EventButton(const analytics::SurveyEvent& event, QWidget* parent)
        : QPushButton(parent),
          m_text(QString::fromStdString(event.description)),
          m_time(shortClock(event.atMs)),
          m_colour(severityColor(event.severity))
    {
        setProperty("role", "row");
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(30);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip(QStringLiteral("%1\nClick to go to %2").arg(m_text, m_time));
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPushButton::paintEvent(event);  // hover background from the sheet
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_colour);
        painter.drawEllipse(QPointF(12, height() / 2.0), 3.5, 3.5);

        QFont mono(QStringLiteral("Cascadia Mono"));
        mono.setStyleHint(QFont::Monospace);
        mono.setPixelSize(11);
        const int timeWidth = QFontMetrics(mono).horizontalAdvance(m_time);
        painter.setFont(mono);
        painter.setPen(palette::TextDim);
        const QRect timeBox(width() - 8 - timeWidth, 0, timeWidth, height());
        painter.drawText(timeBox, Qt::AlignRight | Qt::AlignVCenter, m_time);

        QFont text = font();
        text.setPixelSize(12);
        painter.setFont(text);
        painter.setPen(underMouse() ? palette::TextPrimary : palette::TextSecondary);
        const QRect textBox(24, 0, timeBox.left() - 10 - 24, height());
        painter.drawText(textBox, Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(text).elidedText(m_text, Qt::ElideRight, textBox.width()));
    }

private:
    QString m_text;
    QString m_time;
    QColor m_colour;
};

QPushButton* eventRow(const analytics::SurveyEvent& event, QWidget* parent)
{
    return new EventButton(event, parent);
}

// An observation in the current frame: track colour, ID, species, a tick
// when a reviewer verified it, and its confidence right-aligned.
class ObservationDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return {0, kObservationRow};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF row = QRectF(option.rect).adjusted(2, 1, -2, -1);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool enabled = option.state & QStyle::State_Enabled;
        if (selected || hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? palette::AccentTint : palette::Surface);
            painter->drawRoundedRect(row, 7, 7);
        }

        const QColor colour = index.data(kColourRole).value<QColor>();
        const double confidence = index.data(kConfidenceRole).toDouble();
        const bool verified = index.data(kVerifiedRole).toBool();
        QFont font = option.font;
        font.setPixelSize(12);
        QFont bold = font;
        bold.setWeight(QFont::DemiBold);

        qreal x = row.left() + 10;
        painter->setPen(Qt::NoPen);
        painter->setBrush(enabled ? colour : palette::TextDim);
        painter->drawRoundedRect(QRectF(x, row.center().y() - 5, 10, 10), 3, 3);
        x += 18;

        const QString id = QStringLiteral("ID %1").arg(index.data(kTrackRole).toLongLong());
        painter->setFont(bold);
        painter->setPen(enabled ? palette::TextPrimary : palette::TextDim);
        const int idWidth = QFontMetrics(bold).horizontalAdvance(id);
        painter->drawText(QRectF(x, row.top(), idWidth + 2, row.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, id);
        x += idWidth + 10;

        // Right side, right to left: confidence, then the verified tick.
        painter->setFont(font);
        const QString value = QString::number(confidence, 'f', 2);
        const int valueWidth = QFontMetrics(font).horizontalAdvance(QStringLiteral("0.00"));
        const QRectF valueBox(row.right() - 10 - valueWidth, row.top(), valueWidth, row.height());
        painter->setPen(confidence < 0.35 ? palette::Warning : palette::TextSecondary);
        painter->drawText(valueBox, Qt::AlignRight | Qt::AlignVCenter, value);
        qreal right = valueBox.left() - 10;
        if (verified) {
            const QRectF tick(right - 12, row.center().y() - 6, 12, 12);
            QPainterPath mark;
            mark.moveTo(tick.left() + 1.5, tick.center().y() + 0.5);
            mark.lineTo(tick.left() + 4.8, tick.bottom() - 2.5);
            mark.lineTo(tick.right() - 1, tick.top() + 2.5);
            painter->setPen(QPen(palette::Success, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(mark);
            right = tick.left() - 8;
        }

        painter->setPen(palette::TextSecondary);
        const QString species = index.data(kSpeciesRole).toString();
        painter->drawText(QRectF(x, row.top(), std::max<qreal>(0, right - x), row.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          QFontMetrics(font).elidedText(species, Qt::ElideRight, int(right - x)));
        painter->restore();
    }
};

}  // namespace

InsightPanel::InsightPanel(QWidget* parent) : QFrame(parent)
{
    setFixedWidth(352);
    // Scoped to this panel: an unscoped rule cascades onto every child and
    // strips buttons (the primary gradient included) of their background.
    setStyleSheet(QStringLiteral("InsightPanel { background: transparent; }"));

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = bare(scroll);
    auto* column = new QVBoxLayout(content);
    column->setContentsMargins(0, 0, 6, 0);
    column->setSpacing(8);

    // ---- Notable events ----------------------------------------------
    column->addLayout(heading(QStringLiteral("Notable events"), content, &m_eventCount));
    auto* eventCard = card(content);
    auto* eventBody = new QVBoxLayout(eventCard);
    eventBody->setContentsMargins(6, 6, 6, 6);
    eventBody->setSpacing(0);
    m_eventList = new QVBoxLayout;
    m_eventList->setContentsMargins(0, 0, 0, 0);
    m_eventList->setSpacing(0);
    eventBody->addLayout(m_eventList);
    m_eventEmpty = typedLabel(
        QStringLiteral("Crowding, identity switches and very short tracks show up here once "
                       "there are annotations."),
        "dim", eventCard);
    m_eventEmpty->setWordWrap(true);
    m_eventEmpty->setContentsMargins(8, 6, 8, 6);
    eventBody->addWidget(m_eventEmpty);
    m_allEvents = textButton(QString(), eventCard, "ghost");
    m_allEvents->setObjectName(QStringLiteral("allEvents"));
    connect(m_allEvents, &QPushButton::clicked, this, &InsightPanel::allEventsRequested);
    eventBody->addWidget(m_allEvents, 0, Qt::AlignLeft);
    column->addWidget(eventCard);

    // ---- Identity switch candidate ------------------------------------
    column->addSpacing(6);
    m_switchCard = card(content, "alert");
    auto* switchBody = new QVBoxLayout(m_switchCard);
    switchBody->setContentsMargins(14, 12, 14, 12);
    switchBody->setSpacing(10);

    auto* switchHead = new QHBoxLayout;
    switchHead->setContentsMargins(0, 0, 0, 0);
    switchHead->addWidget(sectionLabel(QStringLiteral("Possible ID switch"), m_switchCard));
    switchHead->addStretch();
    m_switchBadge = new Badge(m_switchCard);
    switchHead->addWidget(m_switchBadge);
    switchBody->addLayout(switchHead);

    m_switchSummary = typedLabel(QStringLiteral("—"), "subtitle", m_switchCard);
    switchBody->addWidget(m_switchSummary);

    auto* stats = new QGridLayout;
    stats->setContentsMargins(0, 0, 0, 0);
    stats->setHorizontalSpacing(18);
    stats->setVerticalSpacing(1);
    m_switchAt = stat(stats, 0, QStringLiteral("At"), m_switchCard);
    m_switchGap = stat(stats, 1, QStringLiteral("Gap"), m_switchCard);
    m_switchSimilarity = stat(stats, 2, QStringLiteral("Similarity"), m_switchCard);
    stats->setColumnStretch(3, 1);
    switchBody->addLayout(stats);

    auto* actions = new QHBoxLayout;
    actions->setContentsMargins(0, 2, 0, 0);
    actions->setSpacing(8);
    m_merge = textButton(QStringLiteral("Merge tracks"), m_switchCard, "primary");
    m_merge->setIcon(glyphIcon(Glyph::Merge, 15, Qt::white, Qt::white));
    m_reviewClip = textButton(QStringLiteral("Go to frame"), m_switchCard);
    actions->addWidget(m_merge, 1);
    actions->addWidget(m_reviewClip, 1);
    switchBody->addLayout(actions);

    m_switchRemaining = typedLabel(QString(), "dim", m_switchCard);
    switchBody->addWidget(m_switchRemaining);

    connect(m_merge, &QPushButton::clicked, this, [this] {
        if (m_candidate)
            emit mergeTracksRequested(m_candidate->endingTrack, m_candidate->startingTrack);
    });
    connect(m_reviewClip, &QPushButton::clicked, this, [this] {
        if (m_candidate)
            emit frameRequested(int(m_candidate->atMs));
    });
    m_switchCard->hide();
    column->addWidget(m_switchCard);

    // ---- Confidence trend ---------------------------------------------
    column->addSpacing(6);
    column->addLayout(heading(QStringLiteral("Confidence over time"), content));
    auto* confidenceCard = card(content);
    auto* confidenceBody = new QVBoxLayout(confidenceCard);
    confidenceBody->setContentsMargins(12, 10, 12, 10);
    m_confidenceChart = new LineChart(confidenceCard);
    m_confidenceChart->setEmptyText(QStringLiteral("Open a sequence"));
    m_confidenceChart->setMinimumHeight(92);
    confidenceBody->addWidget(m_confidenceChart);
    column->addWidget(confidenceCard);

    // ---- Frame annotations --------------------------------------------
    column->addSpacing(6);
    column->addLayout(heading(QStringLiteral("In this frame"), content, &m_frameCount));
    auto* editCard = card(content);
    auto* editBody = new QVBoxLayout(editCard);
    editBody->setContentsMargins(8, 8, 8, 10);
    editBody->setSpacing(8);

    m_editHint = typedLabel(
        QStringLiteral("Create or open a project so edits have somewhere durable to go."),
        "dim", editCard);
    m_editHint->setWordWrap(true);
    m_editHint->setContentsMargins(6, 2, 6, 0);
    editBody->addWidget(m_editHint);

    m_observations = new QListWidget(editCard);
    m_observations->setObjectName(QStringLiteral("frameObservations"));
    m_observations->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_observations->setFrameShape(QFrame::NoFrame);
    m_observations->setMouseTracking(true);
    m_observations->setUniformItemSizes(true);
    m_observations->setItemDelegate(new ObservationDelegate(m_observations));
    m_observations->setStyleSheet(QStringLiteral(
        "QListWidget#frameObservations { background: transparent; border: none; }"));
    m_observations->setMinimumHeight(kObservationRow * 3 + 2);
    editBody->addWidget(m_observations);
    m_observationsEmpty = typedLabel(QStringLiteral("Nothing annotated in this frame."), "dim", editCard);
    m_observationsEmpty->setContentsMargins(6, 0, 6, 0);
    editBody->addWidget(m_observationsEmpty);

    editBody->addWidget(divider(editCard));
    auto* editRow = new QHBoxLayout;
    editRow->setContentsMargins(0, 0, 0, 0);
    editRow->setSpacing(6);
    m_relabel = textButton(QStringLiteral("Relabel..."), editCard);
    m_relabel->setToolTip(QStringLiteral("Change the species of the selected annotation"));
    m_delete = textButton(QStringLiteral("Delete"), editCard);
    m_delete->setToolTip(QStringLiteral("Delete the selected annotation"));
    editRow->addWidget(m_relabel, 1);
    editRow->addWidget(m_delete, 1);
    editBody->addLayout(editRow);

    auto* saveRow = new QHBoxLayout;
    saveRow->setContentsMargins(0, 0, 0, 0);
    saveRow->setSpacing(6);
    m_undo = iconButton(Glyph::Undo, QStringLiteral("Undo"), editCard, "icon", 16);
    m_redo = iconButton(Glyph::Redo, QStringLiteral("Redo"), editCard, "icon", 16);
    m_save = textButton(QStringLiteral("Save annotations"), editCard, "primary");
    m_save->setObjectName(QStringLiteral("saveAnnotations"));
    m_save->setIcon(glyphIcon(Glyph::Save, 15, Qt::white, Qt::white));
    saveRow->addWidget(m_undo);
    saveRow->addWidget(m_redo);
    saveRow->addWidget(m_save, 1);
    editBody->addLayout(saveRow);
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
    connect(m_observations, &QListWidget::currentItemChanged, this, [this] {
        const bool chosen = m_editingEnabled && m_observations->currentItem();
        m_relabel->setEnabled(chosen);
        m_delete->setEnabled(chosen);
    });
    connect(m_undo, &QPushButton::clicked, this, &InsightPanel::undoRequested);
    connect(m_redo, &QPushButton::clicked, this, &InsightPanel::redoRequested);
    connect(m_save, &QPushButton::clicked, this, &InsightPanel::saveRequested);

    setEditingEnabled(false);
    setEvents({});
    setCurrentObservations({});
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

void InsightPanel::setEvents(const std::vector<analytics::SurveyEvent>& events)
{
    while (m_eventList->count() > 0) {
        auto* item = m_eventList->takeAt(0);
        if (QWidget* widget = item->widget()) {
            // Out of the layout, a row keeps painting at its last position
            // until it is deleted; hide it now.
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }
    m_eventEmpty->setVisible(events.empty());
    auto* container = m_eventEmpty->parentWidget();
    const int shown = std::min<int>(int(events.size()), kEventsShown);
    for (int index = 0; index < shown; ++index) {
        const auto& event = events.at(std::size_t(index));
        auto* row = eventRow(event, container);
        const qint64 at = event.atMs;
        connect(row, &QPushButton::clicked, this, [this, at] { emit frameRequested(int(at)); });
        m_eventList->addWidget(row);
    }
    const int hidden = int(events.size()) - shown;
    m_allEvents->setVisible(hidden > 0);
    m_allEvents->setText(QStringLiteral("Show all %1 events").arg(events.size()));
    m_eventCount->setText(events.empty() ? QString() : QString::number(events.size()));
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
    m_switchAt->setText(shortClock(candidate->atMs));
    m_switchGap->setText(QStringLiteral("%1 s").arg(candidate->gapMs / 1000.0, 0, 'f', 1));
    m_switchSimilarity->setText(QStringLiteral("%1%").arg(candidate->similarity * 100.0, 0, 'f', 0));
    const bool strong = candidate->similarity > 0.8;
    m_switchBadge->setText(strong ? QStringLiteral("Likely") : QStringLiteral("Worth a look"));
    m_switchBadge->setAccent(strong ? palette::Warning : palette::TextSecondary);
    m_switchRemaining->setText(remaining > 1
                                   ? QStringLiteral("%1 more in this sequence, marked on the "
                                                    "timeline")
                                         .arg(remaining - 1)
                                   : QStringLiteral("The only one in this sequence"));
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
        auto* item = new QListWidgetItem(QStringLiteral("ID %1 %2")
                                             .arg(observation.localTrackId)
                                             .arg(QString::fromStdString(observation.className)));
        item->setData(Qt::UserRole, QString::fromStdString(observation.id));
        item->setData(kColourRole, trackColor(int(observation.localTrackId)));
        item->setData(kTrackRole, qlonglong(observation.localTrackId));
        item->setData(kSpeciesRole, QString::fromStdString(observation.className));
        item->setData(kConfidenceRole, double(observation.confidence));
        item->setData(kVerifiedRole, observation.humanVerified);
        item->setToolTip(observation.humanVerified ? QStringLiteral("Verified by a reviewer")
                                                   : QStringLiteral("Not verified yet"));
        m_observations->addItem(item);
        if (QString::fromStdString(observation.id) == previous)
            m_observations->setCurrentItem(item);
    }
    // Sized to its rows (up to eight), so the edit buttons sit right below.
    const int rows = std::clamp(int(observations.size()), 3, 8);
    m_observations->setFixedHeight(rows * kObservationRow + 2);
    m_observations->setVisible(!observations.empty());
    m_observationsEmpty->setVisible(observations.empty());
    m_frameCount->setText(observations.empty() ? QString() : QString::number(observations.size()));
    const bool chosen = m_editingEnabled && m_observations->currentItem();
    m_relabel->setEnabled(chosen);
    m_delete->setEnabled(chosen);
}

void InsightPanel::setEditingEnabled(bool enabled)
{
    m_editingEnabled = enabled;
    const bool chosen = enabled && m_observations->currentItem();
    m_relabel->setEnabled(chosen);
    m_delete->setEnabled(chosen);
    m_save->setEnabled(enabled && m_save->isEnabled());
    m_merge->setEnabled(enabled);
    m_merge->setToolTip(enabled ? QString()
                                : QStringLiteral("Create or open a project to edit tracks"));
    m_observations->setEnabled(enabled);
    m_editHint->setVisible(!enabled);
}

void InsightPanel::setHistoryState(bool canUndo, bool canRedo, bool dirty)
{
    m_undo->setEnabled(m_editingEnabled && canUndo);
    m_redo->setEnabled(m_editingEnabled && canRedo);
    m_save->setEnabled(m_editingEnabled && dirty);
    // "Saved" only means something when there is somewhere to save to.
    const bool saved = m_editingEnabled && !dirty;
    m_save->setText(saved ? QStringLiteral("Saved") : QStringLiteral("Save annotations"));
    m_save->setToolTip(dirty    ? QStringLiteral("Write your edits to the project")
                       : saved ? QStringLiteral("No unsaved edits")
                               : QStringLiteral("Open a project to save edits"));
}
