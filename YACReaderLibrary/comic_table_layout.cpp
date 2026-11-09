#include "comic_table_layout.h"

#include "comic_model.h"
#include "yacreader_global_gui.h"

#include <QHeaderView>
#include <QSettings>

namespace {
constexpr auto settingsKey = "COMICS_VIEW_HEADERS_MANGA_1";
}
void ComicTableLayout::reset(QHeaderView &header)
{
    const auto columns = ComicModel::visibleColumns();
    const QList<int> widths { 40, 160, 280, 130, 120, 170, 200, 95, 80, 85, 105, 55, 100, 100 };
    header.setStretchLastSection(false);
    header.setSectionResizeMode(QHeaderView::Interactive);
    for (int i = 0; i < header.count(); ++i)
        header.hideSection(i);
    // Preserve the internal missing-file marker independently of the user columns.
    if (header.count() > ComicModel::State)
        header.moveSection(header.visualIndex(ComicModel::State), 0);
    for (int i = 0; i < columns.size() && columns.at(i) < header.count(); ++i) {
        const int column = columns.at(i);
        header.moveSection(header.visualIndex(column), i + 1);
        header.showSection(column);
        header.resizeSection(column, widths.at(i));
    }
    header.setSortIndicator(-1, Qt::AscendingOrder);
    header.setSortIndicatorShown(false);
}

void ComicTableLayout::restore(QHeaderView &header, QSettings &settings)
{
    settings.remove(COMICS_VIEW_HEADERS);
    settings.remove(COMICS_VIEW_HEADERS_GEOMETRY);
    if (!settings.contains(settingsKey) || !header.restoreState(settings.value(settingsKey).toByteArray()))
        reset(header);
    header.setStretchLastSection(false);
    header.setSectionsMovable(true);
    header.setSectionsClickable(true);
    const auto columns = ComicModel::visibleColumns();
    for (int i = 0; i < header.count(); ++i)
        if (i != ComicModel::State && !columns.contains(i))
            header.hideSection(i);
    // Add the heart to existing saved layouts without discarding column widths.
    header.moveSection(header.visualIndex(ComicModel::Favorite), 1);
    header.showSection(ComicModel::Favorite);
    header.resizeSection(ComicModel::Favorite, 40);
    header.setSectionResizeMode(ComicModel::Favorite, QHeaderView::Fixed);
    if (!ComicModel::isSortableColumn(header.sortIndicatorSection()))
        header.setSortIndicatorShown(false);
}

void ComicTableLayout::save(const QHeaderView &header, QSettings &settings)
{
    settings.setValue(settingsKey, header.saveState());
}

void ComicTableLayout::sortFromHeaderClick(QHeaderView &header, ComicModel &model, int column)
{
    if (ComicModel::isSortableColumn(column)) {
        const auto order = model.tableSortColumn() == column && model.tableSortOrder() == Qt::AscendingOrder
                ? Qt::DescendingOrder
                : Qt::AscendingOrder;
        model.sort(column, order);
    }
    // QHeaderView may already have changed its indicator before emitting sectionClicked.
    header.setSortIndicator(model.tableSortColumn(), model.tableSortOrder());
    header.setSortIndicatorShown(model.tableSortColumn() >= 0);
}
