#include "ui/settings/SettingsPage.h"

#include "application/AnalysisRunner.h"
#include "data/media/MediaTools.h"
#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSpinBox>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

using namespace aquatrack::ui;
using aquatrack::system::settings::AquaTrackSettings;

namespace {

// One setting: what it is and what it does on the left, its control on the
// right, the way system preference panes lay them out.
QWidget *settingRow(const QString &title, const QString &help, QWidget *control,
                    QWidget *parent) {
  auto *row = new QWidget(parent);
  row->setProperty("role", "bare");
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(16, 12, 16, 12);
  layout->setSpacing(24);
  auto *text = new QVBoxLayout;
  text->setSpacing(2);
  text->addWidget(typedLabel(title, "body", row));
  if (!help.isEmpty()) {
    auto *helpLabel = typedLabel(help, "dim", row);
    helpLabel->setWordWrap(true);
    text->addWidget(helpLabel);
  }
  layout->addLayout(text, 1);
  if (control) {
    control->setParent(row);
    layout->addWidget(control, 0, Qt::AlignVCenter);
  }
  return row;
}

// The same row with a label the page updates (a status or a path) as its
// description.
QWidget *settingRow(const QString &title, QLabel *detail, QWidget *control,
                    QWidget *parent) {
  auto *row = new QWidget(parent);
  row->setProperty("role", "bare");
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(16, 12, 16, 12);
  layout->setSpacing(24);
  auto *text = new QVBoxLayout;
  text->setSpacing(2);
  text->addWidget(typedLabel(title, "body", row));
  detail->setParent(row);
  text->addWidget(detail);
  layout->addLayout(text, 1);
  if (control) {
    control->setParent(row);
    layout->addWidget(control, 0, Qt::AlignVCenter);
  }
  return row;
}

// A path that wraps after a folder separator rather than mid-name or after
// the drive letter.
QString wrappablePath(const QString &path) {
  QString native = QDir::toNativeSeparators(path);
  native.replace(QDir::separator(), QString(QDir::separator()) + QChar(0x200B));
  return native;
}

// The rows of one category, in a card with hairlines between them.
QFrame *group(const QList<QWidget *> &rows, QWidget *parent) {
  auto *frame = card(parent);
  auto *column = new QVBoxLayout(frame);
  column->setContentsMargins(0, 2, 0, 2);
  column->setSpacing(0);
  for (int i = 0; i < rows.size(); ++i) {
    if (i > 0)
      column->addWidget(divider(frame));
    rows[i]->setParent(frame);
    column->addWidget(rows[i]);
  }
  return frame;
}

// A category's page: heading, one line on what it covers, then its groups.
QWidget *section(const QString &title, const QString &summary,
                 const QList<QWidget *> &groups, QWidget *parent) {
  auto *scroll = new QScrollArea(parent);
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  scroll->setFrameShape(QFrame::NoFrame);
  auto *content = new QWidget(scroll);
  content->setProperty("role", "bare");
  auto *column = new QVBoxLayout(content);
  column->setContentsMargins(28, 24, 28, 24);
  column->setSpacing(14);
  column->addWidget(typedLabel(title, "title", content));
  auto *about = typedLabel(summary, "small", content);
  about->setWordWrap(true);
  column->addWidget(about);
  column->addSpacing(4);
  for (QWidget *item : groups) {
    item->setParent(content);
    column->addWidget(item);
  }
  column->addStretch(1);
  scroll->setWidget(content);
  return scroll;
}

template <typename Spin> Spin *fixedWidth(Spin *control, int width = 150) {
  control->setFixedWidth(width);
  return control;
}

} // namespace

