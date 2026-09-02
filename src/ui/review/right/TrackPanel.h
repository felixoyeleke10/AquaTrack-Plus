#pragma once

#include "data/BrackishMotSequence.h"
#include "domain/ScientificTypes.h"

#include <QFrame>

class QListWidget;
class QPushButton;

class TrackPanel final : public QFrame {
  Q_OBJECT
public:
  explicit TrackPanel(QWidget *parent = nullptr);
  void setTracks(const QVector<aquatrack::GroundTruth> &annotations,
                 bool fishOnly);
  void setObservations(
      const std::vector<aquatrack::domain::Observation> &observations,
      bool fishOnly);
  void setEditingEnabled(bool enabled);
  void setHistoryState(bool canUndo, bool canRedo, bool dirty);
signals:
  void relabelRequested(QString observationId);
  void deleteRequested(QString observationId);
  void undoRequested();
  void redoRequested();
  void saveRequested();

private:
  QListWidget *m_trackList = nullptr;
  QPushButton *m_relabel = nullptr;
  QPushButton *m_delete = nullptr;
  QPushButton *m_undo = nullptr;
  QPushButton *m_redo = nullptr;
  QPushButton *m_save = nullptr;
  bool m_editingEnabled = false;
};
