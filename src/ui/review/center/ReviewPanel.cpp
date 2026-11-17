#include "ui/review/center/ReviewPanel.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"
#include "ui/review/center/LogView.h"
#include "ui/review/center/video/SequenceView.h"

#include <QActionGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QStyle>
#include <QTabBar>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

using namespace aquatrack::ui;
namespace analytics = aquatrack::analytics;

// A one-line label that shortens its text with an ellipsis as it paints,
// at whatever width the layout gives it. Its size hint is the whole text,
// so a layout can let it hug short names and squeeze long ones, all in the
// same pass (no fix-up after the resize, which made the header jump).
class ElidedLabel final : public QLabel {
public:
    ElidedLabel(const QString& text, Qt::TextElideMode mode, QWidget* parent)
        : QLabel(parent), m_mode(mode)
    {
        setFullText(text);
    }

    void setFullText(const QString& text)
    {
        m_full = text;
        setText(text);  // text() stays the whole text; painting elides it
        updateGeometry();
        updateToolTip();
        update();
    }
    [[nodiscard]] QString fullText() const { return m_full; }

    [[nodiscard]] QSize sizeHint() const override
    {
        const QMargins margins = contentsMargins();
        return {fontMetrics().horizontalAdvance(m_full) + 2 + margins.left() + margins.right(),
                fontMetrics().height() + margins.top() + margins.bottom()};
    }
    [[nodiscard]] QSize minimumSizeHint() const override
    {
        const QSize full = sizeHint();
        return {std::min(full.width(), 48), full.height()};
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setFont(font());
        painter.setPen(palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled,
                                       QPalette::WindowText));
        const QRect area = contentsRect();
        painter.drawText(area, Qt::AlignLeft | Qt::AlignVCenter,
                         fontMetrics().elidedText(m_full, m_mode, area.width()));
    }
    void resizeEvent(QResizeEvent* event) override
    {
        QLabel::resizeEvent(event);
        updateToolTip();
    }

private:
    void updateToolTip()
    {
        // The whole text on hover, only when some of it is hidden.
        const bool cut = fontMetrics().horizontalAdvance(m_full) > contentsRect().width();
        setToolTip(cut ? m_full : QString());
    }

    QString m_full;
    Qt::TextElideMode m_mode;
};

namespace {

// The detector half of the Detect & track capsule: the model's name and a
// chevron, painted over the capsule's own gradient so the two halves read
// as one control. Lights up while hovered or while its menu is open.
class ModelMenuButton final : public QPushButton {
public:
    using QPushButton::QPushButton;

    [[nodiscard]] QSize sizeHint() const override
    {
        const int name = text().isEmpty() ? 0 : fontMetrics().horizontalAdvance(text()) + 12;
        return {name + 30, QPushButton::sizeHint().height()};
    }
    [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF area = rect();
        const bool open = menu() && menu()->isVisible();
        if (isEnabled() && (underMouse() || isDown() || open)) {
            // Rounded on the capsule's outer side only.
            QPainterPath shape;
            shape.setFillRule(Qt::WindingFill);
            shape.addRoundedRect(area.adjusted(0, 0, -1, 0), 7, 7);
            shape.addRect(area.adjusted(0, 0, -area.width() / 2, 0));
            painter.fillPath(shape, QColor(255, 255, 255, isDown() || open ? 34 : 24));
        }
        const QColor ink = isEnabled() ? QColor(255, 255, 255, underMouse() || open ? 255 : 220)
                                       : QColor(0x3a, 0x42, 0x52);
        if (!text().isEmpty()) {
            painter.setPen(ink);
            painter.setFont(font());
            painter.drawText(area.adjusted(12, 0, -28, 0), Qt::AlignLeft | Qt::AlignVCenter,
                             fontMetrics().elidedText(text(), Qt::ElideRight,
                                                      int(area.width()) - 40));
        }
        // A light chevron rather than the style's solid triangle.
        const QPointF centre(area.right() - 15, area.center().y() + (open ? -1 : 1));
        QPen pen(ink, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(pen);
        const qreal flip = open ? -1 : 1;
        painter.drawPolyline(QVector<QPointF>{centre + QPointF(-4, -2 * flip),
                                              centre + QPointF(0, 2 * flip),
                                              centre + QPointF(4, -2 * flip)});
    }
};

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
    // Header (52) + the frame's minimum (130) + transport (54) + border: the card
    // never claims less than its parts need, so a short window cannot
    // squeeze the frame under the transport.
    canvasCard->setMinimumHeight(238);
    split->addWidget(canvasCard);

