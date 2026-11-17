#include "ui/review/center/ReviewPanel.h"

#include "ui/AppTheme.h"
#include "ui/common/Charts.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"
#include "ui/review/center/video/SequenceView.h"

#include <QActionGroup>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

using namespace aquatrack::ui;
namespace analytics = aquatrack::analytics;

namespace {

// HH:MM:SS:FF, the frame within its second, as editing software shows it.
QString timecode(qint64 milliseconds, int frameRate)
{
    const qint64 safe = std::max<qint64>(0, milliseconds);
    const qint64 seconds = safe / 1000;
    const int frame = int((safe % 1000) * std::max(1, frameRate) / 1000);
    return QStringLiteral("%1:%2:%3:%4")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'))
        .arg(frame, 2, 10, QLatin1Char('0'));
}

QString clock(qint64 milliseconds)
{
    const qint64 seconds = std::max<qint64>(0, milliseconds) / 1000;
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString shortClock(qint64 milliseconds)
{
    const qint64 seconds = std::max<qint64>(0, milliseconds) / 1000;
    return QStringLiteral("%1:%2")
        .arg(seconds / 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

// One analytics card: a titled panel wrapping a chart.
QFrame* chartCard(const QString& title, QWidget* body, QWidget* parent, QLabel** footer = nullptr)
{
    auto* frame = card(parent);
    auto* column = new QVBoxLayout(frame);
    column->setContentsMargins(12, 10, 12, 10);
    column->setSpacing(7);

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->addWidget(sectionLabel(title, frame));
    head->addStretch();
    if (footer) {
        *footer = typedLabel(QString(), "dim", frame);
        (*footer)->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        head->addWidget(*footer);
    }
    column->addLayout(head);

    body->setParent(frame);
    column->addWidget(body, 1);
    return frame;
}

// Text columns read from the left, figures line up on the right; headers
// follow their column. `numeric` lists the right-aligned columns.
QTableWidget* buildTable(const QStringList& headers, const QList<int>& numeric, QWidget* parent)
{
    auto* table = new QTableWidget(0, headers.size(), parent);
    table->setHorizontalHeaderLabels(headers);
    for (int column = 0; column < headers.size(); ++column)
        table->horizontalHeaderItem(column)->setTextAlignment(
            (numeric.contains(column) ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(30);
    table->setCursor(Qt::PointingHandCursor);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setHighlightSections(false);
    table->setWordWrap(false);
    return table;
}

QTableWidgetItem* textCell(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return item;
}

QTableWidgetItem* numberCell(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return item;
}

}  // namespace

ReviewPanel::ReviewPanel(QWidget* parent) : QWidget(parent)
{
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    // The frame and the material below it compete for the same vertical
    // space, and how much each deserves depends on the task: scrubbing wants
    // a large frame, auditing tracks wants a large table. A splitter lets the
    // reviewer decide, and stops the transport bar being squeezed out when
    // the window is short.
    auto* split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);
    split->setHandleWidth(10);
    column->addWidget(split);

    // ---- Canvas ------------------------------------------------------
    auto* canvasCard = card(split);
    auto* canvasBody = new QVBoxLayout(canvasCard);
    canvasBody->setContentsMargins(1, 1, 1, 1);
    canvasBody->setSpacing(0);
    canvasBody->addWidget(buildHeader());
    m_view = new SequenceView(canvasCard);
    canvasBody->addWidget(m_view, 1);
    canvasBody->addWidget(buildTransport());
    canvasCard->setMinimumHeight(240);
    split->addWidget(canvasCard);

    auto* lower = new QWidget(split);
    auto* lowerColumn = new QVBoxLayout(lower);
    lowerColumn->setContentsMargins(0, 0, 0, 0);
    lowerColumn->setSpacing(10);

    // ---- Sequence timeline -------------------------------------------
    auto* timelineCard = card(lower);
    auto* timelineBody = new QVBoxLayout(timelineCard);
    timelineBody->setContentsMargins(14, 12, 14, 10);
    timelineBody->setSpacing(4);

    // Fish activity across the sequence as a heat strip, with the track
    // lanes as one of its modes. Clicking either seeks.
    m_activity = new ActivityPanel(timelineCard);
    m_timeline = m_activity->lanes();
    connect(m_timeline, &TrackLanes::frameRequested, this, &ReviewPanel::frameRequested);
    connect(m_activity, &ActivityPanel::timestampRequested, this, &ReviewPanel::timestampRequested);
    timelineBody->addWidget(m_activity);

    m_filmstrip = new Filmstrip(timelineCard);
    connect(m_filmstrip, &Filmstrip::frameRequested, this, &ReviewPanel::frameRequested);
    timelineBody->addWidget(m_filmstrip);
    lowerColumn->addWidget(timelineCard);

    // ---- Tables ------------------------------------------------------
    auto* tabCard = card(lower);
    auto* tabBody = new QVBoxLayout(tabCard);
    tabBody->setContentsMargins(10, 6, 10, 10);
    m_tabs = new QTabWidget(tabCard);
    m_tabs->setDocumentMode(true);
    m_trackTable = buildTable({QStringLiteral("Track"), QStringLiteral("Species"),
                               QStringLiteral("First"), QStringLiteral("Last"),
                               QStringLiteral("Detections"), QStringLiteral("Duration")},
                              {2, 3, 4, 5}, m_tabs);
    m_trackTable->setObjectName(QStringLiteral("trackTable"));
    m_trackTable->horizontalHeaderItem(2)->setToolTip(QStringLiteral("First frame the track is seen"));
    m_trackTable->horizontalHeaderItem(3)->setToolTip(QStringLiteral("Last frame the track is seen"));
    m_trackTable->horizontalHeaderItem(4)->setToolTip(QStringLiteral("Boxes recorded for the track"));
    // The figures fit their headings and values, so none is ever cut short;
    // Species takes the spare width, keeping the figures together on the right.
    for (int column : {0, 2, 3, 4, 5})
        m_trackTable->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    m_trackTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_trackTable->setToolTip(QStringLiteral("Click a track to go to its first frame"));
    connect(m_trackTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (auto* item = m_trackTable->item(row, 2))
            emit frameRequested(item->data(Qt::UserRole).toInt());
    });
    m_eventTable = buildTable({QStringLiteral("Time"), QStringLiteral("Event")}, {}, m_tabs);
    m_eventTable->setObjectName(QStringLiteral("eventTable"));
    m_eventTable->setToolTip(QStringLiteral("Click an event to go to it"));
    m_eventTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    connect(m_eventTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (auto* item = m_eventTable->item(row, 0))
            emit timestampRequested(item->data(Qt::UserRole).toLongLong());
    });
    m_tabs->addTab(m_trackTable, QStringLiteral("Tracks"));
    m_tabs->addTab(m_eventTable, QStringLiteral("Events"));
    // The summary charts: useful, but not while scrubbing, so they wait in
    // a tab instead of squeezing the tables.
    m_tabs->addTab(buildAnalyticsRow(m_tabs), QStringLiteral("Charts"));
    tabBody->addWidget(m_tabs);
    tabCard->setMinimumHeight(190);
    lowerColumn->addWidget(tabCard, 1);

    split->addWidget(lower);
    // The frame is what a reviewer looks at: give it the space, and the
    // charts below their minimum; the handle lets them have more.
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 1);
    split->setSizes({1000, 1});
}

QWidget* ReviewPanel::buildHeader()
{
    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("reviewHeader"));
    header->setAttribute(Qt::WA_StyledBackground, true);
    header->setStyleSheet(QStringLiteral("#reviewHeader{background:%1;border-bottom:1px solid %2}")
                              .arg(palette::Chrome.name(), palette::Line.name()));
    auto* column = new QVBoxLayout(header);
    column->setContentsMargins(12, 7, 10, 7);
    column->setSpacing(6);

    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);
    // Left: what is open. The name comes first and keeps its room; its
    // status badge sits right beside it.
    auto* titles = new QWidget(header);
    titles->setProperty("role", "bare");
    titles->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* titleColumn = new QVBoxLayout(titles);
    titleColumn->setContentsMargins(0, 0, 0, 0);
    titleColumn->setSpacing(1);
    m_titleRow = new QWidget(titles);
    m_titleRow->setProperty("role", "bare");
    auto* titleLine = new QHBoxLayout(m_titleRow);
    titleLine->setContentsMargins(0, 0, 0, 0);
    titleLine->setSpacing(8);
    m_title = new QLabel(QStringLiteral("Nothing open"), m_titleRow);
    m_title->setObjectName(QStringLiteral("reviewTitle"));
    m_title->setProperty("role", "reviewTitle");
    m_title->setTextInteractionFlags(Qt::TextSelectableByMouse);
    titleLine->addWidget(m_title, 0, Qt::AlignVCenter);
    m_source = new QLabel(m_titleRow);
    m_source->setObjectName(QStringLiteral("annotationSource"));
    m_source->hide();
    titleLine->addWidget(m_source, 0, Qt::AlignVCenter);
    titleLine->addStretch(1);
    titleColumn->addWidget(m_titleRow);
    m_detail = typedLabel(QStringLiteral("Open a sequence or video from the Video library"), "dim",
                          titles);
    m_detail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    titleColumn->addWidget(m_detail);
    row->addWidget(titles, 1);

    // Right: a secondary icon action, then the one primary action.
    m_import = iconButton(Glyph::Annotations, QStringLiteral("Import annotations"), header);
    m_import->setObjectName(QStringLiteral("importAnnotations"));
    m_import->setToolTip(QStringLiteral(
        "Import annotations: ground-truth tracks from a MOTChallenge gt.txt or a COCO-style "
        "tracking JSON (for example SFISHTRACK annotations)"));
    connect(m_import, &QPushButton::clicked, this, &ReviewPanel::importAnnotationsRequested);
    row->addWidget(m_import, 0, Qt::AlignVCenter);
    row->addSpacing(2);

    // Detect & track with its detector as one split button: the action on
    // the left, which model runs it on the right.
    auto* split = new QWidget(header);
    split->setProperty("role", "bare");
    auto* splitRow = new QHBoxLayout(split);
    splitRow->setContentsMargins(0, 0, 0, 0);
    splitRow->setSpacing(0);
    m_analyze = textButton(QStringLiteral("Detect && track"), split, "primary");
    m_analyze->setObjectName(QStringLiteral("analyzeMedia"));
    m_analyze->setIcon(glyphIcon(Glyph::Sparkle, 15, Qt::white, Qt::white));
    m_analyze->setToolTip(QStringLiteral(
        "Run the detector and tracker over every frame and load the tracks for review"));
    connect(m_analyze, &QPushButton::clicked, this, &ReviewPanel::analyzeRequested);
    splitRow->addWidget(m_analyze);
    // Which detector Detect & track runs, switchable without leaving Review.
    m_detector = new QPushButton(QStringLiteral("No detector"), split);
    m_detector->setObjectName(QStringLiteral("detectorPicker"));
    m_detector->setCursor(Qt::PointingHandCursor);
    m_detector->setMenu(new QMenu(m_detector));
    splitRow->addWidget(m_detector);
    row->addWidget(split, 0, Qt::AlignVCenter);
    setDetectorChoices({}, {}, false);
    column->addLayout(row);

    m_progressRow = new QWidget(header);
    auto* progressLayout = new QHBoxLayout(m_progressRow);
    progressLayout->setContentsMargins(0, 0, 0, 0);
    progressLayout->setSpacing(8);
    m_progress = new QProgressBar(m_progressRow);
    m_progress->setObjectName(QStringLiteral("analysisProgress"));
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    progressLayout->addWidget(m_progress, 1);
    m_progressText = typedLabel(QString(), "dim", m_progressRow);
    m_progressText->setMinimumWidth(240);
    progressLayout->addWidget(m_progressText);
    auto* cancel = textButton(QStringLiteral("Cancel"), m_progressRow);
    cancel->setObjectName(QStringLiteral("cancelAnalysis"));
    connect(cancel, &QPushButton::clicked, this, &ReviewPanel::analysisCancelRequested);
    progressLayout->addWidget(cancel);
    m_progressRow->hide();
    column->addWidget(m_progressRow);

    setMediaActionsEnabled(false);
    return header;
}

