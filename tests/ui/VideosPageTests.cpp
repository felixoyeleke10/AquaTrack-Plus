#include "TestSupport.h"

#include "application/controllers/ProjectController.h"
#include "ui/videos/VideosPage.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>

namespace {

void writeFile(const QString& path, const QByteArray& content)
{
    QFile file(path);
    requireTest(file.open(QIODevice::WriteOnly), "create fixture");
    file.write(content);
    file.close();
}

int rowFor(QTableWidget& table, const QString& fileName)
{
    for (int row = 0; row < table.rowCount(); ++row)
        if (table.item(row, 0) && table.item(row, 0)->text() == fileName)
            return row;
    return -1;
}

}  // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    QTemporaryDir directory;
    requireTest(directory.isValid(), "temporary project directory");
    const QDir root(directory.path());

    // A reviewable sequence, a video file this build cannot decode, and a
    // reference to something that no longer exists.
    requireTest(root.mkpath(QStringLiteral("survey-01")), "sequence directory");
    writeFile(root.filePath(QStringLiteral("survey-01/seqinfo.ini")),
              "[Sequence]\nname=Survey 01\nseqLength=42\n");
    writeFile(root.filePath(QStringLiteral("dive.mp4")), "not really a video");

    aquatrack::application::ProjectController controller;
    QString error;
    requireTest(controller.createProject(root.filePath(QStringLiteral("survey.aqp")),
                                         QStringLiteral("Survey"), &error),
                "create project");
    requireTest(controller.addMedia(root.filePath(QStringLiteral("survey-01")), &error),
                "add sequence");
    requireTest(controller.addMedia(root.filePath(QStringLiteral("dive.mp4")), &error),
                "add video file");
    requireTest(controller.addMedia(root.filePath(QStringLiteral("gone.mp4")), &error),
                "add missing reference");

    VideosPage page;
    page.setProject(controller.project());

    auto* table = page.findChild<QTableWidget*>();
    requireTest(table != nullptr, "media table exists");
    requireTest(table->rowCount() == 3, "all media listed");

    auto* review = page.findChild<QPushButton*>(QStringLiteral("openInReview"));
    requireTest(review != nullptr, "review button exists");

    // The sequence is recognised by its seqinfo.ini and is reviewable.
    const int sequenceRow = rowFor(*table, QStringLiteral("survey-01"));
    requireTest(sequenceRow >= 0, "sequence row present");
    table->setCurrentCell(sequenceRow, 0);
    requireTest(table->item(sequenceRow, 1)->text() == QStringLiteral("Sequence"),
                "sequence kind detected");
    requireTest(table->item(sequenceRow, 2)->text().contains(QStringLiteral("42")),
                "sequence frame count read from seqinfo");
    requireTest(review->isEnabled(), "sequence is reviewable");

    QStringList reviewed;
    QObject::connect(&page, &VideosPage::reviewRequested, &page,
                     [&reviewed](const QString& path) { reviewed << path; });
    review->click();
    requireTest(reviewed.size() == 1, "review requested for the sequence");

    // A video file is recorded but cannot be opened: this build has no decoder.
    const int videoRow = rowFor(*table, QStringLiteral("dive.mp4"));
    requireTest(videoRow >= 0, "video row present");
    table->setCurrentCell(videoRow, 0);
    requireTest(table->item(videoRow, 1)->text() == QStringLiteral("Video file"),
                "video kind detected");
    requireTest(!review->isEnabled(), "video files are not reviewable");
    requireTest(review->text() == QStringLiteral("Not reviewable in this build"),
                "the reason is stated on the button");

    // A path that no longer resolves is flagged rather than shown as usable.
    const int missingRow = rowFor(*table, QStringLiteral("gone.mp4"));
    requireTest(missingRow >= 0, "missing row present");
    table->setCurrentCell(missingRow, 0);
    requireTest(table->item(missingRow, 1)->text() == QStringLiteral("Missing"),
                "missing media flagged");

    // Removal round-trips through the controller and back into the page.
    requireTest(controller.removeMedia(root.filePath(QStringLiteral("dive.mp4")), &error),
                "remove media");
    requireTest(!controller.removeMedia(root.filePath(QStringLiteral("dive.mp4")), &error),
                "removing twice is rejected");
    page.setProject(controller.project());
    requireTest(table->rowCount() == 2, "removed media disappears");

    return 0;
}