SettingsPage::SettingsPage(QWidget *parent) : QWidget(parent) {
  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->setSpacing(0);

  auto *body = new QHBoxLayout;
  body->setContentsMargins(0, 0, 0, 0);
  body->setSpacing(0);
  outer->addLayout(body, 1);

  // ---- Categories ---------------------------------------------------------
  auto *rail = new QFrame(this);
  rail->setObjectName(QStringLiteral("settingsRail"));
  rail->setFixedWidth(220);
  auto *railColumn = new QVBoxLayout(rail);
  railColumn->setContentsMargins(12, 20, 12, 12);
  railColumn->setSpacing(10);
  auto *heading = typedLabel(QStringLiteral("Settings"), "subtitle", rail);
  heading->setContentsMargins(10, 0, 0, 0);
  railColumn->addWidget(heading);
  m_categories = new QListWidget(rail);
  m_categories->setObjectName(QStringLiteral("settingsCategories"));
  m_categories->setFrameShape(QFrame::NoFrame);
  m_categories->setIconSize(QSize(16, 16));
  railColumn->addWidget(m_categories, 1);
  body->addWidget(rail);

  m_sections = new QStackedWidget(this);
  m_sections->setObjectName(QStringLiteral("settingsSections"));
  body->addWidget(m_sections, 1);
  const auto addCategory = [this](Glyph glyph, const QString &name, QWidget *page) {
    auto *item = new QListWidgetItem(
        glyphIcon(glyph, 16, palette::TextSecondary, palette::AccentHover), name,
        m_categories);
    item->setSizeHint(QSize(0, 36));
    m_sections->addWidget(page);
  };

  // ---- Detection ----------------------------------------------------------
  m_confidence = fixedWidth(new QDoubleSpinBox(this));
  m_confidence->setObjectName(QStringLiteral("confidence"));
  m_confidence->setRange(0.01, 1);
  m_confidence->setSingleStep(.05);
  m_nms = fixedWidth(new QDoubleSpinBox(this));
  m_nms->setObjectName(QStringLiteral("nmsThreshold"));
  m_nms->setRange(0.05, 1);
  m_nms->setSingleStep(.05);
  m_inputSize = fixedWidth(new QSpinBox(this));
  m_inputSize->setObjectName(QStringLiteral("inputSize"));
  m_inputSize->setRange(160, 2560);
  m_inputSize->setSingleStep(32);
  m_inputSize->setSuffix(QStringLiteral(" px"));
  addCategory(
      Glyph::Sparkle, QStringLiteral("Detection"),
      section(QStringLiteral("Detection"),
              QStringLiteral("How the detector decides what counts as a fish in each "
                             "frame. The detector itself is chosen beside Detect & track "
                             "or on the Models page."),
              {group({settingRow(QStringLiteral("Detection confidence"),
                                 QStringLiteral("Boxes scoring below this are ignored. "
                                                "Lower finds more fish, and more false "
                                                "alarms."),
                                 m_confidence, this),
                      settingRow(QStringLiteral("Overlap suppression (NMS IoU)"),
                                 QStringLiteral("Boxes overlapping more than this are "
                                                "merged into one. Raise it for tight "
                                                "schools where fish overlap."),
                                 m_nms, this),
                      settingRow(QStringLiteral("Fallback input size"),
                                 QStringLiteral("Used only for models that do not "
                                                "record the size they were trained at."),
                                 m_inputSize, this)},
                     this)},
              this));

  // ---- Tracking -----------------------------------------------------------
  m_tracker = fixedWidth(new QComboBox(this), 220);
  m_tracker->setObjectName(QStringLiteral("tracker"));
  m_tracker->addItem(QStringLiteral("ByteTrack (recommended)"), QStringLiteral("bytetrack"));
  m_tracker->addItem(QStringLiteral("IoU tracker (baseline)"), QStringLiteral("iou"));
  connect(m_tracker, &QComboBox::currentIndexChanged, this,
          &SettingsPage::updateTrackerFields);
  m_minimumHits = fixedWidth(new QSpinBox(this));
  m_minimumHits->setObjectName(QStringLiteral("minimumHits"));
  m_minimumHits->setRange(1, 60);
  m_minimumHits->setSuffix(QStringLiteral(" frames"));
  m_maxMissed = fixedWidth(new QSpinBox(this));
  m_maxMissed->setObjectName(QStringLiteral("maximumMissed"));
  m_maxMissed->setRange(1, 600);
  m_maxMissed->setSuffix(QStringLiteral(" frames"));
  m_matchIou = fixedWidth(new QDoubleSpinBox(this));
  m_matchIou->setObjectName(QStringLiteral("matchIou"));
  m_matchIou->setRange(0.05, 1);
  m_matchIou->setSingleStep(.05);
  addCategory(
      Glyph::Tracks, QStringLiteral("Tracking"),
      section(QStringLiteral("Tracking"),
              QStringLiteral("How detections are linked from frame to frame into one "
                             "track per fish."),
              {group({settingRow(QStringLiteral("Tracker"),
                                 QStringLiteral("ByteTrack keeps identities through brief "
                                                "occlusions; the IoU tracker matches by "
                                                "overlap only and is easy to reason "
                                                "about."),
                                 m_tracker, this),
                      settingRow(QStringLiteral("Frames to confirm a track"),
                                 QStringLiteral("A fish must be detected this many times "
                                                "before it becomes a track, which drops "
                                                "flickering false detections. IoU tracker "
                                                "only."),
                                 m_minimumHits, this),
                      settingRow(QStringLiteral("Keep lost tracks for"),
                                 QStringLiteral("How long a fish may go undetected (behind "
                                                "weed, out of frame) and still keep its "
                                                "ID."),
                                 m_maxMissed, this),
                      settingRow(QStringLiteral("Match overlap (IoU)"),
                                 QStringLiteral("Overlap a box needs with a track to "
                                                "continue it. IoU tracker only."),
                                 m_matchIou, this)},
                     this)},
              this));

  // ---- Projects & media ---------------------------------------------------
  auto *folderControl = new QWidget(this);
  folderControl->setProperty("role", "bare");
  auto *folderRow = new QHBoxLayout(folderControl);
  folderRow->setContentsMargins(0, 0, 0, 0);
  folderRow->setSpacing(6);
  m_projectFolder = new QLineEdit(folderControl);
  m_projectFolder->setObjectName(QStringLiteral("defaultProjectFolder"));
  m_projectFolder->setPlaceholderText(QStringLiteral("Documents"));
  m_projectFolder->setClearButtonEnabled(true);
  m_projectFolder->setFixedWidth(240);
  folderRow->addWidget(m_projectFolder);
  auto *browseFolder = textButton(QStringLiteral("Browse..."), folderControl);
  connect(browseFolder, &QPushButton::clicked, this, [this] {
    const QString chosen = QFileDialog::getExistingDirectory(
        this, QStringLiteral("Default folder for new projects"),
        QDir::fromNativeSeparators(m_projectFolder->text()));
    if (!chosen.isEmpty())
      m_projectFolder->setText(QDir::toNativeSeparators(chosen));
  });
  folderRow->addWidget(browseFolder);

  m_mediaImport = fixedWidth(new QComboBox(this), 240);
  m_mediaImport->setObjectName(QStringLiteral("mediaImport"));
  m_mediaImport->addItem(QStringLiteral("Ask each time"), QStringLiteral("ask"));
  m_mediaImport->addItem(QStringLiteral("Copy into the project folder"), QStringLiteral("copy"));
  m_mediaImport->addItem(QStringLiteral("Link where it is"), QStringLiteral("link"));

  auto *recentControl = new QWidget(this);
  recentControl->setProperty("role", "bare");
  auto *recentRow = new QHBoxLayout(recentControl);
  recentRow->setContentsMargins(0, 0, 0, 0);
  recentRow->setSpacing(12);
  m_recentCount = typedLabel(QString(), "small", recentControl);
  recentRow->addWidget(m_recentCount);
  m_clearRecent = textButton(QStringLiteral("Clear list"), recentControl);
  m_clearRecent->setObjectName(QStringLiteral("clearRecentProjects"));
  connect(m_clearRecent, &QPushButton::clicked, this, &SettingsPage::clearRecentRequested);
  recentRow->addWidget(m_clearRecent);
  addCategory(
      Glyph::Files, QStringLiteral("Projects & media"),
      section(QStringLiteral("Projects & media"),
              QStringLiteral("Where new projects go and what happens to footage you add."),
              {group({settingRow(QStringLiteral("Default folder for new projects"),
                                 QStringLiteral("New projects are suggested here. It "
                                                "follows the last place you created one."),
                                 folderControl, this),
                      settingRow(QStringLiteral("When adding media"),
                                 QStringLiteral("Copying keeps a project self-contained "
                                                "and portable; linking saves disk space "
                                                "but breaks if the footage moves."),
                                 m_mediaImport, this),
                      settingRow(QStringLiteral("Recent projects"),
                                 QStringLiteral("The projects listed on the dashboard. "
                                                "Clearing the list does not delete any "
                                                "project."),
                                 recentControl, this)},
                     this)},
              this));

  // ---- Playback -----------------------------------------------------------
  m_bufferSize = fixedWidth(new QSpinBox(this));
  m_bufferSize->setObjectName(QStringLiteral("frameCache"));
  m_bufferSize->setRange(2, 512);
  m_bufferSize->setSuffix(QStringLiteral(" frames"));
  addCategory(
      Glyph::Video, QStringLiteral("Playback"),
      section(QStringLiteral("Playback"),
              QStringLiteral("How the Review page decodes and keeps frames."),
              {group({settingRow(QStringLiteral("Frame cache"),
                                 QStringLiteral("Decoded frames kept for instant stepping "
                                                "back and forth. About 8 MB each at "
                                                "1080p."),
                                 m_bufferSize, this)},
                     this)},
              this));

  // ---- Export -------------------------------------------------------------
  m_includeUnverified = new QCheckBox(QStringLiteral("Include"), this);
  m_includeUnverified->setObjectName(QStringLiteral("includeUnverified"));
  addCategory(
      Glyph::Export, QStringLiteral("Export"),
      section(QStringLiteral("Export"),
              QStringLiteral("What goes into CSV and MOTChallenge files. The Export page "
                             "has the same switch for one-off exports."),
              {group({settingRow(QStringLiteral("Unverified detections"),
                                 QStringLiteral("Include model boxes nobody has verified "
                                                "yet. Turn off to export reviewed data "
                                                "only."),
                                 m_includeUnverified, this)},
                     this)},
              this));

  // ---- Environment --------------------------------------------------------
  auto *pythonControl = new QWidget(this);
  pythonControl->setProperty("role", "bare");
  auto *pythonRow = new QHBoxLayout(pythonControl);
  pythonRow->setContentsMargins(0, 0, 0, 0);
  pythonRow->setSpacing(6);
  m_python = new QLineEdit(pythonControl);
  m_python->setObjectName(QStringLiteral("pythonExecutable"));
  m_python->setPlaceholderText(QStringLiteral("Auto-detect (the project's .venv)"));
  m_python->setClearButtonEnabled(true);
  m_python->setFixedWidth(240);
  connect(m_python, &QLineEdit::textChanged, this, &SettingsPage::refreshEnvironment);
  pythonRow->addWidget(m_python);
  auto *browsePython = textButton(QStringLiteral("Browse..."), pythonControl);
  connect(browsePython, &QPushButton::clicked, this, [this] {
    const QString chosen = QFileDialog::getOpenFileName(
        this, QStringLiteral("Python interpreter for detection"), QString(),
#ifdef Q_OS_WIN
        QStringLiteral("Python (python.exe);;All files (*)")
#else
        QStringLiteral("All files (*)")
#endif
    );
    if (!chosen.isEmpty())
      m_python->setText(QDir::toNativeSeparators(chosen));
  });
  pythonRow->addWidget(browsePython);

  m_pythonStatus = typedLabel(QString(), "dim", this);
  m_pythonStatus->setWordWrap(true);
  m_pythonStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_ffmpegStatus = typedLabel(QString(), "dim", this);
  m_ffmpegStatus->setWordWrap(true);
  m_ffmpegStatus->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_settingsFile = typedLabel(QString(), "dim", this);
  m_settingsFile->setWordWrap(true);
  m_settingsFile->setTextInteractionFlags(Qt::TextSelectableByMouse);
  auto *openSettingsFolder = textButton(QStringLiteral("Open folder"), this);
  openSettingsFolder->setObjectName(QStringLiteral("openSettingsFolder"));
  connect(openSettingsFolder, &QPushButton::clicked, this, [this] {
    if (!m_settingsPath.isEmpty())
      QDesktopServices::openUrl(
          QUrl::fromLocalFile(QFileInfo(m_settingsPath).absolutePath()));
  });

  auto *pythonBlock = new QWidget(this);
  pythonBlock->setProperty("role", "bare");
  auto *pythonColumn = new QVBoxLayout(pythonBlock);
  pythonColumn->setContentsMargins(0, 0, 0, 0);
  pythonColumn->setSpacing(4);
  auto *pythonSetting = settingRow(
      QStringLiteral("Python for detection"),
      QStringLiteral("The interpreter that runs Detect & track. Leave empty to use the "
                     "project's .venv, then python on PATH."),
      pythonControl, pythonBlock);
  pythonColumn->addWidget(pythonSetting);
  m_pythonStatus->setParent(pythonBlock);
  m_pythonStatus->setContentsMargins(16, 0, 16, 12);
  pythonColumn->addWidget(m_pythonStatus);

  addCategory(
      Glyph::Settings, QStringLiteral("Environment"),
      section(QStringLiteral("Environment"),
              QStringLiteral("The tools AquaTrack+ runs, and where it keeps its settings."),
              {group({pythonBlock,
                      settingRow(QStringLiteral("Video decoder"), m_ffmpegStatus, nullptr,
                                 this),
                      settingRow(QStringLiteral("Settings file"), m_settingsFile,
                                 openSettingsFolder, this)},
                     this)},
              this));

  connect(m_categories, &QListWidget::currentRowChanged, m_sections,
          &QStackedWidget::setCurrentIndex);
  m_categories->setCurrentRow(0);

  // ---- Footer -------------------------------------------------------------
  auto *footer = new QFrame(this);
  footer->setObjectName(QStringLiteral("settingsFooter"));
  auto *footerRow = new QHBoxLayout(footer);
  footerRow->setContentsMargins(16, 12, 16, 12);
  footerRow->setSpacing(8);
  auto *defaults = textButton(QStringLiteral("Restore defaults"), footer);
  defaults->setObjectName(QStringLiteral("restoreDefaults"));
  defaults->setToolTip(QStringLiteral(
      "Put every field back to its default. Nothing changes until you save."));
  connect(defaults, &QPushButton::clicked, this, [this] {
    AquaTrackSettings defaults;
    // The defaults of what the page shows; the default project folder is
    // where projects already live, so it stays.
    defaults.storage.defaultProjectDirectory = m_base.storage.defaultProjectDirectory;
    fillFields(defaults);
  });
  footerRow->addWidget(defaults);
  footerRow->addStretch(1);
  auto *cancel = textButton(QStringLiteral("Cancel"), footer);
  cancel->setObjectName(QStringLiteral("cancelSettings"));
  connect(cancel, &QPushButton::clicked, this, &SettingsPage::cancelRequested);
  footerRow->addWidget(cancel);
  auto *save = textButton(QStringLiteral("Save"), footer, "primary");
  save->setObjectName(QStringLiteral("saveSettings"));
  save->setDefault(true);
  save->setMinimumWidth(96);
  connect(save, &QPushButton::clicked, this, &SettingsPage::saveRequested);
  footerRow->addWidget(save);
  outer->addWidget(footer);

  auto *saveShortcut = new QShortcut(QKeySequence::Save, this);
  connect(saveShortcut, &QShortcut::activated, this, &SettingsPage::saveRequested);
  refreshEnvironment();
  updateTrackerFields();
}

