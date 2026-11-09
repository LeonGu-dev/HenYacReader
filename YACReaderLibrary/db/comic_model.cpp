
#include "comic_model.h"

#include "comic.h"
#include "comic_db.h"
#include "comic_item.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "overloaded.h"
#include "qnaturalsorting.h"
#include "reading_list_model.h"
#include "yacreader_global_gui.h"

#include <QMap>
#include <QSet>
#include <QSqlRecord>
#include <QStringBuilder>
#include <QTimer>
#include <QtDebug>
#include <QtGui>

#include <algorithm>
#include <numeric>
#include <optional>
#include <utility>

#ifdef use_unarr
#include <unarr.h>
#endif

// ci.number,ci.title,c.fileName,ci.numPages,c.id,c.parentId,c.path,ci.hash,ci.read
#include "QsLog.h"

namespace {

// nullptr for a placeholder, which has no comic_info
const ComicInfo *rowInfo(const ComicItem &item)
{
    return std::visit(overloaded {
                              [](const ComicItemCase::Comic &row) -> const ComicInfo * { return &row.comic.info; },
                              [](const ComicItemCase::ComicInList &row) -> const ComicInfo * { return &row.comic.info; },
                              [](const ComicItemCase::DeletedComicInList &row) -> const ComicInfo * { return &row.info; },
                              [](const ComicItemCase::PlaceholderInList &) -> const ComicInfo * { return nullptr; },
                              [](const ComicItemCase::PlaceholderInFolder &) -> const ComicInfo * { return nullptr; },
                      },
                      item);
}

ComicInfo *rowInfo(ComicItem &item)
{
    return const_cast<ComicInfo *>(rowInfo(std::as_const(item)));
}

const Placeholder *rowPlaceholder(const ComicItem &item)
{
    if (const auto *row = std::get_if<ComicItemCase::PlaceholderInList>(&item))
        return &row->placeholder;
    if (const auto *row = std::get_if<ComicItemCase::PlaceholderInFolder>(&item))
        return &row->placeholder;
    return nullptr;
}

Placeholder *rowPlaceholder(ComicItem &item)
{
    return const_cast<Placeholder *>(rowPlaceholder(std::as_const(item)));
}

const ListEntry *rowEntry(const ComicItem &item)
{
    return std::visit(overloaded {
                              [](const ComicItemCase::Comic &) -> const ListEntry * { return nullptr; },
                              [](const ComicItemCase::ComicInList &row) -> const ListEntry * { return &row.entry; },
                              [](const ComicItemCase::DeletedComicInList &row) -> const ListEntry * { return &row.entry; },
                              [](const ComicItemCase::PlaceholderInList &row) -> const ListEntry * { return &row.entry; },
                              [](const ComicItemCase::PlaceholderInFolder &) -> const ListEntry * { return nullptr; },
                      },
                      item);
}

ComicRowId rowIdOf(const ComicItem &item)
{
    return std::visit(overloaded {
                              [](const ComicItemCase::Comic &row) -> ComicRowId { return row.comic.file.id; },
                              [](const ComicItemCase::ComicInList &row) -> ComicRowId { return row.entry.id; },
                              [](const ComicItemCase::DeletedComicInList &row) -> ComicRowId { return row.entry.id; },
                              [](const ComicItemCase::PlaceholderInList &row) -> ComicRowId { return row.entry.id; },
                              [](const ComicItemCase::PlaceholderInFolder &row) -> ComicRowId { return row.placeholder.id; },
                      },
                      item);
}

bool supportsAction(const ComicItem &item, ComicAction action)
{
    const auto withFile = [action] {
        switch (action) {
        case ComicAction::Open:
        case ComicAction::OpenContainingFolder:
        case ComicAction::ChangeFile:
        case ComicAction::SaveCover:
        case ComicAction::SetReadState:
        case ComicAction::SetType:
        case ComicAction::EditMetadata:
        case ComicAction::DeleteMetadata:
        case ComicAction::SearchComicVine:
        case ComicAction::AssignNumbers:
        case ComicAction::SetCustomCover:
        case ComicAction::AddToFavorites:
        case ComicAction::AddToList:
            return true;
        case ComicAction::RemoveFromList:
        case ComicAction::RemoveFromFolder:
        case ComicAction::MatchToLibraryComic:
            return false;
        }
        return false;
    };

    return std::visit(overloaded {
                              [&](const ComicItemCase::Comic &) { return withFile(); },
                              [&](const ComicItemCase::ComicInList &) { return action == ComicAction::RemoveFromList || withFile(); },
                              [action](const ComicItemCase::DeletedComicInList &) {
                                  switch (action) {
                                  case ComicAction::SaveCover:
                                  case ComicAction::SetReadState:
                                  case ComicAction::SetType:
                                  case ComicAction::EditMetadata:
                                  case ComicAction::DeleteMetadata:
                                  case ComicAction::AssignNumbers:
                                  case ComicAction::SearchComicVine:
                                  case ComicAction::SetCustomCover:
                                  case ComicAction::AddToList:
                                  case ComicAction::RemoveFromList:
                                  case ComicAction::MatchToLibraryComic:
                                      return true;
                                  // Favorites is keyed by comic id
                                  case ComicAction::AddToFavorites:
                                  case ComicAction::Open:
                                  case ComicAction::OpenContainingFolder:
                                  case ComicAction::ChangeFile:
                                  case ComicAction::RemoveFromFolder:
                                      return false;
                                  }
                                  return false;
                              },
                              [action](const ComicItemCase::PlaceholderInList &row) {
                                  switch (action) {
                                  case ComicAction::SaveCover:
                                      return row.placeholder.coverKey.has_value();
                                  case ComicAction::EditMetadata:
                                  case ComicAction::SearchComicVine:
                                  case ComicAction::SetCustomCover:
                                  case ComicAction::AssignNumbers:
                                  case ComicAction::AddToList:
                                  case ComicAction::RemoveFromList:
                                  case ComicAction::MatchToLibraryComic:
                                      return true;
                                  // no file, reading state, type, or rating
                                  case ComicAction::Open:
                                  case ComicAction::OpenContainingFolder:
                                  case ComicAction::ChangeFile:
                                  case ComicAction::SetReadState:
                                  case ComicAction::SetType:
                                  case ComicAction::DeleteMetadata:
                                  case ComicAction::AddToFavorites:
                                  case ComicAction::RemoveFromFolder:
                                      return false;
                                  }
                                  return false;
                              },
                              [action](const ComicItemCase::PlaceholderInFolder &row) {
                                  switch (action) {
                                  case ComicAction::SaveCover:
                                      return row.placeholder.coverKey.has_value();
                                  case ComicAction::EditMetadata:
                                  case ComicAction::SearchComicVine:
                                  case ComicAction::SetCustomCover:
                                  case ComicAction::AssignNumbers:
                                  case ComicAction::AddToList:
                                  case ComicAction::RemoveFromFolder:
                                  case ComicAction::MatchToLibraryComic:
                                      return true;
                                  case ComicAction::Open:
                                  case ComicAction::OpenContainingFolder:
                                  case ComicAction::ChangeFile:
                                  case ComicAction::SetReadState:
                                  case ComicAction::SetType:
                                  case ComicAction::DeleteMetadata:
                                  case ComicAction::AddToFavorites:
                                  case ComicAction::RemoveFromList:
                                      return false;
                                  }
                                  return false;
                              },
                      },
                      item);
}

bool sameRow(const ComicItem &a, const ComicItem &b)
{
    return rowIdOf(a) == rowIdOf(b);
}

bool sameValues(const ComicItem &a, const ComicItem &b)
{
    if (a.index() != b.index() || !sameRow(a, b))
        return false;

    const auto *entryA = rowEntry(a);
    const auto *entryB = rowEntry(b);
    if (entryA != nullptr && *entryA != *entryB)
        return false;

    const auto *comicA = rowComic(a);
    const auto *comicB = rowComic(b);
    if (comicA != nullptr && comicA->file != comicB->file)
        return false;

    if (const auto *placeholderA = rowPlaceholder(a))
        return placeholderA->hasSameValues(*rowPlaceholder(b));
    return rowInfo(a)->hasSameValues(*rowInfo(b));
}

// what kind of row this is and where it is; a role that the case does not have is nothing to show
QVariant identityValue(const ComicItem &item, int role)
{
    return std::visit(overloaded {
                              [role](const ComicItemCase::Comic &) -> QVariant {
                                  switch (role) {
                                  case ComicModel::KindRole:
                                      return QStringLiteral("Comic");
                                  case ComicModel::AvailableRole:
                                      return true;
                                  }
                                  return { };
                              },
                              [role](const ComicItemCase::ComicInList &row) -> QVariant {
                                  switch (role) {
                                  case ComicModel::KindRole:
                                      return QStringLiteral("ComicInList");
                                  case ComicModel::AvailableRole:
                                      return true;
                                  case ComicModel::ListItemIdRole:
                                      return row.entry.id.id;
                                  case ComicModel::ListIdRole:
                                      return row.entry.listId;
                                  }
                                  return { };
                              },
                              [role](const ComicItemCase::DeletedComicInList &row) -> QVariant {
                                  switch (role) {
                                  case ComicModel::KindRole:
                                      return QStringLiteral("DeletedComicInList");
                                  case ComicModel::AvailableRole:
                                      return false;
                                  case ComicModel::LastKnownNameRole:
                                      return row.entry.lastKnownFile ? QVariant(row.entry.lastKnownFile->name) : QVariant();
                                  case ComicModel::ListItemIdRole:
                                      return row.entry.id.id;
                                  case ComicModel::ListIdRole:
                                      return row.entry.listId;
                                  }
                                  return { };
                              },
                              [role](const ComicItemCase::PlaceholderInList &row) -> QVariant {
                                  switch (role) {
                                  case ComicModel::KindRole:
                                      return QStringLiteral("PlaceholderInList");
                                  case ComicModel::AvailableRole:
                                      return false;
                                  case ComicModel::ListItemIdRole:
                                      return row.entry.id.id;
                                  case ComicModel::ListIdRole:
                                      return row.entry.listId;
                                  }
                                  return { };
                              },
                              [role](const ComicItemCase::PlaceholderInFolder &) -> QVariant {
                                  switch (role) {
                                  case ComicModel::KindRole:
                                      return QStringLiteral("PlaceholderInFolder");
                                  case ComicModel::AvailableRole:
                                      return false;
                                  }
                                  return { };
                              },
                      },
                      item);
}

// the file columns; a deleted comic shows its last known file and has no id or folder
QVariant fileColumnValue(const ComicItem &item, int column)
{
    const auto fromFile = [column](const ComicFile &file) -> QVariant {
        switch (column) {
        case ComicModel::FileName:
            return file.fileName;
        case ComicModel::Id:
            return file.id.value();
        case ComicModel::Parent_Id:
            return file.folderId.value();
        case ComicModel::Path:
            return file.path;
        }
        return { };
    };

    return std::visit(overloaded {
                              [&](const ComicItemCase::Comic &row) { return fromFile(row.comic.file); },
                              [&](const ComicItemCase::ComicInList &row) { return fromFile(row.comic.file); },
                              [column](const ComicItemCase::DeletedComicInList &row) -> QVariant {
                                  if (!row.entry.lastKnownFile)
                                      return { };
                                  switch (column) {
                                  case ComicModel::FileName:
                                      return row.entry.lastKnownFile->name;
                                  case ComicModel::Path:
                                      return row.entry.lastKnownFile->path;
                                  }
                                  return { };
                              },
                              [](const ComicItemCase::PlaceholderInList &) -> QVariant { return { }; },
                              [](const ComicItemCase::PlaceholderInFolder &) -> QVariant { return { }; },
                      },
                      item);
}

// a placeholder has the descriptive columns only
QVariant columnValue(const ComicItem &item, int column)
{
    const auto &metadata = rowMetadata(item);
    switch (column) {
    case ComicModel::Number:
        return metadata.number;
    case ComicModel::Title:
        return metadata.title;
    case ComicModel::Favorite:
        return metadata.favorite.toBool();
    case ComicModel::Author:
        return metadata.writer;
    case ComicModel::Work:
        return metadata.work;
    case ComicModel::Characters:
        return metadata.characters;
    case ComicModel::Tags:
        return metadata.tags;
    case ComicModel::Color:
        return metadata.color;
    case ComicModel::Censorship:
        return metadata.censorship;
    case ComicModel::Translation:
        return metadata.translationStatus;
    case ComicModel::PublicationDate:
        return metadata.date;
    case ComicModel::Series:
        return metadata.series;
    case ComicModel::Volume:
        return metadata.volume;
    case ComicModel::StoryArc:
        return metadata.storyArc;
    case ComicModel::FileName:
    case ComicModel::Id:
    case ComicModel::Parent_Id:
    case ComicModel::Path:
        return fileColumnValue(item, column);
    }

    const auto *infoOfRow = rowInfo(item);
    if (infoOfRow == nullptr)
        return { };
    const auto &info = *infoOfRow;
    switch (column) {
    case ComicModel::NumPages:
        return info.numPages;
    case ComicModel::Hash:
        return info.hash;
    case ComicModel::ReadColumn:
        return info.read;
    case ComicModel::CurrentPage:
        return info.currentPage;
    case ComicModel::Rating:
        return info.rating;
    case ComicModel::HasBeenOpened:
        return qlonglong(info.hasBeenOpened ? 1 : 0); // the views got the SQL integer
    case ComicModel::Added:
        return info.added;
    case ComicModel::Type:
        return info.type;
    case ComicModel::LastTimeOpened:
        return info.lastTimeOpened;
    }
    return QVariant();
}

}

