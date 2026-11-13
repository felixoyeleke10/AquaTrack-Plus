#include "ui/review/left/SurveyPanel.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QComboBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QPainter>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

using namespace aquatrack::ui;

namespace {

constexpr int kEntryHeight = 30;
constexpr int kVisibleEntries = 6;
constexpr int kTrailingRole = Qt::UserRole + 1;

// A layout-only container; plain QWidgets otherwise take the canvas colour.
QWidget* bare(QWidget* parent)
{
    auto* widget = new QWidget(parent);
    widget->setProperty("role", "bare");
    return widget;
}

// Icon + name on the left, the figure on the right.
QWidget* figureRow(Glyph glyph, const QString& name, QLabel*& value, QWidget* parent)
{
    auto* row = bare(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 5, 0, 5);
    layout->setSpacing(10);

    auto* icon = new QLabel(row);
    icon->setPixmap(glyphPixmap(glyph, 15, palette::TextDim));
    icon->setFixedWidth(15);
    layout->addWidget(icon);
    layout->addWidget(typedLabel(name, "small", row));
    layout->addStretch();

    value = typedLabel(QStringLiteral("—"), "small", row);
    value->setStyleSheet(QStringLiteral("font-weight:600;color:%1").arg(palette::TextPrimary.name()));
    layout->addWidget(value);
    return row;
}

// A clip row: the label, elided in the middle so the clip number and the
// extension survive, and the duration right-aligned in a fixed column.
class EntryDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    {
        return {0, kEntryHeight};
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF row = QRectF(option.rect).adjusted(2, 1, -2, -1);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        if (selected || hovered) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(selected ? palette::AccentTint : palette::Surface);
            painter->drawRoundedRect(row, 7, 7);
        }
        if (selected) {
            painter->setBrush(palette::Accent);
            painter->drawRoundedRect(QRectF(row.left(), row.top() + 7, 2.5, row.height() - 14), 1.2, 1.2);
        }

        QFont font = option.font;
        font.setPixelSize(12);
        const QFontMetrics metrics(font);
        const QString trailing = index.data(kTrailingRole).toString();
        const int trailingWidth = trailing.isEmpty() ? 0 : metrics.horizontalAdvance(trailing);
        const QRectF text = row.adjusted(10, 0, -10, 0);
        painter->setFont(font);
        if (!trailing.isEmpty()) {
            painter->setPen(selected ? palette::TextSecondary : palette::TextDim);
            painter->drawText(text, Qt::AlignRight | Qt::AlignVCenter, trailing);
        }
        const int labelWidth = int(text.width()) - (trailingWidth > 0 ? trailingWidth + 12 : 0);
        painter->setPen(selected ? palette::TextPrimary : palette::TextSecondary);
        painter->drawText(QRectF(text.left(), text.top(), labelWidth, text.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          metrics.elidedText(index.data(Qt::DisplayRole).toString(), Qt::ElideMiddle,
                                             labelWidth));
        painter->restore();
    }
};

}  // namespace