void ReviewPanel::setMediaTitle(const QString& title, const QString& detail)
{
    m_titleText = title;
    m_detailText = detail;
    fitHeaderText();
}

void ReviewPanel::fitHeaderText()
{
    // Shortened with an ellipsis rather than cut mid-word at the edge; the
    // whole text is the tooltip.
    const auto fit = [](QLabel* label, const QString& text, int room) {
        const QString shown = label->fontMetrics().elidedText(text, Qt::ElideMiddle, room);
        label->setText(shown);
        label->setToolTip(shown == text ? QString() : text);
    };
    // The title takes what its text needs, up to what is left beside the
    // badge; middle elision keeps both ends of names like "video_001 to
    // video_054" readable.
    const int badge = m_source->isVisible() ? m_source->sizeHint().width() + 8 : 0;
    const int room = std::max(60, m_titleRow->width() - badge);
    const int needed = m_title->fontMetrics().horizontalAdvance(m_titleText) + 2;
    m_title->setFixedWidth(std::min(needed, room));
    fit(m_title, m_titleText, std::min(needed, room));
    fit(m_detail, m_detailText, std::max(40, m_detail->width()));
}

void ReviewPanel::setAnnotationSource(const QString& text, const QColor& accent,
                                      const QString& details)
{
    m_sourceText = text;
    m_source->setToolTip(details.isEmpty() ? text : text + QStringLiteral("\n") + details);
    m_source->setVisible(!text.isEmpty() && !m_compactHeader);
    m_source->setText(m_source->fontMetrics().elidedText(text, Qt::ElideMiddle, 220));
    // The title's room depends on the badge beside it.
    QTimer::singleShot(0, this, &ReviewPanel::fitHeaderText);
    m_source->setStyleSheet(
        QStringLiteral("padding:1px 8px;border-radius:8px;font-size:11px;font-weight:600;color:%1;"
                       "background:rgba(%2,%3,%4,30);border:1px solid rgba(%2,%3,%4,90)")
            .arg(accent.name())
            .arg(accent.red())
            .arg(accent.green())
            .arg(accent.blue()));
}

