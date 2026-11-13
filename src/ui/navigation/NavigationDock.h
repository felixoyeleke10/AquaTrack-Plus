#pragma once

#include <QVector>
#include <QWidget>

class QPushButton;

// The page bar along the bottom of the window, after the editing suites'
// page bar: one thin full-width strip with the product and the latest status
// message on the left and a centred row of icon-only page slots, in workflow
// order and grouped by stage. The current page is a lit slot marked on its top
// edge; names appear on hover. Each button carries its
// application::AppState::Page.
class NavigationDock final : public QWidget {
    Q_OBJECT

public:
    explicit NavigationDock(QWidget* parent = nullptr);

    // Highlights the button for `page`.
    void setCurrentPage(int page);
    // Shown beside the product name; replaces a separate status bar. Empty
    // falls back to "Ready".
    void setStatusMessage(const QString& message);

signals:
    void pageRequested(int page);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct Entry {
        QPushButton* button = nullptr;
        int page = 0;
    };
    QVector<Entry> m_entries;
    QWidget* m_left = nullptr;
    QString m_status;
};
