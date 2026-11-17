#include "ui/review/ReviewPage.h"

#include "application/AnnotationImporter.h"
#include "application/DetectionMerge.h"
#include "application/controllers/AnnotationController.h"
#include "application/controllers/ReviewController.h"
#include "data/media/FrameDecoder.h"
#include "data/media/VideoProbe.h"
#include "data/observations/JsonObservationRepository.h"
#include "ui/AppTheme.h"
#include "ui/review/center/LogView.h"
#include "ui/review/center/ReviewPanel.h"
#include "ui/review/center/video/SequenceView.h"
#include "ui/review/left/SurveyPanel.h"
#include "ui/review/right/InsightPanel.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QInputDialog>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QPointer>
#include <QShortcut>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <exception>
#include <map>
#include <set>
#include <utility>

namespace analytics = aquatrack::analytics;
namespace domain = aquatrack::domain;
namespace media = aquatrack::media;

namespace {

// `name` inside `folder` (created on demand), or just `name` without one.
QString suggestedPath(const QString& folder, const QString& name)
{
    if (folder.isEmpty())
        return name;
    QDir().mkpath(folder);
    return QDir(folder).filePath(name);
}

constexpr int kFilmstripFrames = 8;
constexpr int kTrailLength = 24;
constexpr int kThumbnailWidth = 190;

int classIdForName(const QString& name)
{
    return aquatrack::BrackishMotSequence::classIdForName(name);
}

QString frameRateText(double rate)
{
    return std::abs(rate - std::round(rate)) < 0.01 ? QString::number(int(std::lround(rate)))
                                                     : QString::number(rate, 'f', 2);
}

}  // namespace

