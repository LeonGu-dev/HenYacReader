#include "comic_metadata_editor.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "properties_dialog.h"
#include "yacreader_global.h"

#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

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
    dialog.findChild<QLineEdit *>("newFilename")->setText("Renamed");
    dialog.findChild<QCheckBox *>("renameOnSave")->setChecked(true);
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

QTEST_MAIN(PropertiesDialogTest)
#include "main.moc"
