#include "data_base_management.h"
#include "yacreader_global.h"
#include "yacreader_global_gui.h"
#include "yacreader_libraries.h"

#include <QColor>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QImage>
#include <QSettings>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("YACReaderLibrary");
    app.setOrganizationName("YACReader");
    if (argc != 3 || qEnvironmentVariable("YACREADER_DATA_DIR").isEmpty())
        return 1;
    const auto root = QDir(QString::fromLocal8Bit(argv[1])).absolutePath();
    const auto database = YACReader::LibraryPaths::libraryDatabasePath(root);
    if (QFile::exists(database)) {
        QTextStream(stderr) << "The demo library already exists; refusing to replace it.\n";
        return 2;
    }
    QDir().mkpath(YACReader::LibraryPaths::libraryCoversFolderPath(root));
    const auto fileName = QStringLiteral("Metadata Demo.cbz");
    const auto comicPath = QDir(root).filePath(fileName);
    if (!QFile::copy(QString::fromLocal8Bit(argv[2]), comicPath))
        return 3;
    QFile file(comicPath);
    if (!file.open(QFile::ReadOnly))
        return 4;
    const auto hash = QString::fromLatin1(QCryptographicHash::hash(file.read(524288), QCryptographicHash::Sha1).toHex()) + QString::number(file.size());
    file.close();
    QString connection;
    {
        auto db = DataBaseManagement::createDatabase(database);
        connection = db.connectionName();
        QSqlQuery query(db);
        const auto exec = [&](const QString &sql) {
            if (query.exec(sql))
                return true;
            QTextStream(stderr) << query.lastError().text() << '\n';
            return false;
        };
        if (!db.isOpen() || !exec("INSERT OR IGNORE INTO folder(id,parentId,name,path,type) VALUES(1,1,'Demo','/',1)"))
            return 5;
        query.prepare("INSERT INTO comic_info(id,hash,title,translatedTitle,writer,isAnthology,circle,work,characters,tags,color,censorship,translationStatus,translator,numPages,coverPage,coverSizeRatio,originalCoverSize,type) "
                      "VALUES(1,:hash,:title,:translatedTitle,:writer,1,:circle,:work,:characters,:tags,1,'censored','translated',:translator,3,1,0.75,'600x800',1)");
        query.bindValue(":hash", hash);
        query.bindValue(":title", QStringLiteral("验收示例漫画"));
        query.bindValue(":translatedTitle", QStringLiteral("示例译名"));
        query.bindValue(":writer", QStringLiteral("作者甲\n作者乙"));
        query.bindValue(":circle", QStringLiteral("示例社团"));
        query.bindValue(":work", QStringLiteral("示例作品"));
        query.bindValue(":characters", QStringLiteral("角色甲\n角色乙"));
        query.bindValue(":tags", QStringLiteral("日常\n冒险"));
        query.bindValue(":translator", QStringLiteral("示例汉化组"));
        if (!query.exec())
            return 6;
        query.prepare("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,1,1,:name,:path)");
        query.bindValue(":name", fileName);
        query.bindValue(":path", "/" + fileName);
        if (!query.exec())
            return 7;
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    QImage cover(600, 800, QImage::Format_RGB32);
    cover.fill(QColor(50, 100, 170));
    if (!cover.save(YACReader::LibraryPaths::coverPath(root, hash)))
        return 8;
    QDir().mkpath(YACReader::getSettingsPath());
    YACReaderLibraries libraries;
    libraries.addLibrary(QStringLiteral("漫画信息验收"), root);
    if (!libraries.save())
        return 9;
    QSettings settings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat);
    settings.beginGroup("libraryConfig");
    settings.setValue(SERVER_ON, false);
    settings.setValue(UI_LANGUAGE, "zh_CN");
    settings.sync();
    QSettings common(YACReader::getCommonSettingsFilePath(), QSettings::IniFormat);
    common.setValue("LAST_VERSION_INSTALLED", VERSION);
    QTextStream(stdout) << "Demo library created: " << root << '\n';
    return 0;
}
