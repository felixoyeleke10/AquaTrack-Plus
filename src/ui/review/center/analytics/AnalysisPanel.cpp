#include "ui/review/center/analytics/AnalysisPanel.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
QFrame* card(QWidget* parent)
{
    auto* frame = new QFrame(parent);
    frame->setProperty("role", "card");
    return frame;
}

QWidget* metricCard(const QString& title, QLabel*& value, QWidget* parent)
{
    auto* frame = card(parent);
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 13, 16, 13);
    auto* name = new QLabel(title, frame);
    name->setProperty("role", "muted");
    value = new QLabel(QStringLiteral("--"), frame);
    value->setProperty("role", "metric");
    layout->addWidget(name);
    layout->addWidget(value);
    return frame;
}

QFrame* chartCard(const QString& title, const QString& body, QWidget* parent)
{
    auto* frame = card(parent);
    auto* layout = new QVBoxLayout(frame);
    auto* heading = new QLabel(title, frame);
    heading->setStyleSheet(QStringLiteral("font-weight:600;color:#e7f2fb"));
    auto* content = new QLabel(body, frame);
    content->setProperty("role", "muted");
    content->setAlignment(Qt::AlignCenter);
    layout->addWidget(heading);
    layout->addWidget(content, 1);
    return frame;
}
}

AnalysisPanel::AnalysisPanel(QWidget* parent) : QTabWidget(parent)
{
    setMinimumHeight(210);
    setMaximumHeight(250);
    auto* summary = new QWidget(this);
    auto* grid = new QGridLayout(summary);
    grid->setContentsMargins(10, 10, 10, 10);
    grid->setSpacing(10);
    grid->addWidget(metricCard(QStringLiteral("Sequence frames"), m_frameValue, summary), 0, 0);
    grid->addWidget(metricCard(QStringLiteral("Visible detections"), m_detectionValue, summary), 0, 1);
    grid->addWidget(metricCard(QStringLiteral("Active track IDs"), m_trackValue, summary), 0, 2);
    grid->addWidget(chartCard(QStringLiteral("Count over time"), QStringLiteral("Frame-level observation trend"), summary), 1, 0, 1, 2);
    grid->addWidget(chartCard(QStringLiteral("Species breakdown"), QStringLiteral("Fish and small-fish distribution"), summary), 1, 2);
    addTab(summary, QStringLiteral("SUMMARY"));

    for (const auto& tab : {QStringLiteral("COUNTS OVER TIME"), QStringLiteral("SPECIES BREAKDOWN")}) {
        auto* page = new QWidget(this);
        auto* layout = new QVBoxLayout(page);
        layout->addWidget(chartCard(tab, QStringLiteral("Analysis view populated from the active sequence"), page));
        addTab(page, tab);
    }
}

void AnalysisPanel::setMetrics(int sequenceFrames, int visibleDetections, int activeTracks)
{
    m_frameValue->setText(QString::number(sequenceFrames));
    m_detectionValue->setText(QString::number(visibleDetections));
    m_trackValue->setText(QString::number(activeTracks));
}
