#pragma once

#include "data/BrackishMotSequence.h"

#include <QImage>
#include <QWidget>

class ReviewPanel;
class SurveyPanel;
class TrackPanel;
namespace aquatrack::application { class ReviewController; }

class ReviewPage final : public QWidget {
    Q_OBJECT

public:
    explicit ReviewPage(QWidget* parent = nullptr);

private:
    void openSequence();
    void displayFrame(const QImage& image, QVector<aquatrack::GroundTruth> annotations, int frame, int total);
    void refreshPanels();

    aquatrack::application::ReviewController* m_controller = nullptr;
    QVector<aquatrack::GroundTruth> m_annotations;
    int m_sequenceFrames = 0;
    SurveyPanel* m_surveyPanel = nullptr;
    ReviewPanel* m_reviewPanel = nullptr;
    TrackPanel* m_trackPanel = nullptr;
};
