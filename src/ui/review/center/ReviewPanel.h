#pragma once

#include "data/BrackishMotSequence.h"

#include <QWidget>

class AnalysisPanel;
class QLabel;
class QPushButton;
class QSlider;
class SequenceView;

class ReviewPanel final : public QWidget {
public:
    explicit ReviewPanel(QWidget* parent = nullptr);
    [[nodiscard]] QPushButton* playButton() const { return m_playButton; }
    [[nodiscard]] QSlider* frameSlider() const { return m_frameSlider; }
    [[nodiscard]] SequenceView* sequenceView() const { return m_view; }
    void configureSequence(int frameCount);
    void showFrame(const QImage& image, QVector<aquatrack::GroundTruth> annotations, int frame, int total);
    void setMetrics(int frames, int detections, int tracks);

private:
    SequenceView* m_view = nullptr;
    QPushButton* m_playButton = nullptr;
    QSlider* m_frameSlider = nullptr;
    QLabel* m_frameLabel = nullptr;
    AnalysisPanel* m_analysis = nullptr;
};
