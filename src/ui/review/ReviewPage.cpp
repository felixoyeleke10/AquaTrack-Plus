#include "ui/review/ReviewPage.h"

#include "application/controllers/AnnotationController.h"
#include "application/controllers/ExportController.h"
#include "application/controllers/ReviewController.h"
#include "data/observations/JsonObservationRepository.h"
#include "ui/review/RuntimeStrip.h"
#include "ui/review/center/ReviewPanel.h"
#include "ui/review/center/video/SequenceView.h"
#include "ui/review/left/SurveyPanel.h"
#include "ui/review/right/InsightPanel.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace analytics = aquatrack::analytics;
namespace domain = aquatrack::domain;

namespace {

constexpr int kFilmstripFrames = 8;
constexpr int kTrailLength = 24;

int classIdForName(const QString& name)
{
    for (int classId = 1; classId <= 6; ++classId)
        if (aquatrack::BrackishMotSequence::className(classId) == name)
            return classId;
    return 0;
}

}  // namespace

ReviewPage::ReviewPage(QWidget* parent)
    : QWidget(parent),
      m_controller(new aquatrack::application::ReviewController(this)),
      m_exportController(new aquatrack::application::ExportController(this))
{
    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(12, 12, 12, 12);
    page->setSpacing(10);

    auto* columns = new QHBoxLayout;
    columns->setContentsMargins(0, 0, 0, 0);
    columns->setSpacing(12);
    m_surveyPanel = new SurveyPanel(this);
    m_reviewPanel = new ReviewPanel(this);
    m_insightPanel = new InsightPanel(this);
    columns->addWidget(m_surveyPanel);
    columns->addWidget(m_reviewPanel, 1);
    columns->addWidget(m_insightPanel);
    page->addLayout(columns, 1);

    m_runtime = new RuntimeStrip(this);
    page->addWidget(m_runtime);

    // ---- Left rail ----------------------------------------------------
    connect(m_surveyPanel, &SurveyPanel::openSequenceRequested, this, &ReviewPage::openSequence);
    connect(m_surveyPanel, &SurveyPanel::filtersChanged, this, &ReviewPage::applyFilters);
    connect(m_surveyPanel, &SurveyPanel::exportCsvRequested, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this,
            QStringLiteral("Export observations"), QStringLiteral("observations.csv"),
            QStringLiteral("CSV files (*.csv)"));
        if (path.isEmpty())
            return;
        QString error;
        if (!m_exportController->exportCsv(path, observations(), &error))
            QMessageBox::warning(this, QStringLiteral("Export failed"), error);
    });
    connect(m_surveyPanel, &SurveyPanel::exportMotRequested, this, [this] {
        const QString path = QFileDialog::getSaveFileName(this,
            QStringLiteral("Export MOTChallenge"), QStringLiteral("gt.txt"),
            QStringLiteral("Text files (*.txt)"));
        if (path.isEmpty())
            return;
        QString error;
        if (!m_exportController->exportMot(path, observations(), &error))
            QMessageBox::warning(this, QStringLiteral("Export failed"), error);
    });

    // ---- Centre -------------------------------------------------------
    connect(m_reviewPanel, &ReviewPanel::playToggled, m_controller,
            &aquatrack::application::ReviewController::togglePlayback);
    connect(m_reviewPanel, &ReviewPanel::frameRequested, m_controller,
            &aquatrack::application::ReviewController::setFrame);
    connect(m_reviewPanel, &ReviewPanel::stepRequested, this, [this](int delta) {
        m_controller->setFrame(m_currentFrame + delta);
    });

    // ---- Right rail ---------------------------------------------------
    connect(m_insightPanel, &InsightPanel::relabelRequested, this, [this](const QString& id) {
        if (!m_annotationController)
            return;
        QStringList classes;
        for (int classId = 1; classId <= 6; ++classId)
            classes << aquatrack::BrackishMotSequence::className(classId);
        classes << QStringLiteral("Unknown");
        bool accepted = false;
        const QString label =
            QInputDialog::getItem(this, QStringLiteral("Relabel observation"),
                                  QStringLiteral("Species"), classes, 0, false, &accepted);
        if (!accepted)
            return;
        const double confidence =
            QInputDialog::getDouble(this, QStringLiteral("Reviewer confidence"),
                                    QStringLiteral("Confidence"), 1.0, 0.0, 1.0, 2, &accepted);
        if (!accepted)
            return;
        if (m_annotationController->relabel(id.toStdString(), label.toStdString(),
                                            static_cast<float>(confidence)))
            refreshFrameDetail();
    });
    connect(m_insightPanel, &InsightPanel::deleteRequested, this, [this](const QString& id) {
        if (m_annotationController && m_annotationController->erase(id.toStdString()))
            refreshFrameDetail();
    });
    connect(m_insightPanel, &InsightPanel::undoRequested, this, [this] {
        if (m_annotationController && m_annotationController->undo())
            refreshFrameDetail();
    });
    connect(m_insightPanel, &InsightPanel::redoRequested, this, [this] {
        if (m_annotationController && m_annotationController->redo())
            refreshFrameDetail();
    });
    connect(m_insightPanel, &InsightPanel::saveRequested, this, [this] {
        if (!m_annotationController)
            return;
        QString error;
        if (!m_annotationController->save(&error))
            QMessageBox::warning(this, QStringLiteral("Save annotations failed"), error);
        refreshHistoryState();
    });
    connect(m_insightPanel, &InsightPanel::mergeTracksRequested, this,
            [this](qint64 source, qint64 target) {
                QMessageBox::information(
                    this, QStringLiteral("Merge tracks"),
                    QStringLiteral("Merging track %1 into %2 is not wired to the annotation "
                                   "session yet. AnnotationSession::mergeTracks exists in the "
                                   "domain layer but AnnotationController does not expose it.")
                        .arg(source)
                        .arg(target));
            });
    connect(m_insightPanel, &InsightPanel::frameRequested, this, [this](int timestampMs) {
        // The insight panel reports a timestamp; convert to a frame index.
        m_controller->setFrame(frameForTimestamp(timestampMs));
    });

    // ---- Controller ---------------------------------------------------
    connect(m_controller, &aquatrack::application::ReviewController::sequenceLoaded, this,
            [this](const QString& name, int frames, int frameRate, int width, int height) {
                m_sequenceFrames = frames;
                m_frameRate = std::max(1, frameRate);
                m_imageWidth = width;
                m_imageHeight = height;

                m_surveyPanel->setSequenceInfo(name, frames, frameRate, width, height);
                m_surveyPanel->setExportEnabled(true);
                m_reviewPanel->configureSequence(frames, frameRate);
                m_runtime->setSequenceState(true, frames);

                if (m_annotationController) {
                    const auto videoId = name.toStdString();
                    if (m_repository->containsVideo(videoId)) {
                        m_annotationController->open(videoId);
                    } else {
                        m_annotationController->begin(videoId, m_controller->observations());
                        QString error;
                        if (!m_annotationController->save(&error))
                            QMessageBox::warning(
                                this, QStringLiteral("Observation storage failed"), error);
                    }
                    m_insightPanel->setEditingEnabled(true);
                }
                buildFilmstrip();
                rebuildAnalytics();
                refreshHistoryState();
            });
    connect(m_controller, &aquatrack::application::ReviewController::frameReady, this,
            [this](const QImage& image, QVector<aquatrack::GroundTruth> annotations, int frame,
                   int total) { displayFrame(image, std::move(annotations), frame, total); });
    connect(m_controller, &aquatrack::application::ReviewController::playbackChanged,
            m_reviewPanel, &ReviewPanel::setPlaying);
    connect(m_controller, &aquatrack::application::ReviewController::errorOccurred, this,
            [this](const QString& error) {
                QMessageBox::warning(this, QStringLiteral("Review error"), error);
            });

    applyFilters();
}

