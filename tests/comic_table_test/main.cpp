#include "comic_model.h"
#include "comic_table_layout.h"
#include "data_base_management.h"
#include "metadata_chip_colors.h"
#include "metadata_chip_delegate.h"
#include "metadata_filter_panel.h"
#include "theme_factory.h"
#include "theme_manager.h"
#include "yacreader_global_gui.h"
#include "yacreader_table_view.h"

#include <QContextMenuEvent>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHeaderView>
#include <QJsonObject>
#include <QLineEdit>
#include <QMenu>
#include <QPaintEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QSqlQuery>
#include <QStandardItemModel>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTranslator>

#include <memory>

class ComicTableTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void cleanupTestCase();
    void oldLayoutIsDiscardedAndDefaultsHaveThirteenColumns();
    void newLayoutPersistsAndDefaultsCanBeRestored();
    void obsoleteColumnsStayOutOfTheLayout();
    void chipColorsAreStable();
    void headerClicksToggleOnlySortableColumns();
    void chipClicksDoNotFilterTheRows();
    void colorMenuTargetsTheClickedChipAndComesFirst();
    void cancelColorPickerDoesNotSave();
    void colorOverridesPersistAndStayInTheirFieldAndProfile();
    void colorsUseReadableForegrounds();
    void blankChipCellDoesNotAddAColorAction();
    void chipTextFitsWithoutUnnecessaryElision_data();
    void chipTextFitsWithoutUnnecessaryElision();
    void narrowChipsStillElideAndKeepTheirFullValue();
    void chipMeasurementUsesTheActualPaintFont();
    void renderTable();
    void filterPanelClicksSearchAndTheme();
    void favoriteHeartClicksPersistAndShareAcrossFiles();
    void oldHeaderKeepsWidthsWhenHeartIsAdded();

private:
    std::unique_ptr<QTemporaryDir> profiles;
    QByteArray originalProfile;
};

void ComicTableTest::initTestCase()
{
    originalProfile = qgetenv("YACREADER_DATA_DIR");
    profiles = std::make_unique<QTemporaryDir>();
    QVERIFY(profiles->isValid());
}
void ComicTableTest::init()
{
    qputenv("YACREADER_DATA_DIR", profiles->filePath(QString::fromLatin1(QTest::currentTestFunction())).toUtf8());
    MetadataChipColors::instance().reload();
}
void ComicTableTest::cleanupTestCase()
{
    if (originalProfile.isNull())
        qunsetenv("YACREADER_DATA_DIR");
    else
        qputenv("YACREADER_DATA_DIR", originalProfile);
    profiles.reset();
}

namespace {
// Inspect the actual strings sent to the painter, rather than repeating the layout formula.
class TextRecordingEngine : public QPaintEngine
{
public:
    TextRecordingEngine() : QPaintEngine(QPaintEngine::AllFeatures) { }
    QStringList text;
    QRectF lastChip;
    bool textExceedsItsBox = false;
    bool begin(QPaintDevice *) override { return true; }
    bool end() override { return true; }
    Type type() const override { return QPaintEngine::User; }
    void updateState(const QPaintEngineState &) override { }
    void drawPixmap(const QRectF &, const QPixmap &, const QRectF &) override { }
    void drawPath(const QPainterPath &path) override { lastChip = path.boundingRect(); }
    void drawTextItem(const QPointF &point, const QTextItem &item) override
    {
        text.append(item.text());
        if (!lastChip.isEmpty() && point.x() + item.width() > lastChip.right() - 7 + 0.01)
            textExceedsItsBox = true;
    }
};
class TextRecordingDevice : public QPaintDevice
{
public:
    mutable TextRecordingEngine engine;
    QPaintEngine *paintEngine() const override { return &engine; }

protected:
    int metric(PaintDeviceMetric metric) const override
    {
        switch (metric) {
        case PdmWidth:
            return 1400;
        case PdmHeight:
            return 64;
        case PdmWidthMM:
            return 370;
        case PdmHeightMM:
            return 17;
        case PdmDpiX:
        case PdmDpiY:
        case PdmPhysicalDpiX:
        case PdmPhysicalDpiY:
            return 96;
        case PdmDepth:
            return 32;
        case PdmNumColors:
            return 0;
        case PdmDevicePixelRatio:
            return 1;
        case PdmDevicePixelRatioScaled:
            return int(QPaintDevice::devicePixelRatioFScale());
        default:
            return QPaintDevice::metric(metric);
        }
    }
};

class ColorPickerTable : public YACReaderTableView
{
public:
    QColor picked;
    QColor initial;
    int requests = 0;

protected:
    QColor requestChipColor(const QColor &color) override
    {
        ++requests;
        initial = color;
        return picked;
    }
};
void showTagTable(QTableView &table, QStandardItemModel &model)
{
    model.setData(model.index(0, ComicModel::Tags), QStringList { "First", "Second" }, ComicModel::ChipValuesRole);
    table.setModel(&model);
    table.verticalHeader()->hide();
    for (int i = 0; i < model.columnCount(); ++i)
        table.setColumnHidden(i, i != ComicModel::Tags);
    table.setColumnWidth(ComicModel::Tags, 220);
    table.resize(330, 150);
    table.show();
    QCoreApplication::processEvents();
}

QList<int> displayed(QHeaderView &header)
{
    QList<int> result;
    for (int i = 0; i < header.count(); ++i) {
        const int column = header.logicalIndex(i);
        if (!header.isSectionHidden(column))
            result.append(column);
    }
    return result;
}
void headers(QStandardItemModel &model)
{
    ComicModel metadata;
    for (int i = 0; i < model.columnCount(); ++i)
        model.setHeaderData(i, Qt::Horizontal, metadata.headerData(i, Qt::Horizontal));
}
}

