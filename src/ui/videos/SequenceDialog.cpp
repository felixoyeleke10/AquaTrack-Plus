#include "ui/videos/SequenceDialog.h"

#include "data/media/MediaTools.h"
#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSet>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

using namespace aquatrack::ui;
namespace media = aquatrack::media;

namespace {

QString key(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toLower();
}

QString rate(double framesPerSecond)
{
    return QLocale::system().toString(framesPerSecond, 'f',
                                      std::abs(framesPerSecond - std::round(framesPerSecond)) < 0.005
                                          ? 0
                                          : 2);
}

}  // namespace

SequenceDialog::SequenceDialog(const QString& title, const QString& name,
                               const QVector<media::VideoInfo>& clips,
                               std::function<QString(const QString&)> nameProblem, QWidget* parent)
    : QDialog(parent), m_nameProblem(std::move(nameProblem))
{
    setWindowTitle(title);
    setMinimumSize(620, 520);
    setStyleSheet(QStringLiteral("SequenceDialog { background: %1; }").arg(palette::Surface.name()));

    auto* body = new QVBoxLayout(this);
    body->setContentsMargins(22, 20, 22, 18);
    body->setSpacing(10);

    body->addWidget(sectionLabel(QStringLiteral("Name"), this));
    m_name = new QLineEdit(name, this);
    m_name->setPlaceholderText(QStringLiteral("e.g. Transect A"));
    m_name->setToolTip(QStringLiteral("Annotations are stored under this name"));
    body->addWidget(m_name);
    m_problem = typedLabel(QString(), "small", this);
    m_problem->setStyleSheet(QStringLiteral("color:%1").arg(palette::Warning.name()));
    m_problem->setWordWrap(true);
    body->addWidget(m_problem);

    body->addSpacing(6);
    auto* clipsHead = new QHBoxLayout;
    clipsHead->setContentsMargins(0, 0, 0, 0);
    clipsHead->addWidget(sectionLabel(QStringLiteral("Clips in play order"), this));
    clipsHead->addStretch();
    m_summary = typedLabel(QString(), "dim", this);
    clipsHead->addWidget(m_summary);
    body->addLayout(clipsHead);

    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(10);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("sequenceClips"));
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // Drag clips into order.
    m_list->setDragDropMode(QAbstractItemView::InternalMove);
    m_list->setDefaultDropAction(Qt::MoveAction);
    m_list->setIconSize(QSize(18, 18));
    // The full path is in each row's tooltip; rows never scroll sideways.
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setTextElideMode(Qt::ElideMiddle);
    connect(m_list->model(), &QAbstractItemModel::rowsMoved, this, &SequenceDialog::refresh);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &SequenceDialog::refresh);
    row->addWidget(m_list, 1);

    auto* side = new QVBoxLayout;
    side->setContentsMargins(0, 0, 0, 0);
    side->setSpacing(6);
    auto* add = textButton(QStringLiteral("Add videos..."), this);
    add->setIcon(glyphIcon(Glyph::Video, 15, palette::TextSecondary, palette::TextPrimary));
    connect(add, &QPushButton::clicked, this, &SequenceDialog::addVideos);
    m_up = textButton(QStringLiteral("Move up"), this);
    connect(m_up, &QPushButton::clicked, this, [this] { moveSelected(-1); });
    m_down = textButton(QStringLiteral("Move down"), this);
    connect(m_down, &QPushButton::clicked, this, [this] { moveSelected(1); });
    m_remove = textButton(QStringLiteral("Remove"), this);
    connect(m_remove, &QPushButton::clicked, this, &SequenceDialog::removeSelected);
    side->addWidget(add);
    side->addSpacing(8);
    side->addWidget(m_up);
    side->addWidget(m_down);
    side->addSpacing(8);
    side->addWidget(m_remove);
    side->addStretch();
    row->addLayout(side);
    body->addLayout(row, 1);

    m_note = typedLabel(QString(), "small", this);
    m_note->setWordWrap(true);
    m_note->hide();
    body->addWidget(m_note);
    body->addWidget(typedLabel(
        QStringLiteral("Clips play back to back on one timeline. Drag to reorder."), "dim", this));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_ok = buttons->button(QDialogButtonBox::Ok);
    m_ok->setProperty("role", "primary");
    // The button box has already styled it; apply the new role.
    m_ok->style()->unpolish(m_ok);
    m_ok->style()->polish(m_ok);
    m_ok->setText(title.startsWith(QStringLiteral("Edit")) ? QStringLiteral("Save sequence")
                                                           : QStringLiteral("Create sequence"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    body->addWidget(buttons);

    connect(m_name, &QLineEdit::textChanged, this, &SequenceDialog::refresh);
    for (const auto& clip : clips)
        appendClip(clip);
    refresh();
}

QString SequenceDialog::name() const
{
    return m_name->text().trimmed();
}

QVector<media::VideoInfo> SequenceDialog::clips() const
{
    QVector<media::VideoInfo> result;
    for (int i = 0; i < m_list->count(); ++i)
        result.push_back(m_infos.value(m_list->item(i)->data(Qt::UserRole).toString()));
    return result;
}

void SequenceDialog::setNote(const QString& note)
{
    m_note->setText(note);
    m_note->setVisible(!note.isEmpty());
}

void SequenceDialog::appendClip(const media::VideoInfo& info)
{
    const QString id = key(info.path);
    m_infos.insert(id, info);
    auto* item = new QListWidgetItem(m_list);
    item->setData(Qt::UserRole, id);
    item->setToolTip(QDir::toNativeSeparators(info.path));
    const bool missing = !QFileInfo::exists(info.path);
    item->setIcon(glyphIcon(missing ? Glyph::Alert : Glyph::Video, 18,
                            missing ? palette::Warning : palette::AccentHover,
                            missing ? palette::Warning : palette::AccentHover));
    if (missing)
        item->setForeground(palette::Warning);
}

void SequenceDialog::refresh()
{
    // Numbering and details follow the current order.
    const QLocale locale = QLocale::system();
    int frames = 0;
    qint64 duration = 0;
    QSet<QString> rates;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        const media::VideoInfo info = m_infos.value(item->data(Qt::UserRole).toString());
        const bool missing = !QFileInfo::exists(info.path);
        frames += info.frameCount;
        duration += info.durationMs;
        if (info.frameRate > 0)
            rates.insert(rate(info.frameRate));
        item->setText(QStringLiteral("%1.  %2\n      %3")
                          .arg(i + 1)
                          .arg(QFileInfo(info.path).fileName(),
                               missing ? QStringLiteral("Not found - remove it, or restore the file")
                                       : QStringLiteral("%1  \u00b7  %2 frames  \u00b7  %3 fps")
                                             .arg(media::formatDuration(info.durationMs),
                                                  locale.toString(info.frameCount),
                                                  rate(info.frameRate))));
    }
    QString summary = m_list->count() == 0
                          ? QStringLiteral("No clips")
                          : QStringLiteral("%1 %2  \u00b7  %3 frames  \u00b7  %4")
                                .arg(m_list->count())
                                .arg(m_list->count() == 1 ? QStringLiteral("clip")
                                                          : QStringLiteral("clips"),
                                     locale.toString(frames), media::formatDuration(duration));
    if (rates.size() > 1)
        summary += QStringLiteral("  \u00b7  mixed frame rates");
    m_summary->setText(summary);

    const auto rows = m_list->selectionModel()->selectedRows();
    int first = m_list->count();
    int last = -1;
    for (const auto& index : rows) {
        first = std::min(first, index.row());
        last = std::max(last, index.row());
    }
    m_remove->setEnabled(!rows.isEmpty());
    m_up->setEnabled(!rows.isEmpty() && first > 0);
    m_down->setEnabled(!rows.isEmpty() && last < m_list->count() - 1);

    QString problem;
    if (name().isEmpty())
        problem = QStringLiteral("Give the sequence a name.");
    else if (m_nameProblem)
        problem = m_nameProblem(name());
    bool anyMissing = false;
    for (const auto& clip : clips())
        anyMissing = anyMissing || !QFileInfo::exists(clip.path);
    if (problem.isEmpty() && m_list->count() == 0)
        problem = QStringLiteral("Add at least one video.");
    else if (problem.isEmpty() && anyMissing)
        problem = QStringLiteral("Some clips cannot be found. Remove them, or restore the files.");
    m_problem->setText(problem);
    m_problem->setVisible(!problem.isEmpty());
    m_ok->setEnabled(problem.isEmpty());
}

