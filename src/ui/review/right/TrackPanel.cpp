#include "ui/review/right/TrackPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

TrackPanel::TrackPanel(QWidget *parent) : QFrame(parent) {
  setProperty("role", "panel");
  setFixedWidth(290);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 16, 12, 14);
  auto *heading = new QLabel(QStringLiteral("TRACKS IN CURRENT FRAME"), this);
  heading->setProperty("role", "section");
  layout->addWidget(heading);
  m_trackList = new QListWidget(this);
  layout->addWidget(m_trackList, 1);
  auto *toolsHeading = new QLabel(QStringLiteral("ANNOTATION TOOLS"), this);
  toolsHeading->setProperty("role", "section");
  layout->addWidget(toolsHeading);
  auto *editTools = new QHBoxLayout;
  m_relabel = new QPushButton(QStringLiteral("Relabel"), this);
  m_delete = new QPushButton(QStringLiteral("Delete"), this);
  editTools->addWidget(m_relabel);
  editTools->addWidget(m_delete);
  layout->addLayout(editTools);
  auto *historyTools = new QHBoxLayout;
  m_undo = new QPushButton(QStringLiteral("Undo"), this);
  m_redo = new QPushButton(QStringLiteral("Redo"), this);
  m_save = new QPushButton(QStringLiteral("Save"), this);
  m_save->setProperty("role", "primary");
  historyTools->addWidget(m_undo);
  historyTools->addWidget(m_redo);
  historyTools->addWidget(m_save);
  layout->addLayout(historyTools);
  auto *note =
      new QLabel(QStringLiteral("Select an observation to relabel or delete. "
                                "Edits are stored in the active project."),
                 this);
  note->setProperty("role", "muted");
  note->setWordWrap(true);
  layout->addWidget(note);
  setEditingEnabled(false);
  connect(m_trackList, &QListWidget::currentRowChanged, this, [this] {
    const bool selected =
        m_trackList->currentItem() &&
        !m_trackList->currentItem()->data(Qt::UserRole).toString().isEmpty();
    m_relabel->setEnabled(m_editingEnabled && selected);
    m_delete->setEnabled(m_editingEnabled && selected);
  });
  connect(m_relabel, &QPushButton::clicked, this, [this] {
    if (auto *item = m_trackList->currentItem())
      emit relabelRequested(item->data(Qt::UserRole).toString());
  });
  connect(m_delete, &QPushButton::clicked, this, [this] {
    if (auto *item = m_trackList->currentItem())
      emit deleteRequested(item->data(Qt::UserRole).toString());
  });
  connect(m_undo, &QPushButton::clicked, this, &TrackPanel::undoRequested);
  connect(m_redo, &QPushButton::clicked, this, &TrackPanel::redoRequested);
  connect(m_save, &QPushButton::clicked, this, &TrackPanel::saveRequested);
}

void TrackPanel::setObservations(
    const std::vector<aquatrack::domain::Observation> &observations,
    bool fishOnly) {
  m_trackList->clear();
  for (const auto &item : observations) {
    if (fishOnly && item.className != "Fish" && item.className != "Small fish")
      continue;
    auto *row = new QListWidgetItem(
        QStringLiteral("ID %1   %2\nconfidence %3%4")
            .arg(item.localTrackId)
            .arg(QString::fromStdString(item.className))
            .arg(item.confidence, 0, 'f', 2)
            .arg(item.humanVerified ? QStringLiteral("   verified")
                                    : QString()),
        m_trackList);
    row->setData(Qt::UserRole, QString::fromStdString(item.id));
  }
  if (m_trackList->count() == 0)
    m_trackList->addItem(
        QStringLiteral("No matching observations in this frame"));
}

void TrackPanel::setEditingEnabled(bool enabled) {
  m_editingEnabled = enabled;
  m_relabel->setEnabled(false);
  m_delete->setEnabled(false);
  m_undo->setEnabled(false);
  m_redo->setEnabled(false);
  m_save->setEnabled(false);
}

void TrackPanel::setHistoryState(bool canUndo, bool canRedo, bool dirty) {
  m_undo->setEnabled(m_editingEnabled && canUndo);
  m_redo->setEnabled(m_editingEnabled && canRedo);
  m_save->setEnabled(m_editingEnabled && dirty);
}

void TrackPanel::setTracks(const QVector<aquatrack::GroundTruth> &annotations,
                           bool fishOnly) {
  m_trackList->clear();
  for (const auto &item : annotations) {
    if (fishOnly && item.classId != 1 && item.classId != 5)
      continue;
    m_trackList->addItem(
        QStringLiteral("ID %1   %2\nconfidence %3   visibility %4")
            .arg(item.trackId)
            .arg(aquatrack::BrackishMotSequence::className(item.classId))
            .arg(item.confidence, 0, 'f', 2)
            .arg(item.visibility, 0, 'f', 2));
  }
  if (m_trackList->count() == 0)
    m_trackList->addItem(
        QStringLiteral("No matching observations in this frame"));
}
