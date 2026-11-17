#pragma once

#include <QWidget>

class QLabel;

namespace aquatrack::ui {

// A folding section's header, as in VS Code's side bar: a chevron (down
// when open, right when folded), the section's title, and an optional
// quiet detail after it. Clicking anywhere on it folds or unfolds.
class SectionHeader final : public QWidget {
    Q_OBJECT

public:
    // An empty `title` gives a chevron-only toggle (for a tab strip).
    explicit SectionHeader(const QString& title, QWidget* parent = nullptr);

    [[nodiscard]] bool isExpanded() const { return m_expanded; }
    // Changes the state without emitting toggled().
    void setExpanded(bool expanded);
    // The dim text after the title (a subtitle, or a summary when folded).
    [[nodiscard]] QLabel* detail() const { return m_detail; }

signals:
    // The reviewer folded or unfolded the section.
    void toggled(bool expanded);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void toggle();
    void updateTip();

    QLabel* m_title = nullptr;
    QLabel* m_detail = nullptr;
    bool m_expanded = true;
    bool m_hovered = false;
    bool m_chevronOnly = false;
};

}  // namespace aquatrack::ui
