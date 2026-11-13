// The flows a new user takes to get footage into a project, driven through
// the real dialogs: modal dialogs are answered by a timer that plays the
// user's part, so the chain confirm -> name -> save location -> add is
// exercised exactly as it runs in the app.
#include "TestSupport.h"

#include "application/controllers/ProjectController.h"
#include "ui/MainWindow.h"
#include "ui/dashboard/DashboardPage.h"
#include "ui/datasets/DatasetsPage.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QImage>
#include <QInputDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

namespace {

QString writeSequence(const QDir& root, const QString& name)
{
    const QString directory = root.filePath(name);
    QDir().mkpath(directory + QStringLiteral("/img1"));
    QDir().mkpath(directory + QStringLiteral("/gt"));
    QFile info(directory + QStringLiteral("/seqinfo.ini"));
    requireTest(info.open(QIODevice::WriteOnly | QIODevice::Text), "write seqinfo.ini");
    QTextStream(&info) << "[Sequence]\nname=" << name
                       << "\nimDir=img1\nframeRate=10\nseqLength=2\nimWidth=32\nimHeight=24\n"
                          "imExt=.png\n";
    QFile truth(directory + QStringLiteral("/gt/gt.txt"));
    requireTest(truth.open(QIODevice::WriteOnly | QIODevice::Text), "write gt.txt");
    QTextStream rows(&truth);
    for (int frame = 1; frame <= 2; ++frame) {
        QImage image(32, 24, QImage::Format_RGB32);
        image.fill(Qt::darkBlue);
        requireTest(image.save(QStringLiteral("%1/img1/%2.png")
                                   .arg(directory)
                                   .arg(frame, 6, 10, QLatin1Char('0'))),
                    "write frame");
        rows << frame << ",1,2,2,6,6,1,1,1\n";
    }
    return directory;
}

// Plays the user at each modal dialog, in order, and records what it saw.
class DialogScript final : public QObject {
public:
    DialogScript(QString projectName, QString projectPath)
        : m_name(std::move(projectName)), m_path(std::move(projectPath))
    {
        m_timer.setInterval(40);
        connect(&m_timer, &QTimer::timeout, this, [this] { answer(); });
        m_timer.start();
    }
    QStringList seen;

private:
    void answer()
    {
        QWidget* modal = QApplication::activeModalWidget();
        if (!modal || modal == m_last)
            return;
        m_last = modal;
        if (auto* box = qobject_cast<QMessageBox*>(modal)) {
            seen << QStringLiteral("message: %1").arg(box->text());
            // Confirmations say yes; the copy-or-link prompt chooses copy.
            QAbstractButton* choice = box->button(QMessageBox::Yes);
            for (auto* button : box->buttons())
                if (button->text().startsWith(QStringLiteral("Copy")))
                    choice = button;
            (choice ? choice : box->buttons().constFirst())->click();
        } else if (auto* input = qobject_cast<QInputDialog*>(modal)) {
            seen << QStringLiteral("input: %1").arg(input->labelText());
            input->setTextValue(m_name);
            input->accept();
        } else if (auto* files = qobject_cast<QFileDialog*>(modal)) {
            seen << QStringLiteral("file dialog: %1").arg(files->windowTitle());
            if (files->fileMode() == QFileDialog::Directory)
                files->setDirectory(m_path);  // navigate into it, as a user would
            else
                files->selectFile(m_path);
            static_cast<QDialog*>(files)->accept();  // QFileDialog::accept is protected
        } else if (modal->inherits("QProgressDialog")) {
            m_last = nullptr;  // a copy in progress: let it run
        } else {
            seen << QStringLiteral("unexpected: %1").arg(modal->metaObject()->className());
            modal->close();
        }
    }

    QString m_name;
    QString m_path;
    QTimer m_timer;
    QWidget* m_last = nullptr;
};

}  // namespace

