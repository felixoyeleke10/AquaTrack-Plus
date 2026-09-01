#include "application/AppState.h"

namespace aquatrack::application {

AppState::AppState(QObject* parent) : QObject(parent) {}

void AppState::setCurrentPage(Page page)
{
    if (m_currentPage == page)
        return;
    m_currentPage = page;
    emit currentPageChanged(page);
}

}  // namespace aquatrack::application
