#pragma once

#include <QTabWidget>

class QLabel;

class AnalysisPanel final : public QTabWidget {
public:
    explicit AnalysisPanel(QWidget* parent = nullptr);
    void setMetrics(int sequenceFrames, int visibleDetections, int activeTracks);

private:
    QLabel* m_frameValue = nullptr;
    QLabel* m_detectionValue = nullptr;
    QLabel* m_trackValue = nullptr;
};
