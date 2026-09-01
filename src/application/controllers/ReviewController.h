#pragma once

#include "data/BrackishMotSequence.h"

#include <QImage>
#include <QObject>

class QTimer;

namespace aquatrack::application {

class ReviewController final : public QObject {
    Q_OBJECT

public:
    explicit ReviewController(QObject* parent = nullptr);
    void loadSequence(const QString& directory);
    void setFrame(int frame);
    void togglePlayback();

signals:
    void sequenceLoaded(QString name, int frames, int frameRate, int width, int height);
    void frameReady(QImage image, QVector<aquatrack::GroundTruth> annotations, int frame, int total);
    void playbackChanged(bool playing);
    void errorOccurred(QString message);

private:
    BrackishMotSequence m_sequence;
    QTimer* m_timer = nullptr;
    int m_frame = 0;
};

}  // namespace aquatrack::application
