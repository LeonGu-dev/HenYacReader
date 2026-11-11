#include "comic_table_layout.h"

#include "comic_model.h"
#include "yacreader_global_gui.h"

#include <QFontMetricsF>
#include <QHeaderView>
#include <QSettings>
#include <QSignalBlocker>
#include <QtMath>

namespace {
constexpr auto settingsKey = "COMICS_VIEW_HEADERS_MANGA_1";
int conventionWidth(const QHeaderView &header)
{
    // Match a typical event chip, including its padding and the cell margins.
    return qMax(header.minimumSectionSize(), qCeil(QFontMetricsF(header.font(), &header).horizontalAdvance(QStringLiteral("c102"))) + 32);
}
}
void ComicTableLayout::reset(QHeaderView &header)
{
    const auto columns = ComicModel::visibleColumns();
    const QList<int> widths { 40, conventionWidth(header), 160, 280, 130, 120, 170, 200, 95, 80, 85, 105, 55, 100, 100 };
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
    // Migrate existing layouts once, preserving later visibility/order choices.
    if (!settings.value("COMICS_VIEW_CONVENTION_LAYOUT_V2", false).toBool()) {
        const QSignalBlocker blocker(header);
        header.moveSection(header.visualIndex(ComicModel::Convention), 2);
        header.showSection(ComicModel::Convention);
        header.resizeSection(ComicModel::Convention, conventionWidth(header));
        // Qt can restore an older section count but then serialize a state that
        // it cannot read back. Rebuild the geometry on a fresh current-size header.
        QHeaderView migrated(Qt::Horizontal);
        migrated.setModel(header.model());
        migrated.setStretchLastSection(false);
        migrated.setSectionsMovable(true);
        migrated.setSectionsClickable(true);
        for (int visual = 0; visual < header.count(); ++visual) {
            const int column = header.logicalIndex(visual);
            const bool hidden = header.isSectionHidden(column);
            if (hidden)
                header.showSection(column);
            const int width = header.sectionSize(column);
            if (hidden)
                header.hideSection(column);
            migrated.moveSection(migrated.visualIndex(column), visual);
            migrated.resizeSection(column, width);
            migrated.setSectionHidden(column, hidden);
            migrated.setSectionResizeMode(column, header.sectionResizeMode(column));
        }
        migrated.setSortIndicator(header.sortIndicatorSection(), header.sortIndicatorOrder());
        migrated.setSortIndicatorShown(header.isSortIndicatorShown());
        header.restoreState(migrated.saveState());
        // setModel can restore the header repeatedly before any user edit/save.
        // Persist the migrated layout together with its marker, not just the marker.
        settings.setValue(settingsKey, header.saveState());
        settings.setValue("COMICS_VIEW_CONVENTION_LAYOUT_V2", true);
    }
    if (!settings.value("COMICS_VIEW_CONVENTION_COMPACT_WIDTH_V2", false).toBool()) {
        const QSignalBlocker blocker(header);
        header.setSectionResizeMode(ComicModel::Convention, QHeaderView::Interactive);
        const bool hidden = header.isSectionHidden(ComicModel::Convention);
        if (hidden)
            header.showSection(ComicModel::Convention);
        header.resizeSection(ComicModel::Convention, conventionWidth(header));
        if (hidden)
            header.hideSection(ComicModel::Convention);
        settings.setValue(settingsKey, header.saveState());
        settings.setValue("COMICS_VIEW_CONVENTION_COMPACT_WIDTH_V2", true);
    }
    if (!ComicModel::isSortableColumn(header.sortIndicatorSection()))
        header.setSortIndicatorShown(false);
}

void ComicTableLayout::save(const QHeaderView &header, QSettings &settings)
{
    settings.setValue(settingsKey, header.saveState());
    settings.setValue("COMICS_VIEW_CONVENTION_LAYOUT_V2", true);
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
