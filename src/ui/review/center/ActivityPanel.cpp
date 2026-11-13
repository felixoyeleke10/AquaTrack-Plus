#include "ui/review/center/ActivityPanel.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <cmath>
#include <numeric>

using namespace aquatrack::ui;

namespace {

// Quiet to busy: deep violet, blue, green, yellow, orange, red.
QColor heat(double t)
{
    static const QVector<QPair<double, QColor>> stops{
        {0.00, QColor(0x2e, 0x10, 0x65)}, {0.18, QColor(0x5b, 0x21, 0xb6)},
        {0.36, QColor(0x25, 0x63, 0xeb)}, {0.54, QColor(0x10, 0xb9, 0x81)},
        {0.72, QColor(0xea, 0xb3, 0x08)}, {0.86, QColor(0xf9, 0x73, 0x16)},
        {1.00, QColor(0xef, 0x44, 0x44)},
    };
    t = std::clamp(t, 0.0, 1.0);
    for (int i = 1; i < stops.size(); ++i) {
        if (t <= stops[i].first) {
            const double span = stops[i].first - stops[i - 1].first;
            const double f = span > 0 ? (t - stops[i - 1].first) / span : 0.0;
            const QColor& a = stops[i - 1].second;
            const QColor& b = stops[i].second;
            return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * f,
                                    a.greenF() + (b.greenF() - a.greenF()) * f,
                                    a.blueF() + (b.blueF() - a.blueF()) * f);
        }
    }
    return stops.last().second;
}

QString clock(qint64 milliseconds)
{
    const qint64 seconds = std::max<qint64>(0, milliseconds) / 1000;
    const qint64 hours = seconds / 3600;
    return hours > 0 ? QStringLiteral("%1:%2:%3")
                           .arg(hours)
                           .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
                           .arg(seconds % 60, 2, 10, QLatin1Char('0'))
                     : QStringLiteral("%1:%2")
                           .arg(seconds / 60, 2, 10, QLatin1Char('0'))
                           .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

// A round number near `value`, for legend bucket edges.
int nice(double value)
{
    if (value <= 10)
        return std::max(1, int(std::round(value)));
    const double magnitude = std::pow(10.0, std::floor(std::log10(value)) - 1);
    return int(std::round(value / magnitude) * magnitude);
}

// Bucket edges for the legend, as fractions of the peak: the colour bands
// of the strip, named in the strip's own unit.
QVector<int> bucketEdges(int peak)
{
    QVector<int> edges;
    for (double fraction : {0.1, 0.3, 0.6, 0.85}) {
        const int edge = nice(peak * fraction);
        if (edges.isEmpty() || edge > edges.last())
            edges << edge;
    }
    return edges;
}

}  // namespace

// ------------------------------------------------------------ The strip

