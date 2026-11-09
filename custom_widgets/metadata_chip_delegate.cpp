#include "metadata_chip_delegate.h"

#include "comic_model.h"
#include "metadata_chip_colors.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QtMath>

namespace {
struct Chip {
    QRect rect;
    QString text;
    QString value;
};
struct ChipLayout {
    QList<Chip> chips;
    QRect overflow;
};
ChipLayout layoutChips(const QStyleOptionViewItem &option, const QModelIndex &index)
{
    ChipLayout result;
    const auto values = index.data(ComicModel::ChipValuesRole).toStringList();
    const auto area = option.rect.adjusted(5, 4, -5, -4);
    // Recreate metrics from the font actually used for painting. Cached integer
    // metrics can be stale after styling/DPI changes and round Latin advances down.
    const QFontMetricsF metrics(option.font, option.widget);
    const int ellipsis = qCeil(metrics.horizontalAdvance(QStringLiteral("\u2026"))) + 4;
    int x = area.left();
    for (int i = 0; i < values.size(); ++i) {
        const int available = area.right() - x + 1;
        const auto &value = values.at(i);
        const int textWidth = qCeil(metrics.horizontalAdvance(value)) + 4;
        const int wanted = textWidth + 16;
        if (available < qMin(wanted, ellipsis + 16)) {
            result.overflow = QRect(x, area.top(), qMax(0, available), area.height());
            break;
        }
        const bool more = i + 1 < values.size();
        const int width = wanted <= available ? wanted : available - (more ? ellipsis + 4 : 0);
        const auto displayed = width >= wanted ? value : metrics.elidedText(value, Qt::ElideRight, qMax(0, width - 16));
        result.chips.append({ QRect(x, area.top(), width, area.height()), displayed, value });
        x += width + 4;
        if (width < wanted) {
            if (more)
                result.overflow = QRect(x, area.top(), qMax(0, area.right() - x + 1), area.height());
            break;
        }
    }
    return result;
}
}
MetadataChipDelegate::MetadataChipDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
    connect(&MetadataChipColors::instance(), &MetadataChipColors::colorsChanged, this, [this] {
        if (auto view = qobject_cast<QAbstractItemView *>(this->parent()))
            view->viewport()->update();
    });
}
QColor MetadataChipDelegate::colorForValue(int column, const QString &value)
{
    return MetadataChipColors::instance().color(column, value);
}
QString MetadataChipDelegate::valueAt(const QStyleOptionViewItem &option, const QModelIndex &index, const QPoint &point) const
{
    QStyleOptionViewItem styled(option);
    initStyleOption(&styled, index);
    for (const auto &chip : layoutChips(styled, index).chips)
        if (chip.rect.contains(point))
            return chip.value;
    return { };
}
void MetadataChipDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItem styled(option);
    initStyleOption(&styled, index);
    styled.text.clear();
    const auto style = styled.widget ? styled.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &styled, painter, styled.widget);
    const auto layout = layoutChips(styled, index);
    painter->save();
    painter->setClipRect(option.rect);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setFont(styled.font);
    for (const auto &chip : layout.chips) {
        const auto color = colorForValue(index.column(), chip.value);
        painter->setPen(color.darker(110));
        painter->setBrush(color);
        painter->drawRoundedRect(chip.rect.adjusted(0, 0, -1, -1), 5, 5);
        painter->setPen(MetadataChipColors::foregroundColor(color));
        painter->drawText(chip.rect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, chip.text);
    }
    if (!layout.overflow.isEmpty()) {
        painter->setPen(styled.palette.color((styled.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text));
        painter->drawText(layout.overflow, Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("\u2026"));
    }
    painter->restore();
}
QSize MetadataChipDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &) const
{
    return QSize(120, qMax(32, option.fontMetrics.height() + 14));
}
bool MetadataChipDelegate::editorEvent(QEvent *event, QAbstractItemModel *, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() != QEvent::MouseButtonRelease)
        return false;
    const auto mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() != Qt::LeftButton)
        return false;
    const auto value = valueAt(option, index, mouse->position().toPoint());
    if (value.isEmpty())
        return false;
    emit chipClicked(index, value);
    return true;
}
