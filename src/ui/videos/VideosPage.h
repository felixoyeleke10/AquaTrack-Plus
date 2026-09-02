#pragma once

#include "domain/ProjectManifest.h"

#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace aquatrack::ui {
class Badge;
}

// Project media library. A project references two very different things: image
// sequences this build can review, and video files it can only record until a
// decoder exists. The page keeps that distinction visible.
class VideosPage final : public QWidget {
    Q_OBJECT

public:
    explicit VideosPage(QWidget* parent = nullptr);

    void setProject(const aquatrack::domain::ProjectManifest* project);

signals:
    void addMediaRequested();
    void addSequenceRequested();
    void removeMediaRequested(QString path);
    void reviewRequested(QString sequencePath);

private:
    // What a referenced path turns out to be on disk.
    enum class MediaKind { Sequence, VideoFile, Missing };

    struct MediaEntry {
        QString path;
        MediaKind kind = MediaKind::Missing;
        qint64 sizeBytes = 0;
        int frameCount = 0;
    };

    void rebuild();
    void applyFilter();
    void showDetail();
    [[nodiscard]] const MediaEntry* selectedEntry() const;

    QVector<MediaEntry> m_entries;
    QVector<int> m_visible;
    QStringList m_mediaPaths;
    bool m_hasProject = false;

    QLabel* m_projectLabel = nullptr;
    QLabel* m_summary = nullptr;
    QLineEdit* m_search = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_addSequence = nullptr;
    QPushButton* m_addVideo = nullptr;

    QWidget* m_detailBody = nullptr;
    QLabel* m_detailEmpty = nullptr;
    QLabel* m_detailName = nullptr;
    QLabel* m_detailPath = nullptr;
    aquatrack::ui::Badge* m_detailKind = nullptr;
    aquatrack::ui::Badge* m_detailState = nullptr;
    QLabel* m_detailFrames = nullptr;
    QLabel* m_detailSize = nullptr;
    QLabel* m_detailNote = nullptr;
    QPushButton* m_review = nullptr;
    QPushButton* m_reveal = nullptr;
    QPushButton* m_remove = nullptr;
};
