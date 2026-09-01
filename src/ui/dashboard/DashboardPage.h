#pragma once

#include <QWidget>

class DashboardPage final : public QWidget {
    Q_OBJECT

public:
    explicit DashboardPage(QWidget* parent = nullptr);

signals:
    void reviewRequested();
};
