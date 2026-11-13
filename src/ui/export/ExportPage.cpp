#include "ui/export/ExportPage.h"

#include "application/controllers/ExportController.h"
#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <set>
#include <utility>

using namespace aquatrack::ui;
namespace domain = aquatrack::domain;

namespace {

QString grouped(std::size_t value)
{
    return QLocale::system().toString(qulonglong(value));
}

// One export format: what it is, who it is for, and the button that writes it.
QFrame* formatCard(Glyph glyph, const QString& title, const QString& purpose,
                   const QString& detail, QPushButton* action, QWidget* parent)
{
    auto* frame = card(parent);
    auto* column = new QVBoxLayout(frame);
    column->setContentsMargins(18, 16, 18, 18);
    column->setSpacing(8);

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(10);
    auto* icon = new QLabel(frame);
    icon->setFixedSize(36, 36);
    icon->setAlignment(Qt::AlignCenter);
    icon->setPixmap(glyphPixmap(glyph, 18, palette::AccentHover));
    icon->setStyleSheet(QStringLiteral("background:%1;border:1px solid %2;border-radius:10px")
                            .arg(palette::Sunken.name(), palette::Line.name()));
    head->addWidget(icon);
    auto* heading = typedLabel(title, "subtitle", frame);
    head->addWidget(heading, 1);
    column->addLayout(head);

    auto* purposeLabel = typedLabel(purpose, "small", frame);
    purposeLabel->setWordWrap(true);
    column->addWidget(purposeLabel);
    auto* detailLabel = typedLabel(detail, "mono", frame);
    detailLabel->setWordWrap(true);
    column->addWidget(detailLabel);
    column->addStretch();
    action->setParent(frame);
    column->addWidget(action, 0, Qt::AlignLeft);
    return frame;
}

}  // namespace