void ReviewPanel::setDetectorChoices(const QVector<DetectorOption>& options,
                                     const QString& current, bool forProject)
{
    QMenu* menu = m_detector->menu();
    menu->clear();
    if (m_detectorGroup)
        m_detectorGroup->deleteLater();
    m_detectorGroup = new QActionGroup(menu);
    m_detectorCurrent = current;
    m_detectorName.clear();

    menu->addSection(forProject ? QStringLiteral("Detector for this project")
                                : QStringLiteral("Detector"));
    for (const auto& option : options) {
        QAction* action = menu->addAction(QString(option.name).replace(QLatin1Char('&'), QStringLiteral("&&")));
        action->setCheckable(true);
        action->setChecked(option.id == current);
        action->setToolTip(option.id);
        m_detectorGroup->addAction(action);
        if (option.id == current)
            m_detectorName = option.name;
        connect(action, &QAction::triggered, this, [this, id = option.id] {
            if (id != m_detectorCurrent)
                emit detectorChosen(id);
        });
    }
    if (options.isEmpty())
        menu->addAction(QStringLiteral("No usable detectors in the registry"))->setEnabled(false);
    menu->addSeparator();
    menu->addAction(options.isEmpty() ? QStringLiteral("Import models...")
                                      : QStringLiteral("Manage models..."),
                    this, &ReviewPanel::manageModelsRequested);

    if (m_detectorName.isEmpty())
        m_detectorName = options.isEmpty() ? QStringLiteral("No detector")
                                           : QStringLiteral("Choose a detector");
    m_detector->setToolTip(
        m_detectorName == QStringLiteral("No detector")
            ? QStringLiteral("Import a fish detector on the Models page to run Detect & track")
            : QStringLiteral("Detect & track runs %1%2. Click to switch.")
                  .arg(m_detectorName, forProject ? QStringLiteral(" in this project")
                                                  : QString()));
    updateDetectorButton();
}

