#pragma once

#include "analytics/SurveyAnalytics.h"
#include "application/AnalysisRunner.h"
#include "data/BrackishMotSequence.h"
#include "data/media/MediaSource.h"
#include "domain/ScientificTypes.h"
#include "ui/review/center/ReviewPanel.h"

#include <QHash>
#include <QImage>
#include <QThreadPool>
#include <QVector>
#include <QWidget>

#include <atomic>
#include <memory>
#include <vector>

class InsightPanel;
class ReviewPanel;
class SurveyPanel;

namespace aquatrack::application {
class AnnotationController;
class ExportController;
class ReviewController;
}
namespace aquatrack::data {
class JsonObservationRepository;
}

class ReviewPage final : public QWidget {
    Q_OBJECT

public:
    explicit ReviewPage(QWidget* parent = nullptr);
    ~ReviewPage() override;

    // Opens an image sequence, video file or video sequence. Returns false
    // when the reviewer cancels at the unsaved-edits prompt or the media
    // cannot be opened (the error has then been shown); the media already on
    // screen stays.
    bool loadSequence(const QString& path);
    // Switches the durable store. A sequence that is already open is
    // re-attached to the new store rather than left without a session.
    bool setObservationStore(const QString& path, QString* error = nullptr);
    [[nodiscard]] std::vector<aquatrack::domain::Observation> observations() const;
    // Path of the sequence or video on screen; empty until one has loaded.
    [[nodiscard]] QString sequenceDirectory() const { return m_sequenceDirectory; }
    // 1-based frame on screen; 0 until the first frame has been shown.
    [[nodiscard]] int currentFrame() const { return m_currentFrame; }
    [[nodiscard]] const aquatrack::media::MediaSource& source() const;
    // Where exports and snapshots start inside the open project; empty
    // strings when no project is open.
    void setProjectFolders(const QString& exports, const QString& annotations,
                           const QString& snapshots);
    void setFrameCacheSize(int frames);
    // Closes whatever is on screen (used when another project opens: its
    // media and annotations belong to the project that was closed).
    void closeMedia();
    // Moves the playhead (used to open a sequence at a chosen clip or frame).
    void goToFrame(int frame);
    // Re-keys stored annotations when media is renamed (for example a copy
    // into the project that had to take a "-2" name). Returns false on error.
    bool migrateAnnotations(const QString& fromId, const QString& toId, QString* error = nullptr);
    // Stored annotations of one media item (empty when it has none). Used
    // when a sequence is edited and its frames are renumbered.
    [[nodiscard]] std::vector<aquatrack::domain::Observation> storedObservations(
        const QString& videoId) const;
    // Replaces the stored annotations of `fromId` with `observations` under
    // `toId` (the same id unless the media was renamed). Close the media in
    // Review first: an open session would save over the change.
    bool replaceStoredObservations(const QString& fromId, const QString& toId,
                                   std::vector<aquatrack::domain::Observation> observations,
                                   QString* error = nullptr);

    [[nodiscard]] bool hasUnsavedAnnotations() const;
    // Asks to save, discard or cancel when there are unsaved annotation
    // edits. Returns true when it is safe to continue.
    bool maybeSaveAnnotations();

    // Ground truth from a MOTChallenge or COCO tracking file, for the media
    // on screen. Asks before replacing existing annotations.
    bool importAnnotations(const QString& file);
    // Runs detection + tracking over the media on screen.
    void startAnalysis(aquatrack::application::AnalysisRequest request);
    [[nodiscard]] bool isAnalysisRunning() const;
    void cancelAnalysis();
    // Adds an app status message to the Review page's log.
    void logStatus(const QString& message);
    // The detectors Detect & track can use, and the one it will use.
    // `forProject`: the choice is remembered by the open project.
    void setDetectorChoices(const QVector<ReviewPanel::DetectorOption>& options,
                            const QString& current, bool forProject);

signals:
    // The observation set changed: a sequence loaded, the store switched,
    // or an edit was made. Totals elsewhere should be recomputed.
    void observationsChanged();
    // A sequence finished loading and is now on screen.
    void sequenceOpened(QString directory);
    // The reviewer asked to detect and track; the window resolves the model.
    void analyzeRequested();
    // The reviewer picked a detector next to Detect & track.
    void detectorChosen(QString modelId);
    void manageModelsRequested();
    // A short note for the status bar.
    void statusMessage(QString text);

private:
    void startAnnotationSession();
    void annotationsEdited();
    void mergeTracks(qint64 endingTrack, qint64 startingTrack);
    void saveSnapshot();
    void applyFilters();
    void displayFrame(const QImage& image, QVector<aquatrack::GroundTruth> annotations, int frame,
                      int total);
    void rebuildAnalytics();
    void refreshFrameDetail();
    void refreshHistoryState();
    void refreshHeader();
    void buildFilmstrip();
    void updateSequenceNavigator();
    void rebuildFrameIndex();
    void chooseAnnotationFile();
    // `askFirst`: confirm before overwriting existing annotations.
    bool replaceObservations(const aquatrack::media::MediaSource& media,
                             std::vector<aquatrack::domain::Observation> observations,
                             const QString& what, bool askFirst = true);
    // Detector output for `media`: keeps verified boxes unless the reviewer
    // chooses to replace everything. Returns false if nothing was applied.
    bool applyDetections(const aquatrack::media::MediaSource& media,
                         std::vector<aquatrack::domain::Observation> detections,
                         const QString& what);
    // Moves the recorded detector with annotations when media is renamed.
    void carryDetector(const std::string& from, const std::string& to);
    void analysisFinished(const QString& resultsPath);
    [[nodiscard]] const std::vector<aquatrack::domain::Observation>& allObservations() const;
    [[nodiscard]] qint64 timestampFor(int frame) const;
    [[nodiscard]] int frameForTimestamp(qint64 milliseconds) const;
    [[nodiscard]] QHash<int, QVector<QPointF>> trailsFor(int frame) const;

    aquatrack::application::ReviewController* m_controller = nullptr;
    std::unique_ptr<aquatrack::data::JsonObservationRepository> m_repository;
    std::unique_ptr<aquatrack::application::AnnotationController> m_annotationController;
    aquatrack::application::AnalysisRunner* m_analysis = nullptr;
    aquatrack::media::MediaSource m_analysisMedia;
    QString m_analysisModelId;
    QString m_analysisModelName;

    SurveyPanel* m_surveyPanel = nullptr;
    ReviewPanel* m_reviewPanel = nullptr;
    InsightPanel* m_insightPanel = nullptr;

    QVector<aquatrack::GroundTruth> m_annotations;
    std::vector<aquatrack::domain::Observation> m_groundTruth;
    std::vector<aquatrack::domain::Observation> m_currentObservations;
    std::vector<aquatrack::analytics::IdSwitchCandidate> m_idSwitches;
    // frame -> indices into allObservations(); rebuilt whenever it changes.
    QHash<int, QVector<int>> m_frameIndex;

    QString m_sequenceDirectory;
    QString m_exportsFolder;
    QString m_annotationsFolder;
    QString m_snapshotsFolder;
    std::string m_videoId;
    QImage m_currentImage;
    int m_currentFrame = 0;
    int m_sequenceFrames = 0;
    int m_frameRate = 15;
    int m_imageWidth = 0;
    int m_imageHeight = 0;

    QThreadPool m_thumbnailPool;
    std::shared_ptr<std::atomic_int> m_filmstripGeneration = std::make_shared<std::atomic_int>(0);
};