ExportPage::ExportPage(QWidget* parent) : QWidget(parent)
{
    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(24, 20, 24, 20);
    page->setSpacing(14);

    // ---- Heading ----------------------------------------------------------
    page->addWidget(sectionLabel(QStringLiteral("Deliver"), this));
    page->addWidget(typedLabel(QStringLiteral("Export"), "display", this));
    m_source = typedLabel(QString(), "dim", this);
    page->addWidget(m_source);

    // ---- Nothing to export ------------------------------------------------
    m_emptyState = card(this);
    auto* empty = new QVBoxLayout(m_emptyState);
    empty->setContentsMargins(18, 18, 18, 18);
    empty->setSpacing(10);
    auto* emptyText = typedLabel(
        QStringLiteral("Nothing to export yet. Open a sequence in Review: its observations, "
                       "including your corrections, are what this page writes out."),
        "small", m_emptyState);
    emptyText->setWordWrap(true);
    empty->addWidget(emptyText);
    auto* toReview = textButton(QStringLiteral("Go to review"), m_emptyState, "primary");
    toReview->setIcon(glyphIcon(Glyph::Review, 15, Qt::white, Qt::white));
    connect(toReview, &QPushButton::clicked, this, &ExportPage::reviewRequested);
    empty->addWidget(toReview, 0, Qt::AlignLeft);
    page->addWidget(m_emptyState);

    // ---- What will be written -------------------------------------------
    m_content = new QWidget(this);
    auto* content = new QVBoxLayout(m_content);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(14);

    auto* summary = card(m_content);
    auto* summaryBody = new QVBoxLayout(summary);
    summaryBody->setContentsMargins(18, 14, 18, 16);
    summaryBody->setSpacing(12);
    auto* tiles = new QHBoxLayout;
    tiles->setContentsMargins(0, 0, 0, 0);
    tiles->setSpacing(10);
    m_rows = new MetricTile(QStringLiteral("Observations to export"), summary);
    m_tracks = new MetricTile(QStringLiteral("Tracks"), summary);
    m_verified = new MetricTile(QStringLiteral("Human-verified"), summary);
    m_frames = new MetricTile(QStringLiteral("Frames covered"), summary);
    for (auto* tile : {m_rows, m_tracks, m_verified, m_frames})
        tiles->addWidget(tile);
    summaryBody->addLayout(tiles);
    summaryBody->addWidget(divider(summary));

    m_includeUnverified =
        new QCheckBox(QStringLiteral("Include observations no reviewer has verified"), summary);
    m_includeUnverified->setObjectName(QStringLiteral("includeUnverified"));
    m_includeUnverified->setChecked(true);
    connect(m_includeUnverified, &QCheckBox::toggled, this, [this](bool include) {
        refresh();
        emit includeUnverifiedChanged(include);
    });
    summaryBody->addWidget(m_includeUnverified);
    auto* optionNote = typedLabel(
        QStringLiteral("Ground truth and anything you relabelled or merged counts as verified. "
                       "Untick to export only verified observations."),
        "dim", summary);
    optionNote->setWordWrap(true);
    summaryBody->addWidget(optionNote);
    content->addWidget(summary);

    // ---- Formats ----------------------------------------------------------
    content->addWidget(sectionLabel(QStringLiteral("Formats"), m_content));
    auto* formats = new QHBoxLayout;
    formats->setContentsMargins(0, 0, 0, 0);
    formats->setSpacing(12);

    m_csv = textButton(QStringLiteral("Export CSV..."), m_content, "primary");
    m_csv->setObjectName(QStringLiteral("exportCsv"));
    m_csv->setIcon(glyphIcon(Glyph::Export, 15, Qt::white, Qt::white));
    connect(m_csv, &QPushButton::clicked, this, [this] { exportAs(Format::Csv); });
    formats->addWidget(formatCard(
        Glyph::Files, QStringLiteral("Observations CSV"),
        QStringLiteral("Every observation with its track, species, confidence, box and review "
                       "state. Opens in Excel, R, pandas or any GIS."),
        QStringLiteral("observation_id, video_id, timestamp_ms, frame_index, local_track_id, "
                       "class, confidence, x1, y1, x2, y2, human_verified"),
        m_csv, m_content), 1);

    m_mot = textButton(QStringLiteral("Export MOTChallenge..."), m_content, "primary");
    m_mot->setObjectName(QStringLiteral("exportMot"));
    m_mot->setIcon(glyphIcon(Glyph::Export, 15, Qt::white, Qt::white));
    connect(m_mot, &QPushButton::clicked, this, [this] { exportAs(Format::MotChallenge); });
    formats->addWidget(formatCard(
        Glyph::Tracks, QStringLiteral("MOTChallenge gt.txt"),
        QStringLiteral("Tracks in the benchmark format used by BrackishMOT and tracker "
                       "evaluation tools such as TrackEval, for training or scoring."),
        QStringLiteral("frame, id, x, y, width, height, confidence, -1, -1, -1"), m_mot,
        m_content), 1);
    content->addLayout(formats);

    // ---- Where it went ----------------------------------------------------
    auto* resultRow = new QHBoxLayout;
    resultRow->setContentsMargins(0, 0, 0, 0);
    resultRow->setSpacing(10);
    m_result = typedLabel(QString(), "small", m_content);
    m_result->setObjectName(QStringLiteral("exportResult"));
    m_result->setWordWrap(true);
    resultRow->addWidget(m_result, 1);
    m_reveal = textButton(QStringLiteral("Show in folder"), m_content);
    m_reveal->setIcon(glyphIcon(Glyph::Files, 15, palette::TextSecondary, palette::TextPrimary));
    m_reveal->hide();
    connect(m_reveal, &QPushButton::clicked, this, [this] {
        if (!m_lastExport.isEmpty())
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(QFileInfo(m_lastExport).absolutePath()));
    });
    resultRow->addWidget(m_reveal);
    content->addLayout(resultRow);

    page->addWidget(m_content);
    page->addStretch();
    refresh();
}