void ReviewPanel::updateDetectorButton()
{
    // The short name ("RT-DETR-L", not "RT-DETR-L (rtdetr_l_converged)");
    // narrow headers keep only the arrow, with the name in the tooltip.
    QString name = m_detectorName;
    if (const int open = name.indexOf(QStringLiteral(" (")); open > 0)
        name.truncate(open);
    m_detector->setText(m_compactHeader
                            ? QString()
                            : m_detector->fontMetrics()
                                  .elidedText(name, Qt::ElideRight, 130)
                                  .replace(QLatin1Char('&'), QStringLiteral("&&")));
}

void ReviewPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    setCompactHeader(width() < 820);
    // After the header has been laid out at the new width.
    QTimer::singleShot(0, this, &ReviewPanel::fitHeaderText);
}

void ReviewPanel::setCompactHeader(bool compact)
{
    if (compact == m_compactHeader)
        return;
    m_compactHeader = compact;
    m_analyze->setText(compact ? QStringLiteral("Detect") : QStringLiteral("Detect && track"));
    updateDetectorButton();
    m_source->setVisible(!m_sourceText.isEmpty() && !compact);
}

void ReviewPanel::setMediaActionsEnabled(bool enabled)
{
    m_import->setEnabled(enabled && !m_analysisRunning);
    m_analyze->setEnabled(enabled && !m_analysisRunning);
    // One split button: both halves enable together.
    m_detector->setEnabled(enabled && !m_analysisRunning);
}