ComicModel::ComicModel(QObject *parent)
    : QAbstractItemModel(parent), showRecent(false), recentDays(1)

{
    connect(this, &QAbstractItemModel::modelAboutToBeReset, this, [this] { coverFileExistence.clear(); });
    connect(this, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex &first, const QModelIndex &last) {
        filterValuesCache.reset();
        if (!currentMetadataFilter.active())
            return;
        for (int row = first.row(); row <= last.row(); ++row)
            for (auto &item : filterSource)
                if (sameRow(item, _data.at(row)))
                    item = _data.at(row);
    });
    connect(this, &QAbstractItemModel::rowsAboutToBeRemoved, this, [this](const QModelIndex &, int first, int last) {
        filterValuesCache.reset();
        if (!currentMetadataFilter.active())
            return;
        QSet<ComicRowId> removed;
        for (int row = first; row <= last; ++row)
            removed.insert(rowIdOf(_data.at(row)));
        filterSource.removeIf([&](const auto &item) { return removed.contains(rowIdOf(item)); });
    });
}

ComicModel::~ComicModel()
{
}

int ComicModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return ComicModel::ColumnCount;
}

bool ComicModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(action);
    Q_UNUSED(row);
    Q_UNUSED(column);
    Q_UNUSED(parent);

    if (!canBeResorted())
        return false;
    return data->formats().contains(YACReader::YACReaderLibrarComiscSelectionMimeDataFormat);
}

// TODO: optimize this method (seriously)
bool ComicModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    QLOG_TRACE() << ">>>>>>>>>>>>>>dropMimeData ComicModel<<<<<<<<<<<<<<<<<" << parent << row << "," << column;

    if (!canDropMimeData(data, action, row, column, parent))
        return false;

    const auto rowIds = YACReader::mimeDataToRows(data);
    if (rowIds.isEmpty())
        return false;

    if (row < 0 || row > _data.count())
        row = _data.count();

    QList<int> currentIndexes;
    for (const auto &current : indexesOf(rowIds)) {
        if (!current.isValid())
            return false;
        currentIndexes << current.row();
    }

    std::sort(currentIndexes.begin(), currentIndexes.end());
    const QSet<int> draggedRows(currentIndexes.cbegin(), currentIndexes.cend());

    if (draggedRows.contains(row)) // no resorting
        return false;

    const int destinationRow = (row == -1 || row >= _data.length()) ? -1 : row;

    QList<int> newSorting;
    newSorting.reserve(_data.size());
    for (int i = 0; i < _data.length(); i++) {
        if (draggedRows.contains(i))
            continue;
        if (i == destinationRow)
            newSorting << currentIndexes;
        newSorting << i;
    }
    if (destinationRow == -1)
        newSorting << currentIndexes;

    QLOG_TRACE() << newSorting;

    int tempRow = row;

    for (const auto &rowId : std::as_const(rowIds)) {
        for (int i = 0; i < _data.size(); i++) {
            if (rowIdOf(_data.at(i)) == rowId) {
                const bool skipElement = i == tempRow || i + 1 == tempRow;

                if (!skipElement) {
                    if (!beginMoveRows(parent, i, i, parent, tempRow))
                        return false;

                    if (i > tempRow)
                        _data.move(i, tempRow);
                    else
                        _data.move(i, tempRow - 1);

                    endMoveRows();
                }

                if (i > tempRow)
                    tempRow++;

                break;
            }
        }
    }

    // TODO fix selection
    QList<qulonglong> allComicIds;
    QList<ListItemId> allEntryIds;
    for (const auto &item : std::as_const(_data)) {
        std::visit(overloaded {
                           [&allComicIds](const ComicItemCase::Comic &row) { allComicIds << row.comic.file.id.value(); },
                           [&allEntryIds](const ComicItemCase::ComicInList &row) { allEntryIds << row.entry.id; },
                           [&allEntryIds](const ComicItemCase::DeletedComicInList &row) { allEntryIds << row.entry.id; },
                           [&allEntryIds](const ComicItemCase::PlaceholderInList &row) { allEntryIds << row.entry.id; },
                           [](const ComicItemCase::PlaceholderInFolder &) { },
                   },
                   item);
    }
    bool reordered = true;
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);
        switch (mode) {
        case Favorites:
            DBHelper::reasignOrderToComicsInFavorites(allComicIds, db);
            break;
        case Label:
            reordered = DBHelper::reorderLabelEntries(LabelId(sourceId), allEntryIds, db);
            break;
        case ReadingList:
            reordered = DBHelper::reorderReadingListEntries(ReadingListId(sourceId), allEntryIds, db);
            break;
        case Folder:
        case Reading:
        case Recent:
        case SearchResult:
        case SmartList:
            break;
        }
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    // the rows show the stored order again
    if (!reordered) {
        reload();
        return false;
    }

    // endMoveRows();

    emit resortedIndexes(newSorting);
    int destSelectedIndex = row < 0 ? _data.length() : row;

    if (destSelectedIndex > currentIndexes.at(0))
        emit newSelectedIndex(index(qMax(0, destSelectedIndex - 1), 0, parent));
    else
        emit newSelectedIndex(index(qMax(0, destSelectedIndex), 0, parent));

    return true;
}

bool ComicModel::canBeResorted() const
{
    return enableResorting && presentationSortColumn < 0 && !currentMetadataFilter.active();
}