void ComicTableTest::oldLayoutIsDiscardedAndDefaultsHaveThirteenColumns()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("table.ini"), QSettings::IniFormat);
    settings.setValue(COMICS_VIEW_HEADERS, QByteArray("obsolete table"));
    QStandardItemModel model(0, ComicModel::ColumnCount);
    QTableView table;
    table.setModel(&model);
    ComicTableLayout::restore(*table.horizontalHeader(), settings);
    QCOMPARE(displayed(*table.horizontalHeader()), ComicModel::visibleColumns());
    QVERIFY(!settings.contains(COMICS_VIEW_HEADERS));
    QVERIFY(!table.horizontalHeader()->isSortIndicatorShown());
    QVERIFY(table.horizontalHeader()->sectionsMovable());
    QCOMPARE(table.columnWidth(ComicModel::FileName), 280);
}

void ComicTableTest::newLayoutPersistsAndDefaultsCanBeRestored()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("table.ini"), QSettings::IniFormat);
    QStandardItemModel model(1, ComicModel::ColumnCount);
    QTableView table;
    table.setModel(&model);
    auto &header = *table.horizontalHeader();
    ComicTableLayout::reset(header);
    header.hideSection(ComicModel::Tags);
    header.moveSection(header.visualIndex(ComicModel::Author), 2);
    header.resizeSection(ComicModel::Author, 175);
    header.setSortIndicator(ComicModel::Author, Qt::DescendingOrder);
    header.setSortIndicatorShown(true);
    ComicTableLayout::save(header, settings);
    QTableView next;
    next.setModel(&model);
    ComicTableLayout::restore(*next.horizontalHeader(), settings);
    QCOMPARE(displayed(*next.horizontalHeader()), displayed(header));
    QVERIFY(next.isColumnHidden(ComicModel::Tags));
    QCOMPARE(next.columnWidth(ComicModel::Author), 175);
    QCOMPARE(next.horizontalHeader()->sortIndicatorSection(), int(ComicModel::Author));
    QCOMPARE(next.horizontalHeader()->sortIndicatorOrder(), Qt::DescendingOrder);
    ComicTableLayout::reset(*next.horizontalHeader());
    QCOMPARE(displayed(*next.horizontalHeader()), ComicModel::visibleColumns());
    QCOMPARE(next.columnWidth(ComicModel::Author), 130);
    QVERIFY(!next.horizontalHeader()->isSortIndicatorShown());
}

void ComicTableTest::obsoleteColumnsStayOutOfTheLayout()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("table.ini"), QSettings::IniFormat);
    QStandardItemModel model(1, ComicModel::ColumnCount);
    QTableView table;
    table.setModel(&model);
    ComicTableLayout::reset(*table.horizontalHeader());
    table.showColumn(ComicModel::Number);
    table.showColumn(ComicModel::Series);
    ComicTableLayout::save(*table.horizontalHeader(), settings);
    ComicTableLayout::restore(*table.horizontalHeader(), settings);
    QVERIFY(table.isColumnHidden(ComicModel::Number));
    QVERIFY(table.isColumnHidden(ComicModel::Series));
}

