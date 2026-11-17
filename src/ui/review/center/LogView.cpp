#include "ui/review/center/LogView.h"

#include "ui/AppTheme.h"
#include "ui/common/Widgets.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTime>
#include <QVBoxLayout>

using namespace aquatrack::ui;

namespace {

QColor colourOf(LogView::Kind kind)
{
    switch (kind) {
    case LogView::Kind::Command:
        return palette::AccentHover;
    case LogView::Kind::Info:
        return palette::TextPrimary;
    case LogView::Kind::Output:
        return palette::TextSecondary;
    case LogView::Kind::Warning:
        return palette::Warning;
    case LogView::Kind::Error:
        return palette::Danger;
    case LogView::Kind::Success:
        return palette::Success;
    }
    return palette::TextSecondary;
}

}  // namespace

LogView::LogView(QWidget* parent) : QWidget(parent)
{
    setProperty("role", "bare");
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 4, 0, 0);
    column->setSpacing(0);

    m_text = new QPlainTextEdit(this);
    m_text->setObjectName(QStringLiteral("activityLog"));
    m_text->setReadOnly(true);
    m_text->setUndoRedoEnabled(false);
    m_text->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    // Long sessions log a lot; the oldest lines go first.
    m_text->setMaximumBlockCount(5000);
    m_text->setPlaceholderText(QStringLiteral(
        "What AquaTrack+ is doing shows here as it happens: projects and media opening, "
        "saves, detector changes, and every Detect & track run with its exact command and "
        "output."));
    m_text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    column->addWidget(m_text);

    // Plain words, not icons: two icons in a corner read as window controls.
    m_tools = new QWidget(this);
    m_tools->setProperty("role", "bare");
    auto* row = new QHBoxLayout(m_tools);
    row->setContentsMargins(0, 0, 2, 0);
    row->setSpacing(2);
    auto* copy = textButton(QStringLiteral("Copy"), m_tools, "ghost");
    copy->setObjectName(QStringLiteral("logCopy"));
    copy->setToolTip(QStringLiteral("Copy the whole log"));
    connect(copy, &QPushButton::clicked, this,
            [this] { QApplication::clipboard()->setText(m_text->toPlainText()); });
    row->addWidget(copy);
    auto* wipe = textButton(QStringLiteral("Clear"), m_tools, "ghost");
    wipe->setObjectName(QStringLiteral("logClear"));
    wipe->setToolTip(QStringLiteral("Empty the log"));
    connect(wipe, &QPushButton::clicked, this, &LogView::clear);
    row->addWidget(wipe);
}

void LogView::append(const QString& text, Kind kind)
{
    // Status messages can repeat (a save, then the same save again), and a
    // result is often logged and then shown as status; one line is enough.
    if (text == m_last)
        return;
    m_last = text;

    // Follow new lines only if the reader is already at the end; someone
    // scrolled up to read an error keeps their place.
    QScrollBar* bar = m_text->verticalScrollBar();
    const bool following = bar->value() >= bar->maximum() - 4;

    QTextCursor cursor(m_text->document());
    cursor.movePosition(QTextCursor::End);
    if (!m_text->document()->isEmpty())
        cursor.insertBlock();
    QTextCharFormat stamp;
    stamp.setForeground(palette::TextDim);
    cursor.insertText(QTime::currentTime().toString(QStringLiteral("HH:mm:ss")) +
                          QStringLiteral("  "),
                      stamp);
    QTextCharFormat body;
    body.setForeground(colourOf(kind));
    if (kind == Kind::Command || kind == Kind::Success || kind == Kind::Error)
        body.setFontWeight(QFont::DemiBold);
    const QString prefix = kind == Kind::Command ? QStringLiteral("$ ") : QString();
    cursor.insertText(prefix + text, body);

    if (following)
        bar->setValue(bar->maximum());
    if (kind == Kind::Error) {
        ++m_errors;
        emit errorLogged();
    }
}

void LogView::clear()
{
    m_text->clear();
    m_last.clear();
    m_errors = 0;
}

QString LogView::text() const
{
    return m_text->toPlainText();
}
