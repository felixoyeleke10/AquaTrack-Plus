#pragma once

#include <QFrame>

class QCheckBox;
class QLabel;
class QPushButton;

class SurveyPanel final : public QFrame {
public:
    explicit SurveyPanel(QWidget* parent = nullptr);
    [[nodiscard]] QPushButton* openButton() const { return m_openButton; }
    [[nodiscard]] QCheckBox* fishOnlyCheckBox() const { return m_fishOnly; }
    void setSequenceInfo(const QString& name, int frames, int frameRate, int width, int height);

private:
    QLabel* m_sequenceLabel = nullptr;
    QLabel* m_metadataLabel = nullptr;
    QPushButton* m_openButton = nullptr;
    QCheckBox* m_fishOnly = nullptr;
};
