#include "data/BrackishMotSequence.h"
#include "TestSupport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

namespace {
void writeFixture(const QString& path, const QByteArray& content)
{
    QFile file(path);
    requireTest(file.open(QIODevice::WriteOnly), "open test fixture");
    file.write(content);
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporary;
    requireTest(temporary.isValid(), "temporary directory");
    QDir root(temporary.path());
    requireTest(root.mkpath(QStringLiteral("img1")), "image directory");
    requireTest(root.mkpath(QStringLiteral("gt")), "ground truth directory");
    writeFixture(root.filePath(QStringLiteral("seqinfo.ini")),
                 "[Sequence]\nname=sample\nimDir=img1\nframeRate=15\nseqLength=2\n"
                 "imWidth=1920\nimHeight=1080\nimExt=.jpg\n");
    writeFixture(root.filePath(QStringLiteral("gt/gt.txt")), "1,4,100,200,50,30,1,5,1\n");
    aquatrack::BrackishMotSequence sequence;
    QString error;
    requireTest(sequence.load(temporary.path(), &error), "sequence load");
    requireTest(sequence.name() == QStringLiteral("sample"), "sequence name");
    requireTest(sequence.annotations(1).size() == 1, "sequence annotations");
    requireTest(aquatrack::BrackishMotSequence::className(5) == QStringLiteral("Small fish"), "class map");
    return 0;
}
