#include "application/controllers/ModelController.h"

#include <algorithm>
#include <utility>

namespace aquatrack::application {

ModelController::ModelController(QObject* parent) : QObject(parent) {}

const data::ModelEntry* ModelController::find(const QString& modelId) const
{
    if (modelId.isEmpty())
        return nullptr;
    const auto found = std::find_if(m_entries.cbegin(), m_entries.cend(),
                                    [&modelId](const data::ModelEntry& entry) {
                                        return QString::fromStdString(entry.manifest.id) == modelId;
                                    });
    return found == m_entries.cend() ? nullptr : &*found;
}

void ModelController::scan(const QString& root)
{
    m_root = root;
    rescan();
}

void ModelController::rescan()
{
    if (m_root.isEmpty())
        return;
    auto result = m_registry.scan(m_root);
    m_entries = std::move(result.entries);
    m_warnings = std::move(result.warnings);
    emit registryChanged();
}

}  // namespace aquatrack::application
