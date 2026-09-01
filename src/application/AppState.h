#pragma once

#include <QObject>

namespace aquatrack::application {

class AppState final : public QObject {
    Q_OBJECT

public:
    enum class Page { Dashboard = 0, Review = 1 };
    Q_ENUM(Page)

    explicit AppState(QObject* parent = nullptr);
    [[nodiscard]] Page currentPage() const { return m_currentPage; }
    void setCurrentPage(Page page);

signals:
    void currentPageChanged(Page page);

private:
    Page m_currentPage = Page::Review;
};

}  // namespace aquatrack::application
