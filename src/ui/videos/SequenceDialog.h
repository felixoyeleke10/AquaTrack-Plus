#pragma once

#include "data/media/VideoProbe.h"

#include <QDialog>
#include <QHash>
#include <QVector>

#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

// Creates or edits a sequence of videos: its name, and its clips in play
// order. Clips can be dragged into order, moved with buttons, added and
// removed; every clip is probed as it is added, so what is accepted is
// ready to save.
class SequenceDialog final : public QDialog {
    Q_OBJECT

public:
    // `nameProblem` says why a name cannot be used (empty when it can).
    SequenceDialog(const QString& title, const QString& name,
                   const QVector<aquatrack::media::VideoInfo>& clips,
                   std::function<QString(const QString&)> nameProblem, QWidget* parent = nullptr);

    [[nodiscard]] QString name() const;
    [[nodiscard]] QVector<aquatrack::media::VideoInfo> clips() const;
    // A line under the list, e.g. what happens to existing annotations.
    void setNote(const QString& note);

private:
    void addVideos();
    void removeSelected();
    void moveSelected(int delta);
    void refresh();
    void appendClip(const aquatrack::media::VideoInfo& info);

    QLineEdit* m_name = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_summary = nullptr;
    QLabel* m_problem = nullptr;
    QLabel* m_note = nullptr;
    QPushButton* m_ok = nullptr;
    QPushButton* m_remove = nullptr;
    QPushButton* m_up = nullptr;
    QPushButton* m_down = nullptr;
    QHash<QString, aquatrack::media::VideoInfo> m_infos;  // by cleaned lower-case path
    std::function<QString(const QString&)> m_nameProblem;
};
