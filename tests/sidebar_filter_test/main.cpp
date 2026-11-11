#include "comic_model.h"
#include "data_base_management.h"
#include "edit_filter_smart_list_dialog.h"
#include "metadata_filter_panel.h"
#include "reading_list_model.h"
#include "save_filter_smart_list_dialog.h"
#include "theme_factory.h"
#include "theme_manager.h"
#include "yacreader_global_gui.h"
#include "yacreader_reading_lists_view.h"
#include "yacreader_sidebar.h"
#include "yacreader_titled_toolbar.h"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollBar>
#include <QSignalSpy>
#include <QSplitter>
#include <QSqlQuery>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTranslator>
#include <QWheelEvent>

#include <memory>

class SidebarFilterTest : public QObject
{
    Q_OBJECT
private slots:
    void init();
    void cleanup();
    void pinPreferencesAreIndependentAndCollapsingPreservesFilters();
    void shortSidebarKeepsFiltersAndReadingListsSeparated();
    void filterTabsDoNotExpandTheHorizontalSidebar();
    void fourTabsScrollOneAtATimeWithoutChangingThePage();
    void threePanesDragAndPersistWithReadingLists();
    void smartListScopeDialogRequiresNameAndRealFolder();
    void smartListEditorReusesPanelAndIsolatesLibrariesAndDrafts();

private:
    std::unique_ptr<QTemporaryDir> profile;
    QByteArray originalProfile;
};

