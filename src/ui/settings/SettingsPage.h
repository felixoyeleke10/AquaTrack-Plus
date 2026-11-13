#pragma once
#include "system/settings/AquaTrackSettings.h"
#include <QWidget>
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class SettingsPage final : public QWidget {
  Q_OBJECT
public:
  explicit SettingsPage(QWidget *parent = nullptr);
  void setSettings(const aquatrack::system::settings::AquaTrackSettings &);
  [[nodiscard]] aquatrack::system::settings::AquaTrackSettings settings() const;
signals:
  void saveRequested();

private:
  void refreshEnvironment();

  QDoubleSpinBox *m_confidence = nullptr;
  QComboBox *m_tracker = nullptr;
  QDoubleSpinBox *m_matchIou = nullptr;
  QSpinBox *m_maxMissed = nullptr;
  QLineEdit *m_python = nullptr;
  QLabel *m_pythonStatus = nullptr;
  QLabel *m_ffmpegStatus = nullptr;
  QSpinBox *m_bufferSize = nullptr;
  QComboBox *m_theme = nullptr;
  QComboBox *m_mediaImport = nullptr;
  // Fields the page has no control for are carried through unchanged.
  aquatrack::system::settings::AquaTrackSettings m_base;
};
