#pragma once

#include "data/catalog/DatasetCatalog.h"

#include <QObject>

namespace aquatrack::application {

class DatasetController final : public QObject {
    Q_OBJECT

public:
    explicit DatasetController(QObject* parent = nullptr);

    [[nodiscard]] const data::CatalogScanResult& result() const { return m_result; }
    [[nodiscard]] QString root() const { return m_root; }

    void scan(const QString& rootPath);
    // Re-reads the catalog already open, so datasets added on disk show up
    // without asking for the folder again.
    void rescan();

signals:
    void catalogChanged();

private:
    data::DatasetCatalog m_catalog;
    data::CatalogScanResult m_result;
    QString m_root;
};

}  // namespace aquatrack::application