void ExportPage::setObservations(std::vector<domain::Observation> observations,
                                 const QString& sourceName)
{
    m_observations = std::move(observations);
    m_sourceName = sourceName;
    refresh();
}

void ExportPage::setDefaultFolders(const QString& exports, const QString& annotations)
{
    m_exportsFolder = exports;
    m_annotationsFolder = annotations;
}

void ExportPage::setIncludeUnverified(bool include)
{
    if (m_includeUnverified->isChecked() == include)
        return;
    const QSignalBlocker blocker(m_includeUnverified);
    m_includeUnverified->setChecked(include);
    refresh();
}

std::vector<domain::Observation> ExportPage::exportable() const
{
    if (m_includeUnverified->isChecked())
        return m_observations;
    std::vector<domain::Observation> verified;
    std::copy_if(m_observations.cbegin(), m_observations.cend(), std::back_inserter(verified),
                 [](const domain::Observation& item) { return item.humanVerified; });
    return verified;
}

void ExportPage::refresh()
{
    const bool any = !m_observations.empty();
    m_emptyState->setVisible(!any);
    m_content->setVisible(any);
    m_source->setText(any ? QStringLiteral("Source: %1").arg(m_sourceName)
                          : QStringLiteral("No sequence open"));
    if (!any)
        return;

    const auto rows = exportable();
    std::set<std::pair<domain::VideoId, domain::LocalTrackId>> tracks;
    std::set<std::pair<domain::VideoId, std::int64_t>> frames;
    for (const auto& item : rows) {
        tracks.emplace(item.videoId, item.localTrackId);
        frames.emplace(item.videoId, item.frameIndex);
    }
    const auto verified = std::count_if(m_observations.cbegin(), m_observations.cend(),
                                        [](const auto& item) { return item.humanVerified; });
    m_rows->setValue(grouped(rows.size()));
    m_tracks->setValue(grouped(tracks.size()));
    m_verified->setValue(QStringLiteral("%1%").arg(
        100.0 * double(verified) / double(m_observations.size()), 0, 'f', 0));
    m_frames->setValue(grouped(frames.size()));
    m_csv->setEnabled(!rows.empty());
    m_mot->setEnabled(!rows.empty());
}

void ExportPage::exportAs(Format format)
{
    const auto rows = exportable();
    if (rows.empty())
        return;
    const QString stem = rows.front().videoId.empty()
                             ? QStringLiteral("observations")
                             : QString::fromStdString(rows.front().videoId);
    const bool csv = format == Format::Csv;
    // Inside a project, start in the folder that format belongs in.
    const QString folder = csv ? m_exportsFolder : m_annotationsFolder;
    if (!folder.isEmpty())
        QDir().mkpath(folder);
    const QString name = csv ? QStringLiteral("%1_observations.csv").arg(stem)
                             : QStringLiteral("%1_gt.txt").arg(stem);
    const QString path = QFileDialog::getSaveFileName(
        this, csv ? QStringLiteral("Export observations CSV")
                  : QStringLiteral("Export MOTChallenge"),
        folder.isEmpty() ? name : QDir(folder).filePath(name),
        csv ? QStringLiteral("CSV files (*.csv)") : QStringLiteral("Text files (*.txt)"));
    if (path.isEmpty())
        return;

    aquatrack::application::ExportController exporter;
    QString error;
    const bool written = csv ? exporter.exportCsv(path, rows, &error)
                             : exporter.exportMot(path, rows, &error);
    if (!written) {
        QMessageBox::warning(this, QStringLiteral("Export failed"), error);
        return;
    }
    m_lastExport = path;
    m_result->setText(QStringLiteral("Wrote %1 observation(s) to %2")
                          .arg(grouped(rows.size()), QDir::toNativeSeparators(path)));
    m_reveal->show();
}