class ActivityStrip final : public QWidget {
public:
    explicit ActivityStrip(QWidget* parent) : QWidget(parent)
    {
        setProperty("role", "bare");
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(86);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setSeries(const QVector<int>& values, const QString& unit)
    {
        m_values = values;
        m_unit = unit;
        m_peak = values.isEmpty() ? 0 : *std::max_element(values.cbegin(), values.cend());
        // Light smoothing so the colour flows like a heat map rather than
        // stepping second by second; figures elsewhere use the raw values.
        m_smoothed.resize(values.size());
        for (int i = 0; i < values.size(); ++i) {
            double sum = 0;
            int count = 0;
            for (int j = std::max(0, i - 1); j <= std::min<int>(int(values.size()) - 1, i + 1); ++j) {
                sum += values[j] * (j == i ? 2 : 1);
                count += j == i ? 2 : 1;
            }
            m_smoothed[i] = sum / count;
        }
        update();
    }

    void setDuration(qint64 milliseconds)
    {
        m_durationMs = std::max<qint64>(0, milliseconds);
        update();
    }

    void setPlayhead(qint64 milliseconds)
    {
        if (milliseconds == m_playheadMs)
            return;
        m_playheadMs = milliseconds;
        update();
    }

    void setRequestHandler(std::function<void(qint64)> handler) { m_request = std::move(handler); }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF strip = bar();
        QPainterPath shape;
        shape.addRoundedRect(strip, 8, 8);

        if (m_values.isEmpty() || m_peak <= 0 || m_durationMs <= 0) {
            painter.fillPath(shape, palette::Sunken);
            painter.setPen(QPen(palette::LineSoft, 1));
            painter.drawPath(shape);
            painter.setPen(palette::TextDim);
            QFont font = this->font();
            font.setPixelSize(12);
            painter.setFont(font);
            painter.drawText(strip, Qt::AlignCenter,
                             m_durationMs <= 0 ? QStringLiteral("Open a sequence to see its activity")
                                               : QStringLiteral("Activity appears once there are annotations"));
        } else {
            QLinearGradient fill(strip.left(), 0, strip.right(), 0);
            const int count = int(m_smoothed.size());
            for (int i = 0; i < count; ++i)
                fill.setColorAt((i + 0.5) / count, heat(m_smoothed[i] / m_peak));
            fill.setColorAt(0, heat(m_smoothed.first() / m_peak));
            fill.setColorAt(1, heat(m_smoothed.last() / m_peak));
            painter.fillPath(shape, fill);
            painter.setPen(QPen(QColor(255, 255, 255, 22), 1));
            painter.drawPath(shape);
        }

        // Time axis.
        QFont axis = font();
        axis.setPixelSize(11);
        painter.setFont(axis);
        painter.setPen(palette::TextDim);
        constexpr int kTicks = 6;
        for (int i = 0; i <= kTicks; ++i) {
            const double x = strip.left() + strip.width() * i / kTicks;
            const Qt::Alignment align = i == 0 ? Qt::AlignLeft : i == kTicks ? Qt::AlignRight : Qt::AlignHCenter;
            const QRectF box = i == 0         ? QRectF(x, strip.bottom() + 4, 60, 16)
                               : i == kTicks ? QRectF(x - 60, strip.bottom() + 4, 60, 16)
                                             : QRectF(x - 30, strip.bottom() + 4, 60, 16);
            painter.drawText(box, align | Qt::AlignVCenter, clock(m_durationMs * i / kTicks));
        }

        // Playhead: a line through the strip, a knob, and the time above.
        if (m_durationMs > 0) {
            const double x = strip.left() +
                             strip.width() * std::clamp(double(m_playheadMs) / m_durationMs, 0.0, 1.0);
            painter.setPen(QPen(QColor(255, 255, 255, 230), 2));
            painter.drawLine(QPointF(x, strip.top() - 2), QPointF(x, strip.bottom() + 2));
            painter.setPen(QPen(palette::Accent, 2));
            painter.setBrush(Qt::white);
            painter.drawEllipse(QPointF(x, strip.top()), 5.5, 5.5);

            QFont bubbleFont = font();
            bubbleFont.setPixelSize(11);
            bubbleFont.setBold(true);
            const QString text = clock(m_playheadMs);
            const qreal width = QFontMetrics(bubbleFont).horizontalAdvance(text) + 16;
            QRectF bubble(x - width / 2, 0, width, 20);
            bubble.moveLeft(std::clamp(bubble.left(), 0.0, double(this->width()) - width));
            painter.setPen(Qt::NoPen);
            painter.setBrush(palette::Accent);
            painter.drawRoundedRect(bubble, 6, 6);
            painter.setFont(bubbleFont);
            painter.setPen(Qt::white);
            painter.drawText(bubble, Qt::AlignCenter, text);
        }
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton)
            seek(event->position().x());
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const double x = event->position().x();
        if (event->buttons() & Qt::LeftButton) {
            seek(x);
            return;
        }
        if (m_durationMs <= 0 || !bar().contains(event->position())) {
            QToolTip::hideText();
            return;
        }
        const qint64 at = timeAt(x);
        const int second = int(at / 1000);
        const int value = second >= 0 && second < m_values.size() ? m_values[second] : 0;
        QToolTip::showText(event->globalPosition().toPoint(),
                           QStringLiteral("%1  ·  %2 %3").arg(clock(at)).arg(value).arg(m_unit), this);
    }

private:
    QRectF bar() const { return QRectF(8, 28, width() - 16, 38); }

    qint64 timeAt(double x) const
    {
        const QRectF strip = bar();
        return qint64(std::clamp((x - strip.left()) / strip.width(), 0.0, 1.0) * m_durationMs);
    }

    void seek(double x)
    {
        if (m_durationMs > 0 && m_request)
            m_request(timeAt(x));
    }

    QVector<int> m_values;
    QVector<double> m_smoothed;
    QString m_unit;
    int m_peak = 0;
    qint64 m_durationMs = 0;
    qint64 m_playheadMs = 0;
    std::function<void(qint64)> m_request;
};