void ComicTableTest::headerClicksToggleOnlySortableColumns()
{
    ComicModel model;
    QTableView table;
    table.setModel(&model);
    auto &header = *table.horizontalHeader();
    ComicTableLayout::reset(header);
    QObject::connect(&header, &QHeaderView::sectionClicked, &table, [&](int column) {
        ComicTableLayout::sortFromHeaderClick(header, model, column);
    });
    table.resize(800, 150);
    table.show();
    QCoreApplication::processEvents();
    const auto click = [&](int column) {
        const auto position = header.sectionViewportPosition(column) + header.sectionSize(column) / 2;
        QTest::mouseClick(header.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(position, header.height() / 2));
    };
    click(ComicModel::Author);
    QCOMPARE(model.tableSortColumn(), int(ComicModel::Author));
    QCOMPARE(model.tableSortOrder(), Qt::AscendingOrder);
    click(ComicModel::Author);
    QCOMPARE(model.tableSortOrder(), Qt::DescendingOrder);
    // Scroll to the multi-value column before clicking it.
    table.scrollTo(model.index(0, ComicModel::Characters));
    ComicTableLayout::sortFromHeaderClick(header, model, ComicModel::Characters);
    QCOMPARE(header.sortIndicatorSection(), int(ComicModel::Author));
    QCOMPARE(header.sortIndicatorOrder(), Qt::DescendingOrder);
}

void ComicTableTest::chipColorsAreStable()
{
    const auto first = MetadataChipDelegate::colorForValue(ComicModel::Work, QStringLiteral("原创"));
    const auto second = MetadataChipDelegate::colorForValue(ComicModel::Work, QStringLiteral("原创"));
    QCOMPARE(first, second);
    QVERIFY(first.isValid());
    QVERIFY(first.lightness() >= 200);
    QVERIFY(first != MetadataChipDelegate::colorForValue(ComicModel::Work, QStringLiteral("示例作品")));
}

void ComicTableTest::chipClicksDoNotFilterTheRows()
{
    QStandardItemModel model(1, ComicModel::ColumnCount);
    model.setData(model.index(0, ComicModel::Tags), QStringList { "Sample tag" }, ComicModel::ChipValuesRole);
    QTableView table;
    table.setModel(&model);
    auto delegate = new MetadataChipDelegate(&table);
    table.setItemDelegateForColumn(ComicModel::Tags, delegate);
    for (int i = 0; i < model.columnCount(); ++i)
        table.setColumnHidden(i, i != ComicModel::Tags);
    table.setColumnWidth(ComicModel::Tags, 180);
    table.setRowHeight(0, 32);
    table.resize(300, 140);
    table.show();
    QTest::qWait(5);
    QSignalSpy clicked(delegate, &MetadataChipDelegate::chipClicked);
    const auto rect = table.visualRect(model.index(0, ComicModel::Tags));
    QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier, rect.topLeft() + QPoint(15, rect.height() / 2));
    QCOMPARE(clicked.count(), 1);
    QCOMPARE(clicked.first().at(1).toString(), QString("Sample tag"));
    QCOMPARE(model.rowCount(), 1);
}

