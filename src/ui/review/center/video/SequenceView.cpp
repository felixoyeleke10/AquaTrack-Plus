#include "ui/review/center/video/SequenceView.h"

#include <QPainter>
#include <QPaintEvent>
#include <QSizePolicy>

#include <utility>

namespace {
QColor classColor(int classId)
{
    static const QVector<QColor> colors{
        QColor(), QColor("#15a46d"), QColor("#e09a2d"), QColor("#9b6bd3"),
        QColor("#d4a72c"), QColor("#e05a47"), QColor("#b95cb6")};
    return classId >= 1 && classId < colors.size() ? colors[classId] : QColor(Qt::white);
}
}  // namespace

SequenceView::SequenceView(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(720, 405);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void SequenceView::setFrame(QImage image, QVector<aquatrack::GroundTruth> annotations)
{
    m_image = std::move(image);
    m_annotations = std::move(annotations);
    update();
}

void SequenceView::setFishOnly(bool fishOnly)
{
    m_fishOnly = fishOnly;
    update();
}

void SequenceView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#151719"));
    if (m_image.isNull()) {
        painter.setPen(QColor("#9a9a96"));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("Open a BrackishMOT sequence"));
        return;
    }
    const QSize targetSize = m_image.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect target((width() - targetSize.width()) / 2, (height() - targetSize.height()) / 2,
                       targetSize.width(), targetSize.height());
    painter.drawImage(target, m_image);
    const qreal scaleX = qreal(target.width()) / m_image.width();
    const qreal scaleY = qreal(target.height()) / m_image.height();
    painter.setRenderHint(QPainter::Antialiasing);
    for (const auto& annotation : m_annotations) {
        if (m_fishOnly && annotation.classId != 1 && annotation.classId != 5)
            continue;
        const QColor color = classColor(annotation.classId);
        const QRectF box(target.x() + annotation.box.x() * scaleX,
                         target.y() + annotation.box.y() * scaleY,
                         annotation.box.width() * scaleX,
                         annotation.box.height() * scaleY);
        painter.setPen(QPen(color, 2.0));
        painter.drawRect(box);
        const QString label = QStringLiteral("%1  #%2")
                                  .arg(aquatrack::BrackishMotSequence::className(annotation.classId))
                                  .arg(annotation.trackId);
        const QFontMetrics metrics(painter.font());
        QRectF labelRect(box.left(), qMax(qreal(target.top()), box.top() - metrics.height() - 4.0),
                         metrics.horizontalAdvance(label) + 10.0, metrics.height() + 4.0);
        painter.fillRect(labelRect, color);
        painter.setPen(Qt::white);
        painter.drawText(labelRect.adjusted(5, 0, -5, 0), Qt::AlignVCenter, label);
    }
}
