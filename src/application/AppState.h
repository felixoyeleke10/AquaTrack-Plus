#pragma once

#include <QObject>

namespace aquatrack::application {

class AppState final : public QObject {
  Q_OBJECT

public:
  enum class Page {
    Dashboard = 0,
    Videos = 1,
    Review = 2,
    Analytics = 3,
    Datasets = 4,
    Models = 5,
    Settings = 6,
    Export = 7
  };
  Q_ENUM(Page)

  explicit AppState(QObject *parent = nullptr);
  [[nodiscard]] Page currentPage() const { return m_currentPage; }
  void setCurrentPage(Page page);

signals:
  void currentPageChanged(Page page);

private:
  Page m_currentPage = Page::Dashboard;
};

} // namespace aquatrack::application