void ComicTableTest::colorMenuTargetsTheClickedChipAndComesFirst()
{
    QStandardItemModel model(2, ComicModel::ColumnCount);
    ColorPickerTable table;
    showTagTable(table, model);
    const auto rect = table.visualRect(model.index(0, ComicModel::Tags));
    QStyleOptionViewItem option;
    option.rect = rect;
    option.widget = &table;
    option.font = table.font();
    option.fontMetrics = QFontMetrics(option.font);
    const auto delegate = qobject_cast<MetadataChipDelegate *>(table.itemDelegateForColumn(ComicModel::Tags));
    QVERIFY(delegate);
    QPoint point;
    for (int x = rect.left(); x <= rect.right(); ++x) {
        const QPoint candidate(x, rect.center().y());
        if (delegate->valueAt(option, model.index(0, ComicModel::Tags), candidate) == "Second") {
            point = candidate + QPoint(6, 0);
            break;
        }
    }
    QVERIFY(!point.isNull());
    QMenu menu;
    auto oldAction = menu.addAction("Open comic");
    const auto first = MetadataChipDelegate::colorForValue(ComicModel::Tags, "First");
    const auto initial = MetadataChipDelegate::colorForValue(ComicModel::Tags, "Second");
    table.setContextMenuPolicy(Qt::CustomContextMenu);
    bool added = false;
    QPoint requestedPoint;
    QObject::connect(&table, &QWidget::customContextMenuRequested, &menu, [&](const QPoint &position) {
        requestedPoint = position;
        added = table.addChipColorAction(menu, position);
    });
    QContextMenuEvent event(QContextMenuEvent::Mouse, point, table.viewport()->mapToGlobal(point));
    QApplication::sendEvent(table.viewport(), &event);
    QVERIFY(added);
    QCOMPARE(requestedPoint, point);
    QCOMPARE(menu.actions().first()->objectName(), QString("changeChipColor"));
    QVERIFY(menu.actions().at(1)->isSeparator());
    QCOMPARE(menu.actions().last(), oldAction);
    table.picked = QColor("#123456");
    QSignalSpy changed(&MetadataChipColors::instance(), &MetadataChipColors::colorsChanged);
    menu.actions().first()->trigger();
    QTRY_COMPARE(table.requests, 1);
    QCOMPARE(table.initial, initial);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(MetadataChipDelegate::colorForValue(ComicModel::Tags, "Second"), table.picked);
    QCOMPARE(MetadataChipDelegate::colorForValue(ComicModel::Tags, "First"), first);
    MetadataChipColors::instance().reload();
    QCOMPARE(MetadataChipDelegate::colorForValue(ComicModel::Tags, "Second"), table.picked);
}
void ComicTableTest::cancelColorPickerDoesNotSave()
{
    QStandardItemModel model(1, ComicModel::ColumnCount);
    ColorPickerTable table;
    showTagTable(table, model);
    const auto rect = table.visualRect(model.index(0, ComicModel::Tags));
    const auto initial = MetadataChipDelegate::colorForValue(ComicModel::Tags, "First");
    QMenu menu;
    QVERIFY(table.addChipColorAction(menu, rect.topLeft() + QPoint(14, rect.height() / 2)));
    QSignalSpy changed(&MetadataChipColors::instance(), &MetadataChipColors::colorsChanged);
    menu.actions().first()->trigger();
    QTRY_COMPARE(table.requests, 1);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(MetadataChipDelegate::colorForValue(ComicModel::Tags, "First"), initial);
    QVERIFY(!QFileInfo::exists(YACReader::getSettingsPath() + "/YACReaderLibrary.ini"));
}
void ComicTableTest::colorOverridesPersistAndStayInTheirFieldAndProfile()
{
    auto &store = MetadataChipColors::instance();
    const auto profile = qgetenv("YACREADER_DATA_DIR");
    const QString value = QStringLiteral("a/b 中文标签");
    const auto original = store.color(ComicModel::Work, value);
    const QColor selected("#987654");
    QVERIFY(store.setColor(ComicModel::Tags, value, selected));
    store.reload();
    QCOMPARE(store.color(ComicModel::Tags, value), selected);
    QCOMPARE(store.color(ComicModel::Work, value), original);
    qputenv("YACREADER_DATA_DIR", profiles->filePath("another-profile").toUtf8());
    QCOMPARE(store.color(ComicModel::Tags, value), MetadataChipColors::defaultColor(ComicModel::Tags, value));
    qputenv("YACREADER_DATA_DIR", profile);
    QCOMPARE(store.color(ComicModel::Tags, value), selected);
    QVERIFY(!store.setColor(ComicModel::Title, value, selected));
    QVERIFY(!store.setColor(ComicModel::Tags, value, QColor()));
}
void ComicTableTest::colorsUseReadableForegrounds()
{
    QCOMPARE(MetadataChipColors::foregroundColor(QColor(Qt::black)), QColor(Qt::white));
    QCOMPARE(MetadataChipColors::foregroundColor(QColor(Qt::white)), QColor(Qt::black));
    QCOMPARE(MetadataChipColors::foregroundColor(QColor("#00ff00")), QColor(Qt::black));
    QCOMPARE(MetadataChipColors::foregroundColor(QColor("#0000ff")), QColor(Qt::white));
}
void ComicTableTest::blankChipCellDoesNotAddAColorAction()
{
    QStandardItemModel model(1, ComicModel::ColumnCount);
    ColorPickerTable table;
    showTagTable(table, model);
    const auto rect = table.visualRect(model.index(0, ComicModel::Tags));
    QMenu menu;
    auto original = menu.addAction("Open comic");
    QVERIFY(!table.addChipColorAction(menu, QPoint(rect.right() - 1, rect.center().y())));
    QCOMPARE(menu.actions(), QList<QAction *>({ original }));
}

void ComicTableTest::chipTextFitsWithoutUnnecessaryElision_data()
{
    QTest::addColumn<qreal>("pointSize");
    QTest::addColumn<QStringList>("values");
    const QList<QStringList> cases {
        { "lolicontent" },
        { "11223344556677889900" },
        { "WWW.123456789" },
        { "English tag 2026" },
        { QStringLiteral("中文English123混合") },
        { QStringLiteral("日常"), "lolicontent", "11223344556677889900" },
    };
    for (const qreal size : { 8.0, 8.5, 9.0, 9.5, 10.25, 11.0, 12.0 }) {
        for (int i = 0; i < cases.size(); ++i) {
            QTest::newRow(qPrintable(QString("font-%1-case-%2").arg(size).arg(i))) << size << cases.at(i);
        }
    }
}
void ComicTableTest::chipTextFitsWithoutUnnecessaryElision()
{
    QFETCH(qreal, pointSize);
    QFETCH(QStringList, values);
    QStandardItemModel model(1, ComicModel::ColumnCount);
    const auto index = model.index(0, ComicModel::Tags);
    model.setData(index, values, ComicModel::ChipValuesRole);
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 1200, 40);
    option.font = QFont(QStringLiteral("Segoe UI"));
    option.font.setPointSizeF(pointSize);
    option.font.setHintingPreference(QFont::PreferNoHinting);
    option.fontMetrics = QFontMetrics(option.font);
    TextRecordingDevice device;
    QPainter painter(&device);
    MetadataChipDelegate delegate;
    delegate.paint(&painter, option, index);
    painter.end();
    QCOMPARE(device.engine.text.join(QString()), values.join(QString()));
    QVERIFY2(!device.engine.textExceedsItsBox, "The rendered text does not fit inside its allocated chip text box");
}

