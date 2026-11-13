// Datasets page views (sortable list, card grid) and the image inspector,
// on a small catalog with one BrackishMOT sequence and one YOLO set.
#include "TestSupport.h"

#include "data/catalog/DatasetCatalog.h"
#include "ui/datasets/DatasetInspector.h"
#include "ui/datasets/DatasetsPage.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QImage>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

void writeText(const QString& path, const QByteArray& text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    requireTest(file.open(QIODevice::WriteOnly | QIODevice::Text), "write fixture");
    file.write(text);
}

void writeImage(const QString& path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImage image(40, 20, QImage::Format_RGB32);
    image.fill(Qt::darkCyan);
    requireTest(image.save(path), "write image");
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QTemporaryDir temporary;
    requireTest(temporary.isValid(), "temporary directory");
    const QDir root(temporary.path());

    // A BrackishMOT sequence: 3 frames, more annotations than the YOLO set.
    writeText(root.filePath(QStringLiteral("brackishMOT-07/seqinfo.ini")),
              "[Sequence]\nname=brackishMOT-07\nimDir=img1\nseqLength=3\nimWidth=40\n"
              "imHeight=20\nimExt=.png\n");
    QByteArray truth;
    for (int frame = 1; frame <= 3; ++frame) {
        writeImage(root.filePath(QStringLiteral("brackishMOT-07/img1/%1.png")
                                     .arg(frame, 6, 10, QLatin1Char('0'))));
        truth += QByteArray::number(frame) + ",4,2,2,8,6,1,2,1\n";
        truth += QByteArray::number(frame) + ",5,20,4,6,6,1,1,1\n";
    }
    writeText(root.filePath(QStringLiteral("brackishMOT-07/gt/gt.txt")), truth);

    // A YOLO set: two training images, one labelled box each.
    writeText(root.filePath(QStringLiteral("Fish-YOLO/dataset.yaml")),
              "path: .\ntrain: images/train\nnames: [fish, crab]\n");
    for (const char* name : {"a", "b"}) {
        writeImage(root.filePath(QStringLiteral("Fish-YOLO/images/train/%1.png").arg(name)));
        writeText(root.filePath(QStringLiteral("Fish-YOLO/labels/train/%1.txt").arg(name)),
                  "0 0.5 0.5 0.25 0.5\n");
    }

    const auto catalog = aquatrack::data::DatasetCatalog().scan(temporary.path());
    requireTest(catalog.datasets.size() == 2, "catalog finds both datasets");
    requireTest(!catalog.datasets[0].previewImage.isEmpty() &&
                    !catalog.datasets[1].previewImage.isEmpty(),
                "catalog records a preview image");

    DatasetsPage page;
    page.resize(1200, 700);
    page.show();
    page.setCatalog(temporary.path(), catalog);
    auto* table = page.findChild<QTableWidget*>(QStringLiteral("datasetTable"));
    auto* grid = page.findChild<QListWidget*>(QStringLiteral("datasetGrid"));
    requireTest(table && grid && table->rowCount() == 2, "list shows both datasets");

    // Sorting by Annotations (numeric) keeps rows tied to the right dataset.
    table->sortItems(3, Qt::DescendingOrder);
    requireTest(table->item(0, 0)->text() == QStringLiteral("brackishMOT-07"),
                "numeric column sorts by value");
    table->setCurrentCell(1, 0);
    requireTest(page.selectedSequencePaths().isEmpty(),
                "selection follows the sorted row (the YOLO set is not a sequence)");

    // Grid: same datasets, same order, shared selection.
    auto* gridButton = page.findChild<QPushButton*>(QStringLiteral("gridView"));
    gridButton->click();
    requireTest(page.isGridView() && grid->count() == 2, "grid shows both datasets");
    requireTest(grid->item(0)->text() == QStringLiteral("brackishMOT-07"),
                "grid follows the list's sort order");
    grid->clearSelection();
    grid->item(0)->setSelected(true);
    grid->setCurrentItem(grid->item(0));
    requireTest(page.selectedSequencePaths().size() == 1, "grid selection drives the page");
    for (int tick = 0; tick < 20; ++tick)
        app.processEvents();

    // Inspector: YOLO split with its labelled boxes.
    const auto yolo = std::find_if(catalog.datasets.cbegin(), catalog.datasets.cend(),
                                   [](const auto& item) { return item.name == "Fish-YOLO"; });
    DatasetInspector yoloInspector(*yolo);
    requireTest(yoloInspector.sources().size() == 1, "one split listed");
    requireTest(yoloInspector.itemCount() == 2, "split images listed");
    requireTest(yoloInspector.boxCount() == 1, "YOLO label drawn as a box");
    yoloInspector.showIndex(1);
    requireTest(yoloInspector.currentIndex() == 1 && yoloInspector.boxCount() == 1,
                "stepping to the next image loads its labels");

    // Inspector: BrackishMOT frames with their ground truth.
    const auto sequence = std::find_if(catalog.datasets.cbegin(), catalog.datasets.cend(),
                                       [](const auto& item) { return item.name == "brackishMOT-07"; });
    DatasetInspector sequenceInspector(*sequence);
    requireTest(sequenceInspector.itemCount() == 3, "sequence frames listed");
    requireTest(sequenceInspector.boxCount() == 2, "ground-truth boxes drawn");
    return 0;
}
