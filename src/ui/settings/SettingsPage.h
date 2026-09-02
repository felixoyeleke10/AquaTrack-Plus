#pragma once
#include "system/settings/AquaTrackSettings.h"
#include <QWidget>
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class SettingsPage final : public QWidget {
Q_OBJECT public : explicit SettingsPage(QWidget *parent = nullptr);
  void setSettings(const aquatrack::system::settings::AquaTrackSettings &);
  [[nodiscard]] aquatrack::system::settings::AquaTrackSettings settings() const;
signals:
  void saveRequested();

private:
  QSpinBox *m_decodeThreads = nullptr;
  QSpinBox *m_bufferSize = nullptr;
  QDoubleSpinBox *m_confidence = nullptr;
  QDoubleSpinBox *m_matchIou = nullptr;
  QSpinBox *m_jobLimit = nullptr;
  QCheckBox *m_hardwareDecode = nullptr;
  QComboBox *m_theme = nullptr;
};
