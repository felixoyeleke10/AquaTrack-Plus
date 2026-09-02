#include "ui/review/left/SurveyPanel.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QVBoxLayout>

using namespace aquatrack::ui;

namespace {

// "Duration: 01:09:17" style rows inside the sequence card.
QWidget* metaRow(const QString& caption, QLabel*& value, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(typedLabel(caption, "dim", row));
    value = typedLabel(QStringLiteral("—"), "small", row);
    value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addStretch();
    layout->addWidget(value);
    return row;
}

// Icon + name on the left, count chip on the right.
QWidget* countRow(Glyph glyph, const QString& name, QLabel*& count, QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 4, 0, 4);
    layout->setSpacing(9);

    auto* icon = new QLabel(row);
    icon->setPixmap(glyphPixmap(glyph, 16, palette::TextSecondary));
    icon->setFixedWidth(16);
    layout->addWidget(icon);
    layout->addWidget(typedLabel(name, "small", row));
    layout->addStretch();

    count = typedLabel(QStringLiteral("—"), "small", row);
    count->setStyleSheet(QStringLiteral("font-weight:600;color:%1").arg(palette::TextPrimary.name()));
    layout->addWidget(count);
    return row;
}

QString formatDuration(qint64 milliseconds)
{
    const qint64 totalSeconds = milliseconds / 1000;
    return QStringLiteral("%1:%2:%3")
        .arg(totalSeconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((totalSeconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace

SurveyPanel::SurveyPanel(QWidget* parent) : QFrame(parent)
{
    setFixedWidth(248);
    setStyleSheet(QStringLiteral("background:transparent"));

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    auto* column = new QVBoxLayout(content);
    column->setContentsMargins(0, 0, 6, 0);
    column->setSpacing(10);

    // ---- Sequence card -----------------------------------------------
    column->addWidget(sectionLabel(QStringLiteral("Video & survey"), content));

    auto* sequenceCard = card(content);
    auto* sequenceBody = new QVBoxLayout(sequenceCard);
    sequenceBody->setContentsMargins(12, 11, 12, 12);
    sequenceBody->setSpacing(4);

    auto* titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(6);
    m_sequenceName = typedLabel(QStringLiteral("No sequence loaded"), "subtitle", sequenceCard);
    m_sequenceName->setWordWrap(true);
    m_stateBadge = new Badge(sequenceCard);
    m_stateBadge->setText(QStringLiteral("Empty"));
    m_stateBadge->setAccent(palette::TextDim);
    titleRow->addWidget(m_sequenceName, 1);
    titleRow->addWidget(m_stateBadge, 0, Qt::AlignTop);
    sequenceBody->addLayout(titleRow);
    sequenceBody->addSpacing(4);

    sequenceBody->addWidget(metaRow(QStringLiteral("Frames"), m_recorded, sequenceCard));
    sequenceBody->addWidget(metaRow(QStringLiteral("Duration"), m_duration, sequenceCard));
    sequenceBody->addWidget(metaRow(QStringLiteral("Frame rate"), m_frameRate, sequenceCard));
    sequenceBody->addWidget(metaRow(QStringLiteral("Resolution"), m_resolution, sequenceCard));
    sequenceBody->addWidget(metaRow(QStringLiteral("Annotations"), m_source, sequenceCard));
    column->addWidget(sequenceCard);

    m_open = textButton(QStringLiteral("Open sequence"), content, "primary");
    m_open->setIcon(glyphIcon(Glyph::Video, 15, Qt::white, Qt::white));
    connect(m_open, &QPushButton::clicked, this, &SurveyPanel::openSequenceRequested);
    column->addWidget(m_open);

    // ---- Contents ----------------------------------------------------
    column->addSpacing(2);
    column->addWidget(sectionLabel(QStringLiteral("Contents"), content));
    auto* contentsCard = card(content);
    auto* contentsBody = new QVBoxLayout(contentsCard);
    contentsBody->setContentsMargins(12, 6, 12, 6);
    contentsBody->setSpacing(0);
    contentsBody->addWidget(
        countRow(Glyph::Annotations, QStringLiteral("Annotations"), m_annotationCount, contentsCard));
    contentsBody->addWidget(
        countRow(Glyph::Tracks, QStringLiteral("Tracks"), m_trackCount, contentsCard));
    contentsBody->addWidget(
        countRow(Glyph::Species, QStringLiteral("Species"), m_speciesCount, contentsCard));
    contentsBody->addWidget(
        countRow(Glyph::Events, QStringLiteral("Events"), m_eventCount, contentsCard));
    column->addWidget(contentsCard);

    // ---- Filters -----------------------------------------------------
    column->addSpacing(2);
    column->addWidget(sectionLabel(QStringLiteral("Filters"), content));
    auto* filterCard = card(content);
    auto* filterBody = new QVBoxLayout(filterCard);
    filterBody->setContentsMargins(12, 11, 12, 12);
    filterBody->setSpacing(8);

    filterBody->addWidget(typedLabel(QStringLiteral("Species"), "dim", filterCard));
    m_species = new QComboBox(filterCard);
    m_species->addItem(QStringLiteral("All species"), 0);
    connect(m_species, &QComboBox::currentIndexChanged, this, &SurveyPanel::filtersChanged);
    filterBody->addWidget(m_species);

    auto* confidenceHead = new QHBoxLayout;
    confidenceHead->setContentsMargins(0, 0, 0, 0);
    confidenceHead->addWidget(typedLabel(QStringLiteral("Minimum confidence"), "dim", filterCard));
    confidenceHead->addStretch();
    m_confidenceValue = typedLabel(QStringLiteral("0.00"), "small", filterCard);
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

    filterBody->addWidget(divider(filterCard));

    auto* detections = new ToggleRow(QStringLiteral("Show detections"), true, filterCard);
    auto* tracks = new ToggleRow(QStringLiteral("Show tracks"), true, filterCard);
    auto* ids = new ToggleRow(QStringLiteral("Show track IDs"), true, filterCard);
    m_showDetections = detections->toggle();
    m_showTracks = tracks->toggle();
    m_showIds = ids->toggle();
    for (auto* row : {detections, tracks, ids}) {
        connect(row, &ToggleRow::toggled, this, &SurveyPanel::filtersChanged);
        filterBody->addWidget(row);
    }
    column->addWidget(filterCard);

    // ---- Export ------------------------------------------------------
    column->addSpacing(2);
    column->addWidget(sectionLabel(QStringLiteral("Export"), content));
    m_csv = textButton(QStringLiteral("Observations CSV"), content);
    m_csv->setIcon(glyphIcon(Glyph::Export, 15, palette::TextSecondary, palette::TextPrimary));
    m_mot = textButton(QStringLiteral("MOTChallenge"), content);
    m_mot->setIcon(glyphIcon(Glyph::Export, 15, palette::TextSecondary, palette::TextPrimary));
    m_csv->setEnabled(false);
    m_mot->setEnabled(false);
    connect(m_csv, &QPushButton::clicked, this, &SurveyPanel::exportCsvRequested);
    connect(m_mot, &QPushButton::clicked, this, &SurveyPanel::exportMotRequested);
    column->addWidget(m_csv);
    column->addWidget(m_mot);

    column->addStretch();
    scroll->setWidget(content);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
}

void SurveyPanel::setSequenceInfo(const QString& name, int frames, int frameRate, int width,
                                  int height)
{
    m_sequenceName->setText(name);
    m_stateBadge->setText(QStringLiteral("Loaded"));
    m_stateBadge->setAccent(palette::Success);
    m_recorded->setText(QString::number(frames));
    m_duration->setText(
        formatDuration(frameRate > 0 ? qint64(frames) * 1000 / frameRate : 0));
    m_frameRate->setText(QStringLiteral("%1 fps").arg(frameRate));
    m_resolution->setText(QStringLiteral("%1 x %2").arg(width).arg(height));
    m_source->setText(QStringLiteral("MOTChallenge"));
}

void SurveyPanel::setCounts(int annotations, int tracks, int species, int events)
{
    m_annotationCount->setText(QString::number(annotations));
    m_trackCount->setText(QString::number(tracks));
    m_speciesCount->setText(QString::number(species));
    m_eventCount->setText(QString::number(events));
}

void SurveyPanel::setSpecies(const QVector<QPair<int, QString>>& species)
{
    const int previous = m_species->currentData().toInt();
    QSignalBlocker blocker(m_species);
    m_species->clear();
    m_species->addItem(QStringLiteral("All species"), 0);
    for (const auto& [classId, name] : species)
        m_species->addItem(name, classId);
    const int index = m_species->findData(previous);
    m_species->setCurrentIndex(index >= 0 ? index : 0);
}

void SurveyPanel::setExportEnabled(bool enabled)
{
    m_csv->setEnabled(enabled);
    m_mot->setEnabled(enabled);
}

SurveyPanel::Filters SurveyPanel::filters() const
{
    Filters filters;
    filters.showDetections = m_showDetections->isChecked();
    filters.showTracks = m_showTracks->isChecked();
    filters.showIds = m_showIds->isChecked();
    filters.speciesClassId = m_species->currentData().toInt();
    filters.minConfidence = m_confidence->value() / 100.0;
    return filters;
}