void ReviewPanel::setAnalysisRunning(bool running)
{
    m_analysisRunning = running;
    m_progressRow->setVisible(running);
    setMediaActionsEnabled(m_totalFrames > 0);
    if (running)
        setAnalysisProgress(0, 0, QStringLiteral("Starting the detector..."));
}

void ReviewPanel::setAnalysisProgress(int done, int total, const QString& text)
{
    if (total > 0) {
        m_progress->setRange(0, total);
        m_progress->setValue(std::clamp(done, 0, total));
    } else {
        m_progress->setRange(0, 0);  // busy indicator
    }
    m_progressText->setText(text);
}

QWidget* ReviewPanel::buildTransport()
{
    auto* bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("transportBar"));
    bar->setAttribute(Qt::WA_StyledBackground, true);
    bar->setFixedHeight(54);
    auto* row = new QHBoxLayout(bar);
    row->setContentsMargins(12, 0, 10, 0);
    row->setSpacing(6);

    // The deck: jump, step, play, step, jump, in one capsule.
    auto* deck = new QFrame(bar);
    deck->setProperty("role", "transportDeck");
    auto* controls = new QHBoxLayout(deck);
    controls->setContentsMargins(4, 3, 4, 3);
    controls->setSpacing(2);
    const auto control = [this, deck, controls](Glyph glyph, const QString& tip, auto&& action) {
        auto* button = new QPushButton(deck);
        button->setProperty("role", "transport");
        // Phosphor's carets are drawn smaller than its skip marks; size them
        // up so the four controls read as one family.
        const bool step = glyph == Glyph::StepBack || glyph == Glyph::StepForward;
        const int size = step ? 18 : 14;
        button->setIcon(glyphIcon(glyph, size, palette::TextSecondary, palette::TextPrimary));
        button->setIconSize(QSize(size, size));
        button->setToolTip(tip);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setEnabled(false);
        connect(button, &QPushButton::clicked, this, action);
        controls->addWidget(button);
        m_transport << button;
        return button;
    };
    control(Glyph::JumpStart, QStringLiteral("Go to the first frame  (Home)"),
            [this] { emit frameRequested(1); })
        ->setObjectName(QStringLiteral("transportStart"));
    control(Glyph::StepBack, QStringLiteral("Back one frame  (Left)\nShift+Left: back one second"),
            [this] { emit stepRequested(-1); })
        ->setObjectName(QStringLiteral("transportBack"));

    m_play = new QPushButton(deck);
    m_play->setObjectName(QStringLiteral("transportPlay"));
    m_play->setProperty("role", "play");
    m_play->setIconSize(QSize(16, 16));
    m_play->setToolTip(QStringLiteral("Play / pause  (Space or K)"));
    m_play->setCursor(Qt::PointingHandCursor);
    m_play->setFocusPolicy(Qt::NoFocus);
    m_play->setEnabled(false);
    connect(m_play, &QPushButton::clicked, this, &ReviewPanel::playToggled);
    controls->addSpacing(3);
    controls->addWidget(m_play);
    controls->addSpacing(3);
    setPlaying(false);

    control(Glyph::StepForward,
            QStringLiteral("Forward one frame  (Right)\nShift+Right: forward one second"),
            [this] { emit stepRequested(1); })
        ->setObjectName(QStringLiteral("transportForward"));
    control(Glyph::JumpEnd, QStringLiteral("Go to the last frame  (End)"),
            [this] { emit frameRequested(std::max(1, m_totalFrames)); })
        ->setObjectName(QStringLiteral("transportEnd"));
    row->addWidget(deck, 0, Qt::AlignVCenter);
    row->addSpacing(12);

    // Where the playhead is: timecode over the frame number, stacked so the
    // scrubber keeps the width.
    auto* readout = new QWidget(bar);
    readout->setProperty("role", "bare");
    auto* readoutColumn = new QVBoxLayout(readout);
    readoutColumn->setContentsMargins(0, 0, 0, 0);
    readoutColumn->setSpacing(0);
    m_clock = new QLabel(QStringLiteral("00:00:00:00"), readout);
    m_clock->setProperty("role", "timecode");
    m_clock->setToolTip(QStringLiteral("Hours:minutes:seconds:frames"));
    m_clock->setEnabled(false);
    readoutColumn->addWidget(m_clock);
    m_frameCounter = new QLabel(QStringLiteral("Frame –"), readout);
    m_frameCounter->setProperty("role", "frameCounter");
    readoutColumn->addWidget(m_frameCounter);
    row->addWidget(readout, 0, Qt::AlignVCenter);
    row->addSpacing(16);

    m_frameSlider = new QSlider(Qt::Horizontal, bar);
    m_frameSlider->setObjectName(QStringLiteral("frameScrubber"));
    m_frameSlider->setEnabled(false);
    m_frameSlider->setCursor(Qt::PointingHandCursor);
    connect(m_frameSlider, &QSlider::valueChanged, this, &ReviewPanel::frameRequested);
    row->addWidget(m_frameSlider, 1);
    row->addSpacing(12);

    // How long it all is, at the scrubber's end.
    m_clockTotal = new QLabel(QStringLiteral("00:00:00"), bar);
    m_clockTotal->setProperty("role", "timecodeTotal");
    m_clockTotal->setToolTip(QStringLiteral("Duration"));
    row->addWidget(m_clockTotal, 0, Qt::AlignVCenter);

    auto* divider = new QFrame(bar);
    divider->setProperty("role", "transportDivider");
    divider->setFixedSize(1, 22);
    row->addSpacing(6);
    row->addWidget(divider, 0, Qt::AlignVCenter);
    row->addSpacing(2);

    auto* snapshot = iconButton(Glyph::Camera, QStringLiteral("Save a snapshot of this frame"), bar);
    connect(snapshot, &QPushButton::clicked, this, &ReviewPanel::snapshotRequested);
    row->addWidget(snapshot);
    auto* fullscreen = iconButton(Glyph::Fullscreen, QStringLiteral("Toggle full screen"), bar);
    connect(fullscreen, &QPushButton::clicked, this, &ReviewPanel::fullscreenRequested);
    row->addWidget(fullscreen);
    return bar;
}