// --------------------------------------------------------- Gradient key

namespace {

class GradientKey final : public QWidget {
public:
    explicit GradientKey(QWidget* parent) : QWidget(parent)
    {
        setProperty("role", "bare");
        setFixedSize(150, 8);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QLinearGradient fill(0, 0, width(), 0);
        for (int i = 0; i <= 20; ++i)
            fill.setColorAt(i / 20.0, heat(i / 20.0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(rect(), 4, 4);
    }
};

// Icon, caption and figure in a small framed tile.
QFrame* statTile(Glyph glyph, const QString& caption, QLabel*& value, QWidget* parent,
                 QList<QLabel*>& captions)
{
    auto* tile = card(parent, "cardFlat");
    auto* row = new QHBoxLayout(tile);
    row->setContentsMargins(10, 6, 12, 6);
    row->setSpacing(9);
    auto* icon = new QLabel(tile);
    icon->setPixmap(glyphPixmap(glyph, 18, palette::AccentHover, GlyphStyle::Duotone));
    row->addWidget(icon);
    auto* column = new QVBoxLayout;
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);
    auto* captionLabel = typedLabel(caption, "dim", tile);
    captions << captionLabel;
    column->addWidget(captionLabel);
    tile->setToolTip(caption);
    value = typedLabel(QStringLiteral("—"), "body", tile);
    value->setStyleSheet(QStringLiteral("font-weight:700;font-size:14px"));
    column->addWidget(value);
    row->addLayout(column);
    return tile;
}

}  // namespace

// ---------------------------------------------------------------- Panel

ActivityPanel::ActivityPanel(QWidget* parent) : QWidget(parent)
{
    setProperty("role", "bare");
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(8);

    // ---- Heading, figures and mode.
    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(10);
    auto* mark = new QLabel(this);
    mark->setPixmap(glyphPixmap(Glyph::Events, 22, palette::AccentHover, GlyphStyle::Duotone));
    head->addWidget(mark, 0, Qt::AlignTop);
    auto* titles = new QVBoxLayout;
    titles->setContentsMargins(0, 0, 0, 0);
    titles->setSpacing(1);
    titles->addWidget(sectionLabel(QStringLiteral("Fish activity"), this));
    m_subtitle = typedLabel(QString(), "dim", this);
    titles->addWidget(m_subtitle);
    head->addLayout(titles);
    head->addStretch();
    head->addWidget(statTile(Glyph::Tracks, QStringLiteral("Total tracks"), m_totalValue, this, m_captions));
    head->addWidget(statTile(Glyph::Analytics, QStringLiteral("Peak activity"), m_peakValue, this, m_captions));
    head->addWidget(statTile(Glyph::Events, QStringLiteral("Avg. activity"), m_averageValue, this, m_captions));
    m_mode = new QComboBox(this);
    m_mode->setObjectName(QStringLiteral("activityMode"));
    m_mode->addItem(QStringLiteral("Detection density"));
    m_mode->addItem(QStringLiteral("Detections per second"));
    m_mode->addItem(QStringLiteral("Track lanes"));
    m_mode->setToolTip(QStringLiteral("What the timeline shows"));
    connect(m_mode, &QComboBox::currentIndexChanged, this, &ActivityPanel::applyMode);
    head->addWidget(m_mode, 0, Qt::AlignVCenter);
    column->addLayout(head);

    // ---- The strip, or the lanes.
    m_views = new QStackedWidget(this);
    m_views->setProperty("role", "bare");
    m_strip = new ActivityStrip(m_views);
    m_strip->setRequestHandler([this](qint64 at) { emit timestampRequested(at); });
    m_views->addWidget(m_strip);
    m_lanes = new TrackLanes(m_views);
    m_views->addWidget(m_lanes);
    column->addWidget(m_views);

    // ---- Legend.
    m_legend = new QWidget(this);
    m_legend->setProperty("role", "bare");
    auto* legend = new QHBoxLayout(m_legend);
    legend->setContentsMargins(8, 0, 8, 0);
    legend->setSpacing(8);
    m_lowLabel = typedLabel(QStringLiteral("Low activity"), "dim", m_legend);
    legend->addWidget(m_lowLabel);
    legend->addWidget(new GradientKey(m_legend), 0, Qt::AlignVCenter);
    m_highLabel = typedLabel(QStringLiteral("High activity"), "dim", m_legend);
    legend->addWidget(m_highLabel);
    legend->addStretch();
    m_buckets = typedLabel(QString(), "dim", m_legend);
    m_buckets->setTextFormat(Qt::RichText);
    legend->addWidget(m_buckets);
    column->addWidget(m_legend);

    applyMode();
}

void ActivityPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    setCompact(width() < 820);
}

