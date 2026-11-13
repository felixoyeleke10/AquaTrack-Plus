#include "ui/settings/SettingsPage.h"

#include "application/AnalysisRunner.h"
#include "data/media/MediaTools.h"
#include "ui/common/Widgets.h"

#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

using namespace aquatrack::ui;

SettingsPage::SettingsPage(QWidget *parent) : QWidget(parent) {
  // Forms read best at a readable width, not stretched across the window.
  auto *outer = new QHBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  auto *column = new QWidget(this);
  column->setMaximumWidth(860);
  outer->addWidget(column, 1);
  outer->addStretch();
  auto *layout = new QVBoxLayout(column);
  layout->setContentsMargins(28, 24, 28, 24);
  layout->setSpacing(10);
  auto *section = new QLabel("SYSTEM & PIPELINE", this);
  section->setProperty("type", "label");
  auto *title = new QLabel("Settings", this);
  title->setProperty("type", "display");
  layout->addWidget(section);
  layout->addWidget(title);

  // ---- Detection and tracking ------------------------------------------
  layout->addWidget(sectionLabel(QStringLiteral("Detect & track"), this));
  auto *detection = new QFormLayout;
  m_confidence = new QDoubleSpinBox(this);
  m_confidence->setRange(0.01, 1);
  m_confidence->setSingleStep(.05);
  m_confidence->setToolTip(
      "Detections below this score never start or confirm a track");
  detection->addRow("Detection confidence", m_confidence);

  m_tracker = new QComboBox(this);
  m_tracker->setObjectName("tracker");
  m_tracker->addItem("ByteTrack (recommended)", "bytetrack");
  m_tracker->addItem("IoU tracker (transparent baseline)", "iou");
  m_tracker->setToolTip(
      "ByteTrack keeps identities through brief occlusions by also matching "
      "low-scoring detections. The IoU tracker matches boxes by overlap only.");
  detection->addRow("Tracker", m_tracker);

  m_matchIou = new QDoubleSpinBox(this);
  m_matchIou->setRange(0.05, 1);
  m_matchIou->setSingleStep(.05);
  m_matchIou->setToolTip("Overlap needed to continue a track (IoU tracker)");
  detection->addRow("Tracking match IoU", m_matchIou);

  m_maxMissed = new QSpinBox(this);
  m_maxMissed->setRange(1, 600);
  m_maxMissed->setSuffix(" frames");
  m_maxMissed->setToolTip(
      "How long a fish may go undetected before its track ends");
  detection->addRow("Keep lost tracks for", m_maxMissed);

  auto *pythonRow = new QHBoxLayout;
  pythonRow->setContentsMargins(0, 0, 0, 0);
  m_python = new QLineEdit(this);
  m_python->setObjectName("pythonExecutable");
  m_python->setPlaceholderText("Auto-detect (the project's .venv)");
  m_python->setClearButtonEnabled(true);
  connect(m_python, &QLineEdit::textChanged, this,
          &SettingsPage::refreshEnvironment);
  pythonRow->addWidget(m_python, 1);
  auto *browse = textButton("Browse...", this);
  connect(browse, &QPushButton::clicked, this, [this] {
    const QString chosen = QFileDialog::getOpenFileName(
        this, "Python interpreter for detection", QString(),
#ifdef Q_OS_WIN
        "Python (python.exe);;All files (*)"
#else
        "All files (*)"
#endif
    );
    if (!chosen.isEmpty())
      m_python->setText(QDir::toNativeSeparators(chosen));
  });
  pythonRow->addWidget(browse);
  detection->addRow("Python for detection", pythonRow);
  m_pythonStatus = typedLabel(QString(), "dim", this);
  m_pythonStatus->setWordWrap(true);
  m_pythonStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
  detection->addRow(QString(), m_pythonStatus);
  layout->addLayout(detection);

  // ---- Playback and storage ----------------------------------------------
  layout->addSpacing(6);
  layout->addWidget(sectionLabel(QStringLiteral("Playback & storage"), this));
  auto *form = new QFormLayout;
  m_bufferSize = new QSpinBox(this);
  m_bufferSize->setRange(2, 512);
  m_bufferSize->setSuffix(" frames");
  m_bufferSize->setToolTip(
      "Decoded frames kept for instant stepping back (about 8 MB each at "
      "1080p)");
  form->addRow("Frame cache", m_bufferSize);
  m_ffmpegStatus = typedLabel(QString(), "dim", this);
  m_ffmpegStatus->setWordWrap(true);
  m_ffmpegStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
  form->addRow("Video decoder", m_ffmpegStatus);
  m_mediaImport = new QComboBox(this);
  m_mediaImport->setObjectName("mediaImport");
  m_mediaImport->addItem("Ask each time", "ask");
  m_mediaImport->addItem("Copy into the project folder", "copy");
  m_mediaImport->addItem("Link where it is", "link");
  m_mediaImport->setToolTip(
      "Copying keeps a project self-contained and portable; linking saves disk "
      "space but the project breaks if the media moves.");
  form->addRow("When adding media", m_mediaImport);
  m_theme = new QComboBox(this);
  m_theme->addItems({"dark", "light", "system"});
  // Only the dark stylesheet ships; a choice that changes nothing would
  // mislead, so the value is shown but cannot be edited yet.
  m_theme->setEnabled(false);
  m_theme->setToolTip("Only the dark theme is available in this build");
  form->addRow("Theme", m_theme);
  layout->addLayout(form);

  auto *save = new QPushButton("Save settings", this);
  save->setProperty("role", "primary");
  save->setFixedWidth(130);
  layout->addStretch();
  layout->addWidget(save);
  connect(save, &QPushButton::clicked, this, &SettingsPage::saveRequested);
  refreshEnvironment();
}