void SettingsPage::refreshEnvironment() {
  QString python;
  QString package;
  QString error;
  if (aquatrack::application::AnalysisRunner::locatePython(
          m_python->text().trimmed(), &python, &package, &error))
    m_pythonStatus->setText(
        QStringLiteral("Using %1%2")
            .arg(wrappablePath(python),
                 package.isEmpty()
                     ? QStringLiteral(" (aquatrack must be installed in it)")
                     : QStringLiteral("  ·  package in %1").arg(wrappablePath(package))));
  else
    m_pythonStatus->setText(error);

  const QString ffmpeg = aquatrack::media::ffmpegTool("ffmpeg");
  m_ffmpegStatus->setText(ffmpeg.isEmpty()
                              ? aquatrack::media::ffmpegMissingMessage()
                              : QStringLiteral("FFmpeg at %1").arg(wrappablePath(ffmpeg)));
}

void SettingsPage::updateTrackerFields() {
  // ByteTrack confirms and matches tracks by its own rules
  // (src/aquatrack/analyze.py); these two only steer the IoU tracker.
  const bool iou = m_tracker->currentData().toString() == QStringLiteral("iou");
  m_matchIou->setEnabled(iou);
  m_minimumHits->setEnabled(iou);
}

void SettingsPage::setSettingsFile(const QString &path) {
  m_settingsPath = path;
  m_settingsFile->setText(wrappablePath(path) +
                          QStringLiteral("\nDelete it to start again from the defaults."));
}

