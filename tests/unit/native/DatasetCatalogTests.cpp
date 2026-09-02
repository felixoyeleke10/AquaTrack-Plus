#include "TestSupport.h"
#include "data/catalog/DatasetCatalog.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QString>

namespace {
void writeFile(const QString &path, const QByteArray &content) {
  QFile file(path);
  requireTest(file.open(QIODevice::WriteOnly), "create catalog fixture");
  requireTest(file.write(content) == content.size(), "write catalog fixture");
}
} // namespace

int main() {
  QTemporaryDir temporary;
  requireTest(temporary.isValid(), "temporary directory");
  QDir root(temporary.path());
  requireTest(root.mkpath(QStringLiteral("brackish/seq01/gt")),
              "Brackish directories");
  writeFile(root.filePath(QStringLiteral("brackish/seq01/seqinfo.ini")),
            "[Sequence]\nname=Harbour 01\nseqLength=25\n");
  writeFile(root.filePath(QStringLiteral("brackish/seq01/gt/gt.txt")),
            "1,1,0,0,10,10,1,1,1\n");
  // Give the sequence real imagery so media presence can be asserted.
  requireTest(root.mkpath(QStringLiteral("brackish/seq01/img1")), "image directory");
  for (int frame = 1; frame <= 3; ++frame)
    writeFile(root.filePath(QStringLiteral("brackish/seq01/img1/%1.jpg")
                                .arg(frame, 6, 10, QLatin1Char('0'))),
              "jpg");

  requireTest(root.mkpath(QStringLiteral("sfish/images/train")), "YOLO images");
  requireTest(root.mkpath(QStringLiteral("sfish/labels/train")), "YOLO labels");
  for (int index = 0; index < 4; ++index) {
    writeFile(root.filePath(QStringLiteral("sfish/images/train/%1.jpg").arg(index)), "jpg");
    writeFile(root.filePath(QStringLiteral("sfish/labels/train/%1.txt").arg(index)),
              "0 .5 .5 .1 .1\n");
  }
  writeFile(root.filePath(QStringLiteral("sfish/data.yaml")),
            "path: .\ntrain: images/train\nnames: [fish, crab]\n");
  aquatrack::data::DatasetCatalog catalog;
  const auto first = catalog.scan(temporary.path());
  requireTest(first.datasets.size() == 2, "discover supported datasets");
  requireTest(first.datasets[0].frameCount == 25, "read sequence frame count");
  requireTest(first.datasets[0].hasAnnotations, "annotation availability");
  requireTest(!first.datasets[0].id.isEmpty(), "stable dataset identity");
  const auto second = catalog.scan(temporary.path());
  requireTest(second.datasets[0].id == first.datasets[0].id,
              "stable identity across scans");
  requireTest(!catalog.scan(root.filePath(QStringLiteral("missing")))
                   .warnings.isEmpty(),
              "missing root warning");

  // BrackishMOT: imagery is counted, and a short sequence is flagged rather
  // than silently accepted.
  requireTest(first.datasets[0].imageCount == 3, "sequence images counted");
  requireTest(first.datasets[0].mediaPresent, "sequence media present");
  requireTest(first.datasets[0].annotationCount == 1, "ground-truth rows counted");
  requireTest(first.datasets[0].isReviewable(), "complete sequence is reviewable");

  // YOLO: the manifest is actually read rather than reported as zero frames.
  const auto &yolo = first.datasets[1];
  requireTest(yolo.format == aquatrack::data::DatasetFormat::YoloDetection, "YOLO format");
  requireTest(yolo.classNames.size() == 2, "YOLO class names parsed");
  requireTest(yolo.splits.size() == 1 && yolo.splits[0].name == QStringLiteral("train"),
              "YOLO split discovered");
  requireTest(yolo.imageCount == 4 && yolo.annotationCount == 4, "YOLO split counted");
  requireTest(yolo.frameCount == 4, "YOLO frame count reflects images");
  requireTest(!yolo.isReviewable(), "YOLO datasets are not review targets");
  return 0;
}