int main(int argc, char* argv[])
{
    QStandardPaths::setTestModeEnabled(true);
    // Qt's own file dialog, so the script can answer it.
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("AquaTrack"));
    app.setApplicationName(QStringLiteral("AquaTrackProjectFlowTests"));
    const QString settingsPath =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
        QStringLiteral("/settings.json");
    QFile::remove(settingsPath);

    QTemporaryDir temporary;
    requireTest(temporary.isValid(), "temporary directory");
    const QDir root(temporary.path());
    QDir().mkpath(root.filePath(QStringLiteral("data")));
    const QDir data(root.filePath(QStringLiteral("data")));
    writeSequence(data, QStringLiteral("seq-a"));
    writeSequence(data, QStringLiteral("seq-b"));
    const QString projectsDir = root.filePath(QStringLiteral("projects"));
    QDir().mkpath(projectsDir);

    {
        MainWindow window;
        window.show();
        auto* projects = window.findChild<aquatrack::application::ProjectController*>();
        requireTest(projects && !projects->project(), "starts without a project");

        // Datasets -> select both -> "Add 2 sequences to a new project..."
        window.openDatasets(data.path());
        auto* table = window.findChild<QTableWidget*>(QStringLiteral("datasetTable"));
        auto* add = window.findChild<QPushButton*>(QStringLiteral("addToProject"));
        requireTest(table && add && table->rowCount() == 2, "datasets listed");
        table->selectAll();
        requireTest(add->isEnabled(), "add offered without a project");

        DialogScript script(QStringLiteral("Survey"), projectsDir);
        add->click();

        for (const auto& line : script.seen)
            std::cout << "  dialog -> " << line.toStdString() << '\n';
        requireTest(projects->project() != nullptr, "a project was created");
        std::cout << "  created " << projects->projectPath().toStdString() << '\n';
        requireTest(QDir::cleanPath(projects->projectPath()) ==
                        QDir::cleanPath(QDir(projectsDir).filePath(QStringLiteral("Survey/Survey.aqp"))),
                    "the project gets its own folder");
        requireTest(projects->project()->mediaPaths.size() == 2,
                    "the selected sequences were added to the new project");
        aquatrack::data::ProjectFileStore store;
        aquatrack::domain::ProjectManifest saved;
        requireTest(store.load(projects->projectPath(), saved) && saved.mediaPaths.size() == 2,
                    "the project file on disk lists the sequences");
        // Copied into the project: inside media/, stored relative on disk.
        const QString mediaFolder =
            QDir::cleanPath(QDir(projectsDir).filePath(QStringLiteral("Survey/media/sequences")));
        for (const auto& path : saved.mediaPaths)
            requireTest(QString::fromStdString(path).startsWith(mediaFolder + QLatin1Char('/')) &&
                            QFileInfo::exists(QString::fromStdString(path) +
                                              QStringLiteral("/gt/gt.txt")),
                        "sequences are copied into the project's media folder");
        QFile manifest(projects->projectPath());
        requireTest(manifest.open(QIODevice::ReadOnly) &&
                        manifest.readAll().contains("\"media/sequences/seq-a\""),
                    "media inside the project is stored relative");
        for (const char* folder : {"media/videos", "media/sequences", "annotations", "models",
                                   "exports", "snapshots"})
            requireTest(QDir(QDir(projectsDir).filePath(QStringLiteral("Survey/") +
                                                        QLatin1String(folder)))
                            .exists(),
                        "the project folder has its layout");
    }

    // Dashboard -> New project, then open a sequence in Review.
    {
        MainWindow window;
        window.show();
        auto* projects = window.findChild<aquatrack::application::ProjectController*>();
        auto* dashboard = window.findChild<DashboardPage*>();
        requireTest(dashboard != nullptr, "dashboard exists");
        DialogScript script(QStringLiteral("Holyrood: north bay"), projectsDir);
        emit dashboard->newProjectRequested();
        requireTest(projects->project() != nullptr, "dashboard creates a project");
        requireTest(QFileInfo(projects->projectPath()).fileName() ==
                        QStringLiteral("Holyrood_ north bay.aqp"),
                    "characters Windows forbids are replaced in the folder name");
        auto* browse = window.findChild<QPushButton*>(QStringLiteral("browseDatasets"));
        requireTest(browse && browse->isVisibleTo(&window),
                    "an empty project offers next steps");
        window.openSequence(data.filePath(QStringLiteral("seq-a")));
        requireTest(projects->project()->mediaPaths.size() == 1,
                    "a sequence opened in review joins the project");
        requireTest(!browse->isVisibleTo(&window), "next steps give way once it has content");
    }
    QFile::remove(settingsPath);
    return 0;
}