void SettingsPage::fillFields(const AquaTrackSettings &s) {
  m_confidence->setValue(s.detection.confidenceThreshold);
  m_nms->setValue(s.detection.nmsThreshold);
  m_inputSize->setValue(s.detection.inputSize);
  const int tracker = m_tracker->findData(QString::fromStdString(s.analysis.tracker));
  m_tracker->setCurrentIndex(tracker >= 0 ? tracker : 0);
  m_minimumHits->setValue(std::max(1, s.tracking.minimumHits));
  m_maxMissed->setValue(std::max(1, s.tracking.maximumMissedFrames));
  m_matchIou->setValue(s.tracking.matchIou);
  m_projectFolder->setText(
      QDir::toNativeSeparators(QString::fromStdString(s.storage.defaultProjectDirectory)));
  const int import = m_mediaImport->findData(QString::fromStdString(s.storage.mediaImport));
  m_mediaImport->setCurrentIndex(import >= 0 ? import : 0);
  m_bufferSize->setValue(s.video.frameBufferSize);
  m_includeUnverified->setChecked(s.exportSettings.includeUnverified);
  m_python->setText(
      QDir::toNativeSeparators(QString::fromStdString(s.analysis.pythonExecutable)));
  updateTrackerFields();
  refreshEnvironment();
}

