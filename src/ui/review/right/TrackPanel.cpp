#include "ui/review/right/TrackPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

TrackPanel::TrackPanel(QWidget* parent) : QFrame(parent)
{
    setProperty("role", "panel");
    setFixedWidth(290);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 16, 12, 14);
    auto* heading = new QLabel(QStringLiteral("TRACKS IN CURRENT FRAME"), this);
    heading->setProperty("role", "section");
    layout->addWidget(heading);
    m_trackList = new QListWidget(this);
    layout->addWidget(m_trackList, 1);
    auto* toolsHeading = new QLabel(QStringLiteral("ANNOTATION TOOLS"), this);
    toolsHeading->setProperty("role", "section");
    layout->addWidget(toolsHeading);
    auto* tools = new QHBoxLayout;
    for (const auto& text : {QStringLiteral("Select"), QStringLiteral("Box"), QStringLiteral("Merge"), QStringLiteral("Split")}) {
        auto* button = new QPushButton(text, this);
        button->setEnabled(false);
        button->setToolTip(QStringLiteral("Planned for the annotation milestone"));
        tools->addWidget(button);
    }
    layout->addLayout(tools);
    auto* note = new QLabel(QStringLiteral("Editing tools are disabled until annotation persistence is implemented."), this);
    note->setProperty("role", "muted");
    note->setWordWrap(true);
    layout->addWidget(note);
}

void TrackPanel::setTracks(const QVector<aquatrack::GroundTruth>& annotations, bool fishOnly)
{
    m_trackList->clear();
    for (const auto& item : annotations) {
        if (fishOnly && item.classId != 1 && item.classId != 5)
            continue;
        m_trackList->addItem(QStringLiteral("ID %1   %2\nconfidence %3   visibility %4")
                                 .arg(item.trackId)
                                 .arg(aquatrack::BrackishMotSequence::className(item.classId))
                                 .arg(item.confidence, 0, 'f', 2)
                                 .arg(item.visibility, 0, 'f', 2));
    }
    if (m_trackList->count() == 0)
        m_trackList->addItem(QStringLiteral("No matching observations in this frame"));
}
