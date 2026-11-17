#pragma once

#include "data/media/MediaTools.h"
#include "domain/ProjectManifest.h"

#include <QHash>
#include <QImage>
#include <QSize>
#include <QStringList>
#include <QThreadPool>
#include <QVector>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;

namespace aquatrack::ui {
class Badge;
}

// The project's media library: videos and sequences (frames reviewed on one
// timeline, stored either as video clips or as images). Metadata and thumbnails
// are read in the background so the page opens instantly however many
// videos a project holds. A sequence opens like a folder to show its clips
// or frames, any of which can be opened in Review.
class VideosPage final : public QWidget {
    Q_OBJECT

public:
    explicit VideosPage(QWidget* parent = nullptr);
    ~VideosPage() override;

    // `projectFolder` is the folder holding the .aqp; media inside it is
    // shown as part of the project, media elsewhere as linked.
    void setProject(const aquatrack::domain::ProjectManifest* project,
                    const QString& projectFolder = QString());

    // Paths of the videos selected in the list or grid, in list order.
    [[nodiscard]] QStringList selectedVideoPaths() const;
    // True while background metadata reads are outstanding (tests wait on it).
    [[nodiscard]] bool isProbing() const { return m_probing > 0; }
    // Shows the contents of a sequence in the library, as if opening a folder.
    void openFolder(const QString& sequencePath);
    void closeFolder();
    [[nodiscard]] QString openFolderPath() const { return m_folderPath; }

signals:
    void addMediaRequested();
    void addVideoFolderRequested();
    // Sequences from a scanned training or benchmark dataset.
    void addFromDatasetRequested();
    void addSequenceRequested();
    void createSequenceRequested(QStringList selectedVideos);
    // Reorder, add or remove the clips of a sequence of videos, or rename it.
    void editSequenceRequested(QString path);
    void removeMediaRequested(QString path);
    // Copy linked media into the project's media folder.
    void copyIntoProjectRequested(QString path);
    void reviewRequested(QString path);
    void reviewAtFrameRequested(QString path, int frame);
    void analyzeRequested(QString path);

private:
    struct Metadata {
        bool known = false;   // probe finished (successfully or not)
        QString error;        // why it cannot be decoded
        int frames = 0;
        qint64 durationMs = 0;
        double frameRate = 0.0;
        QSize size;
        int clips = 0;
        qint64 bytes = 0;
    };

    struct MediaEntry {
        QString path;
        aquatrack::media::MediaKind kind = aquatrack::media::MediaKind::Missing;
        qint64 sizeBytes = 0;
        bool insideProject = false;
        Metadata metadata;
    };

    void rebuild();
    void applyFilter();
    void showDetail();
    void requestMetadata(const MediaEntry& entry);
    void metadataReady(const QString& path, const QString& signature, const Metadata& metadata,
                       const QImage& thumbnail);
    void fillRow(int row, const MediaEntry& entry);
    void showContextMenu(const QPoint& position, QWidget* view);
    void populateFolder();
    void updateSelectionActions();
    [[nodiscard]] const MediaEntry* selectedEntry() const;
    [[nodiscard]] MediaEntry* entryForPath(const QString& path);
    [[nodiscard]] QString stateText(const MediaEntry& entry) const;
    [[nodiscard]] static QString signatureOf(const QString& path);

    QVector<MediaEntry> m_entries;
    QVector<int> m_visible;
    // Selection captured before the backing list is replaced. m_visible maps
    // rows into the *old* list, so it must not be read once the list changes.
    QString m_pendingSelection;
    QStringList m_mediaPaths;
    bool m_hasProject = false;
    QString m_projectFolder;
    QString m_folderPath;

    // Background metadata and thumbnails, keyed by path; a changed file
    // (size or date) is read again.
    QThreadPool m_pool;
    QHash<QString, Metadata> m_metadata;
    QHash<QString, QString> m_signatures;
    QHash<QString, QImage> m_thumbnails;
    QHash<QString, QString> m_requested;
    int m_probing = 0;

    QLabel* m_projectLabel = nullptr;
    QLabel* m_summary = nullptr;
    QLineEdit* m_search = nullptr;
    QComboBox* m_typeFilter = nullptr;
    QPushButton* m_listView = nullptr;
    QPushButton* m_gridView = nullptr;
    QTableWidget* m_table = nullptr;
    QListWidget* m_grid = nullptr;
    QStackedWidget* m_views = nullptr;
    QPushButton* m_addSequence = nullptr;
    QPushButton* m_addVideo = nullptr;
    QPushButton* m_createSequence = nullptr;

    // Folder (sequence contents) view.
    QWidget* m_folderBar = nullptr;
    QLabel* m_folderTitle = nullptr;
    QTableWidget* m_contents = nullptr;

    QWidget* m_detailBody = nullptr;
    QLabel* m_detailEmpty = nullptr;
    QLabel* m_detailName = nullptr;
    QLabel* m_detailPath = nullptr;
    aquatrack::ui::Badge* m_detailKind = nullptr;
    aquatrack::ui::Badge* m_detailState = nullptr;
    QLabel* m_detailFrames = nullptr;
    QLabel* m_detailDuration = nullptr;
    QLabel* m_detailResolution = nullptr;
    QLabel* m_detailRate = nullptr;
    QLabel* m_detailSize = nullptr;
    QLabel* m_detailNote = nullptr;
    QLabel* m_detailLocation = nullptr;
    QPushButton* m_review = nullptr;
    QPushButton* m_analyze = nullptr;
    QPushButton* m_browse = nullptr;
    QPushButton* m_reveal = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_copyIn = nullptr;
};
