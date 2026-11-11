#include "comic_item.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "metadata_filter.h"
#include "smart_list.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTest>

class SmartListTest : public QObject
{
    Q_OBJECT
private slots:
    void conventionFiltersPersistCombineAndRecompute();
    void seriesFiltersRecompute();
    void init();
    void cleanup();
    void insertedListsGoToTheEnd();
    void updateChangesTheNameAndTheQuery();
    void removeDeletesTheList();
    void metadataFiltersPersistAndRecomputeWhileSnapshotsStayFixed();
    void failedSnapshotLeavesNoPartialList();
    void folderScopeIsPersistentAndNeverFallsBackToLibrary();

private:
    bool exec(const QString &sql);

    QSqlDatabase db;
};

void SmartListTest::init()
{
    db = QSqlDatabase::addDatabase("QSQLITE", "smart_list_test");
    db.setDatabaseName(":memory:");
    QVERIFY(db.open());
    QVERIFY(DataBaseManagement::createTables(db));
}

void SmartListTest::cleanup()
{
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("smart_list_test");
}

bool SmartListTest::exec(const QString &sql)
{
    QSqlQuery query(db);
    return query.exec(sql);
}

void SmartListTest::insertedListsGoToTheEnd()
{
    const auto unread = DBHelper::insertSmartList("Unread", "read:false", db);
    const auto rated = DBHelper::insertSmartList("Highly rated", "rating>=4", db);
    QVERIFY(unread);
    QVERIFY(rated);

    const auto lists = DBHelper::loadSmartLists(db);
    QVERIFY(lists);
    QCOMPARE(lists->size(), 2);
    QCOMPARE(lists->at(0).id, *unread);
    QCOMPARE(lists->at(0).name, QString("Unread"));
    QCOMPARE(lists->at(0).query, QString("read:false"));
    QCOMPARE(lists->at(0).ordering, 0);
    QCOMPARE(lists->at(1).id, *rated);
    QCOMPARE(lists->at(1).ordering, 1);

    // the order is the ordering column, not the id
    QVERIFY(exec(QString("UPDATE smart_list SET ordering = 5 WHERE id = %1").arg(unread->value())));
    QCOMPARE(DBHelper::loadSmartLists(db)->constFirst().id, *rated);
}

void SmartListTest::updateChangesTheNameAndTheQuery()
{
    const auto id = DBHelper::insertSmartList("Unread", "read:false", db);
    QVERIFY(id);
    QVERIFY(DBHelper::updateSmartList(*id, "Unread Batman", "series:batman read:false", db));
    const auto list = DBHelper::loadSmartList(*id, db);
    QVERIFY(list);
    QCOMPARE(list->name, QString("Unread Batman"));
    QCOMPARE(list->query, QString("series:batman read:false"));
    QCOMPARE(list->ordering, 0);

    QVERIFY(!DBHelper::updateSmartList(SmartListId(99), "Gone", "read:true", db));
    QVERIFY(!DBHelper::loadSmartList(SmartListId(99), db));
}

void SmartListTest::removeDeletesTheList()
{
    const auto id = DBHelper::insertSmartList("Unread", "read:false", db);
    QVERIFY(id);
    QVERIFY(DBHelper::removeSmartList(*id, db));
    QVERIFY(DBHelper::loadSmartLists(db)->isEmpty());
    QVERIFY(!DBHelper::removeSmartList(*id, db));
}

