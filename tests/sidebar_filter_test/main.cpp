#include "comic_model.h"
#include "data_base_management.h"
#include "metadata_filter_panel.h"
#include "reading_list_model.h"
#include "theme_manager.h"
#include "yacreader_global_gui.h"
#include "yacreader_reading_lists_view.h"
#include "yacreader_sidebar.h"
#include "yacreader_titled_toolbar.h"

#include <QAction>
#include <QApplication>
#include <QFontDatabase>
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSplitter>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTranslator>

#include <memory>

class SidebarFilterTest : public QObject
{
    Q_OBJECT
private slots:
    void threePanesDragAndPersistWithReadingLists();
};

void SidebarFilterTest::threePanesDragAndPersistWithReadingLists()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto oldProfile = qgetenv("YACREADER_DATA_DIR");
    qputenv("YACREADER_DATA_DIR", directory.path().toUtf8());
    const auto restore = qScopeGuard([&] {
        if (oldProfile.isNull())
            qunsetenv("YACREADER_DATA_DIR");
        else
            qputenv("YACREADER_DATA_DIR", oldProfile);
    });
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    qApp->setFont(QFont("Microsoft YaHei", 9));
    QTranslator translator;
    if (translator.load(qEnvironmentVariable("COMIC_TABLE_TRANSLATION")))
        qApp->installTranslator(&translator);
    Theme theme;
    theme.sidebar.backgroundColor = QColor("#fafafa");
    theme.sidebar.titleTextColor = Qt::black;
    theme.sidebar.sectionSeparatorColor = QColor("#dddddd");
    ThemeManager::instance().setTheme(theme);
    QString connection;
    {
        auto db = DataBaseManagement::createDatabase(directory.filePath("library.ydb"));
        connection = db.connectionName();
        QSqlQuery query(db);
        QVERIFY(query.exec("INSERT INTO comic_info(id,hash,title,favorite,tags) VALUES(1,'sidebar','Example',1,'daily')"));
        QVERIFY(query.exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,1,1,'a.cbz','/a.cbz')"));
        QVERIFY(query.exec("INSERT INTO reading_list(id,name) VALUES(1,'Author collection')"));
    }
    QSqlDatabase::removeDatabase(connection);
    ComicModel model;
    model.setupFolderModelData(1, directory.path());
    ReadingListModel lists;
    lists.setupReadingListsData(directory.path());
    auto sidebar = std::make_unique<YACReaderSideBar>();
    sidebar->filterPanel->setModel(&model);
    sidebar->readingListsView->setModel(&lists);
    sidebar->resize(340, 900);
    sidebar->show();
    QCoreApplication::processEvents();
    auto *splitter = sidebar->findChild<QSplitter *>("folderFilterSplitter");
    QVERIFY(splitter);
    QCOMPARE(splitter->count(), 3);
    QCOMPARE(splitter->indexOf(sidebar->filterPanel), 1);
    QCOMPARE(splitter->indexOf(sidebar->readingListsView), 2);
    QVERIFY(sidebar->readingListsView->isVisible());
    QVERIFY(sidebar->readingListsTitle->isVisible());
    const auto collection = lists.indexOf(ReadingListModel::ReadingList, 1);
    QVERIFY(collection.isValid());
    sidebar->readingListsView->scrollTo(collection);
    QTest::mouseClick(sidebar->readingListsView->viewport(), Qt::LeftButton, Qt::NoModifier,
                      sidebar->readingListsView->visualRect(collection).center());
    QCOMPARE(sidebar->readingListsView->currentIndex(), collection);
    auto *handle = splitter->handle(1);
    QVERIFY(handle->height() >= 35);
    QCOMPARE(handle->cursor().shape(), Qt::SplitVCursor);
    const auto previous = splitter->sizes();
    auto *favoriteOnly = sidebar->findChild<QAbstractButton *>("metadataFilterFavoritesOnly");
    auto *logic = sidebar->findChild<QToolButton *>("metadataFilterLogic");
    QVERIFY(favoriteOnly && logic);
    QVERIFY(favoriteOnly->isVisible());
    QTest::mouseClick(favoriteOnly, Qt::LeftButton);
    QVERIFY(model.metadataFilter().favoritesOnly);
    QTest::mouseClick(logic, Qt::LeftButton);
    QVERIFY(!model.metadataFilter().matchAll);
    QCOMPARE(splitter->sizes(), previous); // toolbar clicks must not drag the pane
    sidebar->filterPanel->findChild<QPushButton *>("metadataFilterReset")->click();
    QVERIFY(!favoriteOnly->isChecked());
    QVERIFY(model.metadataFilter().matchAll);
    auto *header = handle->findChild<QWidget *>("metadataFilterHeader");
    QVERIFY(header);
    const QPoint center(40, header->height() / 2);
    QTest::mousePress(header, Qt::LeftButton, Qt::NoModifier, center);
    QTest::mouseMove(header, center - QPoint(0, 70), 10);
    QTest::mouseRelease(header, Qt::LeftButton, Qt::NoModifier, center - QPoint(0, 70));
    QCoreApplication::processEvents();
    QVERIFY(splitter->sizes().first() < previous.first());
    QVERIFY(splitter->sizes()[1] > previous[1]);
    auto *listsHandle = splitter->handle(2);
    auto *listsHeader = listsHandle->findChild<QWidget *>("readingListsHeader");
    QVERIFY(listsHeader);
    QCOMPARE(listsHandle->cursor().shape(), Qt::SplitVCursor);
    const auto beforeListsDrag = splitter->sizes();
    // Header actions must remain clickable without resizing the panes.
    auto *probeAction = new QAction("+", sidebar.get());
    sidebar->readingListsTitle->addAction(probeAction);
    QSignalSpy triggered(probeAction, &QAction::triggered);
    QCoreApplication::processEvents();
    QTest::mouseClick(sidebar->readingListsTitle->findChild<QToolButton *>(), Qt::LeftButton);
    QCOMPARE(triggered.count(), 1);
    QCOMPARE(splitter->sizes(), beforeListsDrag);
    const QPoint listsCenter(40, listsHeader->height() / 2);
    QTest::mousePress(listsHeader, Qt::LeftButton, Qt::NoModifier, listsCenter);
    QTest::mouseMove(listsHeader, listsCenter - QPoint(0, 50), 10);
    QTest::mouseRelease(listsHeader, Qt::LeftButton, Qt::NoModifier, listsCenter - QPoint(0, 50));
    QCoreApplication::processEvents();
    QVERIFY(splitter->sizes()[1] < beforeListsDrag[1]);
    QVERIFY(splitter->sizes()[2] > beforeListsDrag[2]);
    const auto expected = splitter->sizes();
    QSettings settings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat);
    settings.beginGroup("libraryConfig");
    QVERIFY(settings.contains("sidebarFolderFilterReadingListsSplitterV3"));
    sidebar.reset();
    sidebar = std::make_unique<YACReaderSideBar>();
    sidebar->filterPanel->setModel(&model);
    sidebar->readingListsView->setModel(&lists);
    sidebar->resize(340, 900);
    sidebar->show();
    QCoreApplication::processEvents();
    splitter = sidebar->findChild<QSplitter *>("folderFilterSplitter");
    QCOMPARE(splitter->count(), 3);
    for (int i = 0; i < 3; ++i)
        QVERIFY(qAbs(splitter->sizes()[i] - expected[i]) <= 2);
    const auto preview = qEnvironmentVariable("SIDEBAR_FILTER_PREVIEW_DIR");
    if (!preview.isEmpty())
        QVERIFY(sidebar->grab().save(preview + "/sidebar-light.png"));
    theme.sidebar.backgroundColor = QColor("#444444");
    theme.sidebar.titleTextColor = Qt::white;
    ThemeManager::instance().setTheme(theme);
    QCoreApplication::processEvents();
    if (!preview.isEmpty())
        QVERIFY(sidebar->grab().save(preview + "/sidebar-dark.png"));
    qApp->removeTranslator(&translator);
}

QTEST_MAIN(SidebarFilterTest)
#include "main.moc"