// comic ids for Favorites, file moves, and the drop checks, and the rows in order
QMimeData *ComicModel::mimeData(const QModelIndexList &indexes) const
{
    QList<qulonglong> ids;
    QList<ComicRowId> rows;
    for (const auto &index : indexes) {
        const auto &item = _data.at(index.row());
        if (const auto *comic = rowComic(item))
            ids << comic->file.id.value();
        rows << rowIdOf(item);
    }

    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << ids; // serialize the list of identifiers

    auto mimeData = new QMimeData();
    mimeData->setData(YACReader::YACReaderLibrarComiscSelectionMimeDataFormat, data);
    YACReader::setRowsMimeData(mimeData, rows);

    return mimeData;
}

QStringList ComicModel::mimeTypes() const
{
    QLOG_DEBUG() << "mimeTypes";
    QStringList list;
    list << YACReader::YACReaderLibrarComiscSelectionMimeDataFormat;
    return list;
}

QHash<int, QByteArray> ComicModel::roleNames() const
{
    QHash<int, QByteArray> roles;

    roles[NumberRole] = "number";
    roles[TitleRole] = "title";
    roles[FileNameRole] = "file_name";
    roles[NumPagesRole] = "num_pages";
    roles[IdRole] = "id";
    roles[Parent_IdRole] = "parent_id";
    roles[PathRole] = "path";
    roles[HashRole] = "hash";
    roles[ReadColumnRole] = "read_column";
    roles[CurrentPageRole] = "current_page";
    roles[RatingRole] = "rating";
    roles[HasBeenOpenedRole] = "has_been_opened";
    roles[CoverPathRole] = "cover_path";
    roles[PublicationDate] = "date";
    roles[ReadableTitle] = "readable_title";
    roles[AddedRole] = "added_date";
    roles[TypeRole] = "type";
    roles[ShowRecentRole] = "show_recent";
    roles[RecentRangeRole] = "recent_range";
    roles[SizeRole] = "size";
    roles[SeriesRole] = "series";
    roles[VolumeRole] = "volume";
    roles[StoryArcRole] = "story_arc";
    roles[KindRole] = "kind";
    roles[AvailableRole] = "available";
    roles[LastKnownNameRole] = "last_known_name";
    roles[ListItemIdRole] = "list_item_id";
    roles[ListIdRole] = "list_id";
    roles[FavoriteRole] = "favorite";

    return roles;
}

QVariant ComicModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == Qt::DecorationRole) {
        return QVariant();
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) // TODO obtener esto de la query
        {
        case ComicModel::Number:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case ComicModel::NumPages:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case ComicModel::Size:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case ComicModel::CurrentPage:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    // TODO check here if any view is asking for TableModel::Roles
    // these roles will be used from QML/GridView

    const auto &item = _data.at(index.row());
    const auto *placeholder = rowPlaceholder(item);
    const auto value = [&item](int column) { return columnValue(item, column); };
    // Display fallback only; the metadata and API keep their original values.
    const auto orDash = [](const QString &title) { return title.isEmpty() ? QStringLiteral("-") : title; };
    const auto title = [&] { return orDash(titleOrFileName(rowMetadata(item), rowFileName(item))); };

    auto sizeString = [=] {
        auto bytes = value(ComicModel::Hash).toString().right(value(ComicModel::Hash).toString().length() - 40).toULongLong();

        QStringList units = { "B", "KB", "MB", "GB", "TB" };
        int i;
        double outputSize = bytes;
        for (i = 0; i < units.size() - 1; i++) {
            if (outputSize < 1024) {
                break;
            }
            outputSize = outputSize / 1024;
        }
        return QString("%1 %2").arg(outputSize, 0, 'f', 2).arg(units[i]);
    };

    if (role == FavoriteRole)
        return rowMetadata(item).favorite.toBool();
    if (role == NumberRole)
        return value(Number);
    else if (role == TitleRole)
        return title();
    else if (role == ReadableTitle) {
        const auto &metadata = rowMetadata(item);
        // Continue-reading cards and action labels have no separate number field.
        return orDash(titleIncludingNumber(metadata, rowFileName(item)));
    } else if (role == FileNameRole)
        return rowComic(item) ? value(FileName) : QVariant();
    else if (role == RatingRole)
        return value(Rating);
    else if (role == CoverPathRole) {
        if (placeholder != nullptr) {
            const auto coverPath = placeholder->coverKey ? LibraryPaths::placeholderCoverPathFromDataPath(_databasePath, *placeholder->coverKey) : QString();
            return coverPath.isEmpty() || !coverFileExists(coverPath) ? QUrl(QStringLiteral("qrc:/images/unavailableCover.png")) : QUrl::fromLocalFile(coverPath);
        }
        auto coverUrl = getCoverUrlPathForComicHash(value(Hash).toString());
        if (!rowComic(item) && !coverFileExists(coverUrl.toLocalFile()))
            return QUrl(QStringLiteral("qrc:/images/unavailableCover.png"));
        const auto revision = coverRevisions.value(rowInfo(item)->id.value());
        if (revision > 0)
            coverUrl.setQuery(QStringLiteral("revision=%1").arg(revision));
        return coverUrl;
    } else if (role == NumPagesRole)
        return value(NumPages);
    else if (role == CurrentPageRole)
        return value(CurrentPage);
    else if (role == ReadColumnRole)
        return value(ReadColumn).toBool();
    else if (role == HasBeenOpenedRole)
        return value(HasBeenOpened);
    else if (role == IdRole)
        return value(Id);
    else if (role == HashRole)
        return value(Hash);
    else if (role == PublicationDateRole)
        return QVariant(localizedDate(value(PublicationDate).toString()));
    else if (role == AddedRole)
        return value(Added);
    else if (role == TypeRole)
        return value(Type);
    else if (role == ShowRecentRole)
        return showRecent;
    else if (role == RecentRangeRole)
        return recentDays * 86400;
    else if (role == SizeRole)
        return placeholder != nullptr ? QVariant() : QVariant(sizeString());
    else if (role == SeriesRole)
        return value(Series);
    else if (role == VolumeRole)
        return value(Volume);
    else if (role == StoryArcRole)
        return value(StoryArc);
    else if (role == KindRole || role == AvailableRole || role == LastKnownNameRole || role == ListItemIdRole || role == ListIdRole)
        return identityValue(item, role);

    if (role == ChipValuesRole) {
        if (index.column() == Work)
            return value(Work).toString().trimmed().isEmpty() ? QStringList() : QStringList { value(Work).toString().trimmed() };
        if (index.column() == Characters || index.column() == Tags)
            return value(index.column()).toString().split('\n', Qt::SkipEmptyParts);
        return QStringList();
    }
    if (role == Qt::ToolTipRole) {
        if (index.column() == Work || index.column() == Characters || index.column() == Tags)
            return data(index, ChipValuesRole).toStringList().join('\n');
        if (index.column() == Author)
            return value(Author).toString();
        return data(index, Qt::DisplayRole);
    }
    if (role != Qt::DisplayRole)
        return QVariant();

    if (index.column() == ComicModel::Hash)
        return value(ComicModel::Hash);
    if (index.column() == ComicModel::Title)
        return value(ComicModel::Title);
    // a placeholder shows "-" for the values it does not have
    if (placeholder != nullptr) {
        switch (index.column()) {
        case ComicModel::FileName:
        case ComicModel::NumPages:
        case ComicModel::CurrentPage:
        case ComicModel::Size:
            return QStringLiteral("-");
        case ComicModel::ReadColumn:
            return QVariant();
        }
    }
    if (index.column() == ComicModel::Size)
        return sizeString();
    if (index.column() == ComicModel::ReadColumn)
        return ((value(NumPages).toInt() > 0 && value(CurrentPage).toInt() >= value(NumPages).toInt()) || value(ReadColumn).toBool()) ? QVariant(tr("yes")) : QVariant(tr("no"));
    if (index.column() == ComicModel::CurrentPage) {
        const auto pages = value(NumPages).toInt();
        return pages > 0 ? QString("%1/%2").arg(qBound(0, value(CurrentPage).toInt(), pages)).arg(pages) : QStringLiteral("\u2014");
    }
    if (index.column() == Author)
        return value(Author).toString().split('\n', Qt::SkipEmptyParts).join(QStringLiteral(" | "));
    if (index.column() == Characters || index.column() == Tags)
        return value(index.column()).toString().split('\n', Qt::SkipEmptyParts).join(QStringLiteral(" | "));
    if (index.column() == Color)
        return value(Color).isNull() ? tr("Not set") : value(Color).toBool() ? tr("Color")
                                                                             : tr("Black and white");
    if (index.column() == Censorship)
        return value(Censorship).toString() == "uncensored" ? tr("Uncensored") : value(Censorship).toString() == "censored" ? tr("Censored")
                                                                                                                            : tr("Not set");
    if (index.column() == Translation) {
        const auto status = value(Translation).toString();
        return status == "original" ? tr("Japanese") : status == "translated" ? tr("Chinese translation")
                : status == "ai"                                              ? tr("AI translation")
                                                                              : tr("Not set");
    }

    if (index.column() == ComicModel::Rating)
        return QVariant();

    if (index.column() == ComicModel::PublicationDate) {
        return QVariant(localizedDate(value(PublicationDate).toString()));
    }

    return value(index.column());
}

Qt::ItemFlags ComicModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return { };
    if (index.column() == ComicModel::Rating && supports(index, ComicAction::SetReadState))
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
}