ReviewPage::~ReviewPage() = default;

std::vector<domain::Observation> ReviewPage::observations() const
{
    if (m_annotationController)
        return m_annotationController->observations();
    return m_controller->observations();
}

void ReviewPage::loadSequence(const QString& directory)
{
    m_sequenceDirectory = directory;
    m_controller->loadSequence(directory);
}

bool ReviewPage::setObservationStore(const QString& path, QString* error)
{
    auto repository = std::make_unique<aquatrack::data::JsonObservationRepository>(path);
    std::string message;
    if (!repository->open(message)) {
        if (error)
            *error = QString::fromStdString(message);
        return false;
    }
    m_annotationController.reset();
    m_repository = std::move(repository);
    m_annotationController =
        std::make_unique<aquatrack::application::AnnotationController>(*m_repository);
    m_runtime->setObservationStore(path);
    return true;
}

void ReviewPage::openSequence()
{
    const QString directory =
        QFileDialog::getExistingDirectory(this, QStringLiteral("Open BrackishMOT sequence"));
    if (!directory.isEmpty())
        loadSequence(directory);
}

void ReviewPage::applyFilters()
{
    const auto filters = m_surveyPanel->filters();
    SequenceView::Overlays overlays;
    overlays.showDetections = filters.showDetections;
    overlays.showTracks = filters.showTracks;
    overlays.showIds = filters.showIds;
    overlays.speciesClassId = filters.speciesClassId;
    overlays.minConfidence = filters.minConfidence;
    m_reviewPanel->sequenceView()->setOverlays(overlays);
    refreshFrameDetail();
}

