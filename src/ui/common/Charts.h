#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

// Small hand-painted chart set. Qt Charts is not part of the Qt build this
// project targets, and these views only ever need a handful of shapes, so
// they are drawn directly with QPainter rather than pulling in a dependency.
namespace aquatrack::ui {

struct ChartSeries {
    QString name;
    QColor color;
    QVector<double> values;
    bool filled = false;  // shade the area under the line
};

// Line/area chart with faint gridlines, optional dual axis, and an optional
// callout on the highest point of the first series.
class LineChart final : public QWidget {
    Q_OBJECT

public:
    explicit LineChart(QWidget* parent = nullptr);

    void setSeries(QVector<ChartSeries> series);
    void setAxisLabels(QStringList xLabels);
    void setAxisCaptions(QString left, QString bottom = {}, QString right = {});
    void setPeakCallout(bool enabled);
    void setLegendVisible(bool visible);
    void setValueBadge(const QString& text);  // e.g. the latest reading
    void setEmptyText(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<ChartSeries> m_series;
    QStringList m_xLabels;
    QString m_leftCaption;
    QString m_bottomCaption;
    QString m_rightCaption;
    QString m_valueBadge;
    QString m_emptyText = QStringLiteral("No data yet");
    bool m_peakCallout = false;
    bool m_legend = false;
};

struct DonutSlice {
    QString name;
    double value = 0.0;
    QColor color;
};

// Ring chart with the legend painted alongside it, so the colour swatches
// line up exactly with the ring segments.
class DonutChart final : public QWidget {
    Q_OBJECT

public:
    explicit DonutChart(QWidget* parent = nullptr);

    void setSlices(QVector<DonutSlice> slices);
    void setCenterCaption(QString value, QString caption);
    void setEmptyText(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<DonutSlice> m_slices;
    QString m_centerValue;
    QString m_centerCaption;
    QString m_emptyText = QStringLiteral("No observations");
};

struct BarRow {
    QString badge;      // short leading chip, e.g. "ID 15"
    QColor color;
    double value = 0.0;
    QString valueText;  // right-aligned readout, e.g. "00:12:48"
};

// Ranked horizontal bars with a coloured id chip, used for "top tracks".
class BarList final : public QWidget {
    Q_OBJECT

public:
    explicit BarList(QWidget* parent = nullptr);

    void setRows(QVector<BarRow> rows);
    void setEmptyText(const QString& text);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<BarRow> m_rows;
    QString m_emptyText = QStringLiteral("No tracks");
};

// Tiny inline trend line with no axes, for use inside dense cards.
class Sparkline final : public QWidget {
    Q_OBJECT

public:
    explicit Sparkline(QWidget* parent = nullptr);

    void setValues(QVector<double> values);
    void setColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<double> m_values;
    QColor m_color;
};

}  // namespace aquatrack::ui
