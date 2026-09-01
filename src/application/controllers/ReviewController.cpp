#include "application/controllers/ReviewController.h"

#include <QTimer>

#include <utility>

namespace aquatrack::application {

ReviewController::ReviewController(QObject* parent) : QObject(parent), m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, [this] {
        if (m_frame >= m_sequence.frameCount()) {
            m_timer->stop();
            emit playbackChanged(false);
            return;
        }
        setFrame(m_frame + 1);
    });
}

void ReviewController::loadSequence(const QString& directory)
{
    QString error;
    if (!m_sequence.load(directory, &error)) {
        emit errorOccurred(error);
        return;
    }
    m_timer->stop();
    m_timer->setInterval(qMax(1, 1000 / m_sequence.frameRate()));
    emit playbackChanged(false);
    emit sequenceLoaded(m_sequence.name(), m_sequence.frameCount(), m_sequence.frameRate(),
                        m_sequence.imageWidth(), m_sequence.imageHeight());
    setFrame(1);
}

void ReviewController::setFrame(int frame)
{
    if (!m_sequence.isLoaded() || frame < 1 || frame > m_sequence.frameCount())
        return;
    QImage image(m_sequence.imagePath(frame));
    if (image.isNull()) {
        emit errorOccurred(QStringLiteral("Could not read frame %1").arg(frame));
        return;
    }
    m_frame = frame;
    emit frameReady(std::move(image), m_sequence.annotations(frame), frame, m_sequence.frameCount());
}

void ReviewController::togglePlayback()
{
    if (!m_sequence.isLoaded())
        return;
    if (m_timer->isActive()) {
        m_timer->stop();
        emit playbackChanged(false);
        return;
    }
    if (m_frame >= m_sequence.frameCount())
        setFrame(1);
    m_timer->start();
    emit playbackChanged(true);
}

}  // namespace aquatrack::application