void SettingsPage::setSettings(const AquaTrackSettings &s) {
  m_base = s;
  fillFields(s);
  setRecentProjects(s.storage.recentProjects);
}

void SettingsPage::setRecentProjects(const std::vector<std::string> &projects) {
  m_base.storage.recentProjects = projects;
  const int recent = int(projects.size());
  m_recentCount->setText(recent == 0 ? QStringLiteral("Empty")
                         : recent == 1 ? QStringLiteral("1 project")
                                       : QStringLiteral("%1 projects").arg(recent));
  m_clearRecent->setEnabled(recent > 0);
}

AquaTrackSettings SettingsPage::settings() const {
  auto s = m_base;
  s.detection.confidenceThreshold = m_confidence->value();
  s.detection.nmsThreshold = m_nms->value();
  s.detection.inputSize = m_inputSize->value();
  s.analysis.tracker = m_tracker->currentData().toString().toStdString();
  s.tracking.minimumHits = m_minimumHits->value();
  s.tracking.maximumMissedFrames = m_maxMissed->value();
  s.tracking.matchIou = m_matchIou->value();
  s.storage.defaultProjectDirectory =
      QDir::cleanPath(QDir::fromNativeSeparators(m_projectFolder->text().trimmed()))
          .toStdString();
  if (m_projectFolder->text().trimmed().isEmpty())
    s.storage.defaultProjectDirectory.clear();
  s.storage.mediaImport = m_mediaImport->currentData().toString().toStdString();
  s.video.frameBufferSize = m_bufferSize->value();
  s.exportSettings.includeUnverified = m_includeUnverified->isChecked();
  s.analysis.pythonExecutable =
      QDir::fromNativeSeparators(m_python->text().trimmed()).toStdString();
  return s;
}

// ------------------------------------------------------------ SettingsWindow

SettingsWindow::SettingsWindow(QWidget *parent) : QDialog(parent) {
  setObjectName(QStringLiteral("settingsWindow"));
  setWindowTitle(QStringLiteral("Settings — AquaTrack+"));
  // A window of its own: it can sit beside the main window, and the page
  // being worked on stays where it was.
  setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
  setModal(false);
  resize(880, 600);
  setMinimumSize(760, 500);
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  m_page = new SettingsPage(this);
  layout->addWidget(m_page);
  connect(m_page, &SettingsPage::cancelRequested, this, &QDialog::reject);
}