qint64 ReviewPage::timestampFor(int frame) const
{
    return qint64(std::max(0, frame - 1)) * 1000 / m_frameRate;
}

int ReviewPage::frameForTimestamp(qint64 milliseconds) const
{
    const int frame = int(std::max<qint64>(0, milliseconds) * m_frameRate / 1000) + 1;
    return std::clamp(frame, 1, std::max(1, m_sequenceFrames));
}

QHash<int, QVector<QPointF>> ReviewPage::trailsFor(int frame) const
{
    QHash<int, QVector<QPointF>> trails;
    const auto all = observations();
    const auto videoId = all.empty() ? std::string() : all.front().videoId;
    for (const auto& observation : all) {
        if (observation.frameIndex > frame || observation.frameIndex <= frame - kTrailLength)
            continue;
        trails[int(observation.localTrackId)].push_back(
            QPointF(observation.box.centerX(), observation.box.centerY()));
    }
    // Drop single-point trails; a dot is noise, not motion.
    for (auto it = trails.begin(); it != trails.end();)
        it = it.value().size() < 2 ? trails.erase(it) : std::next(it);
    return trails;
}

void ReviewPage::displayFrame(const QImage& image, QVector<aquatrack::GroundTruth> annotations,
                              int frame, int total)
{
    m_currentFrame = frame;
    m_annotations = std::move(annotations);

    // With an annotation session open, the edited observations are the truth
    // on screen, not the on-disk ground truth the controller emitted.
    if (m_annotationController) {
        m_annotations.clear();
        for (const auto& observation : m_annotationController->observations()) {
            if (observation.frameIndex != frame)
                continue;
            aquatrack::GroundTruth annotation;
            annotation.frame = frame;
            annotation.trackId = int(observation.localTrackId);
            annotation.box = QRectF(observation.box.x1, observation.box.y1,
                                    observation.box.width(), observation.box.height());
            annotation.confidence = observation.confidence;
            annotation.classId =
                classIdForName(QString::fromStdString(observation.className));
            annotation.visibility = 1.0F;
            m_annotations.push_back(annotation);
        }
    }

    m_reviewPanel->showFrame(image, m_annotations, frame, total, timestampFor(frame));
    m_reviewPanel->sequenceView()->setTrails(trailsFor(frame));
    m_runtime->setFrameInfo(frame, total, m_imageWidth, m_imageHeight);
    refreshFrameDetail();
}

void ReviewPage::refreshFrameDetail()
{
    const auto filters = m_surveyPanel->filters();
    m_currentObservations.clear();
    if (m_annotationController) {
        for (const auto& observation : m_annotationController->observations()) {
            if (observation.frameIndex != m_currentFrame)
                continue;
            const int classId = classIdForName(QString::fromStdString(observation.className));
            if (filters.speciesClassId != 0 && classId != filters.speciesClassId)
                continue;
            if (double(observation.confidence) < filters.minConfidence)
                continue;
            m_currentObservations.push_back(observation);
        }
    }
    m_insightPanel->setCurrentObservations(m_currentObservations);
    refreshHistoryState();
}

void ReviewPage::refreshHistoryState()
{
    if (!m_annotationController) {
        m_insightPanel->setHistoryState(false, false, false);
        return;
    }
    m_insightPanel->setHistoryState(m_annotationController->canUndo(),
                                    m_annotationController->canRedo(),
                                    m_annotationController->isDirty());
}

void ReviewPage::buildFilmstrip()
{
    QVector<aquatrack::ui::Filmstrip::Thumbnail> thumbnails;
    if (m_sequenceFrames <= 0 || m_sequenceDirectory.isEmpty()) {
        m_reviewPanel->filmstrip()->clear();
        return;
    }
    aquatrack::BrackishMotSequence sequence;
    if (!sequence.load(m_sequenceDirectory)) {
        m_reviewPanel->filmstrip()->clear();
        return;
    }
    const int step = std::max(1, m_sequenceFrames / kFilmstripFrames);
    for (int frame = 1; frame <= m_sequenceFrames && thumbnails.size() < kFilmstripFrames;
         frame += step) {
        QImage image(sequence.imagePath(frame));
        if (image.isNull())
            continue;
        aquatrack::ui::Filmstrip::Thumbnail thumbnail;
        thumbnail.frame = frame;
        // Scale on load; keeping eight full frames resident costs far more
        // memory than the strip is worth.
        thumbnail.image = image.scaledToWidth(190, Qt::SmoothTransformation);
        const qint64 milliseconds = timestampFor(frame);
        thumbnail.caption = QStringLiteral("%1:%2")
                                .arg(milliseconds / 60000, 2, 10, QLatin1Char('0'))
                                .arg((milliseconds / 1000) % 60, 2, 10, QLatin1Char('0'));
        thumbnails.push_back(std::move(thumbnail));
    }
    m_reviewPanel->filmstrip()->setThumbnails(std::move(thumbnails));
}

