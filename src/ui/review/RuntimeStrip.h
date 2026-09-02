#pragma once

#include <QFrame>

class QLabel;
class QTimer;

namespace aquatrack::ui {
class MeterCard;
class StatusChip;
}

// Bottom strip: the actual state of this build's runtime pieces. Anything
// that is not implemented says so rather than showing a green light.
class RuntimeStrip final : public QFrame {
    Q_OBJECT

public:
    explicit RuntimeStrip(QWidget* parent = nullptr);

    void setSequenceState(bool loaded, int frames);
    void setObservationStore(const QString& path);
    void setPlaybackRate(double framesPerSecond);
    void setFrameInfo(int frame, int total, int width, int height);

private:
    void refreshResources();

    aquatrack::ui::StatusChip* m_decoder = nullptr;
    aquatrack::ui::StatusChip* m_tracker = nullptr;
    aquatrack::ui::StatusChip* m_detector = nullptr;
    aquatrack::ui::StatusChip* m_store = nullptr;
    aquatrack::ui::StatusChip* m_export = nullptr;
    aquatrack::ui::MeterCard* m_memory = nullptr;
    aquatrack::ui::MeterCard* m_storage = nullptr;

    QLabel* m_summary = nullptr;
    QTimer* m_resourceTimer = nullptr;
    QString m_storePath;
};
