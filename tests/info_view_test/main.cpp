#include "comic_info_object.h"
#include "comic_model.h"
#include "metadata_chip_colors.h"

#include <QApplication>
#include <QDir>
#include <QFontDatabase>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>

class InfoHelper : public QObject
{
    Q_OBJECT
public:
    ComicInfoObject *info = nullptr;
    Q_INVOKABLE void addToFavorites(int row)
    {
        info->setFavorite(true);
        emit changed(row, "favorite", true);
    }
    Q_INVOKABLE void removeFromFavorites(int row)
    {
        info->setFavorite(false);
        emit changed(row, "favorite", false);
    }
    Q_INVOKABLE void rate(int row, int rating)
    {
        info->setRating(rating);
        emit changed(row, "rating", rating);
    }
    Q_INVOKABLE void setRead(int row, bool read)
    {
        info->setRead(read);
        emit changed(row, "read", read);
    }
signals:
    void changed(int row, const QString &field, const QVariant &value);
};

class InfoViewTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void compactLayout_data();
    void compactLayout();
    void controlsAndColorSynchronization();
    void manyTagsRemainScrollable();
    void gridBanner_data();
    void gridBanner();

private:
    QTemporaryDir settings;
    QTranslator translator;
};

static ComicDB sample()
{
    ComicDB comic;
    comic.file.path = QString::fromUtf8("/示例文件夹/(C107) [青空工作室 (作者甲)] 星空旅行記.cbz");
    comic.info.title = QString::fromUtf8("星空旅行記");
    comic.info.translatedTitle = QString::fromUtf8("星空旅行记");
    comic.info.writer = QString::fromUtf8("作者甲\n作者乙");
    comic.info.isAnthology = true;
    comic.info.circle = QString::fromUtf8("青空工作室");
    comic.info.convention = "C107";
    comic.info.work = QString::fromUtf8("星之旅");
    comic.info.characters = QString::fromUtf8("角色甲\n角色乙");
    comic.info.tags = QString::fromUtf8("日常\n冒险\nEnglish-tag\n11234\n一个用于检查自动换行的比较长的标签");
    comic.info.translator = QString::fromUtf8("示例汉化组");
    comic.info.publicationType = "doujinshi";
    comic.info.color = true;
    comic.info.censorship = "uncensored";
    comic.info.translationStatus = "translated";
    comic.info.rating = 4;
    comic.info.currentPage = 38;
    comic.info.numPages = 126;
    comic.info.hash = QString(40, 'a') + "153030000";
    return comic;
}

static QList<QQuickItem *> visualChildren(QQuickItem *item)
{
    QList<QQuickItem *> children;
    for (auto *child : item->childItems()) {
        children.append(child);
        children.append(visualChildren(child));
    }
    return children;
}

static void context(QQmlContext *context, ComicInfoObject &info, InfoHelper &helper, bool dark)
{
    helper.info = &info;
    context->setContextProperty("comicInfo", &info);
    context->setContextProperty("comicInfoHelper", &helper);
    context->setContextProperty("comic_info_index", 7);
    context->setContextProperty("metadataChipColors", &MetadataChipColors::instance());
    context->setContextProperty("infoTextColor", QColor(dark ? "#eeeeee" : "#242424"));
    context->setContextProperty("infoMetadataTextColor", QColor(dark ? "#bbbbbb" : "#666666"));
    context->setContextProperty("favCheckedColor", QColor("#ea526c"));
    context->setContextProperty("favUncheckedColor", QColor("#999999"));
    context->setContextProperty("ratingSelectedColor", QColor("#e6b900"));
    context->setContextProperty("ratingUnselectedColor", QColor("#b4b4b4"));
}

void InfoViewTest::initTestCase()
{
    QVERIFY(settings.isValid());
    qputenv("YACREADER_DATA_DIR", settings.path().toUtf8());
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    QApplication::setFont(QFont("Microsoft YaHei", 10));
    if (translator.load(qEnvironmentVariable("COMIC_TABLE_TRANSLATION")))
        qApp->installTranslator(&translator);
}