void ReviewPage::rebuildAnalytics()
{
    const auto all = observations();
    if (all.empty()) {
        m_reviewPanel->clearAnalytics();
        m_insightPanel->setIdSwitch(std::nullopt, 0);
        return;
    }

    const auto summary = analytics::summarize(all);
    const auto spans = analytics::trackSpans(all);
    const auto peak = analytics::peakConcurrentTracks(all);

    // Aim for roughly forty bins across the sequence, whatever its length.
    const auto sequenceMs = timestampFor(m_sequenceFrames);
    const auto binWidth = std::max<domain::TimestampMs>(1, sequenceMs / 40);
    const auto bins = analytics::binObservations(all, binWidth);
    const auto events = analytics::notableEvents(all, binWidth);
    m_idSwitches = analytics::idSwitchCandidates(all);

    m_reviewPanel->setAnalytics(bins, summary, spans, m_frameRate);
    m_reviewPanel->setTracksTable(spans);
    m_reviewPanel->setEventsTable(events);

    // ---- Sequence timeline -------------------------------------------
    // Where each track enters and leaves.
    QVector<aquatrack::ui::EventTimeline::Tick> ticks;
    ticks.reserve(int(spans.size()) * 2);
    for (const auto& span : spans) {
        ticks.push_back({int(span.firstFrame), int(span.trackId), true});
        ticks.push_back({int(span.lastFrame), int(span.trackId), false});
    }
    m_reviewPanel->timeline()->setTicks(std::move(ticks));

    // Derived events, as pins for moments and bands for intervals. Events
    // with no position in time (totals such as "N very short tracks") are
    // summarised in the right rail instead of being pinned at zero.
    QVector<aquatrack::ui::EventTimeline::Marker> markers;
    for (const auto& event : events) {
        if (event.atMs == 0 && event.untilMs == 0)
            continue;
        aquatrack::ui::EventTimeline::Marker marker;
        marker.startFrame = frameForTimestamp(event.atMs);
        marker.endFrame = frameForTimestamp(std::max(event.atMs, event.untilMs));
        marker.color = event.severity == analytics::EventSeverity::Alert    ? QColor(0xff, 0x6b, 0x6b)
                       : event.severity == analytics::EventSeverity::Warning ? QColor(0xf5, 0xa6, 0x23)
                                                                            : QColor(0x2e, 0x9b, 0xf0);
        marker.label = QString::fromStdString(event.description);
        markers.push_back(std::move(marker));
    }
    m_reviewPanel->timeline()->setMarkers(std::move(markers));

    m_insightPanel->setSummary(summary, peak);
    m_insightPanel->setEvents(events);
    m_insightPanel->setIdSwitch(m_idSwitches.empty()
                                    ? std::optional<analytics::IdSwitchCandidate>{}
                                    : std::optional(m_idSwitches.front()),
                                int(m_idSwitches.size()));

    // Mean confidence per bin.
    std::map<std::size_t, std::pair<double, std::size_t>> perBin;
    for (const auto& observation : all) {
        const auto index =
            std::size_t((observation.timestampMs - summary.firstObservationMs) / binWidth);
        auto& entry = perBin[index];
        entry.first += observation.confidence;
        ++entry.second;
    }
    QVector<double> confidence;
    for (std::size_t index = 0; index < bins.size(); ++index) {
        const auto found = perBin.find(index);
        confidence.push_back(found == perBin.end() || found->second.second == 0
                                 ? 0.0
                                 : found->second.first / double(found->second.second));
    }
    m_insightPanel->setConfidenceSeries(
        confidence, confidence.isEmpty() ? 0.0 : confidence.last());

    // Species present, for the left-rail filter.
    QVector<QPair<int, QString>> species;
    std::set<int> seen;
    for (const auto& [name, count] : summary.observationsByClass) {
        const QString label = QString::fromStdString(name);
        const int classId = classIdForName(label);
        if (seen.insert(classId).second)
            species.push_back({classId, label});
    }
    m_surveyPanel->setSpecies(species);
    m_surveyPanel->setCounts(int(summary.totalObservations), int(summary.uniqueLocalTracks),
                             int(summary.observationsByClass.size()), int(events.size()));
}
