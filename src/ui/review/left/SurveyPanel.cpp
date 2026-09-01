#include "ui/review/left/SurveyPanel.h"

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QLabel* section(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text.toUpper(), parent);
    label->setProperty("role", "section");
    return label;
}
}

SurveyPanel::SurveyPanel(QWidget* parent) : QFrame(parent)
{
    setProperty("role", "panel");
    setFixedWidth(230);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 16, 14, 16);
    layout->setSpacing(11);
    layout->addWidget(section(QStringLiteral("Video & survey"), this));
    m_sequenceLabel = new QLabel(QStringLiteral("No sequence loaded"), this);
    m_sequenceLabel->setStyleSheet(QStringLiteral("font-size:16px;font-weight:600;color:white"));
    m_sequenceLabel->setWordWrap(true);
    layout->addWidget(m_sequenceLabel);
    m_metadataLabel = new QLabel(QStringLiteral("Open a BrackishMOT sequence to begin a native review session."), this);
    m_metadataLabel->setProperty("role", "muted");
    m_metadataLabel->setWordWrap(true);
    layout->addWidget(m_metadataLabel);
    m_openButton = new QPushButton(QStringLiteral("Open sequence"), this);
    m_openButton->setProperty("role", "primary");
    layout->addWidget(m_openButton);
    layout->addSpacing(12);
    layout->addWidget(section(QStringLiteral("Review layers"), this));
    m_fishOnly = new QCheckBox(QStringLiteral("Fish detections only"), this);
    m_fishOnly->setChecked(true);
    auto* showTracks = new QCheckBox(QStringLiteral("Show track IDs"), this);
    showTracks->setChecked(true);
    auto* showBoxes = new QCheckBox(QStringLiteral("Show bounding boxes"), this);
    showBoxes->setChecked(true);
    layout->addWidget(m_fishOnly);
    layout->addWidget(showTracks);
    layout->addWidget(showBoxes);
    layout->addSpacing(12);
    layout->addWidget(section(QStringLiteral("Dataset"), this));
    auto* provenance = new QLabel(QStringLiteral("Source: BrackishMOT\nAnnotations: MOTChallenge\nMode: Ground-truth review"), this);
    provenance->setProperty("role", "muted");
    layout->addWidget(provenance);
    layout->addStretch();
}

void SurveyPanel::setSequenceInfo(const QString& name, int frames, int frameRate, int width, int height)
{
    m_sequenceLabel->setText(name);
    m_metadataLabel->setText(QStringLiteral("%1 frames\n%2 fps\n%3 × %4 pixels")
                                 .arg(frames).arg(frameRate).arg(width).arg(height));
}