void InfoViewTest::compactLayout_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<bool>("dark");
    QTest::addColumn<bool>("empty");
    QTest::newRow("wide-light") << 900 << false << false;
    QTest::newRow("narrow-dark") << 320 << true << false;
    QTest::newRow("wide-dark") << 900 << true << false;
    QTest::newRow("empty") << 320 << false << true;
}

void InfoViewTest::compactLayout()
{
    QFETCH(int, width);
    QFETCH(bool, dark);
    QFETCH(bool, empty);
    ComicInfoObject info(ComicRecordCase::Comic { empty ? ComicDB() : sample() }, true);
    InfoHelper helper;
    QQuickView view;
    context(view.rootContext(), info, helper, dark);
    QSignalSpy warnings(view.engine(), &QQmlEngine::warnings);
    QQmlComponent component(view.engine(), QUrl::fromLocalFile(QStringLiteral(INFO_QML_DIR "/ComicInfoView.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem *>(component.create()));
    QVERIFY2(item != nullptr, qPrintable(component.errorString()));
    item->setParentItem(view.contentItem());
    item->setWidth(width);
    QTRY_VERIFY(item->height() > 100);
    view.resize(width, qCeil(item->height()));
    view.setColor(QColor(dark ? "#242424" : "#fafafa"));
    view.show();
    QTest::qWait(150);
    QVERIFY(item->findChild<QObject *>("detailTitle"));
    const auto children = visualChildren(item.get());
    const bool hasConvention = std::any_of(children.cbegin(), children.cend(), [](auto *child) { return child->objectName() == "detailRow_convention"; });
    QCOMPARE(hasConvention, !empty);
    for (auto *child : children) {
        if (!child->isVisible() || !child->parentItem() || child->width() <= 0)
            continue;
        QVERIFY2(child->x() >= -1 && child->x() + child->width() <= child->parentItem()->width() + 1,
                 qPrintable(QString("Clipped %1: x=%2 width=%3 parentWidth=%4").arg(child->metaObject()->className()).arg(child->x()).arg(child->width()).arg(child->parentItem()->width())));
    }
    auto *toggle = item->findChild<QObject *>("detailFileToggle");
    QVERIFY(toggle);
    const qreal collapsed = item->height();
    QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
    QTRY_VERIFY(item->height() > collapsed);
    QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
    QTRY_COMPARE(item->height(), collapsed);
    QCOMPARE(warnings.count(), 0);
    const auto previews = qEnvironmentVariable("INFO_VIEW_PREVIEW_DIR");
    if (!previews.isEmpty()) {
        QDir().mkpath(previews);
        view.resize(width, qCeil(item->height()));
        QTest::qWait(100);
        QVERIFY(view.grabWindow().save(previews + '/' + QTest::currentDataTag() + ".png"));
    }
}

void InfoViewTest::controlsAndColorSynchronization()
{
    ComicInfoObject info(ComicRecordCase::Comic { sample() }, false);
    InfoHelper helper;
    QSignalSpy changed(&helper, &InfoHelper::changed);
    QQuickView view;
    context(view.rootContext(), info, helper, false);
    QQmlComponent component(view.engine(), QUrl::fromLocalFile(QStringLiteral(INFO_QML_DIR "/ComicInfoView.qml")));
    std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem *>(component.create()));
    QVERIFY2(item != nullptr, qPrintable(component.errorString()));
    item->setParentItem(view.contentItem());
    item->setWidth(760);
    auto *favorite = item->findChild<QObject *>("detailFavorite");
    QVERIFY(favorite);
    QVERIFY(QMetaObject::invokeMethod(favorite, "activeChangedByUser", Q_ARG(bool, true)));
    QVERIFY(info.isFavorite());
    auto *rating = item->findChild<QObject *>("detailRating");
    QVERIFY(QMetaObject::invokeMethod(rating, "ratingChangedByUser", Q_ARG(int, 5)));
    QCOMPARE(info.rating(), 5);
    QVERIFY(QMetaObject::invokeMethod(rating, "ratingChangedByUser", Q_ARG(int, 5)));
    QCOMPARE(info.rating(), 0);
    auto *read = item->findChild<QObject *>("detailRead");
    QVERIFY(QMetaObject::invokeMethod(read, "clicked"));
    QVERIFY(info.read());
    QCOMPARE(changed.count(), 4);
    for (const auto &call : changed)
        QCOMPARE(call[0].toInt(), 7);
    auto &colors = MetadataChipColors::instance();
    const auto revision = item->property("colorRevision").toInt();
    QVERIFY(colors.setColor(ComicModel::Convention, "C107", QColor("#123456")));
    QTRY_VERIFY(item->property("colorRevision").toInt() > revision);
    QCOMPARE(colors.colorsForField("convention", "C107").value("background").value<QColor>(), QColor("#123456"));
}

void InfoViewTest::manyTagsRemainScrollable()
{
    auto comic = sample();
    QStringList tags;
    for (int n = 0; n < 100; ++n)
        tags.append(QString("Long-English-tag-%1").arg(n));
    comic.info.tags = tags.join('\n');
    ComicInfoObject info(ComicRecordCase::Comic { comic }, false);
    InfoHelper helper;
    QQuickView view;
    context(view.rootContext(), info, helper, false);
    QSignalSpy warnings(view.engine(), &QQmlEngine::warnings);
    QQmlComponent component(view.engine());
    component.setData(R"(
        import QtQuick
        Item {
            width: 320; height: 360
            Flickable {
                id: viewport
                objectName: "viewport"
                anchors.fill: parent
                clip: true
                contentWidth: width
                contentHeight: details.height
                ComicInfoView { id: details; width: viewport.width }
            }
        }
    )",
                      QUrl::fromLocalFile(QStringLiteral(INFO_QML_DIR "/Harness.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem *>(component.create()));
    QVERIFY2(item != nullptr, qPrintable(component.errorString()));
    item->setParentItem(view.contentItem());
    auto *viewport = item->findChild<QQuickItem *>("viewport");
    QVERIFY(viewport);
    view.resize(320, 360);
    view.show();
    QTRY_VERIFY(viewport->property("contentHeight").toReal() > 2000);
    const auto bottom = viewport->property("contentHeight").toReal() - viewport->height();
    QVERIFY(viewport->setProperty("contentY", bottom));
    auto *footer = item->findChild<QQuickItem *>("detailFileToggle");
    QVERIFY(footer);
    QTRY_VERIFY(footer->mapToItem(item.get(), QPointF()).y() >= 0);
    QVERIFY(footer->mapToItem(item.get(), QPointF()).y() + footer->height() <= viewport->height());
    QCOMPARE(warnings.count(), 0);
}

void InfoViewTest::gridBanner_data()
{
    QTest::addColumn<int>("width");
    QTest::addColumn<bool>("dark");
    QTest::addColumn<bool>("landscape");
    QTest::addColumn<bool>("empty");
    QTest::newRow("banner-wide") << 1100 << false << false << false;
    QTest::newRow("banner-medium-dark") << 680 << true << true << false;
    QTest::newRow("banner-narrow") << 320 << false << false << false;
    QTest::newRow("banner-empty") << 680 << false << true << true;
}

void InfoViewTest::gridBanner()
{
    QFETCH(int, width);
    QFETCH(bool, dark);
    QFETCH(bool, landscape);
    QFETCH(bool, empty);
    auto comic = empty ? ComicDB() : sample();
    if (!empty) {
        QStringList tags;
        for (int n = 0; n < 20; ++n)
            tags.append(QString("English-tag-%1").arg(n));
        comic.info.tags = tags.join('\n');
    }
    ComicInfoObject info(ComicRecordCase::Comic { comic }, true);
    InfoHelper helper;
    QQuickView view;
    context(view.rootContext(), info, helper, dark);
    view.rootContext()->setContextProperty("buttonColor", QColor("#ffc800"));
    view.rootContext()->setContextProperty("buttonTextColor", QColor("#242424"));
    QSignalSpy warnings(view.engine(), &QQmlEngine::warnings);
    QQmlComponent component(view.engine(), QUrl::fromLocalFile(QStringLiteral(INFO_QML_DIR "/CurrentComicBanner.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QQuickItem> item(qobject_cast<QQuickItem *>(component.create()));
    QVERIFY2(item != nullptr, qPrintable(component.errorString()));
    item->setParentItem(view.contentItem());
    item->setWidth(width);
    item->setProperty("infoObject", QVariant::fromValue(&info));
    item->setProperty("sourceIndex", 13); // Must not use the selected card's context row (7).
    QColor surface(dark ? "#242424" : "#ffffff");
    surface.setAlphaF(0.92);
    item->setProperty("surfaceColor", surface);
    QImage cover(landscape ? 400 : 200, landscape ? 200 : 300, QImage::Format_RGB32);
    cover.fill(QColor("#c7dccf"));
    const QString coverPath = settings.filePath("banner-cover.png");
    QVERIFY(cover.save(coverPath));
    item->setProperty("coverSource", QUrl::fromLocalFile(coverPath));
    QTRY_VERIFY(item->height() > 200);
    view.resize(width, qCeil(item->height()));
    // Opposite background stresses the contrast protection against dark/light covers.
    view.setColor(QColor(dark ? "#f0f0f0" : "#202020"));
    view.show();
    QTest::qWait(150);
    for (auto *child : visualChildren(item.get())) {
        if (!child->isVisible() || !child->parentItem() || child->width() <= 0)
            continue;
        QVERIFY2(child->x() >= -1 && child->x() + child->width() <= child->parentItem()->width() + 1,
                 qPrintable(QString("Banner clipped %1 (%2): x=%3 width=%4 parent=%5 %6 grandparent=%7").arg(child->objectName(), child->metaObject()->className()).arg(child->x()).arg(child->width()).arg(child->parentItem()->width()).arg(child->parentItem()->objectName()).arg(child->parentItem()->parentItem() ? child->parentItem()->parentItem()->objectName() : "none")));
    }
    auto *details = item->findChild<QQuickItem *>("bannerDetails");
    QVERIFY(details);
    QVERIFY(!details->findChild<QQuickItem *>("detailFileToggle")->isVisible());
    if (!empty) {
        auto *tagsToggle = details->findChild<QObject *>("detailTagsToggle");
        // Repeater delegates are visual children, not necessarily QObject children.
        for (auto *child : visualChildren(details))
            if (child->objectName() == "detailTagsToggle" && child->isVisible())
                tagsToggle = child;
        QVERIFY(tagsToggle);
        const qreal collapsed = item->height();
        QVERIFY(QMetaObject::invokeMethod(tagsToggle, "clicked"));
        QTRY_VERIFY(item->height() > collapsed);
        QVERIFY(QMetaObject::invokeMethod(tagsToggle, "clicked"));
        QTRY_COMPARE(item->height(), collapsed);
        QSignalSpy changed(&helper, &InfoHelper::changed);
        QVERIFY(QMetaObject::invokeMethod(details->findChild<QObject *>("detailFavorite"), "activeChangedByUser", Q_ARG(bool, false)));
        QVERIFY(QMetaObject::invokeMethod(details->findChild<QObject *>("detailRating"), "ratingChangedByUser", Q_ARG(int, 5)));
        QVERIFY(QMetaObject::invokeMethod(details->findChild<QObject *>("detailRead"), "clicked"));
        QCOMPARE(changed.count(), 3);
        for (const auto &call : changed)
            QCOMPARE(call[0].toInt(), 13);
        QSignalSpy open(item.get(), SIGNAL(openRequested()));
        QVERIFY(QMetaObject::invokeMethod(item->findChild<QObject *>("bannerContinueReading"), "clicked"));
        QCOMPARE(open.count(), 1);
    }
    QCOMPARE(warnings.count(), 0);
    const auto previews = qEnvironmentVariable("INFO_VIEW_PREVIEW_DIR");
    if (!previews.isEmpty()) {
        QDir().mkpath(previews);
        view.resize(width, qCeil(item->height()));
        QTest::qWait(100);
        QVERIFY(view.grabWindow().save(previews + '/' + QTest::currentDataTag() + ".png"));
    }
    item->setProperty("sectionVisible", false);
    QTRY_COMPARE(item->height(), 20.0);
}

QTEST_MAIN(InfoViewTest)
#include "main.moc"
