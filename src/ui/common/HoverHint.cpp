#include "ui/common/HoverHint.h"

#include "ui/AppTheme.h"

#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>
#include <QWidget>

namespace aquatrack::ui {
namespace {

constexpr int kGap = 8;

// A translucent top-level window, so the rounded corners are real. Style
// sheets do not paint backgrounds on such windows, so the plate is drawn
// here and the style sheet only sets the text.
class HintLabel final : public QLabel {
public:
    HintLabel()
        : QLabel(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_ShowWithoutActivating);
        // A top-level label does not inherit the window's style sheet.
        setStyleSheet(QStringLiteral("QLabel { background: transparent; border: none; color: %1;"
                                     " padding: 5px 11px;"
                                     " font-family: 'Segoe UI Variable Text', 'Segoe UI';"
                                     " font-size: 12px; font-weight: 600; }")
                          .arg(palette::TextPrimary.name()));
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        {
            QPainter painter(this);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(QPen(palette::Line, 1.0));
            painter.setBrush(palette::SurfaceRaised);
            painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        }
        QLabel::paintEvent(event);
    }
};

// One label shared by every hint: only one can be under the pointer.
QLabel* hintLabel()
{
    static QPointer<QLabel> label;
    if (!label)
        label = new HintLabel;
    return label;
}

class HoverHintFilter final : public QObject {
public:
    HoverHintFilter(QWidget* target, QString text, HintPlacement placement)
        : QObject(target), m_target(target), m_text(std::move(text)), m_placement(placement)
    {
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_target) {
            switch (event->type()) {
            case QEvent::Enter:
                show();
                break;
            case QEvent::Leave:
            case QEvent::MouseButtonPress:
            case QEvent::Hide:
                hintLabel()->hide();
                break;
            default:
                break;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void show()
    {
        QLabel* label = hintLabel();
        label->setText(m_text);
        label->adjustSize();
        const QPoint topLeft = m_target->mapToGlobal(QPoint(0, 0));
        const int x = topLeft.x() + (m_target->width() - label->width()) / 2;
        const int y = m_placement == HintPlacement::Above
                          ? topLeft.y() - label->height() - kGap
                          : topLeft.y() + m_target->height() + kGap;
        label->move(x, y);
        label->show();
        label->raise();
    }

    QWidget* m_target;
    QString m_text;
    HintPlacement m_placement;
};

}  // namespace

void attachHoverHint(QWidget* target, const QString& text, HintPlacement placement)
{
    target->setAccessibleName(text);
    target->installEventFilter(new HoverHintFilter(target, text, placement));
}

}  // namespace aquatrack::ui
