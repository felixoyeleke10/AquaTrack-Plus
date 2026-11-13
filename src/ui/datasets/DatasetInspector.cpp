#include "ui/datasets/DatasetInspector.h"

#include "ui/AppTheme.h"
#include "ui/common/Glyphs.h"
#include "ui/common/Widgets.h"
#include "ui/common/WindowChrome.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMap>
#include <QPainter>
#include <QPushButton>
#include <QShortcut>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace aquatrack::ui {

// One annotation box, in image pixels.
struct InspectorBox {
    QRectF rect;
    QString className;
    QString label;
    QColor color;
};

// The image with its boxes, letterboxed into whatever space it is given.
class InspectorCanvas final : public QWidget {
public:
    explicit InspectorCanvas(QWidget* parent) : QWidget(parent)
    {
        setMinimumSize(360, 240);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setContent(QImage image, QVector<InspectorBox> boxes, QString emptyText)
    {
        m_image = std::move(image);
        m_boxes = std::move(boxes);
        m_emptyText = std::move(emptyText);
        update();
    }
    void setShowLabels(bool show)
    {
        m_showLabels = show;
        update();
    }
    [[nodiscard]] const QVector<InspectorBox>& boxes() const { return m_boxes; }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.fillRect(rect(), QColor(0x02, 0x03, 0x05));
        if (m_image.isNull()) {
            painter.setPen(palette::TextDim);
            painter.drawText(rect(), Qt::AlignCenter, m_emptyText);
            return;
        }
        const QSize scaled = m_image.size().scaled(size(), Qt::KeepAspectRatio);
        const QRectF target((width() - scaled.width()) / 2.0, (height() - scaled.height()) / 2.0,
                            scaled.width(), scaled.height());
        painter.drawImage(target, m_image);
        const qreal sx = target.width() / m_image.width();
        const qreal sy = target.height() / m_image.height();

        QFont labelFont = painter.font();
        labelFont.setPointSizeF(8.0);
        labelFont.setBold(true);
        painter.setFont(labelFont);
        const QFontMetrics metrics(labelFont);
        for (const auto& box : m_boxes) {
            const QRectF view(target.left() + box.rect.x() * sx, target.top() + box.rect.y() * sy,
                              box.rect.width() * sx, box.rect.height() * sy);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(box.color, 1.8));
            painter.drawRoundedRect(view, 2, 2);
            if (!m_showLabels)
                continue;
            const QRectF plate(view.left(), view.top() - metrics.height() - 4,
                               metrics.horizontalAdvance(box.label) + 10, metrics.height() + 3);
            const QRectF placed = plate.top() < target.top()
                                      ? plate.translated(0, view.height() + plate.height() + 2)
                                      : plate;
            painter.setPen(Qt::NoPen);
            painter.setBrush(box.color);
            painter.drawRoundedRect(placed, 3, 3);
            painter.setPen(QColor(6, 8, 12));
            painter.drawText(placed, Qt::AlignCenter, box.label);
        }
    }

private:
    QImage m_image;
    QVector<InspectorBox> m_boxes;
    QString m_emptyText;
    bool m_showLabels = true;
};

}  // namespace aquatrack::ui

using namespace aquatrack::ui;
namespace data = aquatrack::data;

namespace {

const QStringList kImageFilters{QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
                                QStringLiteral("*.png"), QStringLiteral("*.bmp")};

QString grouped(qint64 value)
{
    return QLocale::system().toString(value);
}

}  // namespace