void ComicTableTest::chipMeasurementUsesTheActualPaintFont()
{
    QStandardItemModel model(1, ComicModel::ColumnCount);
    const auto index = model.index(0, ComicModel::Tags);
    const QString value = "English 1122334455";
    model.setData(index, QStringList { value }, ComicModel::ChipValuesRole);
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 1200, 48);
    option.font = QFont(QStringLiteral("Segoe UI"), 14);
    // Model a style/DPI change that leaves the cached metrics on the old font.
    option.fontMetrics = QFontMetrics(QFont(QStringLiteral("Segoe UI"), 7));
    TextRecordingDevice device;
    QPainter painter(&device);
    MetadataChipDelegate delegate;
    delegate.paint(&painter, option, index);
    painter.end();
    QVERIFY(!device.engine.lastChip.isEmpty());
    QCOMPARE(device.engine.text.join(QString()), value);
    QVERIFY(!device.engine.textExceedsItsBox);
}

void ComicTableTest::narrowChipsStillElideAndKeepTheirFullValue()
{
    const QString full = "A very long English tag 11223344556677889900";
    QStandardItemModel model(1, ComicModel::ColumnCount);
    const auto index = model.index(0, ComicModel::Tags);
    model.setData(index, QStringList { full }, ComicModel::ChipValuesRole);
    QStyleOptionViewItem option;
    option.rect = QRect(0, 0, 70, 40);
    option.font = QFont(QStringLiteral("Segoe UI"), 10);
    option.fontMetrics = QFontMetrics(option.font);
    TextRecordingDevice device;
    QPainter painter(&device);
    MetadataChipDelegate delegate;
    delegate.paint(&painter, option, index);
    painter.end();
    QVERIFY(device.engine.text.join(QString()) != full);
    QVERIFY(device.engine.text.join(QString()).contains(QChar(0x2026)));
    QCOMPARE(delegate.valueAt(option, index, QPoint(15, 20)), full);
    option.rect.setWidth(1200);
    TextRecordingDevice widened;
    QPainter next(&widened);
    delegate.paint(&next, option, index);
    next.end();
    QCOMPARE(widened.engine.text.join(QString()), full);
}

