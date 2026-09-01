#pragma once

#include <QFrame>

class QPushButton;

class AppHeader final : public QFrame {
    Q_OBJECT

public:
    explicit AppHeader(QWidget* parent = nullptr);
    void setCurrentPage(int page);

signals:
    void pageRequested(int page);

private:
    QPushButton* m_dashboardButton = nullptr;
    QPushButton* m_reviewButton = nullptr;
};