QVariant ComicModel::headerData(int section, Qt::Orientation orientation,
                                int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {

        case ComicModel::Title:
            return QVariant(QString(tr("Title")));
        case ComicModel::FileName:
            return QVariant(QString(tr("File Name")));

        case ComicModel::Size:
            return QVariant(QString(tr("Size")));
        case ComicModel::ReadColumn:
            return QVariant(QString(tr("Read")));
        case ComicModel::CurrentPage:
            return tr("Reading progress");
        case ComicModel::Author:
            return tr("Author");
        case ComicModel::Favorite:
            return QStringLiteral("\u2665");
        case ComicModel::Work:
            return tr("Work / IP");
        case ComicModel::Characters:
            return tr("Characters");
        case ComicModel::Tags:
            return tr("Tags");
        case ComicModel::Color:
            return tr("Color / black and white");
        case ComicModel::Censorship:
            return tr("Censorship");
        case ComicModel::Translation:
            return tr("Translation");

        case ComicModel::Rating:
            return QVariant(QString(tr("Rating")));
        }
    }

    if (orientation == Qt::Horizontal && role == Qt::TextAlignmentRole) {
        switch (section) {
        case ComicModel::Number:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case ComicModel::NumPages:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case ComicModel::Size:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        case ComicModel::CurrentPage:
            return QVariant(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    if (orientation == Qt::Vertical && role == Qt::DecorationRole) {
        const QString fileName = (section >= 0 && section < _data.size()) ? rowFileName(_data.at(section)) : QString();
        QFileInfo fi(fileName);
        QString ext = fi.suffix();

        if (ext.compare("cbr", Qt::CaseInsensitive) == 0)
            return QVariant(QIcon(":/images/comicRar.png"));
        else if (ext.compare("cbz", Qt::CaseInsensitive) == 0)
            return QVariant(QIcon(":/images/comicZip.png"));
        else if (Comic::fileIsEpub(fileName))
            return QVariant(QIcon(":/images/comicZip.png"));
        else if (ext.compare("pdf", Qt::CaseInsensitive) == 0)
            return QVariant(QIcon(":/images/pdf.png"));
        else if (ext.compare("tar", Qt::CaseInsensitive) == 0)
            return QVariant(QIcon(":/images/tar.png"));
        else if (ext.compare("zip", Qt::CaseInsensitive) == 0)
            return QVariant(QIcon(":/images/zip.png"));
        else if (ext.compare("rar", Qt::CaseInsensitive) == 0)
            return QVariant(QIcon(":/images/rar.png"));
#if !defined(use_unarr) || (UNARR_API_VERSION >= 110)
        else if (ext.compare("7z", Qt::CaseInsensitive) == 0)
            return QVariant(QIcon(":/images/7z.png"));
        else if (ext.compare("cb7", Qt::CaseInsensitive) == 0)
            return QVariant(QIcon(":/images/comic7z.png"));
#endif
        else if (ext.compare("cbt", Qt::CaseInsensitive) == 0)
            return QVariant(QIcon(":/images/comicTar.png"));
    }

    return QVariant();
}

QModelIndex ComicModel::index(int row, int column, const QModelIndex &parent)
        const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    return createIndex(row, column);
}

QModelIndex ComicModel::parent(const QModelIndex &index) const
{
    Q_UNUSED(index)
    return QModelIndex();
}

int ComicModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        return _data.count();

    return 0;
}

QStringList ComicModel::getPaths(const QString &_source)
{
    QStringList paths;
    for (const auto &item : std::as_const(_data)) {
        QString path;
        if (const auto *placeholder = rowPlaceholder(item))
            path = placeholder->coverKey ? LibraryPaths::placeholderCoverPathFromDataPath(LibraryPaths::libraryDataPath(_source), *placeholder->coverKey) : QString();
        else
            path = LibraryPaths::coverPath(_source, rowInfo(item)->hash);
        if (rowComic(item) == nullptr && (path.isEmpty() || !coverFileExists(path)))
            path = QStringLiteral(":/images/unavailableCover.png");
        paths << path;
    }

    return paths;
}

QList<bool> ComicModel::getUnavailableList() const
{
    QList<bool> unavailable;
    unavailable.reserve(_data.size());
    for (const auto &item : std::as_const(_data))
        unavailable.append(rowComic(item) == nullptr);
    return unavailable;
}

QList<ComicItem> ComicModel::createFolderModelData(unsigned long long folderId, const QString &databasePath) const
{
    QList<ComicItem> modelData;

    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(databasePath);
        modelData = ComicItems::loadFolder(FolderId(folderId), db);

        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return modelData;
}

void ComicModel::setupFolderModelData(unsigned long long int folderId, const QString &databasePath)
{
    enableResorting = false;
    mode = Folder;
    sourceId = folderId;

    beginResetModel();
    _data.clear();

    _databasePath = databasePath;

    takeData(createFolderModelData(folderId, databasePath));

    endResetModel();
}

QList<ComicItem> ComicModel::createLabelModelData(unsigned long long parentLabel, const QString &databasePath) const
{
    QList<ComicItem> modelData;

    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(databasePath);
        modelData = ComicItems::loadLabel(LabelId(parentLabel), db);
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return modelData;
}

void ComicModel::setupLabelModelData(unsigned long long parentLabel, const QString &databasePath)
{
    enableResorting = true;
    mode = Label;
    sourceId = parentLabel;

    beginResetModel();
    _data.clear();

    _databasePath = databasePath;

    takeData(createLabelModelData(parentLabel, databasePath));

    endResetModel();
}

QList<ComicItem> ComicModel::createReadingListData(unsigned long long parentReadingList, const QString &databasePath, bool &enableResorting) const
{
    QList<ComicItem> modelData;

    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(databasePath);
        bool hasChildLists = false;
        modelData = ComicItems::loadReadingList(ReadingListId(parentReadingList), db, &hasChildLists);
        enableResorting = !hasChildLists; // only resorting if no sublists exist
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return modelData;
}

void ComicModel::setupReadingListModelData(unsigned long long parentReadingList, const QString &databasePath)
{
    mode = ReadingList;
    sourceId = parentReadingList;

    beginResetModel();
    _data.clear();

    _databasePath = databasePath;

    takeData(createReadingListData(parentReadingList, databasePath, enableResorting));

    endResetModel();
}

// a smart list that no longer exists has no rows
QList<ComicItem> ComicModel::createSmartListModelData(unsigned long long smartListId, const QString &databasePath) const
{
    QList<ComicItem> modelData;

    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(databasePath);
        modelData = ComicItems::loadSmartList(SmartListId(smartListId), db).value_or(QList<ComicItem>());
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return modelData;
}

void ComicModel::setupSmartListModelData(unsigned long long smartListId, const QString &databasePath)
{
    enableResorting = false;
    mode = SmartList;
    sourceId = smartListId;

    beginResetModel();
    _data.clear();

    _databasePath = databasePath;

    takeData(createSmartListModelData(smartListId, databasePath));

    endResetModel();
}

QList<ComicItem> ComicModel::createFavoritesModelData(const QString &databasePath) const
{
    QList<ComicItem> modelData;

    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(databasePath);
        QSqlQuery selectQuery(db);
        selectQuery.setForwardOnly(true);
        selectQuery.prepare("SELECT " + ComicDB::selectList("c", "ci") + " "
                                                                         "FROM comic c INNER JOIN comic_info ci ON (c.comicInfoId = ci.id) "
                                                                         "WHERE ci.favorite = 1 ORDER BY c.id");
        selectQuery.exec();
        modelData = ComicItems::fromQuery(selectQuery);
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return modelData;
}

void ComicModel::setupFavoritesModelData(const QString &databasePath)
{
    enableResorting = false;
    mode = Favorites;
    sourceId = -1;

    beginResetModel();
    _data.clear();

    _databasePath = databasePath;

    takeData(createFavoritesModelData(databasePath));

    endResetModel();
}

QList<ComicItem> ComicModel::createReadingModelData(const QString &databasePath) const
{
    QList<ComicItem> modelData;

    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(databasePath);
        QSqlQuery selectQuery(db);
        selectQuery.setForwardOnly(true);
        selectQuery.prepare("SELECT " + ComicDB::selectList("c", "ci") + " "
                                                                         "FROM comic c INNER JOIN comic_info ci ON (c.comicInfoId = ci.id) "
                                                                         "WHERE ci.hasBeenOpened = 1 AND ci.read = 0 "
                                                                         "ORDER BY ci.lastTimeOpened DESC");
        selectQuery.exec();

        modelData = ComicItems::fromQuery(selectQuery);
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return modelData;
}

void ComicModel::setupReadingModelData(const QString &databasePath)
{
    enableResorting = false;
    mode = Reading;
    sourceId = -1;

    beginResetModel();
    _data.clear();

    _databasePath = databasePath;

    takeData(createReadingModelData(databasePath));

    endResetModel();
}

QList<ComicItem> ComicModel::createRecentModelData(const QString &databasePath) const
{
    QList<ComicItem> modelData;

    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(databasePath);
        modelData = ComicItems::loadRecent(recentDays, db);
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return modelData;
}

void ComicModel::setupRecentModelData(const QString &databasePath)
{
    enableResorting = false;
    mode = Recent;
    sourceId = -1;

    beginResetModel();
    _data.clear();

    _databasePath = databasePath;

    takeData(createRecentModelData(databasePath));

    endResetModel();
}

void ComicModel::setModelData(QList<ComicItem> *data, const QString &databasePath)
{
    enableResorting = false;
    mode = SearchResult;
    sourceId = -1;

    _databasePath = databasePath;

    beginResetModel();

    takeData(*data);

    endResetModel();

    delete data;
}

