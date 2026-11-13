#include "ui/dashboard/DashboardCards.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QListView>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>

#include <algorithm>

namespace aquatrack::ui::dashboard {
namespace {

// Space around each card inside its grid cell; neighbouring cards end up
// twice this apart.
constexpr int kGap = 8;
constexpr qreal kRadius = 12;

QColor withAlpha(QColor colour, int alpha)
{
    colour.setAlpha(alpha);
    return colour;
}

QFont sized(QFont font, int pixels, QFont::Weight weight = QFont::Normal)
{
    font.setPixelSize(pixels);
    font.setWeight(weight);
    return font;
}

// The card's outline and fill, lit when hovered or selected.
void paintCardFrame(QPainter& painter, const QRectF& card, const QStyleOptionViewItem& option)
{
    const bool hovered = option.state & QStyle::State_MouseOver;
    const bool selected = option.state & QStyle::State_Selected;
    painter.setPen(QPen(selected  ? palette::Accent
                        : hovered ? withAlpha(palette::AccentHover, 150)
                                  : palette::Line,
                        1));
    painter.setBrush(hovered || selected ? palette::SurfaceRaised : palette::Surface);
    painter.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), kRadius, kRadius);
}

// The poster area at the top of a card, clipped to the card's top corners.
QRectF posterRect(const QRectF& card)
{
    return {card.x(), card.y(), card.width(), card.width() * 9.0 / 16.0};
}

void paintPoster(QPainter& painter, const QRectF& card, const QPixmap& poster, Glyph placeholder,
                 const QColor& placeholderTone)
{
    const QRectF area = posterRect(card);
    painter.save();
    QPainterPath clip;
    clip.addRoundedRect(card.adjusted(1, 1, -1, -1), kRadius - 1, kRadius - 1);
    painter.setClipPath(clip);
    if (!poster.isNull()) {
        drawCover(painter, poster, area);
    } else {
        QLinearGradient fill(area.topLeft(), area.bottomRight());
        fill.setColorAt(0, QColor(0x0f, 0x18, 0x2c));
        fill.setColorAt(1, palette::Sunken);
        painter.fillRect(area, fill);
        const qreal size = std::min<qreal>(34, area.height() / 3);
        paintGlyph(painter, placeholder,
                   QRectF(area.center().x() - size / 2, area.center().y() - size / 2, size, size),
                   placeholderTone, 1.4);
    }
    // Hairline between picture and text.
    painter.fillRect(QRectF(area.left(), area.bottom() - 1, area.width(), 1), palette::LineSoft);
    painter.restore();
}

// A small dark pill over a poster: "Open", a duration.
void paintPill(QPainter& painter, const QPointF& anchor, const QString& text, const QColor& fill,
               const QColor& textColour, const QFont& font, bool alignRight)
{
    const qreal width = QFontMetrics(font).horizontalAdvance(text) + 14;
    QRectF pill(anchor.x() - (alignRight ? width : 0), anchor.y(), width, 20);
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(pill, 10, 10);
    painter.setFont(font);
    painter.setPen(textColour);
    painter.drawText(pill, Qt::AlignCenter, text);
}

void drawLine(QPainter& painter, const QRectF& box, const QString& text, const QFont& font,
              const QColor& colour, Qt::TextElideMode elide = Qt::ElideRight)
{
    painter.setFont(font);
    painter.setPen(colour);
    painter.drawText(box, Qt::AlignLeft | Qt::AlignVCenter,
                     QFontMetrics(font).elidedText(text, elide, int(box.width())));
}

QSize gridCell(const QStyleOptionViewItem& option)
{
    if (const auto* view = qobject_cast<const QListView*>(option.widget))
        if (view->gridSize().isValid())
            return view->gridSize();
    return {280, 240};
}

}  // namespace

void drawCover(QPainter& painter, const QPixmap& pixmap, const QRectF& target)
{
    if (pixmap.isNull() || target.isEmpty())
        return;
    const QSizeF source = pixmap.size();
    const qreal scale = std::max(target.width() / source.width(), target.height() / source.height());
    const QSizeF shown(target.width() / scale, target.height() / scale);
    const QRectF crop((source.width() - shown.width()) / 2, (source.height() - shown.height()) / 2,
                      shown.width(), shown.height());
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(target, pixmap, crop);
    painter.restore();
}

// ------------------------------------------------------------- CardGrid

