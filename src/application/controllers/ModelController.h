#pragma once

#include "data/ModelRegistry.h"

#include <QObject>

namespace aquatrack::application {

class ModelController final : public QObject {
    Q_OBJECT

public:
    explicit ModelController(QObject* parent = nullptr);

    [[nodiscard]] const QVector<data::ModelEntry>& entries() const { return m_entries; }
    [[nodiscard]] QStringList warnings() const { return m_warnings; }
    [[nodiscard]] QString root() const { return m_root; }
    [[nodiscard]] const data::ModelEntry* find(const QString& modelId) const;

    void scan(const QString& root);
    // Re-reads the registry that is already open, so a manifest edited on disk
    // can be picked up without asking for the folder again.
    void rescan();

signals:
    void registryChanged();

private:
    data::ModelRegistry m_registry;
    QVector<data::ModelEntry> m_entries;
    QStringList m_warnings;
    QString m_root;
};

}  // namespace aquatrack::application