    auto* lower = new QWidget(split);
    auto* lowerColumn = new QVBoxLayout(lower);
    lowerColumn->setContentsMargins(0, 0, 0, 0);
    lowerColumn->setSpacing(8);

    // ---- Sequence timeline -------------------------------------------
    auto* timelineCard = card(lower);
    auto* timelineBody = new QVBoxLayout(timelineCard);
    timelineBody->setContentsMargins(14, 10, 14, 8);
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
    // The strip is built from parts rather than a QTabWidget: a tab widget
    // with a corner widget draws its own light rule across the corner,
    // which no style sheet reaches.
    auto* strip = new QWidget(tabCard);
    strip->setProperty("role", "bare");
    auto* stripRow = new QHBoxLayout(strip);
    stripRow->setContentsMargins(0, 0, 0, 0);
    stripRow->setSpacing(0);
    m_tabs = new QTabBar(strip);
    m_tabs->setObjectName(QStringLiteral("reviewLowerTabs"));
    m_tabs->setDrawBase(false);
    m_tabs->setExpanding(false);
    m_tabs->setDocumentMode(true);
    stripRow->addWidget(m_tabs, 0, Qt::AlignBottom);
    stripRow->addStretch(1);
    m_tabPages = new QStackedWidget(tabCard);
    m_tabPages->setProperty("role", "bare");

    // The log first: what the app is doing, Detect & track runs in full.
    // The track list and summary charts live in the inspector and on the
    // Analytics page, so they are not repeated here.
    m_log = new LogView(m_tabPages);
    m_tabs->addTab(QStringLiteral("Log"));
    m_tabPages->addWidget(m_log);
    stripRow->addWidget(m_log->tools(), 0, Qt::AlignVCenter);
    // A problem brings the log forward, wherever the reviewer was.
    connect(m_log, &LogView::errorLogged, this, [this] { m_tabs->setCurrentIndex(0); });