SurveyPanel::SurveyPanel(QWidget* parent) : QFrame(parent)
{
    setFixedWidth(248);
    // Scoped to this panel: an unscoped rule cascades onto every child and
    // strips buttons (the primary gradient included) of their background.
    setStyleSheet(QStringLiteral("SurveyPanel { background: transparent; }"));

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = bare(scroll);
    auto* column = new QVBoxLayout(content);
    column->setContentsMargins(0, 0, 6, 0);
    column->setSpacing(8);

    // ---- What is open --------------------------------------------------
    column->addWidget(sectionLabel(QStringLiteral("Sequence"), content));
    auto* sequenceCard = card(content);
    auto* sequenceBody = new QVBoxLayout(sequenceCard);
    sequenceBody->setContentsMargins(14, 12, 10, 10);
    sequenceBody->setSpacing(2);
    m_sequenceName = typedLabel(QStringLiteral("No sequence loaded"), "body", sequenceCard);
    m_sequenceName->setStyleSheet(QStringLiteral("font-weight:600"));
    m_sequenceName->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    sequenceBody->addWidget(m_sequenceName);
    m_sequenceDetail = typedLabel(QString(), "dim", sequenceCard);
    m_sequenceDetail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    sequenceBody->addWidget(m_sequenceDetail);
    sequenceBody->addSpacing(8);
    m_sequenceFiles = new QListWidget(sequenceCard);
    m_sequenceFiles->setObjectName(QStringLiteral("sequenceFiles"));
    // A list view expands by default, which makes its card claim spare
    // height and float its contents; this one is sized to its rows.
    m_sequenceFiles->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_sequenceFiles->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sequenceFiles->setFrameShape(QFrame::NoFrame);
    m_sequenceFiles->setUniformItemSizes(true);
    m_sequenceFiles->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_sequenceFiles->setMouseTracking(true);
    m_sequenceFiles->setItemDelegate(new EntryDelegate(m_sequenceFiles));
    m_sequenceFiles->setStyleSheet(QStringLiteral(
        "QListWidget#sequenceFiles { background: transparent; border: none; }"));
    connect(m_sequenceFiles, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        const int frame = item->data(Qt::UserRole).toInt();
        if (frame > 0)
            emit sequenceFileRequested(frame);
    });
    sequenceBody->addWidget(m_sequenceFiles);
    column->addWidget(sequenceCard);

    // ---- What it adds up to --------------------------------------------
    column->addSpacing(6);
    column->addWidget(sectionLabel(QStringLiteral("Summary"), content));
    auto* summaryCard = card(content);
    auto* summaryBody = new QVBoxLayout(summaryCard);
    summaryBody->setContentsMargins(14, 7, 14, 7);
    summaryBody->setSpacing(0);
    summaryBody->addWidget(
        figureRow(Glyph::Annotations, QStringLiteral("Annotations"), m_annotationCount, summaryCard));
    summaryBody->addWidget(figureRow(Glyph::Tracks, QStringLiteral("Tracks"), m_trackCount, summaryCard));
    summaryBody->addWidget(
        figureRow(Glyph::Inspect, QStringLiteral("Most in one frame"), m_peakCount, summaryCard));
    summaryBody->addWidget(
        figureRow(Glyph::Species, QStringLiteral("Species"), m_speciesCount, summaryCard));
    summaryBody->addWidget(figureRow(Glyph::Events, QStringLiteral("Events"), m_eventCount, summaryCard));
    column->addWidget(summaryCard);

    // ---- What is drawn ---------------------------------------------------
    column->addSpacing(6);
    column->addWidget(sectionLabel(QStringLiteral("Display"), content));
    auto* filterCard = card(content);
    auto* filterBody = new QVBoxLayout(filterCard);
    filterBody->setContentsMargins(14, 12, 14, 12);
    filterBody->setSpacing(8);

    filterBody->addWidget(typedLabel(QStringLiteral("Species"), "dim", filterCard));
    m_species = new QComboBox(filterCard);
    m_species->addItem(QStringLiteral("All species"), -1);
    connect(m_species, &QComboBox::currentIndexChanged, this, &SurveyPanel::filtersChanged);
    filterBody->addWidget(m_species);
    filterBody->addSpacing(4);

    auto* confidenceHead = new QHBoxLayout;
    confidenceHead->setContentsMargins(0, 0, 0, 0);
    confidenceHead->addWidget(typedLabel(QStringLiteral("Minimum confidence"), "dim", filterCard));
    confidenceHead->addStretch();
    m_confidenceValue = typedLabel(QStringLiteral("0.00"), "small", filterCard);
    m_confidenceValue->setStyleSheet(QStringLiteral("font-weight:600;color:%1").arg(palette::TextPrimary.name()));
    confidenceHead->addWidget(m_confidenceValue);
    filterBody->addLayout(confidenceHead);

    m_confidence = new QSlider(Qt::Horizontal, filterCard);
    m_confidence->setRange(0, 100);
    m_confidence->setValue(0);
    connect(m_confidence, &QSlider::valueChanged, this, [this](int value) {
        m_confidenceValue->setText(QString::number(value / 100.0, 'f', 2));
        emit filtersChanged();
    });
    filterBody->addWidget(m_confidence);

    filterBody->addSpacing(2);
    filterBody->addWidget(divider(filterCard));
    filterBody->addSpacing(2);

    auto* detections = new ToggleRow(QStringLiteral("Boxes"), true, filterCard);
    auto* tracks = new ToggleRow(QStringLiteral("Track trails"), true, filterCard);
    auto* ids = new ToggleRow(QStringLiteral("Track IDs"), true, filterCard);
    detections->setToolTip(QStringLiteral("Draw a box around each detection"));
    tracks->setToolTip(QStringLiteral("Draw the recent path of each track"));
    ids->setToolTip(QStringLiteral("Label each box with its track ID"));
    m_showDetections = detections->toggle();
    m_showTracks = tracks->toggle();
    m_showIds = ids->toggle();
    for (auto* row : {detections, tracks, ids}) {
        row->setProperty("role", "bare");
        connect(row, &ToggleRow::toggled, this, &SurveyPanel::filtersChanged);
        filterBody->addWidget(row);
    }
    column->addWidget(filterCard);

    column->addStretch();
    scroll->setWidget(content);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
    clearSequence();
    setTotals({});
}

