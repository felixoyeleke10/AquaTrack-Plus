#include "ui/review/right/InsightPanel.h"

#include "ui/AppTheme.h"
#include "ui/common/Charts.h"
#include "ui/common/Glyphs.h"
#include "ui/common/InspectorTabs.h"
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
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

using namespace aquatrack::ui;
namespace analytics = aquatrack::analytics;

namespace {

constexpr int kRow = 30;
constexpr int kColourRole = Qt::UserRole + 1;
constexpr int kTrackRole = Qt::UserRole + 2;
constexpr int kSpeciesRole = Qt::UserRole + 3;
constexpr int kConfidenceRole = Qt::UserRole + 4;
constexpr int kVerifiedRole = Qt::UserRole + 5;
constexpr int kTrailingRole = Qt::UserRole + 6;

QString shortClock(qint64 milliseconds)
{
    const qint64 seconds = std::max<qint64>(0, milliseconds) / 1000;
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString percent(double ratio)
{
    return QStringLiteral("%1%").arg(int(std::round(ratio * 100)));
}

QWidget* bare(QWidget* parent)
{
    auto* widget = new QWidget(parent);
    widget->setProperty("role", "bare");
    return widget;
}

// A page that scrolls only if its content is taller than the panel.
QScrollArea* scrollPage(QWidget* content, QWidget* parent)
{
    auto* scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(content);
    return scroll;
}

// A small caption over a value.
QLabel* stat(QGridLayout* grid, int column, const QString& caption, QWidget* parent)
{
    grid->addWidget(typedLabel(caption, "dim", parent), 0, column);
    auto* value = typedLabel(QStringLiteral("—"), "body", parent);
    value->setStyleSheet(QStringLiteral("font-weight:600"));
    grid->addWidget(value, 1, column);
    return value;
}

QListWidget* rowList(const QString& name, QWidget* parent)
{
    auto* list = new QListWidget(parent);
    list->setObjectName(name);
    list->setFrameShape(QFrame::NoFrame);
    list->setMouseTracking(true);
    list->setUniformItemSizes(true);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setStyleSheet(
        QStringLiteral("QListWidget#%1 { background: transparent; border: none; }").arg(name));
    return list;
}

void paintRowBackground(QPainter* painter, const QStyleOptionViewItem& option, QRectF row)
{
    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = option.state & QStyle::State_MouseOver;
    if (!selected && !hovered)
        return;
    painter->setPen(Qt::NoPen);
    painter->setBrush(selected ? palette::AccentTint : palette::Surface);
    painter->drawRoundedRect(row, 7, 7);
}

// An observation in the current frame: track colour, ID, species, a tick
// when a reviewer verified it, and its confidence right-aligned.
class ObservationDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return {0, kRow};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF row = QRectF(option.rect).adjusted(2, 1, -2, -1);
        const bool enabled = option.state & QStyle::State_Enabled;
        paintRowBackground(painter, option, row);

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

// A switch candidate: "Track 9 -> 10" with when and how alike, right-aligned.
class SwitchDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return {0, kRow};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF row = QRectF(option.rect).adjusted(2, 1, -2, -1);
        paintRowBackground(painter, option, row);
        const bool selected = option.state & QStyle::State_Selected;

        QFont font = option.font;
        font.setPixelSize(12);
        const QString trailing = index.data(kTrailingRole).toString();
        const int trailingWidth = QFontMetrics(font).horizontalAdvance(trailing);
        painter->setFont(font);
        painter->setPen(palette::TextDim);
        painter->drawText(row.adjusted(10, 0, -10, 0), Qt::AlignRight | Qt::AlignVCenter, trailing);

        QFont bold = font;
        bold.setWeight(selected ? QFont::DemiBold : QFont::Normal);
        painter->setFont(bold);
        painter->setPen(selected ? palette::TextPrimary : palette::TextSecondary);
        const QRectF text(row.left() + 10, row.top(), row.width() - 30 - trailingWidth, row.height());
        painter->drawText(text, Qt::AlignLeft | Qt::AlignVCenter,
                          index.data(Qt::DisplayRole).toString());
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

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 6, 0);
    m_tabs = new InspectorTabs(this);
    m_framePage = m_tabs->addPage(Glyph::Annotations, QStringLiteral("In frame"), buildFramePage(),
                                  QStringLiteral("What is annotated in this frame, and its edits"));
    m_switchPage = m_tabs->addPage(Glyph::Merge, QStringLiteral("ID switches"), buildSwitchPage(),
                                   QStringLiteral("Tracks that are probably the same animal"));
    m_confidencePage = m_tabs->addPage(Glyph::Analytics, QStringLiteral("Confidence"),
                                       buildConfidencePage(),
                                       QStringLiteral("How sure the detector was, over time"));
    outer->addWidget(m_tabs);