DatasetInspector::DatasetInspector(data::DatasetDescriptor dataset, QWidget* parent)
    : QDialog(parent), m_dataset(std::move(dataset))
{
    setObjectName(QStringLiteral("datasetInspector"));
    setWindowFlag(Qt::Window);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QStringLiteral("Inspect – %1").arg(m_dataset.name));
    resize(1280, 820);
    m_isSequence = m_dataset.format == data::DatasetFormat::BrackishMot;

    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(18, 16, 18, 16);
    page->setSpacing(12);

    // ---- Heading ----------------------------------------------------------
    auto* head = new QHBoxLayout;
    head->setSpacing(10);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(sectionLabel(QStringLiteral("Inspect dataset"), this));
    auto* nameRow = new QHBoxLayout;
    nameRow->setSpacing(8);
    nameRow->addWidget(typedLabel(m_dataset.name, "title", this));
    auto* format = new Badge(this);
    format->setText(data::datasetFormatName(m_dataset.format));
    format->setAccent(m_isSequence ? palette::Accent : palette::Secondary);
    nameRow->addWidget(format);
    if (m_dataset.synthetic) {
        auto* synthetic = new Badge(this);
        synthetic->setText(QStringLiteral("Synthetic"));
        synthetic->setAccent(palette::Warning);
        nameRow->addWidget(synthetic);
    }
    nameRow->addStretch();
    titles->addLayout(nameRow);
    titles->addWidget(typedLabel(QDir::toNativeSeparators(m_dataset.rootPath), "mono", this));
    head->addLayout(titles, 1);
    if (m_isSequence) {
        auto* review = textButton(QStringLiteral("Open in review"), this, "primary");
        review->setIcon(glyphIcon(Glyph::Review, 15, Qt::white, Qt::white));
        review->setEnabled(m_dataset.isReviewable());
        connect(review, &QPushButton::clicked, this, [this] {
            emit reviewRequested(m_dataset.rootPath);
            close();
        });
        head->addWidget(review, 0, Qt::AlignBottom);
    }
    auto* reveal = textButton(QStringLiteral("Show image in folder"), this);
    reveal->setIcon(glyphIcon(Glyph::Files, 15, palette::TextSecondary, palette::TextPrimary));
    connect(reveal, &QPushButton::clicked, this, [this] {
        if (m_current >= 0 && m_current < m_items.size())
            QDesktopServices::openUrl(
                QUrl::fromLocalFile(QFileInfo(m_items.at(m_current).image).absolutePath()));
    });
    head->addWidget(reveal, 0, Qt::AlignBottom);
    page->addLayout(head);

    // ---- Body: images | canvas | legend ------------------------------------
    auto* body = new QHBoxLayout;
    body->setSpacing(12);

    auto* listCard = card(this);
    listCard->setFixedWidth(250);
    auto* listColumn = new QVBoxLayout(listCard);
    listColumn->setContentsMargins(12, 12, 12, 12);
    listColumn->setSpacing(8);
    m_source = new QComboBox(listCard);
    m_source->setObjectName(QStringLiteral("inspectorSource"));
    listColumn->addWidget(m_source);
    m_filter = new QLineEdit(listCard);
    m_filter->setPlaceholderText(QStringLiteral("Filter by file name"));
    m_filter->setClearButtonEnabled(true);
    listColumn->addWidget(m_filter);
    m_list = new QListWidget(listCard);
    m_list->setObjectName(QStringLiteral("inspectorImages"));
    m_list->setUniformItemSizes(true);
    listColumn->addWidget(m_list, 1);
    body->addWidget(listCard);

    auto* centre = new QVBoxLayout;
    centre->setSpacing(8);
    auto* canvasCard = card(this);
    auto* canvasBody = new QVBoxLayout(canvasCard);
    canvasBody->setContentsMargins(1, 1, 1, 1);
    m_canvas = new InspectorCanvas(canvasCard);
    canvasBody->addWidget(m_canvas);
    centre->addWidget(canvasCard, 1);

    auto* transport = new QHBoxLayout;
    transport->setSpacing(8);
    auto* previous = iconButton(Glyph::Previous, QStringLiteral("Previous image (Left)"), this);
    auto* next = iconButton(Glyph::Next, QStringLiteral("Next image (Right)"), this);
    connect(previous, &QPushButton::clicked, this, [this] { step(-1); });
    connect(next, &QPushButton::clicked, this, [this] { step(1); });
    transport->addWidget(previous);
    m_position = typedLabel(QString(), "mono", this);
    m_position->setMinimumWidth(110);
    m_position->setAlignment(Qt::AlignCenter);
    transport->addWidget(m_position);
    transport->addWidget(next);
    transport->addSpacing(10);
    m_fileName = typedLabel(QString(), "small", this);
    transport->addWidget(m_fileName, 1);
    m_showLabels = new QCheckBox(QStringLiteral("Show labels"), this);
    m_showLabels->setChecked(true);
    connect(m_showLabels, &QCheckBox::toggled, m_canvas, &InspectorCanvas::setShowLabels);
    transport->addWidget(m_showLabels);
    centre->addLayout(transport);
    body->addLayout(centre, 1);

    auto* legendCard = card(this);
    legendCard->setFixedWidth(230);
    auto* legendColumn = new QVBoxLayout(legendCard);
    legendColumn->setContentsMargins(12, 12, 12, 12);
    legendColumn->setSpacing(8);
    legendColumn->addWidget(sectionLabel(QStringLiteral("In this image"), legendCard));
    m_imageInfo = typedLabel(QString(), "dim", legendCard);
    m_imageInfo->setWordWrap(true);
    legendColumn->addWidget(m_imageInfo);
    m_legend = new QListWidget(legendCard);
    m_legend->setSelectionMode(QAbstractItemView::NoSelection);
    m_legend->setFocusPolicy(Qt::NoFocus);
    legendColumn->addWidget(m_legend, 1);
    if (!m_dataset.classNames.isEmpty()) {
        auto* classes = typedLabel(QStringLiteral("Dataset classes: %1")
                                       .arg(m_dataset.classNames.join(QStringLiteral(", "))),
                                   "dim", legendCard);
        classes->setWordWrap(true);
        legendColumn->addWidget(classes);
    }
    body->addWidget(legendCard);
    page->addLayout(body, 1);

    // ---- Behaviour ----------------------------------------------------------
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (auto* item = m_list->item(row))
            showIndex(item->data(Qt::UserRole).toInt());
    });
    connect(m_filter, &QLineEdit::textChanged, this, &DatasetInspector::applyListFilter);
    connect(m_source, &QComboBox::currentIndexChanged, this, &DatasetInspector::loadSource);
    new QShortcut(QKeySequence(Qt::Key_Right), this, [this] { step(1); });
    new QShortcut(QKeySequence(Qt::Key_Left), this, [this] { step(-1); });
    new QShortcut(QKeySequence(Qt::Key_Home), this, [this] { showIndex(0); });
    new QShortcut(QKeySequence(Qt::Key_End), this, [this] { showIndex(int(m_items.size()) - 1); });

    // Sources: the splits of a YOLO set, or the one sequence.
    {
        const QSignalBlocker blocker(m_source);
        if (m_isSequence) {
            m_source->addItem(QStringLiteral("Sequence frames"));
            m_source->setEnabled(false);
        } else {
            for (const auto& split : m_dataset.splits)
                m_source->addItem(QStringLiteral("%1  (%2 images)")
                                      .arg(split.name, grouped(split.imageCount)));
        }
    }
    loadSource(0);
    useDarkTitleBar(this);
}

