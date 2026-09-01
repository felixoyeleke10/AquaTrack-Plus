#include "ui/navigation/AppHeader.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

AppHeader::AppHeader(QWidget* parent) : QFrame(parent)
{
    setProperty("role", "header");
    setFixedHeight(58);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(18, 0, 18, 0);
    auto* mark = new QLabel(QStringLiteral("●"), this);
    mark->setStyleSheet(QStringLiteral("color:#2d9cff;font-size:22px"));
    auto* brand = new QLabel(QStringLiteral("AquaTrack+"), this);
    brand->setProperty("role", "brand");
    row->addWidget(mark);
    row->addWidget(brand);
    row->addSpacing(42);

    m_dashboardButton = new QPushButton(QStringLiteral("Dashboard"), this);
    m_reviewButton = new QPushButton(QStringLiteral("Review"), this);
    row->addWidget(m_dashboardButton);
    row->addWidget(m_reviewButton);
    for (const auto& name : {QStringLiteral("Videos"), QStringLiteral("Analytics"),
                             QStringLiteral("Datasets"), QStringLiteral("Models"),
                             QStringLiteral("AquaTrack AI"), QStringLiteral("Settings")}) {
        auto* button = new QPushButton(name, this);
        button->setEnabled(false);
        button->setToolTip(QStringLiteral("This module has not been released yet"));
        row->addWidget(button);
    }
    row->addStretch();
    auto* workspace = new QLabel(QStringLiteral("Marine Research Workspace"), this);
    workspace->setProperty("role", "muted");
    row->addWidget(workspace);

    connect(m_dashboardButton, &QPushButton::clicked, this, [this] { emit pageRequested(0); });
    connect(m_reviewButton, &QPushButton::clicked, this, [this] { emit pageRequested(1); });
    setCurrentPage(1);
}

void AppHeader::setCurrentPage(int page)
{
    m_dashboardButton->setProperty("role", page == 0 ? "navActive" : "");
    m_reviewButton->setProperty("role", page == 1 ? "navActive" : "");
    style()->unpolish(m_dashboardButton);
    style()->polish(m_dashboardButton);
    style()->unpolish(m_reviewButton);
    style()->polish(m_reviewButton);
}
