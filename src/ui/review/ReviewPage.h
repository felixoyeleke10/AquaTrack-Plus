#pragma once

#include "analytics/SurveyAnalytics.h"
#include "data/BrackishMotSequence.h"
#include "domain/ScientificTypes.h"

#include <QHash>
#include <QVector>
#include <QWidget>

#include <memory>
#include <vector>

class InsightPanel;
class ReviewPanel;
class RuntimeStrip;
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

    void loadSequence(const QString& directory);
    bool setObservationStore(const QString& path, QString* error = nullptr);
    [[nodiscard]] std::vector<aquatrack::domain::Observation> observations() const;

private:
    void openSequence();
    void applyFilters();
    void displayFrame(const QImage& image, QVector<aquatrack::GroundTruth> annotations, int frame,
                      int total);
    void rebuildAnalytics();
    void refreshFrameDetail();
    void refreshHistoryState();
    void buildFilmstrip();
    [[nodiscard]] qint64 timestampFor(int frame) const;
    [[nodiscard]] int frameForTimestamp(qint64 milliseconds) const;
    [[nodiscard]] QHash<int, QVector<QPointF>> trailsFor(int frame) const;

    aquatrack::application::ReviewController* m_controller = nullptr;
    aquatrack::application::ExportController* m_exportController = nullptr;
    std::unique_ptr<aquatrack::data::JsonObservationRepository> m_repository;
    std::unique_ptr<aquatrack::application::AnnotationController> m_annotationController;

    SurveyPanel* m_surveyPanel = nullptr;
    ReviewPanel* m_reviewPanel = nullptr;
    InsightPanel* m_insightPanel = nullptr;
    RuntimeStrip* m_runtime = nullptr;

    QVector<aquatrack::GroundTruth> m_annotations;
    std::vector<aquatrack::domain::Observation> m_currentObservations;
    std::vector<aquatrack::analytics::IdSwitchCandidate> m_idSwitches;

    QString m_sequenceDirectory;
    int m_currentFrame = 0;
    int m_sequenceFrames = 0;
    int m_frameRate = 15;
    int m_imageWidth = 0;
    int m_imageHeight = 0;
};
