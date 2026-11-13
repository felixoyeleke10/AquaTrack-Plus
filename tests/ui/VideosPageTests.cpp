#include "TestSupport.h"

#include "application/controllers/ProjectController.h"
#include "ui/videos/VideosPage.h"

#include <QApplication>
#include <QElapsedTimer>
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

void waitForProbes(const VideosPage& page)
{
    QElapsedTimer clock;
    clock.start();
    while (page.isProbing() && clock.elapsed() < 30000)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
    requireTest(!page.isProbing(), "background metadata reads finish");
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

    // A reviewable sequence, a "video" that is not really one, and a
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
    waitForProbes(page);

    auto* table = page.findChild<QTableWidget*>();
    requireTest(table != nullptr, "media table exists");
    requireTest(table->rowCount() == 3, "all media listed");

    auto* review = page.findChild<QPushButton*>(QStringLiteral("openInReview"));
    requireTest(review != nullptr, "review button exists");

    // The sequence is recognised by its seqinfo.ini and is reviewable.
    const int sequenceRow = rowFor(*table, QStringLiteral("survey-01"));
    requireTest(sequenceRow >= 0, "sequence row present");
    table->setCurrentCell(sequenceRow, 0);
    requireTest(table->item(sequenceRow, 1)->text() == QStringLiteral("Sequence \u00b7 image frames"),
                "a frames folder is simply a sequence");
    requireTest(table->item(sequenceRow, 2)->text().contains(QStringLiteral("42")),
                "sequence frame count read from seqinfo");
    requireTest(review->isEnabled(), "sequence is reviewable");

    QStringList reviewed;
    QObject::connect(&page, &VideosPage::reviewRequested, &page,
                     [&reviewed](const QString& path) { reviewed << path; });
    review->click();
    requireTest(reviewed.size() == 1, "review requested for the sequence");

    // A file with a video extension that FFmpeg cannot read is flagged, not
    // offered for review (real videos are covered by the media tests).
    const int videoRow = rowFor(*table, QStringLiteral("dive.mp4"));
    requireTest(videoRow >= 0, "video row present");
    table->setCurrentCell(videoRow, 0);
    requireTest(table->item(videoRow, 1)->text().startsWith(QStringLiteral("Video")),
                "video kind detected from the extension");
    requireTest(table->item(videoRow, 1)->text().contains(QStringLiteral("cannot open")),
                "an undecodable video is flagged in the list");
    requireTest(!review->isEnabled() && review->text() == QStringLiteral("Cannot open"),
                "and cannot be opened in review");
    requireTest(page.selectedVideoPaths().isEmpty(),
                "an undecodable video cannot go into a video sequence");

    // A path that no longer resolves is flagged rather than shown as usable.
    const int missingRow = rowFor(*table, QStringLiteral("gone.mp4"));
    requireTest(missingRow >= 0, "missing row present");
    table->setCurrentCell(missingRow, 0);
    requireTest(table->item(missingRow, 1)->text() == QStringLiteral("Missing"),
                "missing media flagged");

    // A sequence opens like a folder.
    page.openFolder(root.filePath(QStringLiteral("survey-01")));
    auto* contents = page.findChild<QTableWidget*>(QStringLiteral("sequenceContents"));
    requireTest(contents && page.openFolderPath() == root.filePath(QStringLiteral("survey-01")) &&
                    contents->rowCount() >= 1,
                "browsing a sequence lists its contents");
    requireTest(table->item(table->currentRow(), 0)->text() == QStringLiteral("survey-01"),
                "the detail card describes the sequence being browsed");
    page.closeFolder();
    requireTest(page.openFolderPath().isEmpty(), "back to the library");
    table->setCurrentCell(missingRow, 0);

    // Removal round-trips through the controller and back into the page.
    requireTest(controller.removeMedia(root.filePath(QStringLiteral("dive.mp4")), &error),
                "remove media");
    requireTest(!controller.removeMedia(root.filePath(QStringLiteral("dive.mp4")), &error),
                "removing twice is rejected");
    // The last row was selected; the list shrinking under it must not read
    // past the end (it used to, through the stale row-to-entry map).
    page.setProject(controller.project());
    waitForProbes(page);
    requireTest(table->rowCount() == 2, "removed media disappears");
    requireTest(table->currentRow() >= 0 && table->item(table->currentRow(), 0)->text() ==
                                                QStringLiteral("gone.mp4"),
                "selection survives the list shrinking");

    // A folder without seqinfo.ini exists, so it is not "missing".
    requireTest(root.mkpath(QStringLiteral("not-a-sequence")), "plain folder");
    requireTest(controller.addMedia(root.filePath(QStringLiteral("not-a-sequence")), &error),
                "add plain folder");
    page.setProject(controller.project());
    waitForProbes(page);
    const int folderRow = rowFor(*table, QStringLiteral("not-a-sequence"));
    requireTest(folderRow >= 0 &&
                    table->item(folderRow, 1)->text() == QStringLiteral("Not a sequence"),
                "plain folder flagged as not a sequence");

    // A folder with sequence.json is a video sequence; one whose videos are
    // gone says so instead of pretending to be reviewable.
    requireTest(root.mkpath(QStringLiteral("transect")), "video sequence folder");
    writeFile(root.filePath(QStringLiteral("transect/sequence.json")),
              R"({"format":"aquatrack-video-sequence","version":1,"name":"Transect",)"
              R"("clips":[{"path":"gone-1.mp4"},{"path":"gone-2.mp4"}]})");
    requireTest(controller.addMedia(root.filePath(QStringLiteral("transect")), &error),
                "add video sequence");
    page.setProject(controller.project());
    waitForProbes(page);
    const int sequenceFolderRow = rowFor(*table, QStringLiteral("transect"));
    requireTest(sequenceFolderRow >= 0 &&
                    table->item(sequenceFolderRow, 1)->text().startsWith(QStringLiteral("Sequence")) &&
                    table->item(sequenceFolderRow, 1)->text().contains(QStringLiteral("cannot open")),
                "a video sequence with missing clips is flagged");

    // Unsupported files are named for what they are.
    writeFile(root.filePath(QStringLiteral("notes.txt")), "notes");
    requireTest(controller.addMedia(root.filePath(QStringLiteral("notes.txt")), &error), "add text file");
    page.setProject(controller.project());
    waitForProbes(page);
    const int notesRow = rowFor(*table, QStringLiteral("notes.txt"));
    requireTest(notesRow >= 0 && table->item(notesRow, 1)->text() == QStringLiteral("Not a video"),
                "a text file is not listed as a video");

    return 0;
}