QWidget* ReviewPanel::buildAnalyticsRow(QWidget* parent)
{
    auto* row = new QWidget(parent);
    auto* grid = new QGridLayout(row);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(10);

    m_countChart = new LineChart(row);
    m_countChart->setEmptyText(QStringLiteral("Open a sequence"));
    m_countChart->setPeakCallout(true);
    m_countChart->setAxisCaptions(QStringLiteral("Tracks"));
    grid->addWidget(chartCard(QStringLiteral("Count over time"), m_countChart, row, &m_countTotal),
                    0, 0);

    m_speciesChart = new DonutChart(row);
    grid->addWidget(chartCard(QStringLiteral("Species breakdown"), m_speciesChart, row), 0, 1);

    m_activityChart = new LineChart(row);
    m_activityChart->setEmptyText(QStringLiteral("Open a sequence"));
    m_activityChart->setLegendVisible(true);
    grid->addWidget(chartCard(QStringLiteral("Observations vs tracks"), m_activityChart, row), 0, 2);

    m_topTracks = new BarList(row);
    grid->addWidget(chartCard(QStringLiteral("Longest tracks"), m_topTracks, row), 0, 3);

    for (int index = 0; index < 4; ++index)
        grid->setColumnStretch(index, 1);
    grid->setContentsMargins(2, 10, 2, 2);
    row->setProperty("role", "bare");
    // Fits the tab page under the minimum tab card, so no chart is clipped.
    row->setMinimumHeight(126);
    return row;
}