CardGrid::CardGrid(int minimumCardWidth, int textHeight, QWidget* parent)
    : QListWidget(parent), m_minimumWidth(minimumCardWidth), m_textHeight(textHeight)
{
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(true);
    setMovement(QListView::Static);
    setResizeMode(QListView::Adjust);
    setUniformItemSizes(true);
    setSpacing(0);
    setDragEnabled(false);
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    viewport()->setAttribute(Qt::WA_Hover);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setStyleSheet(QStringLiteral("QListWidget { background: transparent; border: none; }"));
}

void CardGrid::relayout()
{
    const int available = std::max(m_minimumWidth, viewport()->width() - 1);
    const int columns = std::max(1, available / m_minimumWidth);
    const int cellWidth = available / columns;
    const int posterHeight = int((cellWidth - 2 * kGap) * 9.0 / 16.0);
    const QSize cell(cellWidth, posterHeight + m_textHeight + 2 * kGap);
    if (gridSize() != cell)
        setGridSize(cell);
    const int rows = (count() + columns - 1) / columns;
    const int height = rows * cell.height();
    if (height != this->height())
        setFixedHeight(height);
    doItemsLayout();
}

QSize CardGrid::sizeHint() const
{
    return {m_minimumWidth, height()};
}

void CardGrid::resizeEvent(QResizeEvent* event)
{
    QListWidget::resizeEvent(event);
    if (event->size().width() != event->oldSize().width())
        relayout();
}

// --------------------------------------------------- ProjectCardDelegate

QSize ProjectCardDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const
{
    return gridCell(option);
}

void ProjectCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRectF card = QRectF(option.rect).adjusted(kGap, kGap, -kGap, -kGap);
    const bool exists = index.data(ExistsRole).toBool();
    paintCardFrame(*painter, card, option);
    paintPoster(*painter, card, index.data(PosterRole).value<QPixmap>(),
                exists ? Glyph::Files : Glyph::Alert,
                exists ? withAlpha(palette::TextDim, 200) : palette::Warning);

    const QRectF poster = posterRect(card);
    const QFont pillFont = sized(option.font, 11, QFont::DemiBold);
    if (index.data(CurrentRole).toBool())
        paintPill(*painter, poster.topLeft() + QPointF(10, 10), QStringLiteral("Open"),
                  palette::Accent, Qt::white, pillFont, false);
    if (!exists)
        paintPill(*painter, poster.topLeft() + QPointF(10, 10), QStringLiteral("Not found"),
                  withAlpha(palette::Warning, 230), QColor(0x1a, 0x12, 0x00), pillFont, false);

    const qreal left = card.left() + 14;
    const qreal width = card.width() - 28;
    qreal y = poster.bottom() + 12;
    drawLine(*painter, QRectF(left, y, width, 20), index.data(Qt::DisplayRole).toString(),
             sized(option.font, 14, QFont::DemiBold),
             exists ? palette::TextPrimary : palette::TextSecondary);
    y += 24;
    drawLine(*painter, QRectF(left, y, width, 17), index.data(MetaRole).toString(),
             sized(option.font, 12), exists ? palette::TextSecondary : palette::Warning);
    y += 20;
    drawLine(*painter, QRectF(left, y, width, 17), index.data(DetailRole).toString(),
             sized(option.font, 12), palette::TextDim, Qt::ElideMiddle);
    painter->restore();
}

// ----------------------------------------------------- MediaCardDelegate

QSize MediaCardDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const
{
    return gridCell(option);
}

void MediaCardDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRectF card = QRectF(option.rect).adjusted(kGap, kGap, -kGap, -kGap);
    const bool exists = index.data(ExistsRole).toBool();
    const bool hovered = option.state & QStyle::State_MouseOver;
    paintCardFrame(*painter, card, option);
    paintPoster(*painter, card, index.data(PosterRole).value<QPixmap>(),
                exists ? Glyph::Video : Glyph::Alert,
                exists ? withAlpha(palette::TextDim, 200) : palette::Warning);

    const QRectF poster = posterRect(card);
    const QString badge = index.data(BadgeRole).toString();
    if (!badge.isEmpty())
        paintPill(*painter, QPointF(poster.right() - 10, poster.bottom() - 30), badge,
                  QColor(5, 6, 8, 200), palette::TextPrimary, sized(option.font, 11, QFont::DemiBold),
                  true);
    if (!exists)
        paintPill(*painter, poster.topLeft() + QPointF(10, 10), QStringLiteral("Not found"),
                  withAlpha(palette::Warning, 230), QColor(0x1a, 0x12, 0x00),
                  sized(option.font, 11, QFont::DemiBold), false);
    // Hovering offers what a click does: open it in Review.
    if (hovered && exists) {
        const QRectF mark(poster.center().x() - 22, poster.center().y() - 22, 44, 44);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(5, 6, 8, 190));
        painter->drawEllipse(mark);
        paintGlyph(*painter, Glyph::Play, mark.adjusted(13, 12, -11, -12), Qt::white, 1.8);
    }

    const qreal left = card.left() + 14;
    const qreal width = card.width() - 28;
    qreal y = poster.bottom() + 11;
    drawLine(*painter, QRectF(left, y, width, 19), index.data(Qt::DisplayRole).toString(),
             sized(option.font, 13, QFont::DemiBold),
             exists ? palette::TextPrimary : palette::TextSecondary);
    y += 21;
    drawLine(*painter, QRectF(left, y, width, 16), index.data(MetaRole).toString(),
             sized(option.font, 12), palette::TextDim);
    y += 25;
    const double ratio = index.data(ProgressRole).toDouble();
    const QRectF track(left, y, width, 4);
    painter->setPen(Qt::NoPen);
    painter->setBrush(palette::LineSoft);
    painter->drawRoundedRect(track, 2, 2);
    if (ratio > 0) {
        painter->setBrush(ratio >= 0.999 ? palette::Success : palette::Accent);
        painter->drawRoundedRect(QRectF(left, y, std::max(4.0, width * ratio), 4), 2, 2);
    }
    y += 11;
    drawLine(*painter, QRectF(left, y, width, 16), index.data(DetailRole).toString(),
             sized(option.font, 12), ratio > 0 ? palette::TextSecondary : palette::TextDim);
    painter->restore();
}

// ------------------------------------------------------------ HeroBanner

HeroBanner::HeroBanner(QWidget* parent) : QWidget(parent)
{
    setProperty("role", "bare");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(232);
}

void HeroBanner::setImages(const QVector<QPixmap>& images)
{
    m_images = images;
    update();
}

void HeroBanner::setText(const QString& eyebrow, const QString& title, const QString& subtitle)
{
    m_eyebrow = eyebrow;
    m_title = title;
    m_subtitle = subtitle;
    update();
}

