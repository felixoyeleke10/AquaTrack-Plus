#pragma once

#include "ui/common/Glyphs.h"

#include <QColor>
#include <QFrame>
#include <QString>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace aquatrack::ui {

// ---- Small factories used across the shell ---------------------------------
// These keep role/type properties in one place instead of scattering
// setStyleSheet calls through every page.

[[nodiscard]] QLabel* typedLabel(const QString& text, const char* type, QWidget* parent);
[[nodiscard]] QLabel* sectionLabel(const QString& text, QWidget* parent);
[[nodiscard]] QFrame* card(QWidget* parent, const char* role = "card");
[[nodiscard]] QFrame* divider(QWidget* parent);
[[nodiscard]] QPushButton* iconButton(Glyph glyph, const QString& tooltip, QWidget* parent,
                                      const char* role = "icon", int size = 17);
[[nodiscard]] QPushButton* textButton(const QString& text, QWidget* parent,
                                      const char* role = "");

// A rounded pill of text, e.g. "Processed" or "High Confidence".
class Badge final : public QWidget {
    Q_OBJECT

public:
    explicit Badge(QWidget* parent = nullptr);
    void setText(const QString& text);
    void setAccent(const QColor& color);

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_text;
    QColor m_accent;
};

// iOS-style switch. QCheckBox cannot be restyled into this shape through a
// stylesheet alone, so it is painted.
class ToggleSwitch final : public QWidget {
    Q_OBJECT

public:
    explicit ToggleSwitch(QWidget* parent = nullptr);

    [[nodiscard]] bool isChecked() const { return m_checked; }
    void setChecked(bool checked);

    [[nodiscard]] QSize sizeHint() const override;

signals:
    void toggled(bool checked);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool m_checked = true;
};

// A labelled row with a toggle on the right.
class ToggleRow final : public QWidget {
    Q_OBJECT

public:
    ToggleRow(const QString& text, bool checked, QWidget* parent = nullptr);
    [[nodiscard]] ToggleSwitch* toggle() const { return m_toggle; }

signals:
    void toggled(bool checked);

private:
    ToggleSwitch* m_toggle = nullptr;
};

// Large figure over a caption, used in the survey summary.
class MetricTile final : public QWidget {
    Q_OBJECT

public:
    MetricTile(const QString& caption, QWidget* parent = nullptr);
    void setValue(const QString& value);
    void setValueColor(const QColor& color);

private:
    QLabel* m_value = nullptr;
    QLabel* m_caption = nullptr;
};

// Icon + name + state dot. Used for the runtime status strip.
class StatusChip final : public QFrame {
    Q_OBJECT

public:
    StatusChip(Glyph glyph, const QString& name, QWidget* parent = nullptr);
    void setState(const QString& text, const QColor& color);

private:
    QLabel* m_state = nullptr;
    QWidget* m_dot = nullptr;
};

// Name, readout, and a proportional bar. Used for memory/storage.
class MeterCard final : public QFrame {
    Q_OBJECT

public:
    MeterCard(Glyph glyph, const QString& name, QWidget* parent = nullptr);
    void setReadout(const QString& text);
    void setRatio(double ratio, const QString& percentText);

private:
    QLabel* m_readout = nullptr;
    QLabel* m_percent = nullptr;
    class Bar;
    Bar* m_bar = nullptr;
};

// A coloured severity dot, a description, and a right-aligned timestamp.
class EventRow final : public QWidget {
    Q_OBJECT

public:
    EventRow(const QColor& severity, const QString& text, const QString& timestamp,
             QWidget* parent = nullptr);
};

}  // namespace aquatrack::ui