void SmartListTest::metadataFiltersPersistAndRecomputeWhileSnapshotsStayFixed()
{
    QVERIFY(exec("INSERT INTO folder(id,parentId,name,path) VALUES(1,1,'root','/')"));
    QVERIFY(exec("INSERT INTO comic_info(id,hash,title,writer,work,tags,favorite,rating,color,censorship,translationStatus,publicationType,read) VALUES "
                 "(1,'a','A','Alice','Work','alpha',1,5,0,'uncensored','translated','doujinshi',0),"
                 "(2,'b','B','Alice','Work','alpha',0,4,0,'uncensored','translated','doujinshi',0),"
                 "(3,'c','C','Bob','Other','beta',1,3,1,'censored','original','magazine',0)"));
    QVERIFY(exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,1,1,'a.cbz','/a.cbz'),(2,1,2,'b.cbz','/b.cbz'),(3,1,3,'c.cbz','/c.cbz')"));
    MetadataFilter filter;
    filter.tags = { "alpha" };
    filter.works = { "Work" };
    filter.authors = { "Alice" };
    filter.ratings = { 4, 5 };
    filter.matchAll = false;
    filter.favoritesOnly = true;
    filter.color = "bw";
    filter.censorship = "uncensored";
    filter.translation = "translated";
    filter.publicationType = "doujinshi";
    const auto query = filter.savedQuery();
    const auto decoded = MetadataFilter::fromSavedQuery(query);
    QVERIFY(decoded);
    QCOMPARE(decoded->savedQuery(), query);
    const auto smart = DBHelper::insertSmartList("Dynamic", query, db);
    QVERIFY(smart);
    QCOMPARE(DBHelper::loadSmartList(*smart, db)->query, query);
    auto result = ComicItems::loadSmartList(*smart, db);
    QVERIFY(result);
    QCOMPARE(result->size(), 1);
    QCOMPARE(rowComic(result->first())->file.id, ComicId(1));
    const auto snapshot = DBHelper::saveReadingListSnapshot("Static", { ComicId(3), ComicId(1) }, db);
    QVERIFY(snapshot);
    auto fixed = ComicItems::loadReadingList(*snapshot, db);
    QCOMPARE(fixed.size(), 2);
    QCOMPARE(rowComic(fixed.at(0))->file.id, ComicId(3));
    QCOMPARE(rowComic(fixed.at(1))->file.id, ComicId(1));
    QVERIFY(exec("UPDATE comic_info SET favorite = CASE WHEN id = 2 THEN 1 ELSE 0 END"));
    result = ComicItems::loadSmartList(*smart, db);
    QVERIFY(result);
    QCOMPARE(result->size(), 1);
    QCOMPARE(rowComic(result->first())->file.id, ComicId(2));
    fixed = ComicItems::loadReadingList(*snapshot, db);
    QCOMPARE(fixed.size(), 2);
    QCOMPARE(rowComic(fixed.at(0))->file.id, ComicId(3));
    QCOMPARE(rowComic(fixed.at(1))->file.id, ComicId(1));
    // Existing syntax-based smart lists remain readable.
    const auto legacy = DBHelper::insertSmartList("Legacy", "read:false", db);
    QVERIFY(legacy);
    QCOMPARE(ComicItems::loadSmartList(*legacy, db)->size(), 3);
    const auto invalid = DBHelper::insertSmartList("Invalid", MetadataFilter::savedPrefix() + "{}", db);
    QVERIFY(invalid);
    QVERIFY(ComicItems::loadSmartList(*invalid, db)->isEmpty());
}

void SmartListTest::failedSnapshotLeavesNoPartialList()
{
    QVERIFY(exec("INSERT INTO folder(id,parentId,name,path) VALUES(1,1,'root','/')"));
    QVERIFY(exec("INSERT INTO comic_info(id,hash) VALUES(1,'a')"));
    QVERIFY(exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,1,1,'a.cbz','/a.cbz')"));
    QVERIFY(!DBHelper::saveReadingListSnapshot("Broken", { ComicId(1), ComicId(99) }, db));
    QSqlQuery query(db);
    QVERIFY(query.exec("SELECT (SELECT COUNT(*) FROM reading_list), (SELECT COUNT(*) FROM reading_list_entry)"));
    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 0);
    QCOMPARE(query.value(1).toInt(), 0);
    QVERIFY(!DBHelper::saveReadingListSnapshot("Empty", { }, db));
}

