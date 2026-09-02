#include "ui/settings/SettingsPage.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
SettingsPage::SettingsPage(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(28, 24, 28, 24);
  auto *section = new QLabel("SYSTEM & PIPELINE", this);
  section->setProperty("role", "section");
  auto *title = new QLabel("Settings", this);
  title->setStyleSheet("font-size:28px;font-weight:650;color:white");
  auto *form = new QFormLayout;
  m_decodeThreads = new QSpinBox(this);
  m_decodeThreads->setRange(1, 32);
  m_bufferSize = new QSpinBox(this);
  m_bufferSize->setRange(2, 512);
  m_confidence = new QDoubleSpinBox(this);
  m_confidence->setRange(0, 1);
  m_confidence->setSingleStep(.05);
  m_matchIou = new QDoubleSpinBox(this);
  m_matchIou->setRange(0, 1);
  m_matchIou->setSingleStep(.05);
  m_jobLimit = new QSpinBox(this);
  m_jobLimit->setRange(1, 8);
  m_hardwareDecode =
      new QCheckBox("Use hardware decoding when available", this);
  m_theme = new QComboBox(this);
  m_theme->addItems({"dark", "light", "system"});
  form->addRow("Decode threads", m_decodeThreads);
  form->addRow("Frame buffer", m_bufferSize);
  form->addRow("Detection confidence", m_confidence);
  form->addRow("Tracking match IoU", m_matchIou);
  form->addRow("Heavy job limit", m_jobLimit);
  form->addRow("Video", m_hardwareDecode);
  form->addRow("Theme", m_theme);
  auto *save = new QPushButton("Save settings", this);
  save->setProperty("role", "primary");
  save->setFixedWidth(130);
  layout->addWidget(section);
  layout->addWidget(title);
  layout->addLayout(form);
  layout->addStretch();
  layout->addWidget(save);
  connect(save, &QPushButton::clicked, this, &SettingsPage::saveRequested);
}
void SettingsPage::setSettings(
    const aquatrack::system::settings::AquaTrackSettings &s) {
  m_decodeThreads->setValue(s.video.decodeThreads);
  m_bufferSize->setValue(s.video.frameBufferSize);
  m_confidence->setValue(s.detection.confidenceThreshold);
  m_matchIou->setValue(s.tracking.matchIou);
  m_jobLimit->setValue(s.pipeline.heavyJobLimit);
  m_hardwareDecode->setChecked(s.video.hardwareDecode);
  m_theme->setCurrentText(QString::fromStdString(s.appearance.theme));
}
aquatrack::system::settings::AquaTrackSettings SettingsPage::settings() const {
  aquatrack::system::settings::AquaTrackSettings s;
  s.video.decodeThreads = m_decodeThreads->value();
  s.video.frameBufferSize = m_bufferSize->value();
  s.video.hardwareDecode = m_hardwareDecode->isChecked();
  s.detection.confidenceThreshold = m_confidence->value();
  s.tracking.matchIou = m_matchIou->value();
  s.pipeline.heavyJobLimit = m_jobLimit->value();
  s.appearance.theme = m_theme->currentText().toStdString();
  return s;
}