void ComicTableTest::renderTable()
{
    QTranslator translator;
    if (translator.load(qEnvironmentVariable("COMIC_TABLE_TRANSLATION")))
        qApp->installTranslator(&translator);
    Theme theme;
    theme.comicsViewTable.starRatingColor = QColor(80, 100, 125);
    theme.comicsViewTable.starRatingSelectedColor = QColor(250, 250, 250);
    ThemeManager::instance().setTheme(theme);
    QStandardItemModel model(4, ComicModel::ColumnCount);
    headers(model);
    for (int row = 0; row < model.rowCount(); ++row) {
        const auto put = [&](int column, const QVariant &value) { model.setData(model.index(row, column), value); };
        put(ComicModel::Title, row == 0 ? QStringLiteral("蓝色午后") : row == 1 ? QStringLiteral("星空旅行")
                    : row == 2                                                  ? QStringLiteral("午后短篇集")
                                                                                : QStringLiteral("另一场冒险"));
        put(ComicModel::FileName, QStringLiteral("[示例社团 (作者甲)] 漫画标题 (示例作品) [中国翻訳].zip"));
        put(ComicModel::Author, row == 2 ? QStringLiteral("作者甲 | 作者乙") : QStringLiteral("作者甲"));
        put(ComicModel::Work, row == 1 ? QStringLiteral("原创") : QStringLiteral("示例作品"));
        put(ComicModel::Characters, QStringLiteral("角色甲 | 角色乙 | 另一个长名称的角色"));
        put(ComicModel::Tags, QStringLiteral("日常 | 冒险 | 温馨 | 彩色"));
        put(ComicModel::Color, row % 2 ? QStringLiteral("黑白") : QStringLiteral("彩色"));
        put(ComicModel::Censorship, QStringLiteral("未设置"));
        put(ComicModel::Translation, row % 2 ? QStringLiteral("日文") : QStringLiteral("中译"));
        put(ComicModel::CurrentPage, QString("%1/120").arg(row * 20));
        put(ComicModel::ReadColumn, QStringLiteral("否"));
        put(ComicModel::Size, "153.03 MB");
        model.setData(model.index(row, ComicModel::Rating), 3, ComicModel::RatingRole);
        model.setData(model.index(row, ComicModel::Work), QStringList { model.index(row, ComicModel::Work).data().toString() }, ComicModel::ChipValuesRole);
        model.setData(model.index(row, ComicModel::Characters), QStringList { QStringLiteral("角色甲"), QStringLiteral("角色乙"), QStringLiteral("另一个长名称的角色") }, ComicModel::ChipValuesRole);
        model.setData(model.index(row, ComicModel::Tags), QStringList { QStringLiteral("日常"), QStringLiteral("冒险"), QStringLiteral("温馨"), QStringLiteral("彩色") }, ComicModel::ChipValuesRole);
    }
    YACReaderTableView table;
    table.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
    table.setModel(&model);
    ComicTableLayout::reset(*table.horizontalHeader());
    table.verticalHeader()->hide();
    table.resize(1850, 240);
    table.selectRow(1);
    table.show();
    QCoreApplication::processEvents();
    const auto preview = qEnvironmentVariable("COMIC_TABLE_PREVIEW_PATH");
    if (!preview.isEmpty())
        QVERIFY(table.grab().save(preview));
    QVERIFY(table.itemDelegateForColumn(ComicModel::Tags));
    QCOMPARE(table.rowHeight(0), qMax(32, table.fontMetrics().height() + 14));
}
void ComicTableTest::filterPanelClicksSearchAndTheme()
{
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    qApp->setFont(QFont("Microsoft YaHei", 9));
    QTemporaryDir library;
    QVERIFY(library.isValid());
    const auto path = library.filePath("library.ydb");
    QString connection;
    {
        auto db = DataBaseManagement::createDatabase(path);
        connection = db.connectionName();
        QSqlQuery query(db);
        QVERIFY(query.exec("INSERT INTO comic_info (id, hash, title, tags, work, writer, color, censorship, translationStatus) VALUES (1, 'a', 'Example', 'daily' || char(10) || '1234567890' || char(10) || 'English tag', 'Work A', 'Author A', 0, 'uncensored', 'translated')"));
        QVERIFY(query.exec("INSERT INTO comic (id, parentId, comicInfoId, fileName, path) VALUES (1, 1, 1, 'example.cbz', '/example.cbz')"));
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    ComicModel model;
    model.setupFolderModelData(1, library.path());
    Theme theme;
    theme.sidebar.backgroundColor = QColor("#fafafa");
    theme.sidebar.titleTextColor = Qt::black;
    ThemeManager::instance().setTheme(theme);
    MetadataFilterPanel panel;
    panel.setModel(&model);
    panel.resize(300, 450);
    panel.show();
    QCoreApplication::processEvents();
    auto *page = panel.findChild<QWidget *>("metadataFilterChips0");
    QVERIFY(page);
    QTest::mouseClick(page, Qt::LeftButton, Qt::NoModifier, QPoint(20, 15));
    QCOMPARE(model.metadataFilter().tags.size(), 1);
    QCOMPARE(model.rowCount(), 1);
    const auto selected = model.metadataFilter().tags;
    auto *search = panel.findChild<QLineEdit *>("metadataFilterSearch");
    QVERIFY(search);
    search->setText("unmatched");
    QTest::mouseClick(page, Qt::LeftButton, Qt::NoModifier, QPoint(20, 15));
    QCOMPARE(model.metadataFilter().tags, selected);
    QCOMPARE(model.rowCount(), 1); // searching the vocabulary leaves the filter alone
    search->clear();
    auto *bw = panel.findChild<QPushButton *>("metadataFilter_bw");
    auto *color = panel.findChild<QPushButton *>("metadataFilter_color");
    auto *translated = panel.findChild<QPushButton *>("metadataFilter_translated");
    QVERIFY(bw && color && translated);
    bw->click();
    QVERIFY(bw->isChecked());
    color->click();
    QVERIFY(!bw->isChecked());
    QVERIFY(color->isChecked());
    QCOMPARE(model.rowCount(), 0);
    color->click();
    QVERIFY(!color->isChecked());
    QCOMPARE(model.rowCount(), 1);
    translated->click();
    QVERIFY(translated->isChecked());
    auto *doujinshi = panel.findChild<QPushButton *>("metadataFilter_doujinshi");
    auto *tankobon = panel.findChild<QPushButton *>("metadataFilter_tankobon");
    QVERIFY(doujinshi && tankobon);
    doujinshi->click();
    QCOMPARE(model.rowCount(), 0); // unset does not silently become a publication type
    tankobon->click();
    QVERIFY(!doujinshi->isChecked());
    QVERIFY(tankobon->isChecked());
    tankobon->click();
    QCOMPARE(model.rowCount(), 1);
    auto *favoriteOnly = panel.headerControls()->findChild<QAbstractButton *>("metadataFilterFavoritesOnly");
    auto *logic = panel.headerControls()->findChild<QToolButton *>("metadataFilterLogic");
    QVERIFY(favoriteOnly && logic);
    favoriteOnly->click();
    QVERIFY(model.metadataFilter().favoritesOnly);
    QCOMPARE(model.rowCount(), 0);
    favoriteOnly->click();
    QVERIFY(!model.metadataFilter().favoritesOnly);
    QCOMPARE(model.rowCount(), 1);
    logic->click();
    QVERIFY(!model.metadataFilter().matchAll);
    auto *choices = panel.findChild<QWidget *>("metadataFilterChoices");
    QVERIFY(choices);
    QVERIFY(choices->geometry().bottom() < search->geometry().top());
    auto *tabs = panel.findChild<QTabWidget *>();
    QVERIFY(tabs);
    QCOMPARE(tabs->count(), 4);
    auto *bar = tabs->findChild<QTabBar *>();
    QVERIFY(bar);
    QVERIFY(qAbs(bar->width() - tabs->width()) <= 2);
    QVERIFY(qAbs(bar->tabRect(0).width() - bar->tabRect(3).width()) <= 1);
    model.updateRating(5, model.index(0, 0));
    tabs->setCurrentIndex(3);
    auto *fiveStars = panel.findChild<QAbstractButton *>("metadataFilterRating5");
    auto *fourStars = panel.findChild<QAbstractButton *>("metadataFilterRating4");
    QVERIFY(fiveStars && fourStars);
    fiveStars->click();
    QCOMPARE(model.rowCount(), 1);
    fourStars->click();
    QCOMPARE(model.metadataFilter().ratings, QSet<int>({ 4, 5 }));
    fiveStars->click();
    QCOMPARE(model.rowCount(), 0);
    fourStars->click();
    QCOMPARE(model.rowCount(), 1);
    fiveStars->click();
    const auto preview = qEnvironmentVariable("METADATA_FILTER_PREVIEW_DIR");
    QTranslator translator;
    if (translator.load(qEnvironmentVariable("COMIC_TABLE_TRANSLATION"))) {
        qApp->installTranslator(&translator);
        // New widgets pick up the translated labels.
        MetadataFilterPanel translatedPanel;
        translatedPanel.setModel(&model);
        translatedPanel.resize(300, 450);
        translatedPanel.show();
        QCoreApplication::processEvents();
        if (!preview.isEmpty())
            QVERIFY(translatedPanel.grab().save(preview + "/filter-light.png"));
        translatedPanel.findChild<QTabWidget *>()->setCurrentIndex(3);
        QCoreApplication::processEvents();
        if (!preview.isEmpty())
            QVERIFY(translatedPanel.grab().save(preview + "/filter-rating-light.png"));
        theme.sidebar.backgroundColor = QColor("#242424");
        theme.sidebar.titleTextColor = Qt::white;
        ThemeManager::instance().setTheme(theme);
        QCoreApplication::processEvents();
        if (!preview.isEmpty())
            QVERIFY(translatedPanel.grab().save(preview + "/filter-rating-dark.png"));
        qApp->removeTranslator(&translator);
    }
    search->setText("hidden");
    auto filterBeforeReset = model.metadataFilter();
    filterBeforeReset.matchAll = false;
    filterBeforeReset.works = { "Missing work" };
    filterBeforeReset.authors = { "Missing author" };
    filterBeforeReset.censorship = "censored";
    filterBeforeReset.publicationType = "magazine";
    filterBeforeReset.favoritesOnly = true;
    model.setMetadataFilter(filterBeforeReset);
    QCOMPARE(model.rowCount(), 0);
    auto *reset = panel.findChild<QPushButton *>("metadataFilterReset");
    QVERIFY(reset);
    reset->click();
    QVERIFY(search->text().isEmpty());
    QVERIFY(!model.metadataFilter().active());
    QVERIFY(model.metadataFilter().matchAll);
    QVERIFY(!favoriteOnly->isChecked());
    QCOMPARE(logic->text(), QCoreApplication::translate("MetadataFilterPanel", "AND"));
    QVERIFY(!fiveStars->isChecked());
    QVERIFY(!translated->isChecked());
    QCOMPARE(model.rowCount(), 1);
    {
        auto db = DataBaseManagement::loadDatabase(library.path());
        connection = db.connectionName();
        QSqlQuery query(db);
        QStringList manyTags;
        for (int i = 0; i < 200; ++i)
            manyTags.append(QStringLiteral("Scroll %1").arg(i, 3, 10, QChar('0')));
        query.prepare("UPDATE comic_info SET tags = ? WHERE id = 1");
        query.addBindValue(manyTags.join('\n'));
        QVERIFY(query.exec());
    }
    QSqlDatabase::removeDatabase(connection);
    tabs->setCurrentIndex(0);
    model.reload();
    auto *area = qobject_cast<QScrollArea *>(page->parentWidget()->parentWidget());
    QVERIFY(area);
    QTRY_VERIFY(area->verticalScrollBar()->maximum() > 0);
    QVERIFY(!area->horizontalScrollBar()->isVisible());
    if (!preview.isEmpty())
        QVERIFY(panel.grab().save(preview + "/filter-many-tags-top.png"));
    area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum());
    QCoreApplication::processEvents();
    QTest::mouseClick(page, Qt::LeftButton, Qt::NoModifier, QPoint(15, page->minimumHeight() - 20));
    QCOMPARE(model.metadataFilter().tags.size(), 1);
    if (!preview.isEmpty())
        QVERIFY(panel.grab().save(preview + "/filter-many-tags-bottom.png"));
    search->setText("Scroll 199");
    QTRY_COMPARE(area->verticalScrollBar()->maximum(), 0);
    reset->click();
    QTRY_VERIFY(area->verticalScrollBar()->maximum() > 0);
}