void SidebarFilterTest::fourTabsScrollOneAtATimeWithoutChangingThePage()
{
    MetadataFilterTabs tabs;
    for (int i = 0; i < 7; ++i)
        tabs.addTab(new QWidget, QString("Tab %1").arg(i));
    tabs.resize(340, 250);
    tabs.show();
    QCoreApplication::processEvents();
    auto *left = tabs.findChild<QToolButton *>("metadataFilterPreviousTabs");
    auto *right = tabs.findChild<QToolButton *>("metadataFilterNextTabs");
    auto *viewport = tabs.findChild<QScrollArea *>("metadataFilterTabViewport");
    QVERIFY(!left->isEnabled());
    QVERIFY(right->isEnabled());
    QCOMPARE(viewport->width(), tabs.tabBar()->tabRect(0).width() * 4);
    right->click();
    QCOMPARE(tabs.firstVisibleTab(), 1);
    QCOMPARE(tabs.currentIndex(), 0);
    QCOMPARE(viewport->horizontalScrollBar()->value(), tabs.tabBar()->tabRect(0).width());
    QWheelEvent wheel(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(tabs.tabBar(), &wheel);
    QCOMPARE(tabs.firstVisibleTab(), 2);
    right->click();
    QCOMPARE(tabs.firstVisibleTab(), 3);
    QVERIFY(!right->isEnabled());
    left->click();
    QCOMPARE(tabs.firstVisibleTab(), 2);
    tabs.setCurrentIndex(0);
    // Re-selecting the current page need not scroll it back into view.
    tabs.setCurrentIndex(6);
    QCOMPARE(tabs.firstVisibleTab(), 3);
    tabs.setCurrentIndex(0);
    QCOMPARE(tabs.firstVisibleTab(), 0);
    tabs.resize(420, 250);
    QCoreApplication::processEvents();
    QCOMPARE(viewport->width(), tabs.tabBar()->tabRect(0).width() * 4);
}

void SidebarFilterTest::init()
{
    originalProfile = qgetenv("YACREADER_DATA_DIR");
    profile = std::make_unique<QTemporaryDir>();
    QVERIFY(profile->isValid());
    qputenv("YACREADER_DATA_DIR", profile->path().toUtf8());
}

void SidebarFilterTest::cleanup()
{
    if (originalProfile.isNull())
        qunsetenv("YACREADER_DATA_DIR");
    else
        qputenv("YACREADER_DATA_DIR", originalProfile);
    profile.reset();
}

void SidebarFilterTest::pinPreferencesAreIndependentAndCollapsingPreservesFilters()
{
    QTemporaryDir library;
    QString connection;
    {
        auto db = DataBaseManagement::createDatabase(library.filePath("library.ydb"));
        connection = db.connectionName();
        QSqlQuery query(db);
        QVERIFY(query.exec("INSERT INTO comic_info(id,hash,tags,translationStatus) VALUES(1,'a','daily','translated')"));
        QVERIFY(query.exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,1,1,'a.cbz','/a.cbz')"));
    }
    QSqlDatabase::removeDatabase(connection);
    ComicModel model;
    model.setupFolderModelData(1, library.path());
    MetadataFilter filter;
    filter.translation = "translated";
    filter.tags = { "daily" };
    model.setMetadataFilter(filter);
    MetadataFilterPanel panel(nullptr, MetadataFilterPanel::Host::Sidebar);
    panel.setModel(&model);
    panel.resize(340, 190);
    panel.show();
    panel.headerControls()->show();
    QCoreApplication::processEvents();
    auto *pin = panel.findChild<QToolButton *>("metadataFilterAlwaysExpanded");
    auto *fold = panel.findChild<QToolButton *>("metadataFilterFold");
    auto *scroll = panel.findChild<QScrollArea *>("metadataFilterChoicesScroll");
    QVERIFY(pin && fold && scroll);
    QVERIFY(pin->isChecked());
    QVERIFY(fold->isHidden());
    auto pinColors = pin->palette();
    pinColors.setColor(QPalette::ButtonText, QColor(40, 40, 40));
    pin->setPalette(pinColors);
    for (bool checked : { true, false }) {
        pin->setChecked(checked);
        QImage icon(pin->size(), QImage::Format_RGB32);
        icon.fill(QColor(240, 240, 240));
        pin->render(&icon, QPoint(), QRegion(), QWidget::DrawChildren);
        QCOMPARE(icon.pixelColor(0, 0), QColor(240, 240, 240));
        int painted = 0;
        for (int y = 0; y < icon.height(); ++y)
            for (int x = 0; x < icon.width(); ++x) {
                const auto pixel = icon.pixelColor(x, y);
                QCOMPARE(pixel.red(), pixel.green());
                QCOMPARE(pixel.green(), pixel.blue());
                painted += pixel != QColor(240, 240, 240);
            }
        QVERIFY(painted > 0);
    }
    pin->setChecked(true);
    QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > 0);
    QVERIFY(scroll->geometry().bottom() < panel.findChild<QLineEdit *>("metadataFilterSearch")->geometry().top());
    pin->click();
    QVERIFY(!pin->isChecked());
    QVERIFY(!fold->isHidden());
    QVERIFY(scroll->isHidden());
    QVERIFY(fold->toolTip().contains(panel.findChild<QPushButton *>("metadataFilter_translated")->text()));
    QCOMPARE(model.metadataFilter().savedQuery(), filter.savedQuery());
    QCOMPARE(model.rowCount(), 1);
    fold->click();
    QVERIFY(!scroll->isHidden());
    QCOMPARE(model.metadataFilter().savedQuery(), filter.savedQuery());
    MetadataFilterPanel restored(nullptr, MetadataFilterPanel::Host::Sidebar);
    QVERIFY(!restored.findChild<QToolButton *>("metadataFilterAlwaysExpanded")->isChecked());
    MetadataFilterPanel editor(nullptr, MetadataFilterPanel::Host::SmartListEditor);
    QVERIFY(editor.findChild<QToolButton *>("metadataFilterAlwaysExpanded")->isChecked());
    editor.setModel(&model);
    editor.findChild<QToolButton *>("metadataFilterAlwaysExpanded")->click();
    pin->click();
    QVERIFY(pin->isChecked());
    QVERIFY(fold->isHidden());
    panel.findChild<QPushButton *>("metadataFilterReset")->click();
    QVERIFY(pin->isChecked());
    QVERIFY(!model.metadataFilter().active());
    MetadataFilterPanel editorAgain(nullptr, MetadataFilterPanel::Host::SmartListEditor);
    QVERIFY(!editorAgain.findChild<QToolButton *>("metadataFilterAlwaysExpanded")->isChecked());
}