void ActivityPanel::setCompact(bool compact)
{
    if (compact == m_compact)
        return;
    m_compact = compact;
    m_subtitle->setVisible(!compact);
    for (auto* caption : m_captions)
        caption->setVisible(!compact);
    m_buckets->setVisible(!compact);
    static_cast<QLabel*>(m_lowLabel)->setText(compact ? QStringLiteral("Low") : QStringLiteral("Low activity"));
    static_cast<QLabel*>(m_highLabel)->setText(compact ? QStringLiteral("High") : QStringLiteral("High activity"));
}

void ActivityPanel::setActivity(const QVector<int>& tracksPerSecond,
                                const QVector<int>& detectionsPerSecond, int totalTracks)
{
    m_tracks = tracksPerSecond;
    m_detections = detectionsPerSecond;
    m_totalTracks = totalTracks;
    applyMode();
}

void ActivityPanel::setDuration(qint64 milliseconds)
{
    m_strip->setDuration(milliseconds);
}

void ActivityPanel::setPlayhead(qint64 milliseconds)
{
    m_strip->setPlayhead(milliseconds);
}

void ActivityPanel::clear()
{
    setActivity({}, {}, 0);
    m_strip->setDuration(0);
    m_strip->setPlayhead(0);
    m_lanes->clear();
}

void ActivityPanel::applyMode()
{
    const int mode = m_mode->currentIndex();
    const bool lanes = mode == 2;
    const bool detections = mode == 1;
    const QVector<int>& series = detections ? m_detections : m_tracks;
    const QString unit = detections ? QStringLiteral("detections/s") : QStringLiteral("tracks/s");
    m_subtitle->setText(lanes        ? QStringLiteral("One row per track; gaps are where it was lost")
                        : detections ? QStringLiteral("Detections per second")
                                     : QStringLiteral("Detection density (tracks per second)"));
    m_views->setCurrentIndex(lanes ? 1 : 0);
    m_legend->setVisible(!lanes);
    m_strip->setSeries(series, unit);

    const QLocale locale = QLocale::system();
    const int peak = series.isEmpty() ? 0 : *std::max_element(series.cbegin(), series.cend());
    const double average =
        series.isEmpty() ? 0.0 : std::accumulate(series.cbegin(), series.cend(), 0.0) / series.size();
    const bool any = peak > 0;
    m_totalValue->setText(m_totalTracks > 0 ? locale.toString(m_totalTracks) : QStringLiteral("—"));
    m_peakValue->setText(any ? locale.toString(peak) : QStringLiteral("—"));
    m_averageValue->setText(any ? locale.toString(int(std::round(average))) : QStringLiteral("—"));
    m_peakValue->setToolTip(QStringLiteral("Most %1 in any one second").arg(unit));
    m_averageValue->setToolTip(QStringLiteral("Average %1 across the sequence").arg(unit));

    // Legend buckets: the strip's colour bands, in its unit.
    QString text;
    if (any) {
        const QVector<int> edges = bucketEdges(peak);
        QStringList parts;
        int low = 0;
        for (int i = 0; i <= edges.size(); ++i) {
            const double middle = i < edges.size() ? (low + edges[i]) / 2.0 / peak
                                                   : (low + peak) / 2.0 / peak;
            const int from = i == 0 ? 0 : low + 1;
            const int to = i < edges.size() ? edges[i] : peak;
            // A band one value wide reads as that value ("2", not "2-2").
            const QString range = i == edges.size() ? QStringLiteral("&gt; %1").arg(low)
                                  : from == to      ? QString::number(from)
                                                    : QStringLiteral("%1–%2").arg(from).arg(to);
            parts << QStringLiteral("<span style='color:%1'>&#9632;</span>&nbsp;%2")
                         .arg(heat(middle).name(), range);
            if (i < edges.size())
                low = edges[i];
        }
        text = QStringLiteral("%1:&nbsp;&nbsp;").arg(detections ? QStringLiteral("Detections per second")
                                                                : QStringLiteral("Tracks per second")) +
               parts.join(QStringLiteral("&nbsp;&nbsp;&nbsp;"));
    }
    m_buckets->setText(text);
}
