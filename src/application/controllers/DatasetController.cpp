#include "application/controllers/DatasetController.h"

namespace aquatrack::application {

DatasetController::DatasetController(QObject* parent) : QObject(parent) {}

void DatasetController::scan(const QString& rootPath)
{
    m_root = rootPath;
    rescan();
}

void DatasetController::rescan()
{
    if (m_root.isEmpty())
        return;
    m_result = m_catalog.scan(m_root);
    emit catalogChanged();
}

}  // namespace aquatrack::application