ReviewPage::ReviewPage(QWidget* parent)
    : QWidget(parent),
      m_controller(new aquatrack::application::ReviewController(this)),
      m_analysis(new aquatrack::application::AnalysisRunner(this))
{
    m_thumbnailPool.setMaxThreadCount(1);

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

    // ---- Left rail ----------------------------------------------------
    connect(m_surveyPanel, &SurveyPanel::filtersChanged, this, &ReviewPage::applyFilters);
    connect(m_surveyPanel, &SurveyPanel::sequenceFileRequested, this,
            [this](int frame) { m_controller->setFrame(frame); });

    // ---- Centre -------------------------------------------------------
    connect(m_reviewPanel, &ReviewPanel::playToggled, m_controller,
            &aquatrack::application::ReviewController::togglePlayback);
    connect(m_reviewPanel, &ReviewPanel::frameRequested, m_controller,
            &aquatrack::application::ReviewController::setFrame);
    connect(m_reviewPanel, &ReviewPanel::stepRequested, this, [this](int delta) {
        m_controller->stopPlayback();
        m_controller->setFrame(std::clamp(m_currentFrame + delta, 1, std::max(1, m_sequenceFrames)));
    });
    connect(m_reviewPanel, &ReviewPanel::snapshotRequested, this, &ReviewPage::saveSnapshot);
    connect(m_reviewPanel, &ReviewPanel::fullscreenRequested, this, [this] {
        QWidget* top = window();
        if (top->isFullScreen())
            top->showNormal();
        else
            top->showFullScreen();
    });
    connect(m_reviewPanel, &ReviewPanel::importAnnotationsRequested, this,
            &ReviewPage::chooseAnnotationFile);
    connect(m_reviewPanel, &ReviewPanel::analyzeRequested, this, [this] {
        if (!m_annotationController) {
            QMessageBox::information(this, QStringLiteral("Detect & track"),
                                     QStringLiteral("Create or open a project first: detected "
                                                    "tracks are saved in it."));
            return;
        }
        emit analyzeRequested();
    });
    connect(m_reviewPanel, &ReviewPanel::analysisCancelRequested, this, &ReviewPage::cancelAnalysis);
    connect(m_reviewPanel, &ReviewPanel::detectorChosen, this, &ReviewPage::detectorChosen);
    connect(m_reviewPanel, &ReviewPanel::manageModelsRequested, this,
            &ReviewPage::manageModelsRequested);

    // ---- Keyboard -----------------------------------------------------
    const auto shortcut = [this](const QKeySequence& keys, auto&& action) {
        auto* item = new QShortcut(keys, this);
        item->setContext(Qt::WidgetWithChildrenShortcut);
        connect(item, &QShortcut::activated, this, std::forward<decltype(action)>(action));
    };
    shortcut(QKeySequence(Qt::Key_Space), [this] { m_controller->togglePlayback(); });
    shortcut(QKeySequence(Qt::Key_K), [this] { m_controller->togglePlayback(); });
    shortcut(QKeySequence(Qt::Key_Right), [this] { emit m_reviewPanel->stepRequested(1); });
    shortcut(QKeySequence(Qt::Key_Left), [this] { emit m_reviewPanel->stepRequested(-1); });
    shortcut(QKeySequence(Qt::SHIFT | Qt::Key_Right),
             [this] { emit m_reviewPanel->stepRequested(std::max(1, m_frameRate)); });
    shortcut(QKeySequence(Qt::SHIFT | Qt::Key_Left),
             [this] { emit m_reviewPanel->stepRequested(-std::max(1, m_frameRate)); });
    shortcut(QKeySequence(Qt::Key_Home), [this] { m_controller->setFrame(1); });
    shortcut(QKeySequence(Qt::Key_End), [this] { m_controller->setFrame(m_sequenceFrames); });

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
            annotationsEdited();
    });
    connect(m_insightPanel, &InsightPanel::deleteRequested, this, [this](const QString& id) {
        if (m_annotationController && m_annotationController->erase(id.toStdString()))
            annotationsEdited();
    });
    connect(m_insightPanel, &InsightPanel::undoRequested, this, [this] {
        if (m_annotationController && m_annotationController->undo())
            annotationsEdited();
    });
    connect(m_insightPanel, &InsightPanel::redoRequested, this, [this] {
        if (m_annotationController && m_annotationController->redo())
            annotationsEdited();
    });
    connect(m_insightPanel, &InsightPanel::saveRequested, this, [this] {
        if (!m_annotationController)
            return;
        QString error;
        if (!m_annotationController->save(&error))
            QMessageBox::warning(this, QStringLiteral("Save annotations failed"), error);
        refreshHistoryState();
    });
    connect(m_insightPanel, &InsightPanel::mergeTracksRequested, this, &ReviewPage::mergeTracks);
    connect(m_reviewPanel, &ReviewPanel::timestampRequested, this, [this](qint64 milliseconds) {
        if (m_controller->isLoaded())
            m_controller->setFrame(frameForTimestamp(milliseconds));
    });
    connect(m_insightPanel, &InsightPanel::frameRequested, this, [this](int timestampMs) {
        // The insight panel reports a timestamp; convert to a frame index.
        m_controller->setFrame(frameForTimestamp(timestampMs));
    });

    // ---- Controller ---------------------------------------------------
    connect(m_controller, &aquatrack::application::ReviewController::sequenceLoaded, this,
            [this](const QString& name, int frames, int frameRate, int width, int height) {
                const auto& source = m_controller->source();
                m_sequenceDirectory = source.path();
                m_videoId = name.toStdString();
                m_sequenceFrames = frames;
                m_frameRate = std::max(1, frameRate);
                m_imageWidth = width;
                m_imageHeight = height;
                m_currentFrame = 0;
                m_currentImage = QImage();
                m_groundTruth = m_controller->observations();

                m_reviewPanel->configureSequence(frames, frameRate, source.durationMs());
                updateSequenceNavigator();
                startAnnotationSession();
                buildFilmstrip();
                rebuildAnalytics();
                refreshHistoryState();
                refreshHeader();
                emit observationsChanged();
                emit sequenceOpened(m_sequenceDirectory);
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

    // ---- Detection ----------------------------------------------------
    connect(m_analysis, &aquatrack::application::AnalysisRunner::progress, this,
            [this](int done, int total, const QString& text) {
                m_reviewPanel->setAnalysisProgress(done, total, text);
            });
    connect(m_analysis, &aquatrack::application::AnalysisRunner::finished, this,
            &ReviewPage::analysisFinished);
    // Everything the run prints goes to the log.
    connect(m_analysis, &aquatrack::application::AnalysisRunner::output, this,
            [this](const QString& text, int kind) {
                m_reviewPanel->activityLog()->append(text, LogView::Kind(kind));
            });
    connect(m_analysis, &aquatrack::application::AnalysisRunner::failed, this,
            [this](const QString& error) {
                m_reviewPanel->setAnalysisRunning(false);
                if (error != QLatin1String("Cancelled"))
                    QMessageBox::warning(this, QStringLiteral("Detection failed"), error);
            });

    applyFilters();
}

ReviewPage::~ReviewPage()
{
    m_filmstripGeneration->fetch_add(1);
    m_thumbnailPool.clear();
    m_thumbnailPool.waitForDone();
}

const media::MediaSource& ReviewPage::source() const
{
    return m_controller->source();
}

void ReviewPage::setFrameCacheSize(int frames)
{
    m_controller->setDecoderCacheSize(frames);
}

void ReviewPage::closeMedia()
{
    if (m_analysis->isRunning())
        m_analysis->cancel();
    m_reviewPanel->setAnalysisRunning(false);
    m_controller->close();
    m_sequenceDirectory.clear();
    m_videoId.clear();
    m_groundTruth.clear();
    m_annotations.clear();
    m_currentImage = QImage();
    m_currentFrame = 0;
    m_sequenceFrames = 0;
    m_filmstripGeneration->fetch_add(1);
    m_reviewPanel->filmstrip()->clear();
    m_reviewPanel->sequenceView()->clear();
    m_reviewPanel->configureSequence(0, m_frameRate, 0);
    // A session on the old media would otherwise keep showing (and saving)
    // its annotations until the store switches.
    if (m_annotationController)
        m_annotationController->begin({}, {});
    updateSequenceNavigator();
    rebuildFrameIndex();
    rebuildAnalytics();
    refreshFrameDetail();
    refreshHistoryState();
    refreshHeader();
    emit observationsChanged();
}

void ReviewPage::goToFrame(int frame)
{
    m_controller->stopPlayback();
    m_controller->setFrame(std::clamp(frame, 1, std::max(1, m_sequenceFrames)));
}

bool ReviewPage::migrateAnnotations(const QString& fromId, const QString& toId, QString* error)
{
    if (!m_repository || fromId == toId)
        return true;
    const std::string from = fromId.toStdString();
    const std::string to = toId.toStdString();
    if (!m_repository->containsVideo(from) || m_repository->containsVideo(to))
        return true;
    auto moved = m_repository->observationsForVideo(from);
    for (auto& observation : moved) {
        observation.videoId = to;
        if (observation.id.rfind(from + ":", 0) == 0)
            observation.id = to + observation.id.substr(from.size());
    }
    std::string message;
    carryDetector(from, to);
    if (!m_repository->replaceObservationsForVideo(to, moved, message) ||
        !m_repository->removeVideo(from, message)) {
        if (error)
            *error = QString::fromStdString(message);
        return false;
    }
    return true;
}

std::vector<domain::Observation> ReviewPage::storedObservations(const QString& videoId) const
{
    const std::string id = videoId.toStdString();
    if (!m_repository || !m_repository->containsVideo(id))
        return {};
    return m_repository->observationsForVideo(id);
}

bool ReviewPage::replaceStoredObservations(const QString& fromId, const QString& toId,
                                           std::vector<domain::Observation> observations,
                                           QString* error)
{
    if (!m_repository)
        return true;
    const std::string from = fromId.toStdString();
    const std::string to = toId.toStdString();
    if (!m_repository->containsVideo(from) && observations.empty())
        return true;
    // Ids begin with the media's name; keep that true after a rename.
    for (auto& observation : observations) {
        observation.videoId = to;
        if (from != to && observation.id.rfind(from + ":", 0) == 0)
            observation.id = to + observation.id.substr(from.size());
    }
    std::string message;
    if (from != to)
        carryDetector(from, to);
    bool ok = m_repository->replaceObservationsForVideo(to, observations, message);
    if (ok && from != to)
        ok = m_repository->removeVideo(from, message);
    if (!ok && error)
        *error = QString::fromStdString(message);
    return ok;
}

const std::vector<domain::Observation>& ReviewPage::allObservations() const
{
    if (m_annotationController && m_annotationController->hasSession())
        return m_annotationController->observations();
    return m_groundTruth;
}

std::vector<domain::Observation> ReviewPage::observations() const
{
    if (m_annotationController)
        return m_annotationController->observations();
    return m_controller->observations();
}

bool ReviewPage::loadSequence(const QString& path)
{
    if (!maybeSaveAnnotations())
        return false;
    return m_controller->loadSequence(path);
}

void ReviewPage::refreshHeader()
{
    const auto& source = m_controller->source();
    if (!source.isOpen()) {
        m_reviewPanel->setMediaTitle(QStringLiteral("Nothing open"),
                                     QStringLiteral("Open a sequence or video from the Video library"));
        m_reviewPanel->setAnnotationSource(QString(), aquatrack::ui::palette::TextDim);
        return;
    }
    const QLocale locale = QLocale::system();
    QStringList parts;
    switch (source.kind()) {
    case media::MediaKind::ImageSequence:
        parts << QStringLiteral("Sequence of image frames");
        break;
    case media::MediaKind::VideoFile:
        parts << QStringLiteral("Video");
        break;
    case media::MediaKind::VideoSequence:
        parts << QStringLiteral("Sequence of %1 videos").arg(source.clips().size());
        break;
    default:
        break;
    }
    parts << QStringLiteral("%1 frames").arg(locale.toString(source.frameCount()))
          << QStringLiteral("%1 fps").arg(frameRateText(source.frameRate()))
          << QStringLiteral("%1×%2").arg(source.frameSize().width()).arg(source.frameSize().height())
          << media::formatDuration(source.durationMs());
    m_reviewPanel->setMediaTitle(source.name(), parts.join(QStringLiteral("  ·  ")));

    const auto& all = allObservations();
    if (all.empty()) {
        m_reviewPanel->setAnnotationSource(QStringLiteral("No annotations yet"),
                                           aquatrack::ui::palette::TextDim);
        return;
    }
    std::set<domain::LocalTrackId> tracks;
    std::size_t unverified = 0;
    for (const auto& observation : all) {
        tracks.insert(observation.localTrackId);
        if (!observation.humanVerified)
            ++unverified;
    }
    // Which model the boxes came from, when a run recorded it.
    QString detector;
    QString provenance;
    if (m_repository)
        if (const auto run = m_repository->detectorFor(source.name().toStdString())) {
            detector = QString::fromStdString(run->modelName.empty() ? run->modelId
                                                                     : run->modelName);
            provenance = QStringLiteral("Detected with %1").arg(detector);
            // The badge uses the short name: "RT-DETR-L", not "RT-DETR-L (rtdetr_l_...)".
            if (const int open = detector.indexOf(QStringLiteral(" (")); open > 0)
                detector.truncate(open);
            if (run->atMs > 0)
                provenance += QStringLiteral(" on %1").arg(locale.toString(
                    QDateTime::fromMSecsSinceEpoch(run->atMs), QLocale::ShortFormat));
            if (!run->modelId.empty() && run->modelName != run->modelId)
                provenance +=
                    QStringLiteral("\nModel id: %1").arg(QString::fromStdString(run->modelId));
        }
    const QString trackCount = locale.toString(int(tracks.size()));
    if (unverified > 0)
        m_reviewPanel->setAnnotationSource(
            detector.isEmpty() ? QStringLiteral("Model detections · %1 tracks").arg(trackCount)
                               : QStringLiteral("%1 · %2 tracks").arg(detector, trackCount),
            aquatrack::ui::palette::Warning, provenance);
    else
        m_reviewPanel->setAnnotationSource(
            QStringLiteral("Verified annotations · %1 tracks").arg(trackCount),
            aquatrack::ui::palette::Success, provenance);
}

void ReviewPage::updateSequenceNavigator()
{
    const auto& source = m_controller->source();
    if (!source.isOpen()) {
        m_surveyPanel->clearSequence();
        return;
    }
    QVector<SurveyPanel::Entry> entries;
    QString detail;
    const QLocale locale = QLocale::system();
    const QString frameCount = QStringLiteral("%1 frames").arg(locale.toString(source.frameCount()));
    switch (source.kind()) {
    case media::MediaKind::ImageSequence: {
        // Listed from the sequence's own numbering rather than by walking
        // the folder, which takes seconds for long sequences.
        const QDir root(source.path());
        entries.push_back({QStringLiteral("seqinfo.ini"), QString(), 0});
        entries.push_back({QStringLiteral("gt/gt.txt"), QString(), 0});
        for (int frame = 1; frame <= source.frameCount(); ++frame)
            entries.push_back({root.relativeFilePath(source.imagePath(frame)),
                               locale.toString(frame), frame});
        detail = QStringLiteral("Image frames  \u00b7  %1").arg(frameCount);
        break;
    }
    case media::MediaKind::VideoFile:
    case media::MediaKind::VideoSequence: {
        const auto& clips = source.clips();
        for (int index = 0; index < clips.size(); ++index) {
            const auto& clip = clips.at(index);
            const QString name = QFileInfo(clip.path).fileName();
            entries.push_back({clips.size() > 1 ? QStringLiteral("%1.  %2").arg(index + 1).arg(name)
                                                : name,
                               media::formatDuration(clip.durationMs), clip.firstFrame});
        }
        detail = clips.size() > 1
                     ? QStringLiteral("%1 clips  \u00b7  %2  \u00b7  %3")
                           .arg(clips.size())
                           .arg(frameCount, media::formatDuration(source.durationMs()))
                     : QStringLiteral("Video  \u00b7  %1  \u00b7  %2")
                           .arg(frameCount, media::formatDuration(source.durationMs()));
        break;
    }
    default:
        break;
    }
    m_surveyPanel->setSequence(source.name(), QDir::toNativeSeparators(source.path()), detail,
                               entries);
}

void ReviewPage::setProjectFolders(const QString& exports, const QString& annotations,
                                   const QString& snapshots)
{
    m_exportsFolder = exports;
    m_annotationsFolder = annotations;
    m_snapshotsFolder = snapshots;
}

bool ReviewPage::hasUnsavedAnnotations() const
{
    return m_annotationController && m_annotationController->isDirty();
}

bool ReviewPage::maybeSaveAnnotations()
{
    if (!hasUnsavedAnnotations())
        return true;
    const auto choice = QMessageBox::warning(
        this, QStringLiteral("Unsaved annotation edits"),
        QStringLiteral("You have unsaved annotation edits for %1. Save them before continuing?")
            .arg(QString::fromStdString(m_annotationController->videoId())),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (choice == QMessageBox::Cancel)
        return false;
    if (choice == QMessageBox::Save) {
        QString error;
        if (!m_annotationController->save(&error)) {
            QMessageBox::warning(this, QStringLiteral("Save annotations failed"), error);
            return false;
        }
        refreshHistoryState();
    } else {
        // Discard means discard: go back to what the store holds, so the
        // edits cannot resurface if the same project or sequence stays open.
        startAnnotationSession();
        annotationsEdited();
    }
    return true;
}

void ReviewPage::startAnnotationSession()
{
    if (!m_annotationController || m_videoId.empty()) {
        rebuildFrameIndex();
        return;
    }
    try {
        if (m_repository->containsVideo(m_videoId)) {
            m_annotationController->open(m_videoId);
        } else {
            m_annotationController->begin(m_videoId, m_groundTruth);
            // Ground truth is recorded so later edits have a baseline; media
            // with nothing to record (a fresh video) stays out of the store.
            if (!m_groundTruth.empty()) {
                QString error;
                if (!m_annotationController->save(&error))
                    QMessageBox::warning(this, QStringLiteral("Observation storage failed"), error);
            }
        }
    } catch (const std::exception& failure) {
        // Never let a bad record escape a Qt slot: that terminates the app.
        QMessageBox::warning(this, QStringLiteral("Could not open annotations"),
                             QString::fromUtf8(failure.what()));
        m_insightPanel->setEditingEnabled(false);
        rebuildFrameIndex();
        return;
    }
    m_insightPanel->setEditingEnabled(true);
    rebuildFrameIndex();
}

void ReviewPage::rebuildFrameIndex()
{
    m_frameIndex.clear();
    const auto& all = allObservations();
    m_frameIndex.reserve(int(std::min<std::size_t>(all.size(), 1u << 20)));
    for (int index = 0; index < int(all.size()); ++index)
        m_frameIndex[int(all[std::size_t(index)].frameIndex)].push_back(index);
}

void ReviewPage::annotationsEdited()
{
    rebuildFrameIndex();
    // Redraw the current frame so a deleted or relabelled box changes on
    // screen immediately, then recompute everything derived from the set.
    if (!m_currentImage.isNull())
        displayFrame(m_currentImage, {}, m_currentFrame, m_sequenceFrames);
    else
        refreshFrameDetail();
    rebuildAnalytics();
    refreshHeader();
    emit observationsChanged();
}

void ReviewPage::mergeTracks(qint64 endingTrack, qint64 startingTrack)
{
    if (!m_annotationController || !m_annotationController->hasSession()) {
        QMessageBox::information(this, QStringLiteral("Merge tracks"),
                                 QStringLiteral("Create or open a project before editing tracks."));
        return;
    }
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Merge tracks"),
        QStringLiteral("Treat track %1 as a continuation of track %2? Every observation of "
                       "track %1 will be relabelled as track %2.")
            .arg(startingTrack)
            .arg(endingTrack));
    if (answer != QMessageBox::Yes)
        return;
    if (m_annotationController->mergeTracks(startingTrack, endingTrack))
        annotationsEdited();
}

void ReviewPage::saveSnapshot()
{
    if (m_currentImage.isNull())
        return;
    const QString suggested = QStringLiteral("%1_frame%2.png")
                                  .arg(QString::fromStdString(m_videoId))
                                  .arg(m_currentFrame, 6, 10, QLatin1Char('0'));
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save frame snapshot"), suggestedPath(m_snapshotsFolder, suggested),
        QStringLiteral("PNG images (*.png);;JPEG images (*.jpg)"));
    if (path.isEmpty())
        return;
    // The canvas, not the raw frame, so boxes, IDs and trails are included.
    if (!m_reviewPanel->sequenceView()->grab().save(path))
        QMessageBox::warning(this, QStringLiteral("Snapshot failed"),
                             QStringLiteral("Could not write %1").arg(path));
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
    if (m_controller->isLoaded()) {
        startAnnotationSession();
        if (!m_currentImage.isNull())
            displayFrame(m_currentImage, {}, m_currentFrame, m_sequenceFrames);
        rebuildAnalytics();
        refreshHeader();
    }
    refreshHistoryState();
    emit observationsChanged();
    return true;
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
    const auto& source = m_controller->source();
    if (source.isOpen())
        return source.timestampMs(frame);
    return qint64(std::max(0, frame - 1)) * 1000 / m_frameRate;
}

