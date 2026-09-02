#include "TestSupport.h"
#include "data/ModelRegistry.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>
int main(int argc, char *argv[]) {
  QCoreApplication app(argc, argv);
  QTemporaryDir d;
  QDir root(d.path());
  requireTest(root.mkpath("detector-v1"), "model directory");
  QFile artifact(root.filePath("detector-v1/model.onnx"));
  requireTest(artifact.open(QIODevice::WriteOnly), "artifact");
  artifact.write("onnx");
  artifact.close();
  QFile file(root.filePath("detector-v1/model.json"));
  requireTest(file.open(QIODevice::WriteOnly), "manifest");
  file.write(
      R"({"id":"detector-v1","name":"Fish Detector v1","type":"detector","backend":"ONNX Runtime","precision":"FP16","input":[1280,1280],"classes":["fish"],"metrics":{"map50":0.91},"artifact":"model.onnx"})");
  file.close();
  aquatrack::data::ModelRegistry registry;
  const auto result = registry.scan(d.path());
  requireTest(result.warnings.empty() && result.entries.size() == 1,
              "registry discovery");
  requireTest(result.entries.front().manifest.metrics.at("map50") == 0.91, "metric");
  requireTest(result.entries.front().artifactExists, "artifact located");

  // A manifest whose weights file is absent is still listed, but flagged.
  requireTest(root.mkpath("detector-v2"), "second model directory");
  QFile broken(root.filePath("detector-v2/model.json"));
  requireTest(broken.open(QIODevice::WriteOnly), "second manifest");
  broken.write(
      R"({"id":"detector-v2","name":"Fish Detector v2","type":"detector","backend":"ONNX Runtime","precision":"FP16","input":[1280,1280],"classes":["fish"],"metrics":{"map50":0.93},"artifact":"missing.onnx"})");
  broken.close();
  const auto second = registry.scan(d.path());
  requireTest(second.entries.size() == 2, "invalid artifact still listed");
  requireTest(second.warnings.size() == 1, "missing artifact warns");
  const auto missing = std::find_if(
      second.entries.cbegin(), second.entries.cend(),
      [](const auto &entry) { return entry.manifest.id == "detector-v2"; });
  requireTest(missing != second.entries.cend() && !missing->isUsable(),
              "missing artifact is not usable");
  return 0;
}
