#include "ui/review/ReviewPage.h"

#include "application/controllers/ReviewController.h"
#include "ui/review/center/ReviewPanel.h"
#include "ui/review/center/video/SequenceView.h"
#include "ui/review/left/SurveyPanel.h"
#include "ui/review/right/TrackPanel.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QSlider>

#include <utility>

ReviewPage::ReviewPage(QWidget* parent)
    : QWidget(parent), m_controller(new aquatrack::application::ReviewController(this))
{
    auto* body = new QHBoxLayout(this);
    body->setContentsMargins(12, 12, 12, 10);
    body->setSpacing(12);
    m_surveyPanel = new SurveyPanel(this);
    m_reviewPanel = new ReviewPanel(this);
    m_trackPanel = new TrackPanel(this);
    body->addWidget(m_surveyPanel);
    body->addWidget(m_reviewPanel, 1);
    body->addWidget(m_trackPanel);

    connect(m_surveyPanel->openButton(), &QPushButton::clicked, this, [this] { openSequence(); });
    connect(m_reviewPanel->playButton(), &QPushButton::clicked,
            m_controller, &aquatrack::application::ReviewController::togglePlayback);
    connect(m_reviewPanel->frameSlider(), &QSlider::valueChanged,
            m_controller, &aquatrack::application::ReviewController::setFrame);
    connect(m_surveyPanel->fishOnlyCheckBox(), &QCheckBox::toggled,
            m_reviewPanel->sequenceView(), &SequenceView::setFishOnly);
    connect(m_surveyPanel->fishOnlyCheckBox(), &QCheckBox::toggled, this, [this] { refreshPanels(); });
    connect(m_controller, &aquatrack::application::ReviewController::sequenceLoaded, this,
            [this](const QString& name, int frames, int frameRate, int width, int height) {
                m_sequenceFrames = frames;
                m_reviewPanel->configureSequence(frames);
                m_surveyPanel->setSequenceInfo(name, frames, frameRate, width, height);
            });
    connect(m_controller, &aquatrack::application::ReviewController::frameReady, this,
            [this](const QImage& image, QVector<aquatrack::GroundTruth> annotations, int frame, int total) {
                displayFrame(image, std::move(annotations), frame, total);
            });
    connect(m_controller, &aquatrack::application::ReviewController::playbackChanged, this,
            [this](bool playing) {
                m_reviewPanel->playButton()->setText(playing ? QStringLiteral("Pause") : QStringLiteral("Play"));
            });
    connect(m_controller, &aquatrack::application::ReviewController::errorOccurred, this,
            [this](const QString& error) { QMessageBox::warning(this, QStringLiteral("Review error"), error); });
}

void ReviewPage::openSequence()
{
    const QString directory = QFileDialog::getExistingDirectory(this, QStringLiteral("Open BrackishMOT sequence"));
    if (!directory.isEmpty())
        m_controller->loadSequence(directory);
}

void ReviewPage::displayFrame(const QImage& image, QVector<aquatrack::GroundTruth> annotations,
                              int frame, int total)
{
    const QSignalBlocker blocker(m_reviewPanel->frameSlider());
    m_reviewPanel->frameSlider()->setValue(frame);
    m_annotations = std::move(annotations);
    m_reviewPanel->showFrame(image, m_annotations, frame, total);
    refreshPanels();
}

void ReviewPage::refreshPanels()
{
    const bool fishOnly = m_surveyPanel->fishOnlyCheckBox()->isChecked();
    QSet<int> tracks;
    int visible = 0;
    for (const auto& item : m_annotations) {
        if (fishOnly && item.classId != 1 && item.classId != 5)
            continue;
        ++visible;
        tracks.insert(item.trackId);
    }
    m_trackPanel->setTracks(m_annotations, fishOnly);
    m_reviewPanel->setMetrics(m_sequenceFrames, visible, tracks.size());
}