void SequenceDialog::addVideos()
{
    const QStringList chosen = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Add videos to the sequence"), QString(), media::videoFileDialogFilter());
    if (chosen.isEmpty())
        return;
    QStringList skipped;
    QStringList failed;
    QProgressDialog progress(QStringLiteral("Reading videos..."), QStringLiteral("Cancel"), 0,
                             int(chosen.size()), this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(300);
    for (int i = 0; i < chosen.size(); ++i) {
        progress.setValue(i);
        QApplication::processEvents();
        if (progress.wasCanceled())
            break;
        const QString path = chosen.at(i);
        if (m_infos.contains(key(path))) {
            skipped << QFileInfo(path).fileName();
            continue;
        }
        media::VideoInfo info;
        QString error;
        if (!media::probeVideo(path, info, &error)) {
            failed << QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), error);
            continue;
        }
        appendClip(info);
    }
    progress.setValue(int(chosen.size()));
    refresh();
    QStringList notes;
    if (!skipped.isEmpty())
        notes << QStringLiteral("Already in the sequence: %1").arg(skipped.join(QStringLiteral(", ")));
    if (!failed.isEmpty())
        notes << QStringLiteral("Could not be read:\n%1").arg(failed.join(QLatin1Char('\n')));
    if (!notes.isEmpty())
        QMessageBox::information(this, QStringLiteral("Some videos were not added"),
                                 notes.join(QStringLiteral("\n\n")));
}

void SequenceDialog::removeSelected()
{
    const auto selected = m_list->selectedItems();
    for (auto* item : selected) {
        m_infos.remove(item->data(Qt::UserRole).toString());
        delete item;
    }
    refresh();
}

void SequenceDialog::moveSelected(int delta)
{
    QList<int> rows;
    for (const auto& index : m_list->selectionModel()->selectedRows())
        rows << index.row();
    if (rows.isEmpty())
        return;
    std::sort(rows.begin(), rows.end());
    if (delta > 0)
        std::reverse(rows.begin(), rows.end());
    QList<QListWidgetItem*> moved;
    for (const int row : rows) {
        const int target = row + delta;
        if (target < 0 || target >= m_list->count())
            return;
    }
    for (const int row : rows) {
        QListWidgetItem* item = m_list->takeItem(row);
        m_list->insertItem(row + delta, item);
        moved << item;
    }
    m_list->clearSelection();
    for (auto* item : moved)
        item->setSelected(true);
    refresh();
}