void SidebarFilterTest::shortSidebarKeepsFiltersAndReadingListsSeparated()
{
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    qApp->setFont(QFont("Microsoft YaHei", 9));
    QTranslator translator;
    if (translator.load(qEnvironmentVariable("COMIC_TABLE_TRANSLATION")))
        qApp->installTranslator(&translator);
    QFile lightFile(QFINDTESTDATA("../../YACReaderLibrary/themes/builtin_light.json"));
    QVERIFY(lightFile.open(QIODevice::ReadOnly));
    ThemeManager::instance().setTheme(makeTheme(QJsonDocument::fromJson(lightFile.readAll()).object()));
    QTemporaryDir library;
    QString connection;
    {
        auto db = DataBaseManagement::createDatabase(library.filePath("library.ydb"));
        connection = db.connectionName();
        QSqlQuery query(db);
        QVERIFY(query.exec("INSERT INTO comic_info(id,hash,translationStatus) VALUES(1,'a','translated')"));
        QStringList tags;
        for (int i = 0; i < 100; ++i)
            tags.append(QStringLiteral("tag %1").arg(i));
        query.prepare("UPDATE comic_info SET tags=? WHERE id=1");
        query.addBindValue(tags.join('\n'));
        QVERIFY(query.exec());
        QVERIFY(query.exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,1,1,'a.cbz','/a.cbz')"));
        QVERIFY(query.exec("INSERT INTO reading_list(id,name) VALUES(1,'Evening collection')"));
    }
    QSqlDatabase::removeDatabase(connection);
    ComicModel model;
    model.setupFolderModelData(1, library.path());
    MetadataFilter filter;
    filter.translation = "translated";
    model.setMetadataFilter(filter);
    ReadingListModel lists;
    lists.setupReadingListsData(library.path());
    YACReaderSideBar sidebar;
    sidebar.filterPanel->setModel(&model);
    sidebar.readingListsView->setModel(&lists);
    sidebar.resize(340, 430);
    sidebar.show();
    QCoreApplication::processEvents();
    auto *splitter = sidebar.findChild<QSplitter *>("folderFilterSplitter");
    for (int height : { 430, 540, 700, 460 }) {
        sidebar.resize(340, height);
        QCoreApplication::processEvents();
        auto *panel = sidebar.filterPanel;
        auto *header = splitter->handle(2);
        QVERIFY(panel->geometry().bottom() < header->geometry().top());
        QVERIFY(header->geometry().bottom() < sidebar.readingListsView->geometry().top());
        auto *search = panel->findChild<QLineEdit *>("metadataFilterSearch");
        auto *tabs = panel->findChild<MetadataFilterTabs *>();
        QVERIFY(search->geometry().bottom() < tabs->geometry().top());
        QVERIFY(tabs->geometry().bottom() < panel->height());
    }
    const auto preview = qEnvironmentVariable("SIDEBAR_PIN_PREVIEW_DIR");
    if (!preview.isEmpty()) {
        sidebar.resize(sidebar.sizeHint().width(), sidebar.height());
        QCoreApplication::processEvents();
        QVERIFY(sidebar.grab().save(preview + "/short-sidebar.png"));
        sidebar.filterPanel->setEnabled(true);
        sidebar.filterPanel->headerControls()->setEnabled(true);
        sidebar.filterPanel->headerControls()->findChild<QToolButton *>("metadataFilterAlwaysExpanded")->click();
        QCoreApplication::processEvents();
        QVERIFY(sidebar.grab().save(preview + "/short-sidebar-folded.png"));
        QFile darkFile(QFINDTESTDATA("../../YACReaderLibrary/themes/builtin_dark.json"));
        QVERIFY(darkFile.open(QIODevice::ReadOnly));
        ThemeManager::instance().setTheme(makeTheme(QJsonDocument::fromJson(darkFile.readAll()).object()));
        QCoreApplication::processEvents();
        QVERIFY(sidebar.grab().save(preview + "/short-sidebar-dark-folded.png"));
    }
    auto largeFont = sidebar.font();
    largeFont.setPointSize(14);
    sidebar.setFont(largeFont);
    QCoreApplication::processEvents();
    for (int i = 1; i < splitter->count(); ++i)
        QVERIFY(splitter->handle(i)->height() >= splitter->handle(i)->layout()->totalMinimumSize().height());
    qApp->removeTranslator(&translator);
}

