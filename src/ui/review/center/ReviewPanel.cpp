#include "ui/review/center/ReviewPanel.h"

#include "ui/review/center/analytics/AnalysisPanel.h"
#include "ui/review/center/video/SequenceView.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

ReviewPanel::ReviewPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    m_view = new SequenceView(this);
    layout->addWidget(m_view, 1);

    auto* timeline = new QFrame(this);
    timeline->setProperty("role", "card");
    timeline->setFixedHeight(62);
    auto* timelineRow = new QHBoxLayout(timeline);
    timelineRow->setContentsMargins(12, 8, 12, 8);
    auto* title = new QLabel(QStringLiteral("EVENT TIMELINE"), timeline);
    title->setProperty("role", "section");
    timelineRow->addWidget(title);
    for (int index = 0; index < 9; ++index) {
        auto* marker = new QFrame(timeline);
        marker->setFixedSize(5, 18 + (index % 3) * 7);
        marker->setStyleSheet(index % 2 == 0 ? QStringLiteral("background:#24a5e8;border-radius:2px")
                                             : QStringLiteral("background:#dc5ba5;border-radius:2px"));
        timelineRow->addWidget(marker);
        timelineRow->addStretch();
    }
    layout->addWidget(timeline);

    auto* transport = new QFrame(this);
    transport->setProperty("role", "card");
    auto* controls = new QHBoxLayout(transport);
    controls->setContentsMargins(12, 8, 12, 8);
    m_playButton = new QPushButton(QStringLiteral("Play"), transport);
    m_playButton->setProperty("role", "primary");
    m_playButton->setEnabled(false);
    m_frameSlider = new QSlider(Qt::Horizontal, transport);
    m_frameSlider->setEnabled(false);
    m_frameLabel = new QLabel(QStringLiteral("Frame 0 / 0"), transport);
    m_frameLabel->setMinimumWidth(130);
    controls->addWidget(m_playButton);
    controls->addWidget(m_frameSlider, 1);
    controls->addWidget(m_frameLabel);
    layout->addWidget(transport);

    m_analysis = new AnalysisPanel(this);
    layout->addWidget(m_analysis);
}

void ReviewPanel::configureSequence(int frameCount)
{
    m_playButton->setEnabled(true);
    m_frameSlider->setEnabled(true);
    m_frameSlider->setRange(1, frameCount);
}

void ReviewPanel::showFrame(const QImage& image, QVector<aquatrack::GroundTruth> annotations, int frame, int total)
{
    m_view->setFrame(image, std::move(annotations));
    m_frameLabel->setText(QStringLiteral("Frame %1 / %2").arg(frame).arg(total));
}

void ReviewPanel::setMetrics(int frames, int detections, int tracks)
{
    m_analysis->setMetrics(frames, detections, tracks);
}