QStringList DatasetInspector::sources() const
{
    QStringList names;
    for (int index = 0; index < m_source->count(); ++index)
        names << m_source->itemText(index);
    return names;
}

void DatasetInspector::selectSource(int index)
{
    m_source->setCurrentIndex(index);
}

int DatasetInspector::boxCount() const
{
    return int(m_canvas->boxes().size());
}

void DatasetInspector::loadSource(int index)
{
    m_items.clear();
    m_current = -1;
    if (m_isSequence) {
        QString error;
        if (m_sequence.load(m_dataset.rootPath, &error)) {
            m_items.reserve(m_sequence.frameCount());
            for (int frame = 1; frame <= m_sequence.frameCount(); ++frame) {
                const QString path = m_sequence.imagePath(frame);
                if (QFileInfo::exists(path))
                    m_items.push_back({path, frame});
            }
        }
    } else if (index >= 0 && index < m_dataset.splits.size()) {
        const auto& split = m_dataset.splits.at(index);
        const QDir images(split.imagePath);
        for (const auto& name : images.entryList(kImageFilters, QDir::Files, QDir::Name))
            m_items.push_back({images.filePath(name), 0});
        // Ultralytics layouts mirror images/ into labels/.
        m_labelFolder = split.imagePath;
        m_labelFolder.replace(QStringLiteral("/images"), QStringLiteral("/labels"));
    }

    // The list shows file names; each row remembers its item index so the
    // filter can hide rows without renumbering.
    m_list->clear();
    for (int item = 0; item < m_items.size(); ++item) {
        auto* row = new QListWidgetItem(
            m_isSequence ? QStringLiteral("Frame %1").arg(m_items.at(item).frame)
                         : QFileInfo(m_items.at(item).image).fileName());
        row->setData(Qt::UserRole, item);
        m_list->addItem(row);
    }
    applyListFilter();
    if (m_items.isEmpty()) {
        m_canvas->setContent({}, {}, QStringLiteral("No images found here"));
        m_position->setText(QStringLiteral("0 / 0"));
        m_fileName->clear();
        m_legend->clear();
        m_imageInfo->clear();
        return;
    }
    showIndex(0);
}

