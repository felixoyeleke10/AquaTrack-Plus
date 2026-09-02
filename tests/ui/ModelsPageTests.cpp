#include "TestSupport.h"

#include "data/ModelRegistry.h"
#include "ui/models/ModelsPage.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>

namespace {

void writeManifest(const QDir& root, const QString& id, const QString& name, const QString& type,
                   const QString& artifact, bool createArtifact)
{
    requireTest(root.mkpath(id), "model directory");
    if (createArtifact) {
        QFile weights(root.filePath(id + QStringLiteral("/") + artifact));
        requireTest(weights.open(QIODevice::WriteOnly), "artifact");
        weights.write("weights");
        weights.close();
    }
    QFile manifest(root.filePath(id + QStringLiteral("/model.json")));
    requireTest(manifest.open(QIODevice::WriteOnly), "manifest");
    manifest.write(QStringLiteral(R"({"id":"%1","name":"%2","type":"%3","backend":"ONNX Runtime",)"
                                  R"("precision":"FP16","input":[640,640],"classes":["fish"],)"
                                  R"("metrics":{"map50":0.5},"artifact":"%4"})")
                       .arg(id, name, type, artifact)
                       .toUtf8());
    manifest.close();
}

int rowForModel(QTableWidget& table, const QString& displayName)
{
    for (int row = 0; row < table.rowCount(); ++row)
        if (table.item(row, 0) && table.item(row, 0)->text().startsWith(displayName))
            return row;
    return -1;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    QTemporaryDir directory;
    requireTest(directory.isValid(), "temporary registry");
    const QDir root(directory.path());
    writeManifest(root, QStringLiteral("detector-ready"), QStringLiteral("Ready Detector"),
                  QStringLiteral("detector"), QStringLiteral("model.onnx"), true);
    writeManifest(root, QStringLiteral("detector-broken"), QStringLiteral("Broken Detector"),
                  QStringLiteral("detector"), QStringLiteral("model.onnx"), false);
    writeManifest(root, QStringLiteral("tracker-one"), QStringLiteral("Some Tracker"),
                  QStringLiteral("tracker"), QStringLiteral("model.onnx"), true);

    const aquatrack::data::ModelRegistry registry;
    const auto scan = registry.scan(directory.path());
    requireTest(scan.entries.size() == 3, "three entries discovered");
    requireTest(scan.warnings.size() == 1, "missing artifact warned once");

    ModelsPage page;
    page.setRegistry(directory.path(), scan.entries, scan.warnings);

    auto* table = page.findChild<QTableWidget*>();
    requireTest(table != nullptr, "models table exists");
    requireTest(table->rowCount() == 3, "all models listed");

    QStringList activated;
    QObject::connect(&page, &ModelsPage::activeDetectorRequested, &page,
                     [&activated](const QString& modelId) { activated << modelId; });
    auto* activate = page.findChild<QPushButton*>(QStringLiteral("activateDetector"));
    requireTest(activate != nullptr, "activate button exists");

    // A usable detector can be activated.
    const int readyRow = rowForModel(*table, QStringLiteral("Ready Detector"));
    requireTest(readyRow >= 0, "ready detector listed");
    table->setCurrentCell(readyRow, 0);
    requireTest(activate->isEnabled(), "usable detector is activatable");
    requireTest(activate->text() == QStringLiteral("Set as active detector"),
                "activate button reads as available");
    activate->click();
    requireTest(activated.size() == 1, "activation signal emitted");
    requireTest(activated.first() == QStringLiteral("detector-ready"),
                "activation carries the model id");

    // A detector whose weights are missing cannot.
    const int brokenRow = rowForModel(*table, QStringLiteral("Broken Detector"));
    requireTest(brokenRow >= 0, "broken detector still listed");
    table->setCurrentCell(brokenRow, 0);
    requireTest(!activate->isEnabled() && activate->text() == QStringLiteral("Artifact missing"),
                "missing artifact blocks activation and says so");

    // Neither can a non-detector.
    const int trackerRow = rowForModel(*table, QStringLiteral("Some Tracker"));
    requireTest(trackerRow >= 0, "tracker listed");
    table->setCurrentCell(trackerRow, 0);
    requireTest(!activate->isEnabled()
                    && activate->text() == QStringLiteral("Only detectors can be activated"),
                "non-detectors cannot be activated and say so");

    // The active model is not offered for re-activation.
    table->setCurrentCell(readyRow, 0);
    page.setActiveDetector(QStringLiteral("detector-ready"));
    requireTest(!activate->isEnabled()
                    && activate->text() == QStringLiteral("Already the active detector"),
                "active detector is not re-activatable");

    // Selection survives a filter change rather than snapping back to row 0.
    page.setActiveDetector(QString());
    table->setCurrentCell(trackerRow, 0);
    page.setRegistry(directory.path(), scan.entries, scan.warnings);
    requireTest(table->item(table->currentRow(), 0)->text().startsWith(QStringLiteral("Some Tracker")),
                "selection survives a refresh");

    return 0;
}
