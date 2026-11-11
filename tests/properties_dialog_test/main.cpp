#include "comic_metadata_editor.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "metadata_picker.h"
#include "metadata_value_chips.h"
#include "properties_dialog.h"
#include "yacreader_global.h"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QToolButton>

class PropertiesDialogTest : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void unchangedSavePreservesOldAndNewMetadata();
    void batchSaveAndSequentialSavePreserveUneditedFields();
    void confirmedRenameUpdatesFileAndDatabaseTogether();
    void customCoverPersistsWithoutChangingMetadata();
    void vocabularyIsScopedToItsLibrary();
    void ratingIsDeferredAndAtomic();
    void renameDoesNotSaveMetadataDraft();

private:
    ComicRecord load(int id);
    void prepare(PropertiesDialog &dialog);
    QTemporaryDir temporary;
    QString root;
    QString data;
};

void PropertiesDialogTest::init()
{
    QVERIFY(temporary.isValid());
    root = temporary.filePath(QString::fromLatin1(QTest::currentTestFunction()));
    data = YACReader::LibraryPaths::libraryDataPath(root);
    QVERIFY(QDir().mkpath(YACReader::LibraryPaths::libraryCoversFolderPath(root)));
    QString connection;
    {
        auto db = DataBaseManagement::createDatabase(YACReader::LibraryPaths::libraryDatabasePath(root));
        connection = db.connectionName();
        QVERIFY(db.isOpen());
        QSqlQuery query(db);
        for (int id = 1; id <= 2; ++id) {
            const auto hash = QString(40, QChar('0')) + QString::number(id);
            query.prepare("INSERT INTO comic_info(id,hash,title,writer,tags,favorite,publicationType,publisher,series,date,numPages,coverPage,type) "
                          "VALUES(:id,:hash,'Title',:author,'Tag',1,'doujinshi','Old publisher','Old series','1/2/2020',2,1,1)");
            query.bindValue(":id", id);
            query.bindValue(":hash", hash);
            query.bindValue(":author", QString("Author %1").arg(id));
            QVERIFY(query.exec());
            const auto name = QString("%1.cbz").arg(id);
            query.prepare("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(:id,1,:id,:name,:path)");
            query.bindValue(":id", id);
            query.bindValue(":name", name);
            query.bindValue(":path", "/" + name);
            QVERIFY(query.exec());
            QFile file(QDir(root).filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write("archive bytes remain unchanged"), qint64(30));
            QImage image(60, 80, QImage::Format_RGB32);
            image.fill(Qt::blue);
            QVERIFY(image.save(YACReader::LibraryPaths::coverPath(root, hash)));
        }
    }
    QSqlDatabase::removeDatabase(connection);
}

void PropertiesDialogTest::cleanup()
{
    // Each dialog operation owns and removes its database connection.
    QVERIFY(QDir(root).removeRecursively());
}

ComicRecord PropertiesDialogTest::load(int id)
{
    ComicDB comic;
    QString connection;
    bool found = false;
    {
        auto db = DataBaseManagement::loadDatabase(data);
        connection = db.connectionName();
        comic = DBHelper::loadComic(id, db, found);
    }
    QSqlDatabase::removeDatabase(connection);
    Q_ASSERT(found);
    return ComicRecordCase::Comic { comic };
}

void PropertiesDialogTest::prepare(PropertiesDialog &dialog)
{
    dialog.basePath = root;
    dialog.databasePath = data;
}

void PropertiesDialogTest::unchangedSavePreservesOldAndNewMetadata()
{
    const auto original = load(1);
    PropertiesDialog dialog;
    prepare(dialog);
    dialog.setComics({ original });
    QVERIFY(dialog.findChildren<QTabWidget *>().isEmpty());
    dialog.saveAndClose();
    const auto saved = load(1);
    const auto &before = recordMetadata(original);
    const auto &after = recordMetadata(saved);
    QCOMPARE(after.title, before.title);
    QCOMPARE(after.writer, before.writer);
    QCOMPARE(after.tags, before.tags);
    QCOMPARE(after.favorite, before.favorite);
    QCOMPARE(after.publicationType, before.publicationType);
    QCOMPARE(after.publisher, before.publisher);
    QCOMPARE(after.series, before.series);
    QCOMPARE(after.date, before.date);
}