void SidebarFilterTest::filterTabsDoNotExpandTheHorizontalSidebar()
{
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *splitter = new QSplitter(Qt::Horizontal);
    auto *left = new QWidget;
    auto *leftLayout = new QVBoxLayout(left);
    auto *panel = new MetadataFilterPanel;
    // Isolate tab geometry from the unrelated intrinsic widths of status buttons.
    panel->findChild<QWidget *>("metadataFilterChoices")->hide();
    leftLayout->addWidget(panel);
    splitter->addWidget(left);
    auto *content = new QWidget;
    content->setMinimumWidth(200);
    splitter->addWidget(content);
    layout->addWidget(splitter);
    window.resize(1100, 800);
    window.show();
    QCoreApplication::processEvents();
    splitter->setSizes({ 340, splitter->width() - splitter->handleWidth() - 340 });
    const auto initialWindow = window.size();
    const int initialWidth = left->width();
    QVERIFY(initialWidth <= 350);
    for (int i = 0; i < 30; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QCOMPARE(window.size(), initialWindow);
    QCOMPARE(left->width(), initialWidth);
    const auto *tabs = panel->findChild<MetadataFilterTabs *>();
    QCOMPARE(tabs->tabBar()->minimumWidth(), 0);
    for (int width : { 280, 420, 320 }) {
        splitter->setSizes({ width, 1000 - width });
        const int requestedWidth = left->width();
        for (int i = 0; i < 15; ++i)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QCOMPARE(left->width(), requestedWidth);
        QVERIFY(tabs->tabBar()->width() >= tabs->findChild<QScrollArea *>("metadataFilterTabViewport")->width());
    }
    auto changedFont = panel->font();
    changedFont.setPointSize(14);
    panel->setFont(changedFont);
    for (int i = 0; i < 15; ++i)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    QVERIFY(tabs->tabBar()->width() >= tabs->findChild<QScrollArea *>("metadataFilterTabViewport")->width());
}

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
    // the two fixed rows come first, filled by the library; favorites has no row here
    QCOMPARE(lists.index(0, 0).data().toString(), QString("正在阅读"));
    QCOMPARE(lists.index(1, 0).data().toString(), QString("最近添加"));
    QVERIFY(!lists.indexOf(ReadingListModel::SpecialList, int(ReadingListModel::TypeSpecialList::Favorites)).isValid());
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

void SidebarFilterTest::smartListScopeDialogRequiresNameAndRealFolder()
{
    QTranslator translator;
    if (translator.load(qEnvironmentVariable("COMIC_TABLE_TRANSLATION")))
        qApp->installTranslator(&translator);
    const auto removeTranslator = qScopeGuard([&] { qApp->removeTranslator(&translator); });
    SaveFilterSmartListDialog inFolder(QString("/Example"));
    QVERIFY(inFolder.currentFolderOnly());
    auto *buttons = inFolder.findChild<QDialogButtonBox *>();
    QVERIFY(!buttons->button(QDialogButtonBox::Save)->isEnabled());
    inFolder.findChild<QLineEdit *>("smartListName")->setText("  Example list  ");
    QVERIFY(buttons->button(QDialogButtonBox::Save)->isEnabled());
    QCOMPARE(inFolder.listName(), QString("Example list"));
    inFolder.findChild<QRadioButton *>("smartListLibraryScope")->click();
    QVERIFY(!inFolder.currentFolderOnly());
    inFolder.findChild<QRadioButton *>("smartListFolderScope")->click();
    QVERIFY(inFolder.currentFolderOnly());
    SaveFilterSmartListDialog inList(std::nullopt);
    QVERIFY(!inList.currentFolderOnly());
    QVERIFY(!inList.findChild<QRadioButton *>("smartListFolderScope")->isEnabled());
    inList.findChild<QRadioButton *>("smartListFolderScope")->click();
    QVERIFY(!inList.currentFolderOnly());
    QVERIFY(inList.findChild<QLabel *>("smartListFolderUnavailable"));
    SaveFilterSmartListDialog root(QString("//"));
    QVERIFY(!root.findChild<QLabel *>("smartListFolderDescription")->text().contains('/'));
    SaveFilterSmartListDialog duplicateSlash(QString("//Example///Child"));
    QVERIFY(duplicateSlash.findChild<QLabel *>("smartListFolderDescription")->text().endsWith("/Example/Child"));
    const auto preview = qEnvironmentVariable("SMART_LIST_SCOPE_PREVIEW_DIR");
    if (!preview.isEmpty()) {
        inFolder.show();
        QCoreApplication::processEvents();
        QVERIFY(inFolder.grab().save(preview + "/folder.png"));
        inFolder.hide();
        inList.show();
        QCoreApplication::processEvents();
        QVERIFY(inList.grab().save(preview + "/reading-list.png"));
    }
}

void SidebarFilterTest::smartListEditorReusesPanelAndIsolatesLibrariesAndDrafts()
{
    QTemporaryDir temporary;
    QVERIFY(temporary.isValid());
    const auto seed = [&](const QString &directory, const QString &name) {
        QDir().mkpath(directory);
        QString connection;
        bool success = false;
        {
            auto db = DataBaseManagement::createDatabase(directory + "/library.ydb");
            connection = db.connectionName();
            QSqlQuery query(db);
            query.prepare("INSERT INTO folder(id,parentId,name,path) VALUES(2,1,:name,:path)");
            query.bindValue(":name", name);
            query.bindValue(":path", "/" + name);
            success = query.exec();
            query.prepare("INSERT INTO comic_info(id,hash,tags,favorite,convention) VALUES(1,'a',:tag,0,:tag)");
            query.bindValue(":tag", name);
            success = success && query.exec();
            success = success && query.exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,2,1,'a.cbz','/a.cbz')");
        }
        QSqlDatabase::removeDatabase(connection);
        return success;
    };
    const auto first = temporary.filePath("first");
    const auto second = temporary.filePath("second");
    QVERIFY(seed(first, "OnlyA"));
    QVERIFY(seed(second, "OnlyB"));
    MetadataFilter filter;
    filter.tags = { "OnlyA" };
    filter.conventions = { "OnlyA" };
    ComicModel mainModel;
    mainModel.setupFolderModelData(2, first);
    mainModel.setMetadataFilter(filter);
    EditFilterSmartListDialog editor(first, "Example", filter.savedQuery());
    QCOMPARE(editor.findChildren<MetadataFilterPanel *>().size(), 1);
    auto *tabs = editor.findChild<MetadataFilterTabs *>();
    QCOMPARE(tabs->count(), 6);
    QVERIFY(editor.findChild<QWidget *>("metadataFilterChips3"));
    QCOMPARE(editor.findChild<ComicModel *>()->metadataFilterValues().value(3), QStringList({ "OnlyA" }));
    QCOMPARE(MetadataFilter::fromSavedQuery(editor.savedQuery())->conventions, filter.conventions);
    QVERIFY(editor.findChild<ComicModel *>()->metadataFilterValues().value(0).contains("OnlyA"));
    QVERIFY(!editor.findChild<ComicModel *>()->metadataFilterValues().value(0).contains("OnlyB"));
    auto *save = editor.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save);
    QVERIFY(save->isEnabled());
    auto *folder = editor.findChild<QRadioButton *>("editSmartListFolderScope");
    folder->click();
    QVERIFY(!save->isEnabled());
    editor.accept();
    QCOMPARE(editor.result(), int(QDialog::Rejected));
    editor.findChild<QAbstractButton *>("metadataFilterFavoritesOnly")->click();
    QVERIFY(MetadataFilter::fromSavedQuery(editor.savedQuery())->favoritesOnly);
    QVERIFY(!mainModel.metadataFilter().favoritesOnly);

    auto pick = [](EditFilterSmartListDialog &dialog) {
        QStringList names;
        QTimer::singleShot(0, &dialog, [&] {
            auto *tree = dialog.findChild<QTreeWidget *>("smartListFolderTree");
            if (!tree)
                return;
            QTreeWidgetItem *selected = nullptr;
            for (QTreeWidgetItemIterator it(tree); *it; ++it) {
                names.append((*it)->text(0));
                if ((*it)->data(0, Qt::UserRole).toULongLong() == 2)
                    selected = *it;
            }
            tree->setCurrentItem(selected);
            qobject_cast<QDialog *>(tree->window())->accept();
        });
        dialog.findChild<QPushButton *>("editSmartListChooseFolder")->click();
        return names;
    };
    const auto names = pick(editor);
    QVERIFY(names.contains("OnlyA"));
    QVERIFY(!names.contains("OnlyB"));
    QVERIFY(save->isEnabled());
    QCOMPARE(MetadataFilter::savedFolderId(editor.savedQuery()), std::optional<qulonglong>(2));
    QVERIFY(folder->text().contains("OnlyA"));
    EditFilterSmartListDialog other(second, "Other", filter.savedQuery());
    QVERIFY(other.findChild<ComicModel *>()->metadataFilterValues().value(0).contains("OnlyB"));
    QVERIFY(!other.findChild<ComicModel *>()->metadataFilterValues().value(0).contains("OnlyA"));
    const auto otherNames = pick(other);
    QVERIFY(otherNames.contains("OnlyB"));
    QVERIFY(!otherNames.contains("OnlyA"));

    editor.show();
    QCoreApplication::processEvents();
    QVERIFY(editor.width() <= editor.screen()->availableGeometry().width());
    QVERIFY(editor.height() <= editor.screen()->availableGeometry().height());
    tabs->setCurrentIndex(3);
    QCoreApplication::processEvents();
    auto *eventChips = editor.findChild<QWidget *>("metadataFilterChips3");
    QTest::mouseClick(eventChips, Qt::LeftButton, Qt::NoModifier, QPoint(15, 15));
    QVERIFY(MetadataFilter::fromSavedQuery(editor.savedQuery())->conventions.isEmpty());
    QCOMPARE(mainModel.metadataFilter().conventions, filter.conventions);
    QTest::mouseClick(eventChips, Qt::LeftButton, Qt::NoModifier, QPoint(15, 15));
    QCOMPARE(MetadataFilter::fromSavedQuery(editor.savedQuery())->conventions, filter.conventions);
    auto *scroll = editor.findChild<QScrollArea *>("smartListEditorScroll");
    const auto oldScrollRange = scroll->verticalScrollBar()->maximum();
    auto *sharedPanel = editor.findChild<MetadataFilterPanel *>();
    // Simulate future filter rows: the editor must accommodate them without growing off-screen.
    for (int i = 0; i < 8; ++i) {
        auto *extra = new QPushButton("Additional filter", sharedPanel);
        extra->setMinimumHeight(32);
        sharedPanel->layout()->addWidget(extra);
        extra->show();
    }
    QTRY_VERIFY(scroll->verticalScrollBar()->maximum() > oldScrollRange);
    QVERIFY(editor.height() <= editor.screen()->availableGeometry().height());
    editor.reject();
    QCOMPARE(mainModel.metadataFilter().savedQuery(), filter.savedQuery());
    EditFilterSmartListDialog missing(first, "Missing", filter.savedQuery(999));
    QVERIFY(!missing.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->isEnabled());
    missing.findChild<QRadioButton *>("editSmartListLibraryScope")->click();
    QVERIFY(missing.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->isEnabled());
    QVERIFY(!MetadataFilter::savedFolderId(missing.savedQuery()));
    const auto preview = qEnvironmentVariable("SMART_LIST_EDITOR_PREVIEW_DIR");
    if (!preview.isEmpty()) {
        Theme previewTheme;
        previewTheme.sidebar.backgroundColor = QColor("#f3f3f3");
        previewTheme.sidebar.titleTextColor = Qt::black;
        previewTheme.comicsViewTable.starRatingColor = QColor("#e4b000");
        ThemeManager::instance().setTheme(previewTheme);
        QTranslator translator;
        if (translator.load(qEnvironmentVariable("COMIC_TABLE_TRANSLATION")))
            qApp->installTranslator(&translator);
        EditFilterSmartListDialog image(first, "Example", editor.savedQuery());
        image.show();
        QCoreApplication::processEvents();
        QVERIFY(image.grab().save(preview + "/editor.png"));
        image.findChild<QToolButton *>("metadataFilterAlwaysExpanded")->click();
        QCoreApplication::processEvents();
        QVERIFY(image.grab().save(preview + "/editor-folded.png"));
        qApp->removeTranslator(&translator);
    }
}

QTEST_MAIN(SidebarFilterTest)
#include "main.moc"