void ReviewPanel::configureSequence(int frames, int frameRate, qint64 durationMs)
{
    m_totalFrames = frames;
    m_frameRate = std::max(1, frameRate);
    m_durationMs = durationMs >= 0 ? durationMs : qint64(frames) * 1000 / m_frameRate;
    setMediaActionsEnabled(frames > 0);
    m_play->setEnabled(frames > 0);
    for (auto* button : m_transport)
        button->setEnabled(frames > 0);
    m_frameSlider->setEnabled(frames > 0);
    m_clock->setEnabled(frames > 0);
    if (frames <= 0) {
        showClock(0, 0, 0);
        setPlaying(false);
    }
    const QSignalBlocker blocker(m_frameSlider);
    m_frameSlider->setRange(1, std::max(1, frames));
    m_timeline->setSequenceLength(frames, m_frameRate);
    m_activity->setDuration(frames > 0 ? m_durationMs : 0);
}

void ReviewPanel::showFrame(const QImage& image, const QVector<aquatrack::GroundTruth>& annotations,
                            int frame, int total, qint64 timeMs, QSize sourceSize)
{
    m_view->setFrame(image, annotations, sourceSize);
    m_view->setFrameInfo(frame, total, timeMs);
    {
        const QSignalBlocker blocker(m_frameSlider);
        m_frameSlider->setValue(frame);
    }
    m_timeline->setCurrentFrame(frame);
    m_activity->setPlayhead(timeMs);
    m_filmstrip->setCurrentFrame(frame);
    showClock(frame, total, timeMs);
}

void ReviewPanel::showClock(int frame, int total, qint64 timeMs)
{
    if (total <= 0) {
        m_clock->setText(QStringLiteral("00:00:00:00"));
        m_clockTotal->setText(QStringLiteral("00:00:00"));
        m_frameCounter->setText(QStringLiteral("Frame –"));
        return;
    }
    const QLocale locale = QLocale::system();
    m_clock->setText(timecode(timeMs, m_frameRate));
    m_clockTotal->setText(
        clock(m_durationMs > 0 ? m_durationMs : qint64(total) * 1000 / m_frameRate));
    m_frameCounter->setText(QStringLiteral("Frame %1 / %2")
                                .arg(locale.toString(frame), locale.toString(total)));
}

void ReviewPanel::setPlaying(bool playing)
{
    // White on the accent disc; the triangle sits a hair right of centre,
    // where the eye expects it.
    m_play->setIcon(glyphIcon(playing ? Glyph::Pause : Glyph::Play, 16, Qt::white, Qt::white));
    m_play->setToolTip(playing ? QStringLiteral("Pause  (Space or K)")
                               : QStringLiteral("Play  (Space or K)"));
    m_play->setProperty("playing", playing);
    m_play->style()->unpolish(m_play);
    m_play->style()->polish(m_play);
}

