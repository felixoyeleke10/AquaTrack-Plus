#pragma once

#include "data/BrackishMotSequence.h"

#include <QImage>
#include <QWidget>

class SequenceView : public QWidget {
public:
    explicit SequenceView(QWidget* parent = nullptr);
    void setFrame(QImage image, QVector<aquatrack::GroundTruth> annotations);
    void setFishOnly(bool fishOnly);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_image;
    QVector<aquatrack::GroundTruth> m_annotations;
    bool m_fishOnly = true;
};