void PropertiesDialogTest::batchSaveAndSequentialSavePreserveUneditedFields()
{
    PropertiesDialog dialog;
    prepare(dialog);
    dialog.setComics({ load(1), load(2) });
    dialog.findChild<QLineEdit *>("title")->setText("Batch title");
    dialog.saveAndClose();
    for (int id = 1; id <= 2; ++id) {
        const auto metadata = recordMetadata(load(id));
        QCOMPARE(metadata.title.toString(), QString("Batch title"));
        QCOMPARE(metadata.writer.toString(), QString("Author %1").arg(id));
        QCOMPARE(metadata.publisher.toString(), QString("Old publisher"));
        QVERIFY(metadata.favorite.toBool());
    }
    QVERIFY(dialog.setComicsForSequentialEditing(0, { load(1), load(2) }));
    dialog.findChild<QLineEdit *>("title")->setText("First only");
    dialog.saveAndOpenNext();
    QCOMPARE(dialog.findChild<QLineEdit *>("title")->text(), QString("Batch title"));
    dialog.findChild<QLineEdit *>("title")->setText("Second only");
    dialog.saveAndClose();
    QCOMPARE(recordMetadata(load(1)).title.toString(), QString("First only"));
    QCOMPARE(recordMetadata(load(2)).title.toString(), QString("Second only"));
}

void PropertiesDialogTest::confirmedRenameUpdatesFileAndDatabaseTogether()
{
    PropertiesDialog dialog;
    prepare(dialog);
    dialog.setComics({ load(1) });
    dialog.findChild<QPushButton *>("generateFilename")->click();
    dialog.findChild<QLineEdit *>("newFilename")->setText("Renamed.cbz");
    dialog.findChild<QPushButton *>("renameFile")->click();
    dialog.saveAndClose();
    QVERIFY(!QFile::exists(QDir(root).filePath("1.cbz")));
    QFile renamed(QDir(root).filePath("Renamed.cbz"));
    QVERIFY(renamed.open(QIODevice::ReadOnly));
    QCOMPARE(renamed.readAll(), QByteArray("archive bytes remain unchanged"));
    const auto stored = load(1);
    QCOMPARE(recordComic(stored)->file.fileName, QString("Renamed.cbz"));
    QCOMPARE(recordComic(stored)->file.path, QString("/Renamed.cbz"));
}

void PropertiesDialogTest::customCoverPersistsWithoutChangingMetadata()
{
    PropertiesDialog dialog;
    prepare(dialog);
    const auto original = load(1);
    dialog.setComics({ original });
    QImage image(100, 120, QImage::Format_RGB32);
    image.fill(Qt::red);
    const auto path = QDir(root).filePath("new-cover.png");
    QVERIFY(image.save(path));
    QSignalSpy changed(&dialog, &PropertiesDialog::coverChangedSignal);
    dialog.loadCustomCoverImageFromPath(path);
    dialog.saveAndClose();
    QCOMPARE(changed.count(), 1);
    const QImage stored(YACReader::LibraryPaths::coverPath(root, recordInfo(original)->hash));
    QVERIFY(stored.pixelColor(stored.width() / 2, stored.height() / 2).red() > 240);
    QCOMPARE(recordMetadata(load(1)).title, recordMetadata(original).title);
}

void PropertiesDialogTest::vocabularyIsScopedToItsLibrary()
{
    QString connection;
    {
        auto db = DataBaseManagement::loadDatabase(data);
        connection = db.connectionName();
        QSqlQuery query(db);
        QVERIFY(query.exec("UPDATE comic_info SET writer='One\nTwo', circle='Circle', work='Work', characters='Character A\nCharacter B', tags=' Alpha \nBeta\nAlpha', convention='C107', translator='Translator' WHERE id=1"));
        QVERIFY(query.exec("INSERT INTO comic_info(id,hash,tags) VALUES(99,'orphan','Orphan tag')"));
        const auto vocabulary = DBHelper::metadataVocabulary(db);
        QCOMPARE(vocabulary.value("tags"), QStringList({ "Alpha", "Beta", "Tag" }));
        QVERIFY(!vocabulary.value("tags").contains("Orphan tag"));
        QCOMPARE(vocabulary.value("circle"), QStringList({ "Circle" }));
        QCOMPARE(vocabulary.value("characters"), QStringList({ "Character A", "Character B" }));
        QCOMPARE(vocabulary.value("convention"), QStringList({ "C107" }));
        QCOMPARE(vocabulary.value("translator"), QStringList({ "Translator" }));
    }
    QSqlDatabase::removeDatabase(connection);
    // Opening a picker must not replace an active save connection or see an
    // uncommitted draft from that transaction.
    PropertiesDialog dialog;
    prepare(dialog);
    dialog.setComics({ load(1) });
    {
        auto db = DataBaseManagement::loadDatabase(data);
        connection = db.connectionName();
        QVERIFY(db.transaction());
        QSqlQuery query(db);
        QVERIFY(query.exec("UPDATE comic_info SET tags='Uncommitted tag' WHERE id=1"));
        auto *picker = dialog.findChild<MetadataPicker *>("picker_tags");
        picker->findChild<QToolButton *>("pickerToggle")->click();
        auto *chips = static_cast<MetadataValueChips *>(picker->findChild<QWidget *>("pickerChips"));
        QVERIFY(chips->values.contains("Beta"));
        QVERIFY(!chips->values.contains("Uncommitted tag"));
        QVERIFY(db.isOpen());
        QVERIFY(db.rollback());
    }
    QSqlDatabase::removeDatabase(connection);
    const auto other = temporary.filePath("other-library/.yacreaderlibrary");
    QVERIFY(QDir().mkpath(other));
    {
        auto db = DataBaseManagement::createDatabase(other + "/library.ydb");
        connection = db.connectionName();
        QSqlQuery query(db);
        QVERIFY(query.exec("INSERT INTO comic_info(id,hash,tags) VALUES(1,'other','Other library only')"));
        QVERIFY(query.exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,1,1,'Other.cbz','/Other.cbz')"));
        const auto vocabulary = DBHelper::metadataVocabulary(db);
        QCOMPARE(vocabulary.value("tags"), QStringList({ "Other library only" }));
        QVERIFY(vocabulary.value("circle").isEmpty());
    }
    QSqlDatabase::removeDatabase(connection);
}

