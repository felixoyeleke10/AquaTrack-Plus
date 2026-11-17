#pragma once
#include "system/settings/AquaTrackSettings.h"
#include <QDialog>
#include <QWidget>
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;

// The preferences: a category list on the left, that category's fields on
// the right, and Restore defaults / Cancel / Save underneath. Only settings
// the application actually uses have a field; the rest are carried through
// unchanged.
class SettingsPage final : public QWidget {
  Q_OBJECT
public:
  explicit SettingsPage(QWidget *parent = nullptr);
  void setSettings(const aquatrack::system::settings::AquaTrackSettings &);
  [[nodiscard]] aquatrack::system::settings::AquaTrackSettings settings() const;
  // Shown under Environment, with a button to open its folder.
  void setSettingsFile(const QString &path);
  // Keeps the recent list current without touching fields being edited.
  void setRecentProjects(const std::vector<std::string> &projects);

signals:
  void saveRequested();
  void cancelRequested();
  // Forget the recent-projects list (the window asks first).
  void clearRecentRequested();

private:
  // Puts `s` into the fields without touching what the page carries through.
  void fillFields(const aquatrack::system::settings::AquaTrackSettings &s);
  void refreshEnvironment();
  void updateTrackerFields();

  QListWidget *m_categories = nullptr;
  QStackedWidget *m_sections = nullptr;

  // Detection
  QDoubleSpinBox *m_confidence = nullptr;
  QDoubleSpinBox *m_nms = nullptr;
  QSpinBox *m_inputSize = nullptr;
  // Tracking
  QComboBox *m_tracker = nullptr;
  QSpinBox *m_minimumHits = nullptr;
  QSpinBox *m_maxMissed = nullptr;
  QDoubleSpinBox *m_matchIou = nullptr;
  // Projects & media
  QLineEdit *m_projectFolder = nullptr;
  QComboBox *m_mediaImport = nullptr;
  QLabel *m_recentCount = nullptr;
  QPushButton *m_clearRecent = nullptr;
  // Playback
  QSpinBox *m_bufferSize = nullptr;
  // Export
  QCheckBox *m_includeUnverified = nullptr;
  // Environment
  QLineEdit *m_python = nullptr;
  QLabel *m_pythonStatus = nullptr;
  QLabel *m_ffmpegStatus = nullptr;
  QLabel *m_settingsFile = nullptr;
  QString m_settingsPath;

  // Fields the page has no control for are carried through unchanged.
  aquatrack::system::settings::AquaTrackSettings m_base;
};

// Settings in a window of their own, beside the main window rather than in
// place of the page being worked on.
class SettingsWindow final : public QDialog {
  Q_OBJECT
public:
  explicit SettingsWindow(QWidget *parent = nullptr);
  [[nodiscard]] SettingsPage *page() const { return m_page; }

private:
  SettingsPage *m_page = nullptr;
};
