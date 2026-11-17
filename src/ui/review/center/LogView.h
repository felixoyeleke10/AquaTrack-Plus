#pragma once

#include <QWidget>

class QPlainTextEdit;

// The Review page's log: what the app is doing, as it happens. Projects
// and media opening, saves, settings and detector changes, and every
// Detect & track run in full (the exact command, what Python prints, how
// it ended), timestamped and coloured by kind.
class LogView final : public QWidget {
    Q_OBJECT

public:
    // Mirrors aquatrack::application::OutputKind.
    enum class Kind { Command, Info, Output, Warning, Error, Success };

    explicit LogView(QWidget* parent = nullptr);

    void append(const QString& text, Kind kind);
    void clear();
    [[nodiscard]] QString text() const;
    [[nodiscard]] int errorCount() const { return m_errors; }

    // Copy and Clear, for the tab bar's corner.
    [[nodiscard]] QWidget* tools() const { return m_tools; }

signals:
    // An error was logged (the panel uses it to bring the log forward).
    void errorLogged();

private:
    QPlainTextEdit* m_text = nullptr;
    QWidget* m_tools = nullptr;
    QString m_last;  // the previous line, so a repeated status is logged once
    int m_errors = 0;
};