    m_eventTable = buildTable({QStringLiteral("Time"), QStringLiteral("Event")}, {}, m_tabPages);
    m_eventTable->setObjectName(QStringLiteral("eventTable"));
    m_eventTable->setToolTip(QStringLiteral("Click an event to go to it"));
    m_eventTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    connect(m_eventTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        if (auto* item = m_eventTable->item(row, 0))
            emit timestampRequested(item->data(Qt::UserRole).toLongLong());
    });
    m_tabs->addTab(QStringLiteral("Events"));
    m_tabPages->addWidget(m_eventTable);
    connect(m_tabs, &QTabBar::currentChanged, this, [this](int index) {
        m_tabPages->setCurrentIndex(index);
        // Copy and Clear belong to the log only.
        m_log->tools()->setVisible(index == 0);
    });
    tabBody->addWidget(strip);
    tabBody->addWidget(m_tabPages, 1);
    tabCard->setMinimumHeight(154);
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
    // Middle elision keeps both ends of names like "video_001 to video_054".
    m_title = new ElidedLabel(QStringLiteral("Nothing open"), Qt::ElideMiddle, m_titleRow);
    m_title->setObjectName(QStringLiteral("reviewTitle"));
    m_title->setProperty("role", "reviewTitle");
    m_title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    titleLine->addWidget(m_title, 0, Qt::AlignVCenter);
    m_source = new QLabel(m_titleRow);
    m_source->setObjectName(QStringLiteral("annotationSource"));
    m_source->hide();
    titleLine->addWidget(m_source, 0, Qt::AlignVCenter);
    titleLine->addStretch(1);
    titleColumn->addWidget(m_titleRow);
    m_detail = new ElidedLabel(QStringLiteral("Open a sequence or video from the Video library"),
                               Qt::ElideRight, titles);
    m_detail->setProperty("type", "dim");
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
    auto* split = new QFrame(header);
    split->setObjectName(QStringLiteral("detectCapsule"));
    m_split = split;
    auto* splitRow = new QHBoxLayout(split);
    splitRow->setContentsMargins(0, 0, 0, 0);
    splitRow->setSpacing(0);
    m_analyze = textButton(QStringLiteral("Detect && track"), split);
    m_analyze->setObjectName(QStringLiteral("analyzeMedia"));
    m_analyze->setIcon(glyphIcon(Glyph::Sparkle, 15, Qt::white, Qt::white));
    m_analyze->setToolTip(QStringLiteral(
        "Run the detector and tracker over every frame and load the tracks for review"));
    connect(m_analyze, &QPushButton::clicked, this, &ReviewPanel::analyzeRequested);
    splitRow->addWidget(m_analyze);
    auto* divider = new QFrame(split);
    divider->setObjectName(QStringLiteral("detectCapsuleDivider"));
    divider->setFixedWidth(1);
    splitRow->addWidget(divider);
    // Which detector Detect & track runs, switchable without leaving Review.
    m_detector = new ModelMenuButton(QStringLiteral("No detector"), split);
    m_detector->setObjectName(QStringLiteral("detectorPicker"));
    m_detector->setCursor(Qt::PointingHandCursor);
    m_detector->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto* menu = new QMenu(m_detector);
    connect(menu, &QMenu::aboutToShow, m_detector, qOverload<>(&QWidget::update));
    connect(menu, &QMenu::aboutToHide, m_detector, qOverload<>(&QWidget::update));
    m_detector->setMenu(menu);
    splitRow->addWidget(m_detector);
    splitRow->setAlignment(divider, Qt::AlignVCenter);
    divider->setFixedHeight(18);
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
    m_title->setFullText(title);
    m_detail->setFullText(detail);
}

void ReviewPanel::setAnnotationSource(const QString& text, const QColor& accent,
                                      const QString& details)
{
    m_sourceText = text;
    m_source->setToolTip(details.isEmpty() ? text : text + QStringLiteral("\n") + details);
    m_source->setVisible(!text.isEmpty() && !m_compactHeader);
    m_source->setText(m_source->fontMetrics().elidedText(text, Qt::ElideMiddle, 220));
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
    // Painted by ModelMenuButton, which elides it itself; no mnemonic to escape.
    m_detector->setText(m_compactHeader ? QString()
                                        : m_detector->fontMetrics().elidedText(
                                              name, Qt::ElideRight, 130));
    m_detector->updateGeometry();
}

void ReviewPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    setCompactHeader(width() < 820);
    // A narrow transport keeps step and play; Home and End still jump.
    const bool narrow = width() < 600;
    if (!m_transport.isEmpty()) {
        m_transport.first()->setVisible(!narrow);
        m_transport.last()->setVisible(!narrow);
    }
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
    // One capsule: both halves enable together.
    m_detector->setEnabled(enabled && !m_analysisRunning);
    m_split->setEnabled(enabled && !m_analysisRunning);
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
    m_frameCounter = new QLabel(QStringLiteral("Frame â€“"), readout);
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
        m_frameCounter->setText(QStringLiteral("Frame â€“"));
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
    m_tabs->setTabText(1, events.empty() ? QStringLiteral("Events")
                                         : QStringLiteral("Events (%1)").arg(events.size()));
}

void ReviewPanel::clearAnalytics()
{
    m_eventTable->setRowCount(0);
    m_activity->setActivity({}, {}, 0);
    m_tabs->setTabText(1, QStringLiteral("Events"));
}