void ComicTableTest::favoriteHeartClicksPersistAndShareAcrossFiles()
{
    QTemporaryDir library;
    QString connection;
    {
        auto db = DataBaseManagement::createDatabase(library.filePath("library.ydb"));
        connection = db.connectionName();
        QSqlQuery query(db);
        QVERIFY(query.exec("INSERT INTO comic_info(id,hash,title) VALUES(1,'shared','Favorite example')"));
        QVERIFY(query.exec("INSERT INTO comic(id,parentId,comicInfoId,fileName,path) VALUES(1,1,1,'a.cbz','/a.cbz'),(2,1,1,'b.cbz','/b.cbz')"));
    }
    QSqlDatabase::removeDatabase(connection);
    ComicModel model;
    model.setupFolderModelData(1, library.path());
    YACReaderTableView table;
    table.setModel(&model);
    ComicTableLayout::reset(*table.horizontalHeader());
    table.setColumnHidden(ComicModel::State, true);
    table.resize(600, 180);
    table.show();
    QCoreApplication::processEvents();
    const auto heart = model.index(0, ComicModel::Favorite);
    QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier, table.visualRect(heart).center());
    QVERIFY(model.index(0, 0).data(ComicModel::FavoriteRole).toBool());
    QVERIFY(model.index(1, 0).data(ComicModel::FavoriteRole).toBool());
    model.reload();
    QVERIFY(model.index(1, 0).data(ComicModel::FavoriteRole).toBool());
    const auto preview = qEnvironmentVariable("COMIC_FAVORITE_PREVIEW_PATH");
    if (!preview.isEmpty())
        QVERIFY(table.grab().save(preview));
    QTest::mouseClick(table.viewport(), Qt::LeftButton, Qt::NoModifier, table.visualRect(model.index(0, ComicModel::Favorite)).center());
    QVERIFY(!model.index(0, 0).data(ComicModel::FavoriteRole).toBool());
    model.setupFavoritesModelData(library.path());
    QCOMPARE(model.rowCount(), 0);
}

