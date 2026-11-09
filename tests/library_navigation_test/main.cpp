#include "yacreader_library_item_widget.h"
#include "yacreader_library_list_widget.h"

#include <QLabel>
#include <QSignalSpy>
#include <QTest>
#include <QToolButton>

class LibraryNavigationTest : public QObject
{
    Q_OBJECT
private slots:
    void clickingCurrentLibraryReturnsToRoot_data();
    void clickingCurrentLibraryReturnsToRoot();
    void switchingLibrariesStillLoadsTheNewLibrary();
    void otherClicksDoNotReturnToRoot();
};

void LibraryNavigationTest::clickingCurrentLibraryReturnsToRoot_data()
{
    QTest::addColumn<int>("libraryCount");
    QTest::newRow("single-library") << 1;
    QTest::newRow("multiple-libraries") << 2;
}

void LibraryNavigationTest::clickingCurrentLibraryReturnsToRoot()
{
    QFETCH(int, libraryCount);
    YACReaderLibraryListWidget list;
    list.addItem("A", "/comics");
    if (libraryCount == 2)
        list.addItem("B", "/other-comics");
    list.setCurrentIndex(0);
    list.resize(300, 150);
    list.show();
    QVERIFY(QTest::qWaitForWindowExposed(&list));
    QSignalSpy activated(&list, &YACReaderLibraryListWidget::currentLibraryActivated);
    QSignalSpy changed(&list, &YACReaderLibraryListWidget::currentIndexChanged);

    auto *item = list.findChildren<YACReaderLibraryItemWidget *>().first();
    // Click the name label: the mouse event must propagate through the row.
    auto *label = item->findChildren<QLabel *>().at(1);
    QTest::mouseClick(label, Qt::LeftButton);
    QCOMPARE(activated.count(), 1);
    QCOMPARE(changed.count(), 0);
    QCOMPARE(list.currentText(), QString("A"));
    QTest::mouseClick(label, Qt::LeftButton);
    QCOMPARE(activated.count(), 2);
}

void LibraryNavigationTest::switchingLibrariesStillLoadsTheNewLibrary()
{
    YACReaderLibraryListWidget list;
    list.addItem("A", "/comics");
    list.addItem("B", "/other-comics");
    list.setCurrentIndex(0);
    list.resize(300, 150);
    list.show();
    QVERIFY(QTest::qWaitForWindowExposed(&list));
    QSignalSpy activated(&list, &YACReaderLibraryListWidget::currentLibraryActivated);
    QSignalSpy changed(&list, &YACReaderLibraryListWidget::currentIndexChanged);
    auto *item = list.findChildren<YACReaderLibraryItemWidget *>().at(1);
    QTest::mouseClick(item, Qt::LeftButton);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.first().first().toString(), QString("B"));
    QCOMPARE(list.currentText(), QString("B"));
    QCOMPARE(activated.count(), 0);
}

void LibraryNavigationTest::otherClicksDoNotReturnToRoot()
{
    YACReaderLibraryListWidget list;
    list.addItem("A", "/comics");
    list.setCurrentIndex(0);
    list.resize(300, 150);
    list.show();
    QVERIFY(QTest::qWaitForWindowExposed(&list));
    QSignalSpy activated(&list, &YACReaderLibraryListWidget::currentLibraryActivated);
    QSignalSpy changed(&list, &YACReaderLibraryListWidget::currentIndexChanged);
    auto *item = list.findChild<YACReaderLibraryItemWidget *>();
    QTest::mouseClick(item, Qt::RightButton);
    QTest::mouseClick(&list, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    auto *options = item->findChild<QToolButton *>();
    QTest::mouseClick(options, Qt::LeftButton);
    QCOMPARE(activated.count(), 0);
    QCOMPARE(changed.count(), 0);
}

QTEST_MAIN(LibraryNavigationTest)
#include "main.moc"