QString ComicModel::getComicPath(QModelIndex mi)
{
    if (mi.isValid())
        if (const auto *comic = rowComic(_data.at(mi.row())))
            return comic->file.path;
    return "";
}

void ComicModel::takeData(const QList<ComicItem> &data)
{
    if (!applyingMetadataFilter)
        filterValuesCache.reset();
    if (filterLibraryPath != _databasePath) {
        currentMetadataFilter = { };
        filterLibraryPath = _databasePath;
    }
    filterSource = data;
    _data.clear();
    for (const auto &item : data)
        if (currentMetadataFilter.matches(rowMetadata(item), rowInfo(item) ? rowInfo(item)->rating : 0))
            _data.append(item);
    rememberSourceOrder(data);
    if (presentationSortColumn >= 0)
        std::stable_sort(_data.begin(), _data.end(), [this](const auto &a, const auto &b) { return tableLessThan(a, b); });
}

void ComicModel::takeUpdatedData(const QList<ComicItem> &updatedData, std::function<bool(const ComicItem &, const ComicItem &)> comparator)
{
    filterValuesCache.reset();
    if (currentMetadataFilter.active()) {
        beginResetModel();
        takeData(updatedData);
        endResetModel();
        emit metadataFilterChanged();
        return;
    }
    coverFileExistence.clear();
    rememberSourceOrder(updatedData);
    if (presentationSortColumn >= 0) {
        QHash<ComicRowId, const ComicItem *> updated;
        for (const auto &item : updatedData)
            updated.insert(rowIdOf(item), &item);
        // Reconcile by row identity, independent of the source query's order.
        for (int i = _data.size() - 1; i >= 0;) {
            if (updated.contains(rowIdOf(_data.at(i)))) {
                --i;
                continue;
            }
            const int last = i;
            while (i >= 0 && !updated.contains(rowIdOf(_data.at(i))))
                --i;
            beginRemoveRows({ }, i + 1, last);
            _data.remove(i + 1, last - i);
            endRemoveRows();
        }
        QSet<ComicRowId> present;
        for (int i = 0; i < _data.size(); ++i) {
            const auto id = rowIdOf(_data.at(i));
            present.insert(id);
            const auto &fresh = *updated.value(id);
            if (!sameValues(_data.at(i), fresh)) {
                _data[i] = fresh;
                emit dataChanged(index(i, 0), index(i, columnCount() - 1), { });
            }
        }
        QList<ComicItem> added;
        for (const auto &item : updatedData)
            if (!present.contains(rowIdOf(item)))
                added.append(item);
        if (!added.isEmpty()) {
            beginInsertRows({ }, _data.size(), _data.size() + added.size() - 1);
            _data.append(added);
            endInsertRows();
        }
        reorderRows([this](const auto &a, const auto &b) { return tableLessThan(a, b); });
        return;
    }
    int length = _data.size();
    int lengthUpdated = updatedData.size();

    int i; // index of the internal data
    int j; // index of the updated children
    for (i = 0, j = 0; i < length && j < lengthUpdated;) {
        const auto &comic = _data.at(i);
        const auto &updatedComic = updatedData.at(j);

        if (sameRow(comic, updatedComic)) {
            if (!sameValues(comic, updatedComic)) {
                auto modelIndexToUpdate = index(i, 0, QModelIndex());

                _data[i] = updatedComic;

                emit dataChanged(modelIndexToUpdate, modelIndexToUpdate);
            }

            i++;
            j++;
            continue;
        }

        auto lessThan = comparator(comic, updatedComic);

        // comic added
        if (!lessThan) {
            beginInsertRows(QModelIndex(), i, i);

            _data.insert(i, updatedComic);

            endInsertRows();

            i++;
            j++;
            length++;
            continue;
        }

        // comic removed
        if (lessThan) {
            beginRemoveRows(QModelIndex(), i, i);

            _data.removeAt(i);

            endRemoveRows();

            length--;
            continue;
        }
    }

    // add remaining comics
    for (; j < lengthUpdated; j++) {
        beginInsertRows(QModelIndex(), i, i);

        _data.append(updatedData.at(j));

        endInsertRows();

        i++;
    }

    // remove remaining comics
    for (int k = length - 1; k >= i; k--) {
        beginRemoveRows(QModelIndex(), k, k);

        _data.removeAt(k);

        endRemoveRows();
    }
}

void ComicModel::setMetadataFilter(const MetadataFilter &filter)
{
    // Keep the unfiltered source, including an existing search result or list order.
    if (!currentMetadataFilter.active())
        filterSource = _data;
    if (!currentMetadataFilter.active() && presentationSortColumn >= 0)
        std::stable_sort(filterSource.begin(), filterSource.end(), [this](const auto &a, const auto &b) {
            return sourceRowOrder.value(rowIdOf(a)) < sourceRowOrder.value(rowIdOf(b));
        });
    currentMetadataFilter = filter;
    const auto source = filterSource;
    beginResetModel();
    applyingMetadataFilter = true;
    takeData(source);
    applyingMetadataFilter = false;
    endResetModel();
    emit metadataFilterChanged();
}

QMap<int, QStringList> ComicModel::metadataFilterValues() const
{
    if (filterValuesCache)
        return *filterValuesCache;
    QMap<int, QSet<QString>> unique;
    if (_databasePath.isEmpty())
        return { };
    QString connection;
    {
        auto db = DataBaseManagement::loadDatabase(_databasePath);
        connection = db.connectionName();
        QSqlQuery query(db);
        if (query.exec("SELECT DISTINCT tags, work, writer FROM comic_info WHERE id IN (SELECT comicInfoId FROM comic)")) {
            while (query.next())
                for (int i = 0; i < 3; ++i)
                    for (const auto &value : MetadataFilter::values(query.value(i)))
                        unique[i].insert(value);
        }
    }
    QSqlDatabase::removeDatabase(connection);
    QMap<int, QStringList> result;
    for (int i = 0; i < 3; ++i) {
        result[i] = unique[i].values();
        result[i].sort(Qt::CaseInsensitive);
    }
    filterValuesCache = result;
    return result;
}

std::optional<ComicDB> ComicModel::getComic(const QModelIndex &mi)
{
    const auto *rowFile = rowComic(_data.at(mi.row()));
    if (rowFile == nullptr)
        return std::nullopt;

    ComicDB c;
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);
        bool found;
        c = DBHelper::loadComic(rowFile->file.id.value(), db, found);
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return c;
}

QVector<YACReaderComicReadStatus> ComicModel::getReadList()
{
    int numComics = _data.count();
    QVector<YACReaderComicReadStatus> readList(numComics);
    for (int i = 0; i < numComics; i++) {
        const auto *infoOfRow = rowInfo(_data.at(i));
        if (infoOfRow == nullptr) {
            readList[i] = YACReader::Unread;
            continue;
        }
        const auto &info = *infoOfRow;
        if (info.read)
            readList[i] = YACReader::Read;
        else if (info.currentPage == info.numPages.toInt())
            readList[i] = YACReader::Read;
        else if (info.hasBeenOpened)
            readList[i] = YACReader::Opened;
        else
            readList[i] = YACReader::Unread;
    }
    return readList;
}
QList<ComicDB> ComicModel::getComics(QList<QModelIndex> list)
{
    QList<ComicDB> comics;
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);

        QList<qulonglong> ids;
        ids.reserve(list.size());
        for (const auto &mi : std::as_const(list)) {
            if (const auto *comic = rowComic(_data.at(mi.row())))
                ids.append(comic->file.id.value());
        }
        comics = DBHelper::loadComics(ids, db);

        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    return comics;
}

std::optional<ComicDB> ComicModel::firstUnreadComic() const
{
    for (const auto &item : _data) {
        const auto *comic = rowComic(item);
        if (comic != nullptr && !comic->info.read) {
            return *comic;
        }
    }
    return std::nullopt;
}

ComicRecord ComicModel::record(const QModelIndex &index) const
{
    return std::visit(overloaded {
                              [](const ComicItemCase::Comic &row) -> ComicRecord { return ComicRecordCase::Comic { row.comic }; },
                              [](const ComicItemCase::ComicInList &row) -> ComicRecord { return ComicRecordCase::Comic { row.comic }; },
                              [](const ComicItemCase::DeletedComicInList &row) -> ComicRecord { return ComicRecordCase::DeletedComic { row.info, row.entry.lastKnownFile }; },
                              [](const ComicItemCase::PlaceholderInList &row) -> ComicRecord { return ComicRecordCase::Placeholder { row.placeholder }; },
                              [](const ComicItemCase::PlaceholderInFolder &row) -> ComicRecord { return ComicRecordCase::Placeholder { row.placeholder }; },
                      },
                      _data.at(index.row()));
}

std::optional<ListEntry> ComicModel::listEntry(const QModelIndex &index) const
{
    const auto *entry = index.isValid() ? rowEntry(_data.at(index.row())) : nullptr;
    return entry != nullptr ? std::optional(*entry) : std::nullopt;
}

bool ComicModel::hasUnavailableRows() const
{
    return std::any_of(_data.cbegin(), _data.cend(), [](const ComicItem &item) { return rowComic(item) == nullptr; });
}

bool ComicModel::supports(const QModelIndex &index, ComicAction action) const
{
    if (!index.isValid())
        return false;
    // Favorites and Reading remove by comic; Recent is read only
    if (action == ComicAction::RemoveFromList && (mode == Favorites || mode == Reading))
        return true;
    return supportsAction(_data.at(index.row()), action);
}