void HeroBanner::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath clip;
    clip.addRoundedRect(frame, 14, 14);
    painter.save();
    painter.setClipPath(clip);

    if (m_images.isEmpty()) {
        QLinearGradient fill(0, 0, width(), height());
        fill.setColorAt(0, QColor(0x10, 0x1f, 0x42));
        fill.setColorAt(0.55, QColor(0x0b, 0x12, 0x22));
        fill.setColorAt(1, palette::Surface);
        painter.fillRect(rect(), fill);
        paintGlyph(painter, Glyph::Drop, QRectF(width() - 300, -40, 320, 320),
                   withAlpha(palette::Accent, 34), 1.0);
    } else {
        // Frames side by side, a hairline apart, like a contact sheet.
        const int count = int(m_images.size());
        const qreal gap = 2;
        const qreal each = (width() - gap * (count - 1)) / count;
        for (int i = 0; i < count; ++i)
            drawCover(painter, m_images[i], QRectF(i * (each + gap), 0, each, height()));
    }

    // Darken towards the bottom-left where the title sits.
    QLinearGradient shade(0, 0, 0, height());
    shade.setColorAt(0, withAlpha(palette::Canvas, m_images.isEmpty() ? 0 : 60));
    shade.setColorAt(0.5, withAlpha(palette::Canvas, m_images.isEmpty() ? 40 : 140));
    shade.setColorAt(1, withAlpha(palette::Canvas, 235));
    painter.fillRect(rect(), shade);
    QLinearGradient side(0, 0, width() * 0.7, 0);
    side.setColorAt(0, withAlpha(palette::Canvas, 150));
    side.setColorAt(1, withAlpha(palette::Canvas, 0));
    painter.fillRect(rect(), side);

    const qreal left = 28;
    const qreal textWidth = width() - 2 * left;
    const QRectF subtitle(left, height() - 26 - 20, textWidth, 20);
    const QRectF title(left, subtitle.top() - 4 - 40, textWidth, 40);
    const QRectF eyebrow(left, title.top() - 2 - 16, textWidth, 16);
    QFont eyebrowFont = sized(font(), 11, QFont::Bold);
    eyebrowFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    drawLine(painter, eyebrow, m_eyebrow.toUpper(), eyebrowFont, palette::AccentHover);
    drawLine(painter, title, m_title, sized(font(), 32, QFont::Bold), Qt::white);
    drawLine(painter, subtitle, m_subtitle, sized(font(), 13), QColor(0xc9, 0xd2, 0xe0));
    painter.restore();

    painter.setPen(QPen(palette::Line, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(frame, 14, 14);
}

// ---------------------------------------------------------- FramePreview

FramePreview::FramePreview(QWidget* parent) : QWidget(parent)
{
    setProperty("role", "bare");
}

void FramePreview::setPixmap(const QPixmap& pixmap)
{
    m_pixmap = pixmap;
    update();
}

void FramePreview::setPlayMark(bool show)
{
    m_play = show;
    update();
}

void FramePreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF frame = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath clip;
    clip.addRoundedRect(frame, 10, 10);
    painter.save();
    painter.setClipPath(clip);
    if (!m_pixmap.isNull()) {
        drawCover(painter, m_pixmap, rect());
    } else {
        QLinearGradient fill(0, 0, width(), height());
        fill.setColorAt(0, QColor(0x0f, 0x18, 0x2c));
        fill.setColorAt(1, palette::Sunken);
        painter.fillRect(rect(), fill);
    }
    painter.restore();
    if (m_play) {
        const QRectF mark(width() / 2.0 - 24, height() / 2.0 - 24, 48, 48);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(5, 6, 8, 180));
        painter.drawEllipse(mark);
        paintGlyph(painter, Glyph::Play, mark.adjusted(15, 13, -12, -13), Qt::white, 1.8);
    }
    painter.setPen(QPen(palette::LineSoft, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(frame, 10, 10);
}

// -------------------------------------------------------------- RatioBar

RatioBar::RatioBar(QWidget* parent, int height) : QWidget(parent), m_fill(palette::Accent)
{
    setProperty("role", "bare");
    setFixedHeight(height);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void RatioBar::setRatio(double ratio, const QColor& fill)
{
    m_ratio = std::clamp(ratio, 0.0, 1.0);
    m_fill = fill;
    update();
}

void RatioBar::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    const qreal radius = height() / 2.0;
    painter.setBrush(palette::LineSoft);
    painter.drawRoundedRect(rect(), radius, radius);
    if (m_ratio <= 0)
        return;
    painter.setBrush(m_fill);
    painter.drawRoundedRect(QRectF(0, 0, std::max<qreal>(height(), width() * m_ratio), height()),
                            radius, radius);
}

// ---------------------------------------------------------- SpeciesChart

namespace {
constexpr int kSpeciesRow = 26;
}

SpeciesChart::SpeciesChart(QWidget* parent) : QWidget(parent)
{
    // Without Q_OBJECT the style sheet sees a plain QWidget and would paint
    // the canvas colour behind the rows; the same holds for the widgets above.
    setProperty("role", "bare");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void SpeciesChart::setSpecies(const QVector<application::SpeciesCount>& species, int rows)
{
    m_species = species.mid(0, rows);
    updateGeometry();
    update();
}

QSize SpeciesChart::sizeHint() const
{
    return {240, int(m_species.size()) * kSpeciesRow};
}

void SpeciesChart::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    if (m_species.isEmpty())
        return;
    const int most = std::max(1, m_species.first().tracks);
    const QFont nameFont = sized(font(), 12);
    const QFont countFont = sized(font(), 12, QFont::DemiBold);
    const qreal nameWidth = std::min<qreal>(120, width() * 0.36);
    const qreal countWidth = 44;
    const qreal barLeft = 16 + nameWidth + 10;
    const qreal barWidth = std::max<qreal>(20, width() - barLeft - countWidth - 8);

    for (int i = 0; i < m_species.size(); ++i) {
        const auto& item = m_species[i];
        const QColor colour = speciesColorForName(item.name, i);
        const qreal y = i * kSpeciesRow;
        const qreal middle = y + kSpeciesRow / 2.0;
        painter.setPen(Qt::NoPen);
        painter.setBrush(colour);
        painter.drawEllipse(QPointF(4, middle), 4, 4);
        drawLine(painter, QRectF(16, y, nameWidth, kSpeciesRow), item.name, nameFont,
                 palette::TextSecondary);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette::LineSoft);
        painter.drawRoundedRect(QRectF(barLeft, middle - 3, barWidth, 6), 3, 3);
        painter.setBrush(withAlpha(colour, 210));
        painter.drawRoundedRect(
            QRectF(barLeft, middle - 3, std::max<qreal>(6, barWidth * item.tracks / most), 6), 3, 3);
        painter.setFont(countFont);
        painter.setPen(palette::TextPrimary);
        painter.drawText(QRectF(width() - countWidth, y, countWidth, kSpeciesRow),
                         Qt::AlignRight | Qt::AlignVCenter, QLocale::system().toString(item.tracks));
    }
}

}  // namespace aquatrack::ui::dashboard
