#include "ui/review/RuntimeStrip.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QStorageInfo>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

using namespace aquatrack::ui;

namespace {

QString formatBytes(quint64 bytes)
{
    constexpr double kGigabyte = 1024.0 * 1024.0 * 1024.0;
    return QStringLiteral("%1 GB").arg(bytes / kGigabyte, 0, 'f', 1);
}

}  // namespace

RuntimeStrip::RuntimeStrip(QWidget* parent) : QFrame(parent)
{
    setProperty("role", "card");

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(12, 9, 12, 9);
    column->setSpacing(8);

    auto* head = new QHBoxLayout;
    head->setContentsMargins(0, 0, 0, 0);
    head->setSpacing(8);
    head->addWidget(sectionLabel(QStringLiteral("Runtime"), this));
    head->addStretch();
    m_summary = typedLabel(QStringLiteral("No sequence loaded"), "dim", this);
    head->addWidget(m_summary);
    column->addLayout(head);

    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    m_decoder = new StatusChip(Glyph::Video, QStringLiteral("Sequence decoder"), this);
    m_decoder->setState(QStringLiteral("Idle"), palette::TextDim);

    m_tracker = new StatusChip(Glyph::Tracks, QStringLiteral("IoU tracker"), this);
    m_tracker->setState(QStringLiteral("Ready"), palette::Success);

    // There is no IDetector implementation in this build. Saying so is the
    // whole point of this chip.
    m_detector = new StatusChip(Glyph::Models, QStringLiteral("Detector backend"), this);
    m_detector->setState(QStringLiteral("Not installed"), palette::Warning);
    m_detector->setToolTip(
        QStringLiteral("src/inference/IDetector.h has no implementation in this build, so the "
                       "application reviews ground truth rather than running inference."));

    m_store = new StatusChip(Glyph::Files, QStringLiteral("Observation store"), this);
    m_store->setState(QStringLiteral("Not open"), palette::TextDim);

    m_export = new StatusChip(Glyph::Export, QStringLiteral("Export"), this);
    m_export->setState(QStringLiteral("Ready"), palette::Success);

    for (auto* chip : {m_decoder, m_tracker, m_detector, m_store, m_export})
        row->addWidget(chip, 1);

    m_memory = new MeterCard(Glyph::Analytics, QStringLiteral("System memory"), this);
    m_storage = new MeterCard(Glyph::Datasets, QStringLiteral("Storage"), this);
    row->addWidget(m_memory, 1);
    row->addWidget(m_storage, 1);
    column->addLayout(row);

    m_resourceTimer = new QTimer(this);
    m_resourceTimer->setInterval(4000);
    connect(m_resourceTimer, &QTimer::timeout, this, &RuntimeStrip::refreshResources);
    m_resourceTimer->start();
    refreshResources();
}

void RuntimeStrip::setSequenceState(bool loaded, int frames)
{
    if (loaded)
        m_decoder->setState(QStringLiteral("%1 frames").arg(frames), palette::Success);
    else
        m_decoder->setState(QStringLiteral("Idle"), palette::TextDim);
}

void RuntimeStrip::setObservationStore(const QString& path)
{
    m_storePath = path;
    if (path.isEmpty()) {
        m_store->setState(QStringLiteral("Not open"), palette::TextDim);
    } else {
        m_store->setState(QFileInfo(path).fileName(), palette::Success);
        m_store->setToolTip(path);
    }
    refreshResources();
}

void RuntimeStrip::setPlaybackRate(double framesPerSecond)
{
    if (framesPerSecond > 0.0)
        m_decoder->setState(QStringLiteral("%1 fps").arg(framesPerSecond, 0, 'f', 1),
                            palette::Success);
}

void RuntimeStrip::setFrameInfo(int frame, int total, int width, int height)
{
    if (total <= 0) {
        m_summary->setText(QStringLiteral("No sequence loaded"));
        return;
    }
    m_summary->setText(QStringLiteral("Frame %1 / %2   ·   %3 x %4")
                           .arg(frame)
                           .arg(total)
                           .arg(width)
                           .arg(height));
}

void RuntimeStrip::refreshResources()
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        const quint64 total = status.ullTotalPhys;
        const quint64 used = total - status.ullAvailPhys;
        m_memory->setReadout(QStringLiteral("%1 / %2").arg(formatBytes(used), formatBytes(total)));
        m_memory->setRatio(total ? double(used) / double(total) : 0.0,
                           QStringLiteral("%1% in use").arg(status.dwMemoryLoad));
    }
#else
    m_memory->setReadout(QStringLiteral("unavailable"));
    m_memory->setRatio(0.0, QString());
#endif

    const QStorageInfo storage =
        m_storePath.isEmpty() ? QStorageInfo::root() : QStorageInfo(QFileInfo(m_storePath).absolutePath());
    if (storage.isValid() && storage.bytesTotal() > 0) {
        const quint64 total = quint64(storage.bytesTotal());
        const quint64 used = total - quint64(storage.bytesAvailable());
        m_storage->setReadout(QStringLiteral("%1 / %2").arg(formatBytes(used), formatBytes(total)));
        m_storage->setRatio(double(used) / double(total),
                            QStringLiteral("%1 free on %2")
                                .arg(formatBytes(quint64(storage.bytesAvailable())),
                                     storage.rootPath()));
    }
}
