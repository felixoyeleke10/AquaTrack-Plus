#pragma once

#include "data/BrackishMotSequence.h"

#include <QFrame>

class QListWidget;

class TrackPanel final : public QFrame {
public:
    explicit TrackPanel(QWidget* parent = nullptr);
    void setTracks(const QVector<aquatrack::GroundTruth>& annotations, bool fishOnly);

private:
    QListWidget* m_trackList = nullptr;
};