int ReviewPage::frameForTimestamp(qint64 milliseconds) const
{
    const auto& source = m_controller->source();
    if (source.isOpen())
        return source.frameAtTimestamp(milliseconds);
    const int frame = int(std::max<qint64>(0, milliseconds) * m_frameRate / 1000) + 1;
    return std::clamp(frame, 1, std::max(1, m_sequenceFrames));
}

QHash<int, QVector<QPointF>> ReviewPage::trailsFor(int frame) const
{
    QHash<int, QVector<QPointF>> trails;
    const auto& all = allObservations();
    for (int past = std::max(1, frame - kTrailLength + 1); past <= frame; ++past) {
        const auto found = m_frameIndex.constFind(past);
        if (found == m_frameIndex.constEnd())
            continue;
        for (const int index : *found) {
            const auto& observation = all[std::size_t(index)];
            trails[int(observation.localTrackId)].push_back(
                QPointF(observation.box.centerX(), observation.box.centerY()));
        }
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
    m_currentImage = image;
    m_annotations = std::move(annotations);

    // With an annotation session open, the edited observations are the truth
    // on screen, not the on-disk ground truth the controller emitted.
    if (m_annotationController && m_annotationController->hasSession()) {
        m_annotations.clear();
        const auto& all = m_annotationController->observations();
        for (const int index : m_frameIndex.value(frame)) {
            const auto& observation = all[std::size_t(index)];
            aquatrack::GroundTruth annotation;
            annotation.frame = frame;
            annotation.trackId = int(observation.localTrackId);
            annotation.box = QRectF(observation.box.x1, observation.box.y1,
                                    observation.box.width(), observation.box.height());
            annotation.confidence = observation.confidence;
            annotation.classId = classIdForName(QString::fromStdString(observation.className));
            annotation.visibility = 1.0F;
            m_annotations.push_back(annotation);
        }
    }

    m_reviewPanel->showFrame(image, m_annotations, frame, total, timestampFor(frame),
                             m_controller->source().frameSizeAt(frame));
    m_reviewPanel->sequenceView()->setTrails(trailsFor(frame));
    m_surveyPanel->setCurrentFrame(frame);
    refreshFrameDetail();
}

void ReviewPage::refreshFrameDetail()
{
    const auto filters = m_surveyPanel->filters();
    m_currentObservations.clear();
    if (m_annotationController && m_annotationController->hasSession()) {
        const auto& all = m_annotationController->observations();
        for (const int index : m_frameIndex.value(m_currentFrame)) {
            const auto& observation = all[std::size_t(index)];
            const int classId = classIdForName(QString::fromStdString(observation.className));
            if (filters.speciesClassId >= 0 && classId != filters.speciesClassId)
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
    const int generation = m_filmstripGeneration->fetch_add(1) + 1;
    m_reviewPanel->filmstrip()->clear();
    const media::MediaSource source = m_controller->source();
    if (!source.isOpen())
        return;

    QVector<int> frames;
    const int step = std::max(1, source.frameCount() / kFilmstripFrames);
    for (int frame = 1; frame <= source.frameCount() && frames.size() < kFilmstripFrames;
         frame += step)
        frames.push_back(frame);

    // Thumbnails decode off the UI thread (for video each one is an FFmpeg
    // seek) and appear as they arrive; a newer sequence cancels the batch.
    QPointer<ReviewPage> guard(this);
    auto current = m_filmstripGeneration;
    m_thumbnailPool.clear();
    m_thumbnailPool.start([guard, current, generation, source, frames] {
        QVector<aquatrack::ui::Filmstrip::Thumbnail> thumbnails;
        for (const int frame : frames) {
            if (current->load() != generation)
                return;
            QImage image;
            if (source.isVideo()) {
                const auto& clip = source.clips().at(source.clipIndexForFrame(frame));
                image = media::grabVideoFrame(clip.path, (frame - clip.firstFrame) / clip.frameRate,
                                              kThumbnailWidth);
            } else {
                QImageReader reader(source.imagePath(frame));
                const QSize full = reader.size();
                if (full.isValid() && full.width() > kThumbnailWidth)
                    reader.setScaledSize(QSize(kThumbnailWidth,
                                               kThumbnailWidth * full.height() / full.width()));
                image = reader.read();
            }
            if (image.isNull())
                continue;
            aquatrack::ui::Filmstrip::Thumbnail thumbnail;
            thumbnail.frame = frame;
            thumbnail.image = std::move(image);
            const qint64 milliseconds = source.timestampMs(frame);
            thumbnail.caption = media::formatDuration(milliseconds);
            thumbnails.push_back(std::move(thumbnail));
            if (!guard)
                return;
            QMetaObject::invokeMethod(
                guard.data(),
                [guard, current, generation, thumbnails] {
                    if (guard && current->load() == generation)
                        guard->m_reviewPanel->filmstrip()->setThumbnails(thumbnails);
                },
                Qt::QueuedConnection);
        }
    });
}

void ReviewPage::rebuildAnalytics()
{
    const auto& all = allObservations();
    if (all.empty()) {
        m_idSwitches.clear();
        m_reviewPanel->clearAnalytics();
        m_reviewPanel->timeline()->setLanes({});
        m_reviewPanel->timeline()->setMarkers({});
        m_reviewPanel->timeline()->setLinks({});
        m_insightPanel->setConfidenceSeries({}, 0.0);
        m_insightPanel->setQuality(0, 0, 0, 0);
        m_insightPanel->setIdSwitches({});
        m_surveyPanel->setSpecies({});
        m_surveyPanel->setTotals({});
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


    // ---- Fish activity: per second, distinct tracks and detections ------
    {
        const int seconds = int(std::max<qint64>(1, (timestampFor(m_sequenceFrames) + 999) / 1000));
        QVector<int> detections(seconds, 0);
        std::vector<std::vector<domain::LocalTrackId>> tracksIn(static_cast<std::size_t>(seconds));
        for (const auto& observation : all) {
            const int second = int(std::clamp<qint64>(observation.timestampMs / 1000, 0, seconds - 1));
            ++detections[second];
            tracksIn[std::size_t(second)].push_back(observation.localTrackId);
        }
        QVector<int> tracks(seconds, 0);
        for (int second = 0; second < seconds; ++second) {
            auto& ids = tracksIn[std::size_t(second)];
            std::sort(ids.begin(), ids.end());
            tracks[second] = int(std::unique(ids.begin(), ids.end()) - ids.begin());
        }
        m_reviewPanel->activity()->setActivity(tracks, detections, int(summary.uniqueLocalTracks));
    }
    m_reviewPanel->setEventsTable(events);

    // ---- Track lanes -------------------------------------------------
    // One lane per track: the runs of consecutive frames it was observed
    // in. A break between runs is a frame where the tracker lost it.
    std::map<domain::LocalTrackId, std::vector<std::int64_t>> framesByTrack;
    for (const auto& observation : all)
        framesByTrack[observation.localTrackId].push_back(observation.frameIndex);
    std::map<domain::LocalTrackId, std::string> speciesByTrack;
    for (const auto& span : spans)
        speciesByTrack[span.trackId] = span.className;
    QVector<aquatrack::ui::TrackLanes::Lane> lanes;
    lanes.reserve(int(framesByTrack.size()));
    for (auto& [trackId, frames] : framesByTrack) {
        aquatrack::ui::TrackLanes::Lane lane;
        lane.trackId = int(trackId);
        lane.species = QString::fromStdString(speciesByTrack[trackId]);
        lane.color = aquatrack::ui::trackColor(int(trackId));
        lane.observations = int(frames.size());
        std::sort(frames.begin(), frames.end());
        frames.erase(std::unique(frames.begin(), frames.end()), frames.end());
        for (std::size_t index = 0; index < frames.size(); ++index) {
            const int frame = int(frames[index]);
            if (lane.segments.isEmpty() || frame != lane.segments.last().second + 1)
                lane.segments.push_back({frame, frame});
            else
                lane.segments.last().second = frame;
        }
        lanes.push_back(std::move(lane));
    }
    m_reviewPanel->timeline()->setLanes(std::move(lanes));

    // Likely identity switches, linked from the track that ends to the one
    // that starts.
    std::map<domain::LocalTrackId, const analytics::TrackSpan*> spanByTrack;
    for (const auto& span : spans)
        spanByTrack[span.trackId] = &span;
    QVector<aquatrack::ui::TrackLanes::Link> links;
    for (const auto& candidate : m_idSwitches) {
        const auto ending = spanByTrack.find(candidate.endingTrack);
        const auto starting = spanByTrack.find(candidate.startingTrack);
        if (ending == spanByTrack.end() || starting == spanByTrack.end())
            continue;
        links.push_back({int(candidate.endingTrack), int(candidate.startingTrack),
                         int(ending->second->lastFrame), int(starting->second->firstFrame)});
    }
    m_reviewPanel->timeline()->setLinks(std::move(links));

    // Derived events, as pins for moments and bands for intervals. Events
    // with no position in time (totals such as "N very short tracks") are
    // summarised in the right rail instead of being pinned at zero.
    QVector<aquatrack::ui::TrackLanes::Marker> markers;
    for (const auto& event : events) {
        if (event.atMs == 0 && event.untilMs == 0)
            continue;
        aquatrack::ui::TrackLanes::Marker marker;
        marker.startFrame = frameForTimestamp(event.atMs);
        marker.endFrame = frameForTimestamp(std::max(event.atMs, event.untilMs));
        marker.color = event.severity == analytics::EventSeverity::Alert     ? aquatrack::ui::palette::Danger
                       : event.severity == analytics::EventSeverity::Warning ? aquatrack::ui::palette::Warning
                                                                             : aquatrack::ui::palette::Accent;
        marker.label = QString::fromStdString(event.description);
        markers.push_back(std::move(marker));
    }
    m_reviewPanel->timeline()->setMarkers(std::move(markers));

    m_insightPanel->setIdSwitches(m_idSwitches);

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

    // Quality figures under the confidence chart.
    double confidenceSum = 0.0;
    std::size_t weak = 0;
    std::size_t verified = 0;
    for (const auto& observation : all) {
        confidenceSum += observation.confidence;
        weak += observation.confidence < 0.5F ? 1 : 0;
        verified += observation.humanVerified ? 1 : 0;
    }
    const double observationCount = double(all.size());
    m_insightPanel->setQuality(confidenceSum / observationCount, double(weak) / observationCount,
                               double(verified) / observationCount, int(all.size()));

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
    SurveyPanel::Totals totals;
    totals.annotations = int(summary.totalObservations);
    totals.tracks = int(summary.uniqueLocalTracks);
    totals.peakInFrame = int(peak);
    totals.species = int(summary.observationsByClass.size());
    totals.events = int(events.size());
    std::size_t mostObserved = 0;
    for (const auto& [name, count] : summary.observationsByClass)
        if (count > mostObserved) {
            mostObserved = count;
            totals.dominantSpecies = QString::fromStdString(name);
        }
    m_surveyPanel->setTotals(totals);
}

// ---- Import and detection ---------------------------------------------

void ReviewPage::chooseAnnotationFile()
{
    if (!m_controller->isLoaded())
        return;
    if (!m_annotationController) {
        QMessageBox::information(this, QStringLiteral("Import annotations"),
                                 QStringLiteral("Create or open a project first: imported "
                                                "annotations are saved in it."));
        return;
    }
    const QString start = m_annotationsFolder.isEmpty()
                              ? QFileInfo(m_controller->source().path()).absolutePath()
                              : m_annotationsFolder;
    const QString file = QFileDialog::getOpenFileName(
        this, QStringLiteral("Import annotations for %1").arg(m_controller->source().name()), start,
        aquatrack::application::AnnotationImporter::fileDialogFilter());
    if (!file.isEmpty())
        importAnnotations(file);
}

bool ReviewPage::importAnnotations(const QString& file)
{
    const auto& source = m_controller->source();
    aquatrack::application::AnnotationImport imported;
    QString error;
    if (!aquatrack::application::AnnotationImporter::read(file, source, imported, &error)) {
        QMessageBox::warning(this, QStringLiteral("Could not import annotations"), error);
        return false;
    }
    const QLocale locale = QLocale::system();
    QString what = QStringLiteral("%1 boxes in %2 tracks from %3 (%4)")
                       .arg(locale.toString(qlonglong(imported.observations.size())),
                            locale.toString(imported.tracks), QFileInfo(file).fileName(),
                            imported.format);
    const std::size_t count = imported.observations.size();
    if (!replaceObservations(source, std::move(imported.observations), what))
        return false;
    // The boxes now come from the file, not from a detector.
    if (m_repository) {
        std::string message;
        if (m_repository->setDetector(source.name().toStdString(), std::nullopt, message))
            refreshHeader();
    }
    QString note;
    if (imported.outOfRange > 0)
        note += QStringLiteral(" %1 rows were for frames past the end of the media and were left out.")
                    .arg(locale.toString(imported.outOfRange));
    if (imported.skipped > 0)
        note += QStringLiteral(" %1 rows had no usable box.").arg(locale.toString(imported.skipped));
    if (!note.isEmpty())
        QMessageBox::information(this, QStringLiteral("Annotations imported"),
                                 QStringLiteral("Imported %1 boxes.%2")
                                     .arg(locale.toString(qlonglong(count)), note));
    return true;
}

bool ReviewPage::replaceObservations(const media::MediaSource& media,
                                     std::vector<domain::Observation> observations,
                                     const QString& what, bool askFirst)
{
    if (!m_repository || !m_annotationController)
        return false;
    const std::string videoId = media.name().toStdString();
    const bool onScreen = videoId == m_videoId && m_controller->isLoaded();
    if (onScreen && !maybeSaveAnnotations())
        return false;

    const std::size_t existing = onScreen ? allObservations().size()
                                          : m_repository->observationsForVideo(videoId).size();
    if (askFirst && existing > 0) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Replace annotations?"),
            QStringLiteral("%1 already has %2 annotations. Replace them with %3?\n\n"
                           "The current annotations cannot be recovered afterwards; export them "
                           "first if you may need them.")
                .arg(media.name(), QLocale::system().toString(qlonglong(existing)), what),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes)
            return false;
    }
    std::string message;
    if (!m_repository->replaceObservationsForVideo(videoId, observations, message)) {
        QMessageBox::warning(this, QStringLiteral("Could not save annotations"),
                             QString::fromStdString(message));
        return false;
    }
    if (onScreen) {
        startAnnotationSession();
        annotationsEdited();
        refreshHistoryState();
    } else {
        emit observationsChanged();
    }
    return true;
}

void ReviewPage::startAnalysis(aquatrack::application::AnalysisRequest request)
{
    if (m_analysis->isRunning() || !m_controller->isLoaded())
        return;
    m_analysisMedia = m_controller->source();
    m_analysisModelId = request.modelId;
    m_analysisModelName = request.modelName;
    request.source = m_analysisMedia.path();
    if (request.output.isEmpty()) {
        const QString folder = m_annotationsFolder.isEmpty() ? QDir::tempPath() : m_annotationsFolder;
        QDir().mkpath(folder);
        request.output = QDir(folder).filePath(
            QStringLiteral("%1.detections.json").arg(m_analysisMedia.name()));
    }
    m_reviewPanel->setAnalysisRunning(true);
    m_analysis->start(request);
}

bool ReviewPage::isAnalysisRunning() const
{
    return m_analysis->isRunning();
}

void ReviewPage::cancelAnalysis()
{
    m_analysis->cancel();
}

void ReviewPage::analysisFinished(const QString& resultsPath)
{
    m_reviewPanel->setAnalysisRunning(false);
    aquatrack::application::AnalysisResult result;
    QString error;
    if (!aquatrack::application::AnalysisRunner::readResults(resultsPath, m_analysisMedia, result,
                                                             &error)) {
        QMessageBox::warning(this, QStringLiteral("Could not load detections"), error);
        return;
    }
    const QLocale locale = QLocale::system();
    const QString what = QStringLiteral("%1 detections in %2 tracks from %3")
                             .arg(locale.toString(qlonglong(result.observations.size())),
                                  locale.toString(result.tracks), result.modelName);
    if (result.observations.empty()) {
        QMessageBox::information(
            this, QStringLiteral("No fish detected"),
            QStringLiteral("%1 found nothing above the confidence threshold in %2. Try a lower "
                           "Detection confidence in Settings, or a model trained on similar footage.")
                .arg(result.modelName, m_analysisMedia.name()));
        return;
    }
    applyDetections(m_analysisMedia, std::move(result.observations), what);
}

bool ReviewPage::applyDetections(const media::MediaSource& media,
                                 std::vector<domain::Observation> detections, const QString& what)
{
    if (!m_repository || !m_annotationController)
        return false;
    const std::string videoId = media.name().toStdString();
    const bool onScreen = videoId == m_videoId && m_controller->isLoaded();
    if (onScreen && !maybeSaveAnnotations())
        return false;
    const std::vector<domain::Observation> existing =
        onScreen ? allObservations() : m_repository->observationsForVideo(videoId);
    const auto verified = std::count_if(existing.begin(), existing.end(),
                                        [](const auto& item) { return item.humanVerified; });

    const QLocale locale = QLocale::system();
    bool applied = false;
    QString note;
    if (verified == 0) {
        // Nothing reviewed to lose: the usual replace (asks if boxes exist).
        applied = replaceObservations(media, std::move(detections), what);
    } else {
        // Review work is at stake: keeping it is the default.
        const auto unverified = qlonglong(existing.size()) - qlonglong(verified);
        QMessageBox box(QMessageBox::Question, QStringLiteral("Keep your verified boxes?"),
                        QStringLiteral("%1 has %2 boxes you verified and %3 unverified model boxes.")
                            .arg(media.name(), locale.toString(qlonglong(verified)),
                                 locale.toString(unverified)),
                        QMessageBox::NoButton, this);
        box.setInformativeText(
            QStringLiteral("Keep the verified boxes and replace only the unverified ones with %1? "
                           "New detections that overlap a verified box are skipped, and a track "
                           "that follows a verified fish keeps its ID.\n\n"
                           "Replacing everything discards your review of this media.")
                .arg(what));
        auto* keep = box.addButton(QStringLiteral("Keep verified, replace the rest"),
                                   QMessageBox::AcceptRole);
        auto* everything = box.addButton(QStringLiteral("Replace everything"),
                                         QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(keep);
        box.exec();
        if (box.clickedButton() == keep) {
            auto merge = aquatrack::application::mergeDetections(existing, std::move(detections));
            note = QStringLiteral("Kept %1 verified boxes, added %2 new detections")
                       .arg(locale.toString(merge.keptVerified), locale.toString(merge.added));
            if (merge.continuedTracks > 0)
                note += QStringLiteral(" (%1 tracks continue a verified ID)")
                            .arg(locale.toString(merge.continuedTracks));
            applied = replaceObservations(media, std::move(merge.observations), what, false);
        } else if (box.clickedButton() == everything) {
            applied = replaceObservations(media, std::move(detections), what, false);
        }
    }
    if (!applied)
        return false;

    std::string message;
    const aquatrack::data::DetectorRun run{m_analysisModelId.toStdString(),
                                           m_analysisModelName.toStdString(),
                                           QDateTime::currentMSecsSinceEpoch()};
    if (m_repository->setDetector(videoId, run, message)) {
        if (onScreen)
            refreshHeader();
        emit observationsChanged();
    }
    if (note.isEmpty())
        note = QStringLiteral("Loaded %1").arg(what);
    m_reviewPanel->activityLog()->append(note, LogView::Kind::Success);
    emit statusMessage(note);
    return true;
}

ReviewPanel::PanelHeights ReviewPage::panelHeights() const
{
    return m_reviewPanel->panelHeights();
}

void ReviewPage::setPanelHeights(ReviewPanel::PanelHeights heights)
{
    m_reviewPanel->setPanelHeights(heights);
}

void ReviewPage::logStatus(const QString& message)
{
    if (message.trimmed().isEmpty())
        return;
    // Status lines are the app's own account; failures read as problems.
    const QString lower = message.toLower();
    const bool problem = lower.contains(QLatin1String("could not")) ||
                         lower.contains(QLatin1String("failed")) ||
                         lower.contains(QLatin1String("error"));
    const bool caution = lower.contains(QLatin1String("warning")) ||
                         lower.contains(QLatin1String("missing")) ||
                         lower.contains(QLatin1String("not found"));
    m_reviewPanel->activityLog()->append(message, problem   ? LogView::Kind::Error
                                                  : caution ? LogView::Kind::Warning
                                                            : LogView::Kind::Info);
}

void ReviewPage::carryDetector(const std::string& from, const std::string& to)
{
    if (!m_repository || from == to)
        return;
    std::string message;
    if (const auto run = m_repository->detectorFor(from))
        (void)m_repository->setDetector(to, run, message);
}

void ReviewPage::setDetectorChoices(const QVector<ReviewPanel::DetectorOption>& options,
                                    const QString& current, bool forProject)
{
    m_reviewPanel->setDetectorChoices(options, current, forProject);
}