void ComicTableTest::oldHeaderKeepsWidthsWhenHeartIsAdded()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("old.ini"), QSettings::IniFormat);
    QStandardItemModel oldModel(0, ComicModel::ColumnCount - 1);
    QTableView oldTable;
    oldTable.setModel(&oldModel);
    auto *oldHeader = oldTable.horizontalHeader();
    oldHeader->setStretchLastSection(false);
    for (int i = 0; i < oldHeader->count(); ++i)
        oldHeader->hideSection(i);
    oldHeader->moveSection(oldHeader->visualIndex(ComicModel::State), 0);
    oldHeader->moveSection(oldHeader->visualIndex(ComicModel::Title), 1);
    oldHeader->showSection(ComicModel::Title);
    oldHeader->resizeSection(ComicModel::Title, 345);
    settings.setValue("COMICS_VIEW_HEADERS_MANGA_1", oldHeader->saveState());
    QStandardItemModel model(0, ComicModel::ColumnCount);
    QTableView table;
    table.setModel(&model);
    ComicTableLayout::restore(*table.horizontalHeader(), settings);
    QCOMPARE(displayed(*table.horizontalHeader()), QList<int>({ ComicModel::Favorite, ComicModel::Title }));
    QCOMPARE(table.columnWidth(ComicModel::Title), 345);
    QCOMPARE(table.columnWidth(ComicModel::Favorite), 40);
}

QTEST_MAIN(ComicTableTest)
#include "main.moc"
