#pragma once

#include <QFrame>
#include <QVector>

class QLabel;
class QPushButton;

namespace aquatrack::ui {
class Badge;
}

// Application chrome: brand block, primary navigation, and workspace status.
// Navigation is data-driven so adding a page means adding one row to the
// table in the constructor rather than editing seven parallel members.
class AppHeader final : public QFrame {
    Q_OBJECT

public:
    explicit AppHeader(QWidget* parent = nullptr);

    void setCurrentPage(int page);
    void setProjectName(const QString& name);
    void setNotificationCount(int count);

signals:
    void pageRequested(int page);
    void helpRequested();

private:
    QVector<QPushButton*> m_navButtons;
    QLabel* m_projectLabel = nullptr;
    QLabel* m_projectCaption = nullptr;
    QLabel* m_avatar = nullptr;
    aquatrack::ui::Badge* m_notifications = nullptr;
};