void DatasetInspector::applyListFilter()
{
    const QString needle = m_filter->text().trimmed();
    for (int row = 0; row < m_list->count(); ++row) {
        auto* item = m_list->item(row);
        item->setHidden(!needle.isEmpty() && !item->text().contains(needle, Qt::CaseInsensitive));
    }
}

void DatasetInspector::step(int delta)
{
    if (m_items.isEmpty())
        return;
    showIndex(std::clamp(m_current + delta, 0, int(m_items.size()) - 1));
}

void DatasetInspector::showIndex(int index)
{
    if (index < 0 || index >= m_items.size() || index == m_current)
        return;
    m_current = index;
    loadCurrent();
    // Keep the list in step without re-entering showIndex.
    const QSignalBlocker blocker(m_list);
    m_list->setCurrentRow(index);
    if (auto* row = m_list->item(index))
        m_list->scrollToItem(row);
}

void DatasetInspector::loadCurrent()
{
    const Item& item = m_items.at(m_current);
    QImage image(item.image);
    QVector<InspectorBox> boxes;

    if (m_isSequence) {
        for (const auto& truth : m_sequence.annotations(item.frame)) {
            const QString className = aquatrack::BrackishMotSequence::className(truth.classId);
            boxes.push_back({truth.box, className,
                             QStringLiteral("ID %1  %2").arg(truth.trackId).arg(className),
                             speciesColorForName(className, truth.classId)});
        }
    } else if (!image.isNull()) {
        // YOLO label: one "class cx cy w h" line per box, normalised 0..1.
        QFile labels(QDir(m_labelFolder).filePath(QFileInfo(item.image).completeBaseName() +
                                                  QStringLiteral(".txt")));
        if (labels.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&labels);
            while (!stream.atEnd()) {
                const auto fields =
                    stream.readLine().split(QLatin1Char(' '), Qt::SkipEmptyParts);
                if (fields.size() < 5)
                    continue;
                const int classId = fields[0].toInt();
                const double cx = fields[1].toDouble() * image.width();
                const double cy = fields[2].toDouble() * image.height();
                const double w = fields[3].toDouble() * image.width();
                const double h = fields[4].toDouble() * image.height();
                const QString className = m_dataset.classNames.value(
                    classId, QStringLiteral("class %1").arg(classId));
                boxes.push_back({QRectF(cx - w / 2, cy - h / 2, w, h), className, className,
                                 speciesColorForName(className, classId)});
            }
        }
    }

    // Legend: boxes per class in this image.
    QMap<QString, QPair<int, QColor>> perClass;
    for (const auto& box : boxes) {
        auto& entry = perClass[box.className];
        ++entry.first;
        entry.second = box.color;
    }
    m_legend->clear();
    for (auto it = perClass.cbegin(); it != perClass.cend(); ++it) {
        auto* row = new QListWidgetItem(
            glyphIcon(Glyph::Species, 14, it.value().second, it.value().second),
            QStringLiteral("%1   × %2").arg(it.key()).arg(it.value().first));
        row->setForeground(it.value().second);
        m_legend->addItem(row);
    }
    m_imageInfo->setText(image.isNull()
                             ? QStringLiteral("Could not read this image")
                             : QStringLiteral("%1 × %2 px  ·  %3 box(es)")
                                   .arg(image.width())
                                   .arg(image.height())
                                   .arg(boxes.size()));
    m_position->setText(QStringLiteral("%1 / %2").arg(grouped(m_current + 1),
                                                       grouped(m_items.size())));
    m_fileName->setText(QFileInfo(item.image).fileName());
    m_canvas->setContent(std::move(image), std::move(boxes),
                         QStringLiteral("Could not read %1").arg(QFileInfo(item.image).fileName()));
}
