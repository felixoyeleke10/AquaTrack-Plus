#pragma once

#include <QProxyStyle>

namespace aquatrack::ui {

// Base style under the application style sheet. It fixes what a sheet
// cannot reach: Qt draws a push button's icon 4 px from its text, which with
// stroked glyphs reads as the two touching. Here they sit 8 px apart.
class AppStyle final : public QProxyStyle {
public:
    explicit AppStyle(QStyle* base = nullptr);

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption* option, const QSize& size,
                           const QWidget* widget = nullptr) const override;
};

}  // namespace aquatrack::ui
