#pragma once

#include "application/ProjectSummary.h"

#include <QColor>
#include <QListWidget>
#include <QPixmap>
#include <QStyledItemDelegate>
#include <QVector>
#include <QWidget>

// Painted building blocks of the dashboard: card grids for projects and
// media, the project hero, and two small charts. Kept apart from the page so
// the page reads as layout and data flow.
namespace aquatrack::ui::dashboard {

// Item data used by the card delegates. Qt::UserRole holds the path.
enum Role {
    PathRole = Qt::UserRole,
    PosterRole,     // QPixmap, empty until loaded
    CurrentRole,    // bool: the open project
    ExistsRole,     // bool
    MetaRole,       // QString: second line
    DetailRole,     // QString: third line (location, or progress caption)
    ProgressRole,   // double 0..1, or -1 for no bar
    BadgeRole,      // QString drawn over the poster's corner (duration)
};

// A wrapping grid of equal cards that fills its width: as many columns as
// fit at the minimum card width, each stretched to share the row. It grows
// to show every card, so the page around it scrolls instead of the grid.
class CardGrid final : public QListWidget {
public:
    // `textHeight`: space under each card's 16:9 poster.
    CardGrid(int minimumCardWidth, int textHeight, QWidget* parent = nullptr);

    // Recomputes cell size and height; call after adding or removing items.
    void relayout();
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    int m_minimumWidth;
    int m_textHeight;
};

// Projects: poster, name, "7 media \u00b7 edited 2 h ago", location.
class ProjectCardDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    static constexpr int TextHeight = 86;
};

// Media: poster with duration, name, frames, review progress bar.
class MediaCardDelegate final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

    static constexpr int TextHeight = 92;
};

// The project's title over a strip of frames from its media.
class HeroBanner final : public QWidget {
public:
    explicit HeroBanner(QWidget* parent = nullptr);
    void setImages(const QVector<QPixmap>& images);
    void setText(const QString& eyebrow, const QString& title, const QString& subtitle);

    [[nodiscard]] QSize sizeHint() const override { return {640, 232}; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<QPixmap> m_images;
    QString m_eyebrow;
    QString m_title;
    QString m_subtitle;
};

// A poster-shaped frame with an optional play mark, rounded like the cards.
class FramePreview final : public QWidget {
public:
    explicit FramePreview(QWidget* parent = nullptr);
    void setPixmap(const QPixmap& pixmap);
    void setPlayMark(bool show);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap m_pixmap;
    bool m_play = true;
};

// A thin rounded bar filled to a ratio.
class RatioBar final : public QWidget {
public:
    explicit RatioBar(QWidget* parent = nullptr, int height = 6);
    void setRatio(double ratio, const QColor& fill);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double m_ratio = 0.0;
    QColor m_fill;
};

// A rounded bar split into coloured parts by weight (e.g. footage time per
// review state), with a hairline gap between parts.
class SegmentBar final : public QWidget {
public:
    explicit SegmentBar(QWidget* parent = nullptr, int height = 8);
    void setSegments(const QVector<QPair<double, QColor>>& segments);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<QPair<double, QColor>> m_segments;
};

// Species by number of individuals: colour dot, name, bar, count.
class SpeciesChart final : public QWidget {
public:
    explicit SpeciesChart(QWidget* parent = nullptr);
    void setSpecies(const QVector<application::SpeciesCount>& species, int rows);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<application::SpeciesCount> m_species;
};

// Draws `pixmap` to fill `target`, cropping the overflow (CSS "cover").
void drawCover(QPainter& painter, const QPixmap& pixmap, const QRectF& target);

}  // namespace aquatrack::ui::dashboard