QModelIndexList ComicModel::supportedIndexes(const QModelIndexList &indexes, ComicAction action) const
{
    QModelIndexList supported;
    for (const auto &index : indexes) {
        if (supports(index, action))
            supported.append(index);
    }
    return supported;
}

void ComicModel::deleteMetadata(const QModelIndexList &indexes)
{
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);
        db.transaction();
        QSet<ComicInfoId> done;
        for (const auto &mi : indexes) {
            const auto *info = rowInfo(_data.at(mi.row()));
            if (info == nullptr)
                continue;
            const auto id = info->id;
            if (done.contains(id))
                continue;
            done.insert(id);
            if (auto info = DBHelper::loadComicInfo(id, db)) {
                info->deleteMetadata();
                DBHelper::update(&*info, db);
            }
        }
        db.commit();
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    reload();
}
// TODO
QVector<YACReaderComicReadStatus> ComicModel::setComicsRead(QList<QModelIndex> list, YACReaderComicReadStatus read)
{
    QString connectionName = "";
    const auto &comicIndexes = list;
    QSet<ComicInfoId> changed;
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);
        db.transaction();
        for (const auto &mi : comicIndexes) {
            const auto *rowComicInfo = rowInfo(_data.at(mi.row()));
            if (rowComicInfo == nullptr)
                continue;
            const auto id = rowComicInfo->id;
            if (read == YACReader::Read) {
                if (auto info = DBHelper::loadComicInfo(id, db)) {
                    info->read = true;
                    DBHelper::update(&*info, db);
                }
            }
            if (read == YACReader::Unread) {
                if (auto info = DBHelper::loadComicInfo(id, db)) {
                    info->read = false;
                    info->currentPage = 1;
                    info->hasBeenOpened = false;
                    info->lastTimeOpened.setValue(QVariant());
                    DBHelper::update(&*info, db);
                }
            }
            if (read == YACReader::Read || read == YACReader::Unread)
                changed.insert(id);
        }
        db.commit();
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    updateRowsOf(changed, [read](ComicInfo &info) {
        info.read = read == YACReader::Read;
        if (read == YACReader::Unread) {
            info.currentPage = 1;
            info.hasBeenOpened = false;
        } }, ComicModel::ReadColumn, ComicModel::HasBeenOpened, { ReadColumnRole, CurrentPageRole, HasBeenOpenedRole });

    return getReadList();
}

void ComicModel::setComicsType(QList<QModelIndex> list, FileType type)
{
    QString connectionName = "";
    const auto &comicIndexes = list;
    QSet<ComicInfoId> changed;
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);
        db.transaction();
        for (const auto &mi : comicIndexes) {
            const auto *rowComicInfo = rowInfo(_data.at(mi.row()));
            if (rowComicInfo == nullptr)
                continue;
            const auto id = rowComicInfo->id;
            if (auto info = DBHelper::loadComicInfo(id, db)) {
                info->type = QVariant::fromValue(type);
                DBHelper::update(&*info, db);
            }
            changed.insert(id);
        }
        db.commit();
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    updateRowsOf(changed, [type](ComicInfo &info) { info.type = QVariant::fromValue(type); }, ComicModel::Type, ComicModel::Type, { TypeRole });
}

void ComicModel::updateRowsOf(const QSet<ComicInfoId> &ids, const std::function<void(ComicInfo &)> &change, int firstColumn, int lastColumn, const QList<int> &roles)
{
    if (ids.isEmpty())
        return;
    QList<int> rows;
    for (int row = 0; row < _data.size(); row++) {
        auto *info = rowInfo(_data[row]);
        if (info == nullptr || !ids.contains(info->id))
            continue;
        change(*info);
        rows.append(row);
    }
    emitRowRuns(rows, firstColumn, lastColumn, roles);
    if (presentationSortColumn >= 0)
        reorderRows([this](const auto &a, const auto &b) { return tableLessThan(a, b); });
}

void ComicModel::emitRowRuns(const QList<int> &rows, int firstColumn, int lastColumn, const QList<int> &roles)
{
    for (qsizetype first = 0; first < rows.size();) {
        auto last = first;
        while (last + 1 < rows.size() && rows[last + 1] == rows[last] + 1)
            last++;
        emit dataChanged(index(rows[first], firstColumn), index(rows[last], lastColumn), roles);
        first = last + 1;
    }
}

// each comic_info or placeholder gets one number, at its first appearance in `list`
ComicRowId ComicModel::asignNumbers(QList<QModelIndex> list, int startingNumber)
{
    ComicRowId idFirst;
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);
        db.transaction();
        idFirst = rowIdOf(_data.at(list[0].row()));
        QSet<ComicInfoId> numberedComics;
        QSet<PlaceholderId> numberedPlaceholders;
        auto number = startingNumber;
        for (const auto &mi : std::as_const(list)) {
            const auto &item = _data.at(mi.row());
            DescriptiveMetadataPatch patch;
            patch.set(&DescriptiveComicMetadata::number, QString::number(number));
            if (const auto *placeholder = rowPlaceholder(item)) {
                if (numberedPlaceholders.contains(placeholder->id))
                    continue;
                numberedPlaceholders.insert(placeholder->id);
                DBHelper::updatePlaceholderMetadata(placeholder->id, patch, db);
            } else {
                const auto id = rowInfo(item)->id;
                if (numberedComics.contains(id))
                    continue;
                numberedComics.insert(id);
                DBHelper::updateComicInfoMetadata(id, patch, db);
            }
            number++;
        }
        db.commit();
        connectionName = db.connectionName();
    }

    QSqlDatabase::removeDatabase(connectionName);

    return idFirst;
}
QModelIndex ComicModel::getIndexFromId(quint64 id)
{
    int i = 0;
    for (const auto &item : std::as_const(_data)) {
        const auto *comic = rowComic(item);
        if (comic != nullptr && comic->file.id.value() == id)
            break;
        i++;
    }

    return index(i, 0);
}

ComicRowId ComicModel::rowId(const QModelIndex &index) const
{
    return rowIdOf(_data.at(index.row()));
}

QModelIndex ComicModel::indexOf(const ComicRowId &rowId) const
{
    for (int row = 0; row < _data.size(); row++) {
        if (rowIdOf(_data.at(row)) == rowId)
            return index(row, 0);
    }
    return { };
}

QModelIndexList ComicModel::indexesOf(const QList<ComicRowId> &rowIds) const
{
    QHash<ComicRowId, int> rows;
    rows.reserve(_data.size());
    for (int row = _data.size() - 1; row >= 0; row--)
        rows.insert(rowIdOf(_data.at(row)), row);

    QModelIndexList indexes;
    indexes.reserve(rowIds.size());
    for (const auto &rowId : rowIds) {
        const auto row = rows.find(rowId);
        indexes.append(row == rows.end() ? QModelIndex() : index(*row, 0));
    }
    return indexes;
}

// TODO completely inefficiently
QList<QModelIndex> ComicModel::getIndexesFromIds(const QList<qulonglong> &comicIds)
{
    QList<QModelIndex> comicsIndexes;

    for (const auto id : comicIds)
        comicsIndexes << getIndexFromId(id);

    return comicsIndexes;
}

void ComicModel::startTransaction()
{
    auto dbTransaction = DataBaseManagement::loadDatabase(_databasePath);
    _databaseConnection = dbTransaction.connectionName();
    dbTransaction.transaction();
}

void ComicModel::finishTransaction()
{
    {
        QSqlDatabase::database(_databaseConnection).commit();
    }
    QSqlDatabase::removeDatabase(_databaseConnection);
}

void ComicModel::removeInTransaction(int row)
{
    const auto *rowFile = rowComic(_data.at(row));
    if (rowFile == nullptr)
        return;

    auto dbTransaction = QSqlDatabase::database(_databaseConnection);
    bool found;
    ComicDB c = DBHelper::loadComic(rowFile->file.id.value(), dbTransaction, found);

    DBHelper::removeFromDB(&c, dbTransaction);
    beginRemoveRows(QModelIndex(), row, row);

    removeRow(row);
    _data.removeAt(row);

    endRemoveRows();
}

void ComicModel::reloadContinueReading()
{
    setupReadingModelData(_databasePath);
}

// The `comparator` passed to `takeUpdatedData` is used to determine if a row has been removed or added
void ComicModel::reload()
{
    switch (mode) {
    case Folder:
        takeUpdatedData(createFolderModelData(sourceId, _databasePath), ComicItems::folderLessThan);
        break;
    case Favorites:
        setupFavoritesModelData(_databasePath); // TODO we need a comparator
        break;
    case Reading:
        takeUpdatedData(createReadingModelData(_databasePath), [](const ComicItem &c1, const ComicItem &c2) {
            return rowInfo(c1)->lastTimeOpened.toLongLong() > rowInfo(c2)->lastTimeOpened.toLongLong();
        });
        break;
    case Recent:
        takeUpdatedData(createRecentModelData(_databasePath), [](const ComicItem &c1, const ComicItem &c2) {
            return rowInfo(c1)->added.toLongLong() > rowInfo(c2)->added.toLongLong();
        });
        break;
    case Label:
        setupLabelModelData(sourceId, _databasePath); // TODO we need a comparator
        break;
    case ReadingList:
        setupReadingListModelData(sourceId, _databasePath); // TODO we need a comparator
        break;
    case SearchResult:
        // TODO: reload search results, we don't have a way to recreate the query in this class
        break;
    case SmartList:
        takeUpdatedData(createSmartListModelData(sourceId, _databasePath), ComicItems::searchLessThan);
        break;
    }
}

void ComicModel::remove(int row)
{
    removeInTransaction(row);
}

