#ifndef COMIC_TABLE_LAYOUT_H
#define COMIC_TABLE_LAYOUT_H
class QHeaderView;
class QSettings;
class ComicModel;
namespace ComicTableLayout {
void reset(QHeaderView &header);
void sortFromHeaderClick(QHeaderView &header, ComicModel &model, int column);
void restore(QHeaderView &header, QSettings &settings);
void save(const QHeaderView &header, QSettings &settings);
}
#endif