void SurveyPanel::setSequence(const QString& name, const QString& location, const QString& detail,
                              const QVector<Entry>& entries)
{
    m_sequenceName->setText(name.isEmpty() ? QStringLiteral("Unnamed sequence") : name);
    m_sequenceName->setToolTip(location);
    m_sequenceDetail->setText(detail);
    m_sequenceDetail->setVisible(!detail.isEmpty());
    m_sequenceFiles->setUpdatesEnabled(false);
    m_sequenceFiles->clear();
    m_rowsByFrame.clear();
    m_highlightedRow = -1;
    for (int index = 0; index < entries.size(); ++index) {
        const Entry& entry = entries.at(index);
        auto* item = new QListWidgetItem(entry.label, m_sequenceFiles);
        item->setData(Qt::UserRole, entry.frame);
        item->setData(kTrailingRole, entry.trailing);
        item->setToolTip(entry.frame > 0
                             ? QStringLiteral("%1\nClick to jump to frame %2")
                                   .arg(entry.label, QLocale::system().toString(entry.frame))
                             : entry.label);
        if (entry.frame > 0)
            m_rowsByFrame.push_back({entry.frame, index});
    }
    std::sort(m_rowsByFrame.begin(), m_rowsByFrame.end());
    m_sequenceFiles->setUpdatesEnabled(true);
    // Exactly as tall as its rows, up to a handful; longer lists scroll.
    const int rows = std::min<int>(int(entries.size()), kVisibleEntries);
    m_sequenceFiles->setFixedHeight(rows * kEntryHeight + (rows > 0 ? 2 : 0));
    m_sequenceFiles->setVisible(rows > 0);
}

void SurveyPanel::setCurrentFrame(int frame)
{
    // The row whose first frame is the last one at or before the playhead.
    const auto found = std::upper_bound(m_rowsByFrame.cbegin(), m_rowsByFrame.cend(),
                                        QPair<int, int>{frame, std::numeric_limits<int>::max()});
    const int row = found == m_rowsByFrame.cbegin() ? -1 : std::prev(found)->second;
    if (row == m_highlightedRow)
        return;
    m_highlightedRow = row;
    const QSignalBlocker blocker(m_sequenceFiles);
    if (row < 0) {
        m_sequenceFiles->clearSelection();
        return;
    }
    m_sequenceFiles->setCurrentRow(row);
    m_sequenceFiles->scrollToItem(m_sequenceFiles->item(row), QAbstractItemView::EnsureVisible);
}

void SurveyPanel::clearSequence()
{
    setSequence(QStringLiteral("No sequence loaded"), QString(),
                QStringLiteral("Open a video or sequence to review it"), {});
}

void SurveyPanel::setTotals(const Totals& totals)
{
    const QLocale locale = QLocale::system();
    const auto figure = [&](QLabel* label, int value) {
        label->setText(value > 0 ? locale.toString(value) : QStringLiteral("—"));
    };
    figure(m_annotationCount, totals.annotations);
    figure(m_trackCount, totals.tracks);
    figure(m_peakCount, totals.peakInFrame);
    figure(m_eventCount, totals.events);
    // One species is named outright; several are counted, the most common
    // named in the tooltip.
    if (totals.species == 1 && !totals.dominantSpecies.isEmpty())
        m_speciesCount->setText(totals.dominantSpecies);
    else
        figure(m_speciesCount, totals.species);
    m_speciesCount->setToolTip(totals.dominantSpecies.isEmpty()
                                   ? QString()
                                   : QStringLiteral("Most observed: %1").arg(totals.dominantSpecies));
}

void SurveyPanel::setSpecies(const QVector<QPair<int, QString>>& species)
{
    const int previous = m_species->currentData().isValid() ? m_species->currentData().toInt() : -1;
    int index = 0;
    {
        QSignalBlocker blocker(m_species);
        m_species->clear();
        m_species->addItem(QStringLiteral("All species"), -1);
        for (const auto& [classId, name] : species)
            m_species->addItem(name, classId);
        index = m_species->findData(previous);
        m_species->setCurrentIndex(index >= 0 ? index : 0);
    }
    // The selected species disappeared (for example after a relabel), so the
    // filter fell back to every species; the canvas must hear about it.
    if (index < 0)
        emit filtersChanged();
}

SurveyPanel::Filters SurveyPanel::filters() const
{
    Filters filters;
    filters.showDetections = m_showDetections->isChecked();
    filters.showTracks = m_showTracks->isChecked();
    filters.showIds = m_showIds->isChecked();
    filters.speciesClassId =
        m_species->currentData().isValid() ? m_species->currentData().toInt() : -1;
    filters.minConfidence = m_confidence->value() / 100.0;
    return filters;
}