void ComicModel::reload(const ComicDB &comic)
{
    updateRowsOf({ comic.info.id }, [&comic](ComicInfo &info) {
        info.read = comic.info.read;
        info.currentPage = comic.info.currentPage;
        info.hasBeenOpened = true; }, ReadColumn, HasBeenOpened, { ReadColumnRole, CurrentPageRole, HasBeenOpenedRole });
}

void ComicModel::resetComicRating(const QModelIndex &mi)
{
    const auto *info = rowInfo(_data.at(mi.row()));
    if (info == nullptr)
        return;
    const auto id = info->id;
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);

        if (auto info = DBHelper::loadComicInfo(id, db)) {
            info->rating = 0;
            DBHelper::update(&*info, db);
        }

        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    updateRowsOf({ id }, [](ComicInfo &info) { info.rating = 0; }, 0, columnCount() - 1, { });
}

void ComicModel::notifyCoverChange(const QList<ComicInfoId> &ids)
{
    // Keep cover changes non-structural. Removing and reinserting the row makes
    // views adjust their selection and scroll position before the edit refresh can
    // capture them. Changing the URL also makes QML reload the image even though
    // the underlying cover file path is unchanged.
    coverFileExistence.clear();
    for (const auto id : ids)
        ++coverRevisions[id.value()];
    updateRowsOf(QSet<ComicInfoId>(ids.cbegin(), ids.cend()), [](ComicInfo &) { }, 0, columnCount() - 1, { CoverPathRole });
}

void ComicModel::notifyPlaceholderCoverChange(const QHash<PlaceholderId, std::optional<QString>> &coverKeys)
{
    if (coverKeys.isEmpty())
        return;
    coverFileExistence.clear();
    QList<int> rows;
    for (int row = 0; row < _data.size(); row++) {
        auto *placeholder = rowPlaceholder(_data[row]);
        if (placeholder == nullptr)
            continue;
        const auto key = coverKeys.constFind(placeholder->id);
        if (key == coverKeys.cend())
            continue;
        placeholder->coverKey = *key;
        rows.append(row);
    }
    emitRowRuns(rows, 0, columnCount() - 1, { CoverPathRole });
}

bool ComicModel::coverFileExists(const QString &path) const
{
    auto known = coverFileExistence.constFind(path);
    if (known == coverFileExistence.cend())
        known = coverFileExistence.insert(path, QFileInfo::exists(path));
    return *known;
}

QUrl ComicModel::getCoverUrlPathForComicHash(const QString &hash) const
{
    auto coverPath = LibraryPaths::coverPathFromLibraryDataPath(_databasePath, hash);
    return QUrl::fromLocalFile(coverPath);
}

void ComicModel::addComicsToFavorites(const QList<qulonglong> &comicIds)
{
    addComicsToFavorites(getIndexesFromIds(comicIds));
}

void ComicModel::addComicsToFavorites(const QList<QModelIndex> &comicsList)
{
    QList<ComicDB> comics = getComics(comicsList);
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);

        DBHelper::insertComicsInFavorites(comics, db);
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    QSet<ComicInfoId> changedInfo;
    for (const auto &comic : comics)
        changedInfo.insert(comic.info.id);
    updateRowsOf(changedInfo, [](ComicInfo &info) { info.favorite = true; }, Favorite, Favorite, { FavoriteRole, Qt::DisplayRole });
    QList<qulonglong> comicIds;
    comicIds.reserve(comics.size());
    for (const auto &comic : std::as_const(comics))
        comicIds.append(comic.file.id.value());
    emit favoritesChanged(comicIds);
}

void ComicModel::addComicsToLabel(const QList<QModelIndex> &comicsList, qulonglong labelId)
{
    QList<ComicRowId> rows;
    rows.reserve(comicsList.size());
    for (const auto &index : comicsList)
        rows.append(rowIdOf(_data.at(index.row())));

    addComicsToLabel(rows, labelId);
}

void ComicModel::addComicsToLabel(const QList<ComicRowId> &rows, qulonglong labelId)
{
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);

        DBHelper::appendRowsToLabel(LabelId(labelId), rows, db);
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ComicModel::addComicsToReadingList(const QList<QModelIndex> &comicsList, qulonglong readingListId)
{
    QList<ComicRowId> rows;
    rows.reserve(comicsList.size());
    for (const auto &index : comicsList)
        rows.append(rowIdOf(_data.at(index.row())));

    addComicsToReadingList(rows, readingListId);
}

void ComicModel::addComicsToReadingList(const QList<ComicRowId> &rows, qulonglong readingListId)
{
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);

        DBHelper::appendRowsToReadingList(ReadingListId(readingListId), rows, db);
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

void ComicModel::deleteComicsFromFavorites(const QList<QModelIndex> &comicsList)
{
    QList<ComicDB> comics = getComics(comicsList);
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);

        DBHelper::deleteComicsFromFavorites(comics, db);
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    QSet<ComicInfoId> changedInfo;
    for (const auto &comic : comics)
        changedInfo.insert(comic.info.id);
    updateRowsOf(changedInfo, [](ComicInfo &info) { info.favorite = false; }, Favorite, Favorite, { FavoriteRole, Qt::DisplayRole });
    QList<qulonglong> comicIds;
    comicIds.reserve(comics.size());
    for (const auto &comic : std::as_const(comics))
        comicIds.append(comic.file.id.value());
    emit favoritesChanged(comicIds);

    if (mode == Favorites)
        reload();
    emit metadataFilterChanged();
}

void ComicModel::deleteComicsFromReading(const QList<QModelIndex> &comicsList)
{
    QList<ComicDB> comics = getComics(comicsList);
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);

        DBHelper::deleteComicsFromReading(comics, db);
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    if (mode == Reading)
        deleteComicsFromModel(comicsList);
}

void ComicModel::deleteComicsFromSpecialList(const QList<QModelIndex> &comicsList, qulonglong specialListId)
{
    auto type = (ReadingListModel::TypeSpecialList)specialListId;

    switch (type) {
    case ReadingListModel::TypeSpecialList::Reading:
        deleteComicsFromReading(comicsList);
        break;
    case ReadingListModel::TypeSpecialList::Favorites:
        deleteComicsFromFavorites(comicsList);
        break;
    case ReadingListModel::TypeSpecialList::Recent:
        // do nothing, recent is read only
        break;
    }
}

// a reading list also shows the rows of its child lists, so each row is removed from the list that holds it
void ComicModel::deleteComicsFromLists(const QList<QModelIndex> &comicsList)
{
    QSet<int> rows;
    QMap<std::pair<ListKind, qulonglong>, QList<ListItemId>> entryIdsByList;
    for (const auto &index : comicsList) {
        if (const auto *entry = rowEntry(_data.at(index.row())); entry != nullptr && !rows.contains(index.row())) {
            rows.insert(index.row());
            entryIdsByList[{ entry->id.listKind, entry->listId }].append(entry->id);
        }
    }

    bool removed = true;
    QStringList placeholderCovers;
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);

        for (auto it = entryIdsByList.cbegin(); it != entryIdsByList.cend(); ++it) {
            const auto [listKind, listId] = it.key();
            std::optional<QStringList> covers;
            switch (listKind) {
            case ListKind::ReadingList:
                covers = DBHelper::removeReadingListEntries(ReadingListId(listId), it.value(), db);
                break;
            case ListKind::Label:
                covers = DBHelper::removeLabelEntries(LabelId(listId), it.value(), db);
                break;
            }
            if (covers)
                placeholderCovers.append(*covers);
            else
                removed = false;
        }
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);
    DBHelper::removePlaceholderCovers(_databasePath, placeholderCovers);

    if (!removed) {
        reload();
        return;
    }

    QList<int> sortedRows(rows.begin(), rows.end());
    std::sort(sortedRows.begin(), sortedRows.end());
    QList<QModelIndex> removedRows;
    for (const auto row : std::as_const(sortedRows))
        removedRows.append(index(row, 0));
    deleteComicsFromModel(removedRows);
}

void ComicModel::removeMissingComicsFromFolder(const QList<QModelIndex> &comicsList)
{
    QSet<int> rows;
    QMap<FolderId, QList<PlaceholderId>> placeholderIdsByFolder;
    for (const auto &index : comicsList) {
        if (const auto *row = std::get_if<ComicItemCase::PlaceholderInFolder>(&_data.at(index.row())); row != nullptr && !rows.contains(index.row())) {
            rows.insert(index.row());
            placeholderIdsByFolder[row->folderId].append(row->placeholder.id);
        }
    }

    bool removed = true;
    QStringList placeholderCovers;
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);
        for (auto it = placeholderIdsByFolder.cbegin(); it != placeholderIdsByFolder.cend(); ++it) {
            if (const auto covers = DBHelper::removePlaceholdersFromFolder(it.value(), it.key(), db))
                placeholderCovers.append(*covers);
            else
                removed = false;
        }
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);
    DBHelper::removePlaceholderCovers(_databasePath, placeholderCovers);

    if (!removed) {
        reload();
        return;
    }

    QList<int> sortedRows(rows.begin(), rows.end());
    std::sort(sortedRows.begin(), sortedRows.end());
    QList<QModelIndex> removedRows;
    for (const auto row : std::as_const(sortedRows))
        removedRows.append(index(row, 0));
    deleteComicsFromModel(removedRows);
}