    setEditingEnabled(false);
    setCurrentObservations({});
    setIdSwitches({});
    setConfidenceSeries({}, 0.0);
    setQuality(0, 0, 0, 0);
}

// ------------------------------------------------------------ In this frame

QWidget* InsightPanel::buildFramePage()
{
    auto* page = bare(nullptr);
    auto* column = new QVBoxLayout(page);
    column->setContentsMargins(0, 0, 0, 0);
    auto* body = card(page);
    column->addWidget(body);
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(8, 8, 8, 10);
    layout->setSpacing(8);

    m_editHint = typedLabel(
        QStringLiteral("Create or open a project so edits have somewhere durable to go."),
        "dim", body);
    m_editHint->setWordWrap(true);
    m_editHint->setContentsMargins(6, 2, 6, 0);
    layout->addWidget(m_editHint);

    m_observations = rowList(QStringLiteral("frameObservations"), body);
    m_observations->setItemDelegate(new ObservationDelegate(m_observations));
    m_observations->setMinimumHeight(kRow * 3 + 2);
    layout->addWidget(m_observations, 1);
    m_observationsEmpty = typedLabel(QStringLiteral("Nothing annotated in this frame."), "dim", body);
    m_observationsEmpty->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_observationsEmpty, 1);

    layout->addWidget(divider(body));
    auto* editRow = new QHBoxLayout;
    editRow->setContentsMargins(0, 0, 0, 0);
    editRow->setSpacing(6);
    m_relabel = textButton(QStringLiteral("Relabel..."), body);
    m_relabel->setToolTip(QStringLiteral("Change the species of the selected annotation"));
    m_delete = textButton(QStringLiteral("Delete"), body);
    m_delete->setToolTip(QStringLiteral("Delete the selected annotation"));
    editRow->addWidget(m_relabel, 1);
    editRow->addWidget(m_delete, 1);
    layout->addLayout(editRow);

    auto* saveRow = new QHBoxLayout;
    saveRow->setContentsMargins(0, 0, 0, 0);
    saveRow->setSpacing(6);
    m_undo = iconButton(Glyph::Undo, QStringLiteral("Undo"), body, "icon", 16);
    m_redo = iconButton(Glyph::Redo, QStringLiteral("Redo"), body, "icon", 16);
    m_save = textButton(QStringLiteral("Save annotations"), body, "primary");
    m_save->setObjectName(QStringLiteral("saveAnnotations"));
    m_save->setIcon(glyphIcon(Glyph::Save, 15, Qt::white, Qt::white));
    saveRow->addWidget(m_undo);
    saveRow->addWidget(m_redo);
    saveRow->addWidget(m_save, 1);
    layout->addLayout(saveRow);

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
    return page;
}

// ------------------------------------------------------------ ID switches

