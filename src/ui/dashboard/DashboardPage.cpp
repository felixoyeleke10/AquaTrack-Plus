#include "ui/dashboard/DashboardPage.h"

#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
QFrame* capabilityCard(const QString& title, const QString& description, QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setProperty("role", "card");
    auto* layout = new QVBoxLayout(card);
    auto* heading = new QLabel(title, card);
    heading->setStyleSheet(QStringLiteral("font-size:16px;font-weight:600;color:white"));
    auto* copy = new QLabel(description, card);
    copy->setProperty("role", "muted");
    copy->setWordWrap(true);
    layout->addWidget(heading);
    layout->addWidget(copy);
    layout->addStretch();
    return card;
}
}

DashboardPage::DashboardPage(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(18);
    auto* eyebrow = new QLabel(QStringLiteral("MARINE VIDEO INTELLIGENCE"), this);
    eyebrow->setProperty("role", "section");
    auto* title = new QLabel(QStringLiteral("AquaTrack+ workspace"), this);
    title->setStyleSheet(QStringLiteral("font-size:28px;font-weight:650;color:white"));
    auto* intro = new QLabel(QStringLiteral("Review underwater survey imagery, inspect tracked observations, and prepare reproducible scientific outputs."), this);
    intro->setProperty("role", "muted");
    intro->setWordWrap(true);
    layout->addWidget(eyebrow);
    layout->addWidget(title);
    layout->addWidget(intro);

    auto* grid = new QGridLayout;
    grid->setSpacing(12);
    grid->addWidget(capabilityCard(QStringLiteral("Review sequences"), QStringLiteral("Open BrackishMOT sequences with frame-accurate ground truth and track inspection."), this), 0, 0);
    grid->addWidget(capabilityCard(QStringLiteral("Scientific state"), QStringLiteral("Controllers isolate workflow state from the Qt presentation layer."), this), 0, 1);
    grid->addWidget(capabilityCard(QStringLiteral("Model-ready runtime"), QStringLiteral("Detector contracts are prepared for ONNX Runtime and TensorRT implementations."), this), 1, 0);
    grid->addWidget(capabilityCard(QStringLiteral("Reproducible pipeline"), QStringLiteral("C++ modules and Python training tools are tested independently."), this), 1, 1);
    layout->addLayout(grid, 1);
    auto* review = new QPushButton(QStringLiteral("Open review workspace"), this);
    review->setProperty("role", "primary");
    review->setFixedWidth(190);
    layout->addWidget(review);
    connect(review, &QPushButton::clicked, this, &DashboardPage::reviewRequested);
}