void SmartListTest::folderScopeIsPersistentAndNeverFallsBackToLibrary()
{
    QVERIFY(exec("INSERT INTO folder(id,parentId,name,path) VALUES(1,1,'root','/'),(2,1,'A','/A'),(3,1,'AB','/AB'),(4,2,'Child','/A/Child')"));
    QVERIFY(exec("INSERT INTO comic_info(id,hash,tags) VALUES(1,'a','x'),(2,'b','x'),(3,'c','x')"));
    QVERIFY(exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,2,1,'a.cbz','/A/a.cbz'),(2,3,2,'b.cbz','/AB/b.cbz'),(3,4,3,'c.cbz','/A/Child/c.cbz')"));
    MetadataFilter filter;
    filter.tags = { "x" };
    const auto scoped = DBHelper::insertSmartList("Folder", filter.savedQuery(2), db);
    const auto full = DBHelper::insertSmartList("Library", filter.savedQuery(), db);
    QVERIFY(scoped && full);
    QCOMPARE(MetadataFilter::savedFolderId(DBHelper::loadSmartList(*scoped, db)->query), std::optional<qulonglong>(2));
    auto result = ComicItems::loadSmartList(*scoped, db);
    QVERIFY(result);
    QCOMPARE(result->size(), 1);
    QCOMPARE(rowComic(result->first())->file.id, ComicId(1));
    QCOMPARE(ComicItems::loadSmartList(*full, db)->size(), 3);
    auto oldQuery = filter.savedQuery();
    oldQuery.replace(":v3:", ":v1:");
    const auto old = DBHelper::insertSmartList("Version one", oldQuery, db);
    QVERIFY(old);
    QCOMPARE(ComicItems::loadSmartList(*old, db)->size(), 3);
    // The folder identity survives renaming, and new matching members are included.
    QVERIFY(exec("UPDATE folder SET name = 'Renamed', path = '/Renamed' WHERE id = 2"));
    QVERIFY(exec("INSERT INTO comic_info(id,hash,tags) VALUES(4,'d','x')"));
    QVERIFY(exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(4,2,4,'d.cbz','/Renamed/d.cbz')"));
    QCOMPARE(ComicItems::loadSmartList(*scoped, db)->size(), 2);
    QVERIFY(exec("UPDATE comic_info SET tags = 'y' WHERE id = 1"));
    QCOMPARE(ComicItems::loadSmartList(*scoped, db)->size(), 1);
    QVERIFY(exec("UPDATE comic SET parentId = 1 WHERE parentId = 2"));
    QVERIFY(exec("DELETE FROM folder WHERE id = 2"));
    QVERIFY(ComicItems::loadSmartList(*scoped, db)->isEmpty());
    QVERIFY(!ComicItems::loadSmartList(*full, db)->isEmpty());
}

void SmartListTest::conventionFiltersPersistCombineAndRecompute()
{
    QVERIFY(exec("INSERT INTO folder(id,parentId,name,path) VALUES(1,1,'root','/')"));
    QVERIFY(exec("INSERT INTO comic_info(id,hash,convention,tags,rating) VALUES(1,'a','C107','x',5),(2,'b','C106','x',4),(3,'c',NULL,'y',5)"));
    QVERIFY(exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,1,1,'a.cbz','/a.cbz'),(2,1,2,'b.cbz','/b.cbz'),(3,1,3,'c.cbz','/c.cbz')"));
    MetadataFilter filter;
    filter.conventions = { "C107", "C106" };
    filter.matchAll = false;
    filter.ratings = { 5 };
    auto restored = MetadataFilter::fromSavedQuery(filter.savedQuery());
    QVERIFY(restored);
    QCOMPARE(restored->conventions, filter.conventions);
    const auto list = DBHelper::insertSmartList("Events", filter.savedQuery(), db);
    QVERIFY(list);
    auto result = ComicItems::loadSmartList(*list, db);
    QCOMPARE(result->size(), 1);
    QCOMPARE(rowComic(result->first())->file.id, ComicId(1));
    QVERIFY(exec("UPDATE comic_info SET convention='C107' WHERE id=3"));
    QCOMPARE(ComicItems::loadSmartList(*list, db)->size(), 2);
    filter.conventions = { "C107" };
    filter.tags = { "x" };
    filter.matchAll = true;
    DescriptiveComicMetadata metadata;
    metadata.convention = "C107";
    metadata.tags = "y";
    QVERIFY(!filter.matches(metadata, 5));
    metadata.tags = "x";
    QVERIFY(filter.matches(metadata, 5));
    QVERIFY(!filter.matches(metadata, 4));
    for (const auto &version : { "v1", "v2" }) {
        auto old = filter.savedQuery();
        old.replace(":v3:", ':' + QString(version) + ':');
        old.replace("\"conventions\":[\"C107\"],", "");
        const auto legacy = MetadataFilter::fromSavedQuery(old);
        QVERIFY(legacy);
        QVERIFY(legacy->conventions.isEmpty());
    }
}

void SmartListTest::seriesFiltersRecompute()
{
    QVERIFY(exec("INSERT INTO folder(id,parentId,name,path) VALUES(1,1,'root','/')"));
    QVERIFY(exec("INSERT INTO comic_info(id,hash,seriesName,tags,rating) VALUES(1,'a','Series A','x',5),(2,'b','Series B','x',5),(3,'c',NULL,'y',4)"));
    QVERIFY(exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,1,1,'a.cbz','/a.cbz'),(2,1,2,'b.cbz','/b.cbz'),(3,1,3,'c.cbz','/c.cbz')"));
    MetadataFilter filter;
    filter.seriesNames = { "Series A" };
    filter.tags = { "x" };
    filter.ratings = { 5 };
    const auto restored = MetadataFilter::fromSavedQuery(filter.savedQuery());
    QVERIFY(restored);
    QCOMPARE(restored->seriesNames, filter.seriesNames);
    const auto list = DBHelper::insertSmartList("Series", filter.savedQuery(), db);
    QVERIFY(list);
    QCOMPARE(ComicItems::loadSmartList(*list, db)->size(), 1);
    QVERIFY(exec("UPDATE comic_info SET seriesName='Series A' WHERE id=2"));
    QCOMPARE(ComicItems::loadSmartList(*list, db)->size(), 2);
    QVERIFY(exec("UPDATE comic_info SET seriesName='Series A' WHERE id=3"));
    QCOMPARE(ComicItems::loadSmartList(*list, db)->size(), 2);
}

QTEST_GUILESS_MAIN(SmartListTest)

#include "main.moc"