void SettingsPage::refreshEnvironment() {
  QString python;
  QString package;
  QString error;
  if (aquatrack::application::AnalysisRunner::locatePython(
          m_python->text().trimmed(), &python, &package, &error))
    m_pythonStatus->setText(
        QStringLiteral("Using %1%2")
            .arg(QDir::toNativeSeparators(python),
                 package.isEmpty()
                     ? QStringLiteral(" (aquatrack must be installed in it)")
                     : QStringLiteral("  ·  package in %1")
                           .arg(QDir::toNativeSeparators(package))));
  else
    m_pythonStatus->setText(error);

  const QString ffmpeg = aquatrack::media::ffmpegTool("ffmpeg");
  m_ffmpegStatus->setText(ffmpeg.isEmpty()
                              ? aquatrack::media::ffmpegMissingMessage()
                              : QStringLiteral("FFmpeg at %1")
                                    .arg(QDir::toNativeSeparators(ffmpeg)));
}

void SettingsPage::setSettings(
    const aquatrack::system::settings::AquaTrackSettings &s) {
  m_base = s;
  m_bufferSize->setValue(s.video.frameBufferSize);
  m_confidence->setValue(s.detection.confidenceThreshold);
  m_matchIou->setValue(s.tracking.matchIou);
  m_maxMissed->setValue(std::max(1, s.tracking.maximumMissedFrames));
  const int tracker =
      m_tracker->findData(QString::fromStdString(s.analysis.tracker));
  m_tracker->setCurrentIndex(tracker >= 0 ? tracker : 0);
  m_python->setText(QDir::toNativeSeparators(
      QString::fromStdString(s.analysis.pythonExecutable)));
  m_theme->setCurrentText(QString::fromStdString(s.appearance.theme));
  const int import =
      m_mediaImport->findData(QString::fromStdString(s.storage.mediaImport));
  m_mediaImport->setCurrentIndex(import >= 0 ? import : 0);
  refreshEnvironment();
}

aquatrack::system::settings::AquaTrackSettings SettingsPage::settings() const {
  auto s = m_base;
  s.video.frameBufferSize = m_bufferSize->value();
  s.detection.confidenceThreshold = m_confidence->value();
  s.tracking.matchIou = m_matchIou->value();
  s.tracking.maximumMissedFrames = m_maxMissed->value();
  s.analysis.tracker = m_tracker->currentData().toString().toStdString();
  s.analysis.pythonExecutable =
      QDir::fromNativeSeparators(m_python->text().trimmed()).toStdString();
  s.storage.mediaImport = m_mediaImport->currentData().toString().toStdString();
  return s;
}