QWidget* InsightPanel::buildSwitchPage()
{
    auto* content = bare(nullptr);
    auto* column = new QVBoxLayout(content);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(10);

    m_switchEmpty = typedLabel(
        QStringLiteral("No likely identity switches. A switch is a track that ends as another "
                       "begins close by with a similar box, which is often one animal the "
                       "tracker lost and picked up again."),
        "dim", content);
    m_switchEmpty->setWordWrap(true);
    m_switchEmpty->setContentsMargins(4, 0, 4, 0);
    column->addWidget(m_switchEmpty);

    m_switchContent = bare(content);
    auto* switchColumn = new QVBoxLayout(m_switchContent);
    switchColumn->setContentsMargins(0, 0, 0, 0);
    switchColumn->setSpacing(10);

    // The chosen candidate, with what to do about it.
    auto* chosen = card(m_switchContent, "alert");
    auto* chosenBody = new QVBoxLayout(chosen);
    chosenBody->setContentsMargins(14, 12, 14, 12);
    chosenBody->setSpacing(10);
    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    m_switchSummary = typedLabel(QStringLiteral("—"), "subtitle", chosen);
    head->addWidget(m_switchSummary);
    head->addStretch();
    m_switchBadge = new Badge(chosen);
    head->addWidget(m_switchBadge);
    chosenBody->addLayout(head);

    auto* stats = new QGridLayout;
    stats->setContentsMargins(0, 0, 0, 0);
    stats->setHorizontalSpacing(18);
    stats->setVerticalSpacing(1);
    m_switchAt = stat(stats, 0, QStringLiteral("At"), chosen);
    m_switchGap = stat(stats, 1, QStringLiteral("Gap"), chosen);
    m_switchSimilarity = stat(stats, 2, QStringLiteral("Similarity"), chosen);
    stats->setColumnStretch(3, 1);
    chosenBody->addLayout(stats);

    auto* actions = new QHBoxLayout;
    actions->setContentsMargins(0, 2, 0, 0);
    actions->setSpacing(8);
    m_merge = textButton(QStringLiteral("Merge tracks"), chosen, "primary");
    m_merge->setIcon(glyphIcon(Glyph::Merge, 15, Qt::white, Qt::white));
    m_merge->setToolTip(QStringLiteral("Join the two tracks into one"));
    m_reviewClip = textButton(QStringLiteral("Go to frame"), chosen);
    actions->addWidget(m_merge, 1);
    actions->addWidget(m_reviewClip, 1);
    chosenBody->addLayout(actions);
    switchColumn->addWidget(chosen);

    connect(m_merge, &QPushButton::clicked, this, [this] {
        if (m_selectedSwitch >= 0 && m_selectedSwitch < int(m_candidates.size())) {
            const auto& candidate = m_candidates[std::size_t(m_selectedSwitch)];
            emit mergeTracksRequested(candidate.endingTrack, candidate.startingTrack);
        }
    });
    connect(m_reviewClip, &QPushButton::clicked, this, [this] {
        if (m_selectedSwitch >= 0 && m_selectedSwitch < int(m_candidates.size()))
            emit frameRequested(int(m_candidates[std::size_t(m_selectedSwitch)].atMs));
    });

    // Every candidate; choosing one shows it above.
    auto* listHead = new QHBoxLayout;
    listHead->setContentsMargins(2, 4, 2, 0);
    listHead->addWidget(sectionLabel(QStringLiteral("All candidates"), m_switchContent));
    listHead->addStretch();
    m_switchListTitle = typedLabel(QString(), "dim", m_switchContent);
    listHead->addWidget(m_switchListTitle);
    switchColumn->addLayout(listHead);
    auto* listCard = card(m_switchContent);
    auto* listBody = new QVBoxLayout(listCard);
    listBody->setContentsMargins(6, 6, 6, 6);
    m_switchList = rowList(QStringLiteral("switchCandidates"), listCard);
    m_switchList->setItemDelegate(new SwitchDelegate(m_switchList));
    m_switchList->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_switchList->setToolTip(QStringLiteral("Click to inspect; double-click to go there"));
    connect(m_switchList, &QListWidget::currentRowChanged, this, &InsightPanel::showSwitch);
    connect(m_switchList, &QListWidget::itemDoubleClicked, this, [this] {
        if (m_selectedSwitch >= 0 && m_selectedSwitch < int(m_candidates.size()))
            emit frameRequested(int(m_candidates[std::size_t(m_selectedSwitch)].atMs));
    });
    listBody->addWidget(m_switchList);
    switchColumn->addWidget(listCard);
    column->addWidget(m_switchContent);
    column->addStretch();
    return scrollPage(content, nullptr);
}

void InsightPanel::setIdSwitches(const std::vector<analytics::IdSwitchCandidate>& candidates)
{
    m_candidates = candidates;
    const bool any = !m_candidates.empty();
    m_switchEmpty->setVisible(!any);
    m_switchContent->setVisible(any);
    m_tabs->setPageCount(m_switchPage, int(m_candidates.size()));
    m_tabs->setPageEnabled(m_switchPage, any);

    const int keep = std::clamp(m_selectedSwitch, 0, std::max(0, int(m_candidates.size()) - 1));
    {
        const QSignalBlocker blocker(m_switchList);
        m_switchList->clear();
        for (const auto& candidate : m_candidates) {
            auto* item = new QListWidgetItem(QStringLiteral("Track %1  →  %2")
                                                 .arg(candidate.endingTrack)
                                                 .arg(candidate.startingTrack),
                                             m_switchList);
            item->setData(kTrailingRole, QStringLiteral("%1   %2")
                                             .arg(shortClock(candidate.atMs),
                                                  percent(candidate.similarity)));
        }
        m_switchList->setFixedHeight(int(m_candidates.size()) * kRow + 2);
    }
    m_switchListTitle->setText(any ? QString::number(m_candidates.size()) : QString());
    m_selectedSwitch = -1;
    if (any) {
        m_switchList->setCurrentRow(keep);
        showSwitch(keep);
    }
}