void ReviewPanel::setAnalytics(const std::vector<analytics::TimeBin>& bins,
                               const analytics::SurveySummary& summary,
                               const std::vector<analytics::TrackSpan>& spans, int frameRate)
{
    m_frameRate = std::max(1, frameRate);

    // Count over time.
    ChartSeries tracks;
    tracks.name = QStringLiteral("Tracks");
    tracks.color = palette::Accent;
    tracks.filled = true;
    ChartSeries observations;
    observations.name = QStringLiteral("Observations");
    observations.color = palette::Secondary;
    for (const auto& bin : bins) {
        tracks.values.push_back(double(bin.uniqueTracks));
        observations.values.push_back(double(bin.observations));
    }
    m_countChart->setSeries({tracks});
    if (!bins.empty()) {
        m_countChart->setAxisLabels({shortClock(bins.front().startMs), shortClock(bins.back().endMs)});
        m_countTotal->setText(QStringLiteral("%1 bins").arg(bins.size()));
    }

    ChartSeries tracksCopy = tracks;
    tracksCopy.filled = false;
    m_activityChart->setSeries({observations, tracksCopy});
    if (!bins.empty())
        m_activityChart->setAxisLabels(
            {shortClock(bins.front().startMs), shortClock(bins.back().endMs)});

    // Species breakdown.
    QVector<DonutSlice> slices;
    for (const auto& [name, count] : summary.observationsByClass) {
        DonutSlice slice;
        slice.name = QString::fromStdString(name);
        slice.value = double(count);
        slices.push_back(slice);
    }
    std::sort(slices.begin(), slices.end(),
              [](const DonutSlice& left, const DonutSlice& right) { return left.value > right.value; });
    // Colour by species, not rank, so a slice matches the box colour and the
    // "Species in frame" legend on the canvas.
    for (int index = 0; index < slices.size(); ++index)
        slices[index].color = speciesColorForName(slices[index].name, index);
    m_speciesChart->setSlices(slices);
    m_speciesChart->setCenterCaption(QString::number(summary.totalObservations),
                                     QStringLiteral("total"));

    // Longest tracks.
    QVector<BarRow> rows;
    for (const auto& span : spans) {
        if (rows.size() >= 5)
            break;
        BarRow row;
        row.badge = QStringLiteral("ID %1").arg(span.trackId);
        row.color = trackColor(int(span.trackId));
        row.value = double(span.durationMs());
        row.valueText = shortClock(span.durationMs());
        rows.push_back(row);
    }
    m_topTracks->setRows(rows);
}

void ReviewPanel::setTracksTable(const std::vector<analytics::TrackSpan>& spans)
{
    const QLocale locale = QLocale::system();
    m_trackTable->setRowCount(int(spans.size()));
    for (int row = 0; row < int(spans.size()); ++row) {
        const auto& span = spans.at(std::size_t(row));
        auto* badge = textCell(QStringLiteral("ID %1").arg(span.trackId));
        badge->setForeground(trackColor(int(span.trackId)));
        m_trackTable->setItem(row, 0, badge);
        m_trackTable->setItem(row, 1, textCell(QString::fromStdString(span.className)));
        auto* first = numberCell(locale.toString(qlonglong(span.firstFrame)));
        first->setData(Qt::UserRole, int(span.firstFrame));
        m_trackTable->setItem(row, 2, first);
        m_trackTable->setItem(row, 3, numberCell(locale.toString(qlonglong(span.lastFrame))));
        m_trackTable->setItem(row, 4, numberCell(locale.toString(qulonglong(span.observations))));
        m_trackTable->setItem(row, 5, numberCell(shortClock(span.durationMs())));
    }
    // A new list starts at its top (longest track first), wherever the
    // previous one had been scrolled to.
    m_trackTable->scrollToTop();
    m_tabs->setTabText(0, QStringLiteral("Tracks (%1)").arg(locale.toString(qulonglong(spans.size()))));
}

void ReviewPanel::setEventsTable(const std::vector<analytics::SurveyEvent>& events)
{
    m_eventTable->setRowCount(int(events.size()));
    for (int row = 0; row < int(events.size()); ++row) {
        const auto& event = events.at(std::size_t(row));
        auto* time = textCell(shortClock(event.atMs));
        time->setData(Qt::UserRole, qlonglong(event.atMs));
        time->setForeground(palette::TextSecondary);
        m_eventTable->setItem(row, 0, time);
        auto* description = textCell(QString::fromStdString(event.description));
        switch (event.severity) {
        case analytics::EventSeverity::Alert:
            description->setForeground(palette::Danger);
            break;
        case analytics::EventSeverity::Warning:
            description->setForeground(palette::Warning);
            break;
        case analytics::EventSeverity::Info:
            description->setForeground(palette::TextSecondary);
            break;
        }
        m_eventTable->setItem(row, 1, description);
    }
    m_tabs->setTabText(1, QStringLiteral("Events (%1)").arg(events.size()));
}

void ReviewPanel::clearAnalytics()
{
    m_countChart->setSeries({});
    m_activityChart->setSeries({});
    m_speciesChart->setSlices({});
    m_topTracks->setRows({});
    m_trackTable->setRowCount(0);
    m_eventTable->setRowCount(0);
    m_countTotal->clear();
    m_activity->setActivity({}, {}, 0);
    m_tabs->setTabText(0, QStringLiteral("Tracks"));
    m_tabs->setTabText(1, QStringLiteral("Events"));
}
