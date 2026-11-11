#include "metadata_picker.h"

#include "compact_disclosure_button.h"
#include "metadata_value_chips.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

MetadataPicker::MetadataPicker(QWidget *input, int colorColumn, QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    entryFrame = new QFrame(this);
    entryFrame->setObjectName("metadataPickerEntry");
    entryFrame->setProperty("focused", false);
    auto *entry = new QHBoxLayout(entryFrame);
    entry->setContentsMargins(3, 2, 3, 2);
    entry->setSpacing(4);
    entry->addWidget(input, 1);
    toggle = new CompactDisclosureButton(this);
    toggle->setObjectName("pickerToggle");
    toggle->setFixedWidth(22);
    toggle->setAutoRaise(true);
    toggle->setStyleSheet("QToolButton { border:0; background:transparent; padding:0; }");
    entry->addWidget(toggle, 0, Qt::AlignVCenter);
    layout->addWidget(entryFrame);
    auto editors = input->findChildren<QLineEdit *>();
    if (auto *line = qobject_cast<QLineEdit *>(input))
        editors.append(line);
    for (auto *line : editors)
        line->installEventFilter(this);
    area = new QScrollArea(this);
    area->setObjectName("pickerArea");
    area->setWidgetResizable(true);
    area->setFrameShape(QFrame::NoFrame);
    area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    chips = new MetadataValueChips(colorColumn);
    chips->setObjectName("pickerChips");
    chips->clicked = [this](const QString &value) { emit valueClicked(value); };
    area->setWidget(chips);
    area->setAutoFillBackground(false);
    area->viewport()->setAutoFillBackground(false);
    chips->setAutoFillBackground(false);
    layout->addWidget(area);
    empty = new QLabel(tr("No entries in this library yet"), this);
    empty->setObjectName("pickerEmpty");
    empty->setWordWrap(true);
    layout->addWidget(empty);
    chips->installEventFilter(this);
    connect(toggle, &QToolButton::clicked, this, &MetadataPicker::toggleRequested);
    setExpanded(false);
}

void MetadataPicker::setValues(QStringList values, const QString &firstValue)
{
    for (auto &value : values)
        value = value.trimmed();
    values.removeAll(QString());
    values.removeDuplicates();
    values.sort(Qt::CaseInsensitive);
    if (!firstValue.isEmpty()) {
        values.removeAll(firstValue);
        values.prepend(firstValue);
    }
    if (values != chips->values) {
        chips->values = std::move(values);
        chips->refresh();
    }
    area->setVisible(expanded && !chips->values.isEmpty());
    empty->setVisible(expanded && chips->values.isEmpty());
    updateAreaHeight();
}

void MetadataPicker::setSelected(const QStringList &values)
{
    chips->selected = QSet<QString>(values.begin(), values.end());
    chips->update();
}

void MetadataPicker::setExpanded(bool value)
{
    expanded = value;
    area->setVisible(value && !chips->values.isEmpty());
    empty->setVisible(value && chips->values.isEmpty());
    toggle->setArrowType(value ? Qt::UpArrow : Qt::DownArrow);
    toggle->setToolTip(value ? tr("Collapse choices") : tr("Expand choices"));
    toggle->setAccessibleName(toggle->toolTip());
    if (value) {
        chips->refresh();
        updateAreaHeight();
    }
}

bool MetadataPicker::isExpanded() const
{
    return expanded;
}

void MetadataPicker::updateAreaHeight()
{
    const int height = qBound(42, chips->minimumHeight() + 2, 180);
    if (area->minimumHeight() != height || area->maximumHeight() != height)
        area->setFixedHeight(height);
}

bool MetadataPicker::eventFilter(QObject *object, QEvent *event)
{
    if (qobject_cast<QLineEdit *>(object) && (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut)) {
        entryFrame->setProperty("focused", event->type() == QEvent::FocusIn);
        entryFrame->style()->unpolish(entryFrame);
        entryFrame->style()->polish(entryFrame);
        entryFrame->update();
    }
    if (object == chips && (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest) && !heightPending) {
        heightPending = true;
        QTimer::singleShot(0, this, [this] {
            heightPending = false;
            if (expanded)
                updateAreaHeight();
        });
    }
    return QWidget::eventFilter(object, event);
}