void InsightPanel::showSwitch(int index)
{
    if (index < 0 || index >= int(m_candidates.size()) || index == m_selectedSwitch)
        return;
    m_selectedSwitch = index;
    const auto& candidate = m_candidates[std::size_t(index)];
    m_switchSummary->setText(QStringLiteral("Track %1  →  Track %2")
                                 .arg(candidate.endingTrack)
                                 .arg(candidate.startingTrack));
    m_switchAt->setText(shortClock(candidate.atMs));
    m_switchGap->setText(QStringLiteral("%1 s").arg(candidate.gapMs / 1000.0, 0, 'f', 1));
    m_switchSimilarity->setText(percent(candidate.similarity));
    const bool strong = candidate.similarity > 0.8;
    m_switchBadge->setText(strong ? QStringLiteral("Likely") : QStringLiteral("Worth a look"));
    m_switchBadge->setAccent(strong ? palette::Warning : palette::TextSecondary);
}

// ------------------------------------------------------------- Confidence

QWidget* InsightPanel::buildConfidencePage()
{
    auto* content = bare(nullptr);
    auto* column = new QVBoxLayout(content);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(10);

    auto* chartCard = card(content);
    auto* chartBody = new QVBoxLayout(chartCard);
    chartBody->setContentsMargins(12, 12, 12, 12);
    chartBody->setSpacing(8);
    chartBody->addWidget(sectionLabel(QStringLiteral("Mean confidence over time"), chartCard));
    m_confidenceChart = new LineChart(chartCard);
    m_confidenceChart->setEmptyText(QStringLiteral("Open a sequence"));
    m_confidenceChart->setMinimumHeight(170);
    chartBody->addWidget(m_confidenceChart);
    column->addWidget(chartCard);

    auto* figuresCard = card(content);
    auto* figures = new QGridLayout(figuresCard);
    figures->setContentsMargins(14, 12, 14, 12);
    figures->setHorizontalSpacing(18);
    figures->setVerticalSpacing(1);
    m_meanConfidence = stat(figures, 0, QStringLiteral("Mean"), figuresCard);
    m_lowConfidence = stat(figures, 1, QStringLiteral("Below 0.5"), figuresCard);
    m_verified = stat(figures, 2, QStringLiteral("Verified"), figuresCard);
    m_lowConfidence->setToolTip(QStringLiteral("Share of boxes the detector was less than 50% sure of"));
    m_verified->setToolTip(QStringLiteral("Share of boxes a reviewer has confirmed"));
    figures->setColumnStretch(3, 1);
    column->addWidget(figuresCard);

    auto* note = typedLabel(
        QStringLiteral("Dips mark stretches worth reviewing first: murky water, crowding or "
                       "unfamiliar species. Use Minimum confidence in the left rail to hide "
                       "weak boxes."),
        "dim", content);
    note->setWordWrap(true);
    note->setContentsMargins(4, 0, 4, 0);
    column->addWidget(note);
    column->addStretch();
    return scrollPage(content, nullptr);
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
    m_tabs->setPageEnabled(m_confidencePage, !values.isEmpty());
}

void InsightPanel::setQuality(double meanConfidence, double lowConfidenceShare,
                              double verifiedShare, int observations)
{
    const bool any = observations > 0;
    m_meanConfidence->setText(any ? QString::number(meanConfidence, 'f', 2) : QStringLiteral("—"));
    m_lowConfidence->setText(any ? percent(lowConfidenceShare) : QStringLiteral("—"));
    m_lowConfidence->setStyleSheet(QStringLiteral("font-weight:600;color:%1")
                                       .arg((any && lowConfidenceShare > 0.25 ? palette::Warning
                                                                              : palette::TextPrimary)
                                                .name()));
    m_verified->setText(any ? percent(verifiedShare) : QStringLiteral("—"));
}

// ------------------------------------------------------- Shared plumbing

QString InsightPanel::selectedObservationId() const
{
    auto* item = m_observations->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
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
    m_observations->setVisible(!observations.empty());
    m_observationsEmpty->setVisible(observations.empty());
    m_tabs->setPageCount(m_framePage, int(observations.size()));
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
    m_merge->setToolTip(enabled ? QStringLiteral("Join the two tracks into one")
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