// one removal for each run of consecutive rows, from the last run to the first
void ComicModel::deleteComicsFromModel(const QList<QModelIndex> &comicsList)
{
    QList<int> rows;
    rows.reserve(comicsList.size());
    for (const auto &index : comicsList)
        rows.append(index.row());
    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

    for (auto last = rows.size() - 1; last >= 0;) {
        auto first = last;
        while (first > 0 && rows.at(first - 1) == rows.at(first) - 1)
            first--;
        beginRemoveRows(QModelIndex(), rows.at(first), rows.at(last));
        _data.remove(rows.at(first), rows.at(last) - rows.at(first) + 1);
        endRemoveRows();
        last = first - 1;
    }

    if (_data.isEmpty())
        emit isEmpty();
}

bool ComicModel::isFavorite(const QModelIndex &index)
{
    return index.isValid() && rowMetadata(_data.at(index.row())).favorite.toBool();
}

bool ComicModel::setFavorite(const QModelIndex &index, bool favorite)
{
    if (!index.isValid() || index.row() >= _data.size())
        return false;
    const auto record = _data.at(index.row());
    DescriptiveMetadataPatch patch;
    patch.set(&DescriptiveComicMetadata::favorite, favorite);
    QString connection;
    bool saved = false;
    {
        auto db = DataBaseManagement::loadDatabase(_databasePath);
        connection = db.connectionName();
        if (const auto *info = rowInfo(record))
            saved = DBHelper::updateComicInfoMetadata(info->id, patch, db);
        else if (const auto *placeholder = rowPlaceholder(record))
            saved = DBHelper::updatePlaceholderMetadata(placeholder->id, patch, db);
    }
    QSqlDatabase::removeDatabase(connection);
    if (!saved)
        return false;
    if (const auto *info = rowInfo(record))
        updateRowsOf({ info->id }, [favorite](ComicInfo &info) { info.favorite = favorite; }, Favorite, Favorite, { FavoriteRole, Qt::DisplayRole });
    else {
        for (int row = 0; row < _data.size(); ++row)
            if (auto *placeholder = rowPlaceholder(_data[row]); placeholder && placeholder->id == rowPlaceholder(record)->id) {
                placeholder->favorite = favorite;
                emit dataChanged(this->index(row, Favorite), this->index(row, Favorite));
            }
    }
    QList<qulonglong> fileIds;
    for (const auto &item : _data)
        if (const auto *comic = rowComic(item))
            fileIds.append(comic->file.id.value());
    emit favoritesChanged(fileIds);
    if (mode == Favorites || currentMetadataFilter.favoritesOnly)
        QTimer::singleShot(0, this, [this] {
            if (mode == Favorites) {
                reload();
                emit metadataFilterChanged();
            } else if (currentMetadataFilter.favoritesOnly)
                setMetadataFilter(currentMetadataFilter);
        });
    return true;
}

void ComicModel::setShowRecent(bool showRecent)
{
    if (this->showRecent == showRecent)
        return;

    this->showRecent = showRecent;

    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), { ComicModel::ShowRecentRole });
}

void ComicModel::setRecentRange(int days)
{
    if (this->recentDays == days)
        return;

    this->recentDays = days;

    emit dataChanged(index(0, 0), index(rowCount() - 1, 0), { ComicModel::RecentRangeRole });

    if (mode == ComicModel::Recent)
        reload();
}

void ComicModel::updateRating(int rating, QModelIndex mi)
{
    const auto *info = rowInfo(_data.at(mi.row()));
    if (info == nullptr)
        return;
    const auto id = info->id;
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(_databasePath);
        // TODO optimize update

        if (auto info = DBHelper::loadComicInfo(id, db)) {
            info->rating = rating;
            DBHelper::update(&*info, db);
        }

        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    updateRowsOf({ id }, [rating](ComicInfo &info) { info.rating = rating; }, 0, columnCount() - 1, { });
    // Let the star editor finish committing before the row can disappear.
    if (!currentMetadataFilter.ratings.isEmpty())
        QTimer::singleShot(0, this, [this] {
            if (!currentMetadataFilter.ratings.isEmpty())
                setMetadataFilter(currentMetadataFilter);
        });
}

QString ComicModel::localizedDate(const QString &dbDate) const
{
    auto dateComponents = dbDate.split("/");

    if (dateComponents.length() == 3) {
        auto dayString = dateComponents[0];
        auto monthString = dateComponents[1];
        auto yearString = dateComponents[2];

        auto hasDay = !dayString.isEmpty();
        auto hasMonth = !monthString.isEmpty();
        auto hasYear = !yearString.isEmpty();

        auto day = hasDay ? dayString.toInt() : 1;
        auto month = hasMonth ? monthString.toInt() : 1;
        auto year = hasYear ? yearString.toInt() : 1;

        auto locale = QLocale();
        auto date = QDate(year, month, day);
        auto dateTime = QDateTime();
        dateTime.setDate(date);

        if (hasDay && hasMonth && hasYear) {
            return locale.toString(dateTime, locale.dateFormat(QLocale::ShortFormat));
        }
    }

    return dbDate; // TODO
}

QList<int> ComicModel::visibleColumns()
{
    return { Favorite, Title, FileName, Author, Work, Characters, Tags, Color, Censorship, Translation, CurrentPage, ReadColumn, Rating, Size };
}

bool ComicModel::isSortableColumn(int column)
{
    return visibleColumns().contains(column) && column != Characters && column != Tags;
}

void ComicModel::rememberSourceOrder(const QList<ComicItem> &items)
{
    sourceRowOrder.clear();
    for (int i = 0; i < items.size(); ++i)
        sourceRowOrder.insert(rowIdOf(items.at(i)), i);
}

bool ComicModel::tableLessThan(const ComicItem &a, const ComicItem &b) const
{
    const auto &left = rowMetadata(a);
    const auto &right = rowMetadata(b);
    const int column = presentationSortColumn;
    if (column == Author) {
        const auto anthology = [](const auto &metadata) {
            return metadata.isAnthology.isNull() ? metadata.writer.toString().split('\n', Qt::SkipEmptyParts).size() > 1 : metadata.isAnthology.toBool();
        };
        if (anthology(left) != anthology(right))
            return !anthology(left);
    }
    int comparison = 0;
    if (column == CurrentPage) {
        const auto *one = rowInfo(a);
        const auto *two = rowInfo(b);
        const bool knownOne = one && one->numPages.toInt() > 0;
        const bool knownTwo = two && two->numPages.toInt() > 0;
        if (knownOne != knownTwo)
            return knownOne;
        if (knownOne) {
            const qint64 first = qint64(qBound(0, one->currentPage, one->numPages.toInt())) * two->numPages.toInt();
            const qint64 second = qint64(qBound(0, two->currentPage, two->numPages.toInt())) * one->numPages.toInt();
            comparison = first < second ? -1 : first > second ? 1
                                                              : 0;
        }
    } else if (column == Size) {
        const auto bytes = [](const auto &item) { return columnValue(item, Hash).toString().mid(40).toULongLong(); };
        comparison = bytes(a) < bytes(b) ? -1 : bytes(a) > bytes(b) ? 1
                                                                    : 0;
    } else if (column == Rating || column == ReadColumn || column == Color) {
        const auto number = [column](const auto &item) {
            const auto value = columnValue(item, column);
            if (column == ReadColumn) {
                const auto info = rowInfo(item);
                return info && (info->read || (info->numPages.toInt() > 0 && info->currentPage >= info->numPages.toInt())) ? 1 : 0;
            }
            return column == Color ? value.isNull() ? 0 : value.toBool() ? 2
                                                                         : 1
                                   : value.toInt();
        };
        comparison = number(a) < number(b) ? -1 : number(a) > number(b) ? 1
                                                                        : 0;
    } else {
        const auto first = columnValue(a, column).toString();
        const auto second = columnValue(b, column).toString();
        comparison = naturalSortLessThanCI(first, second) ? -1 : naturalSortLessThanCI(second, first) ? 1
                                                                                                      : 0;
    }
    if (comparison == 0)
        return sourceRowOrder.value(rowIdOf(a)) < sourceRowOrder.value(rowIdOf(b));
    return presentationSortOrder == Qt::AscendingOrder ? comparison < 0 : comparison > 0;
}

void ComicModel::reorderRows(const std::function<bool(const ComicItem &, const ComicItem &)> &lessThan)
{
    QList<int> order(_data.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [this, &lessThan](int a, int b) { return lessThan(_data.at(a), _data.at(b)); });
    bool changed = false;
    for (int i = 0; i < order.size(); ++i)
        changed = changed || order.at(i) != i;
    if (!changed)
        return;
    emit layoutAboutToBeChanged({ }, QAbstractItemModel::VerticalSortHint);
    const auto previous = persistentIndexList();
    QList<int> inverse(order.size());
    QList<ComicItem> sorted;
    sorted.reserve(_data.size());
    for (int i = 0; i < order.size(); ++i) {
        inverse[order.at(i)] = i;
        sorted.append(_data.at(order.at(i)));
    }
    _data.swap(sorted);
    QModelIndexList next;
    for (const auto &old : previous)
        next.append(index(inverse.at(old.row()), old.column()));
    changePersistentIndexList(previous, next);
    emit layoutChanged({ }, QAbstractItemModel::VerticalSortHint);
    emit resortedIndexes(order);
}

void ComicModel::sort(int column, Qt::SortOrder order)
{
    if (column != -1 && !isSortableColumn(column))
        return;
    presentationSortColumn = column;
    presentationSortOrder = order;
    if (column < 0) {
        reorderRows([this](const auto &a, const auto &b) { return sourceRowOrder.value(rowIdOf(a)) < sourceRowOrder.value(rowIdOf(b)); });
    } else {
        reorderRows([this](const auto &a, const auto &b) { return tableLessThan(a, b); });
    }
}