void PropertiesDialogTest::ratingIsDeferredAndAtomic()
{
    PropertiesDialog dialog;
    prepare(dialog);
    dialog.setComics({ load(1) });
    dialog.findChild<QLineEdit *>("title")->setText("Draft title");
    dialog.findChild<QAbstractButton *>("rating4")->click();
    QCOMPARE(recordInfo(load(1))->rating, 0);
    QString connection;
    {
        auto db = DataBaseManagement::loadDatabase(data);
        connection = db.connectionName();
        QSqlQuery query(db);
        QVERIFY(query.exec("CREATE TRIGGER reject_rating BEFORE UPDATE OF rating ON comic_info BEGIN SELECT RAISE(ABORT,'test'); END"));
    }
    QSqlDatabase::removeDatabase(connection);
    QTimer::singleShot(0, [] { if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) box->accept(); });
    dialog.saveAndClose();
    QCOMPARE(recordMetadata(load(1)).title.toString(), QString("Title"));
    QCOMPARE(recordInfo(load(1))->rating, 0);
    {
        auto db = DataBaseManagement::loadDatabase(data);
        connection = db.connectionName();
        QSqlQuery query(db);
        QVERIFY(query.exec("DROP TRIGGER reject_rating"));
    }
    QSqlDatabase::removeDatabase(connection);
    dialog.saveAndClose();
    QCOMPARE(recordMetadata(load(1)).title.toString(), QString("Draft title"));
    QCOMPARE(recordInfo(load(1))->rating, 4);
}
void PropertiesDialogTest::renameDoesNotSaveMetadataDraft()
{
    PropertiesDialog dialog;
    prepare(dialog);
    dialog.setComics({ load(1) });
    dialog.findChild<QLineEdit *>("title")->setText("Unsaved title");
    dialog.findChild<QAbstractButton *>("rating5")->click();
    dialog.findChild<QCheckBox *>("isSeries")->setChecked(true);
    dialog.findChild<QLineEdit *>("seriesName")->setText("New series");
    dialog.findChild<QPushButton *>("generateFilename")->click();
    dialog.findChild<QLineEdit *>("newFilename")->setText("Independent.cbz");
    dialog.findChild<QPushButton *>("renameFile")->click();
    QVERIFY(QFile::exists(QDir(root).filePath("Independent.cbz")));
    QCOMPARE(recordMetadata(load(1)).title.toString(), QString("Title"));
    QCOMPARE(recordInfo(load(1))->rating, 0);
    QVERIFY(recordMetadata(load(1)).seriesName.isNull());
    QCOMPARE(dialog.findChild<QLineEdit *>("title")->text(), QString("Unsaved title"));
    dialog.saveAndClose();
    QCOMPARE(recordMetadata(load(1)).seriesName.toString(), QString("New series"));
    QCOMPARE(recordInfo(load(1))->rating, 5);
    QCOMPARE(recordComic(load(1))->file.fileName, QString("Independent.cbz"));
}

QTEST_MAIN(PropertiesDialogTest)
#include "main.moc"
