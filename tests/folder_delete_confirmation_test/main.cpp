#include "folder_delete_confirmation.h"

#include <QApplication>
#include <QPushButton>
#include <QTest>
#include <QTimer>

class FolderDeleteConfirmationTest : public QObject
{
    Q_OBJECT
private slots:
    void confirmationGates_data();
    void confirmationGates();
};

void FolderDeleteConfirmationTest::confirmationGates_data()
{
    QTest::addColumn<bool>("dark");
    QTest::addColumn<bool>("firstYes");
    QTest::addColumn<int>("secondChoice");
    QTest::addColumn<bool>("expected");
    QTest::addColumn<bool>("comic");
    for (const bool comic : { false, true }) {
        const auto prefix = comic ? QByteArray("comic-") : QByteArray("folder-");
        QTest::newRow((prefix + "cancel-first").constData()) << false << false << 0 << false << comic;
        QTest::newRow((prefix + "cancel-second").constData()) << false << true << 0 << false << comic;
        QTest::newRow((prefix + "both-confirmed").constData()) << false << true << 1 << true << comic;
        QTest::newRow((prefix + "escape-second-dark").constData()) << true << true << 2 << false << comic;
        QTest::newRow((prefix + "close-second").constData()) << false << true << 3 << false << comic;
    }
}

void FolderDeleteConfirmationTest::confirmationGates()
{
    QFETCH(bool, dark);
    QFETCH(bool, firstYes);
    QFETCH(int, secondChoice);
    QFETCH(bool, expected);
    QFETCH(bool, comic);
    QWidget parent;
    auto palette = parent.palette();
    palette.setColor(QPalette::Window, QColor(dark ? "#242424" : "#fafafa"));
    palette.setColor(QPalette::WindowText, QColor(dark ? "#eeeeee" : "#242424"));
    parent.setPalette(palette);
    const QString path = QStringLiteral("D:/Test/<folder & name>");
    int dialogs = 0;
    QString error;
    QTimer timer;
    connect(&timer, &QTimer::timeout, this, [&] {
        auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (!box)
            return;
        auto check = [&error](bool condition, const QString &message) {
            if (!condition && error.isEmpty())
                error = message;
        };
        check(box->objectName() == (comic ? "comicDeleteConfirmation" : "folderDeleteConfirmation"), "Unexpected dialog");
        check(box->defaultButton() == box->button(QMessageBox::No), "Default must be No");
        check(box->escapeButton() == box->button(QMessageBox::No), "Escape must cancel");
        check(box->textFormat() == Qt::PlainText, "Path and message must remain plain text");
        auto *warning = box->findChild<QLabel *>("qt_msgbox_label");
        check(warning != nullptr, "Missing warning label");
        if (warning) {
            const auto red = warning->palette().color(QPalette::WindowText);
            check(red.red() > red.green() * 1.5 && red.red() > red.blue() * 1.5, "Warning must be red");
        }
        const bool first = dialogs++ == 0;
        if (first) {
            if (comic) {
                check(box->text() == "The selected comics will be deleted from your disk. Are you sure?", "Wrong comic warning");
                check(box->informativeText().isEmpty(), "Unexpected folder details for comics");
            } else {
                check(box->informativeText().contains(path), "Missing folder path");
                auto *folder = box->findChild<QLabel *>("qt_msgbox_informativelabel");
                check(folder != nullptr, "Missing path label");
                if (folder)
                    check(folder->palette().color(QPalette::WindowText) == box->palette().color(QPalette::WindowText), "Path color changed");
            }
            box->button(firstYes ? QMessageBox::Yes : QMessageBox::No)->click();
        } else {
            check(box->text() == "This operation cannot be undone. Are you really sure?", "Wrong second confirmation");
            check(box->informativeText().isEmpty(), "Second prompt must not repeat the path");
            if (secondChoice == 2)
                QTest::keyClick(box, Qt::Key_Escape);
            else if (secondChoice == 3)
                box->close();
            else
                box->button(secondChoice == 1 ? QMessageBox::Yes : QMessageBox::No)->click();
        }
    });
    timer.start(1);
    const auto confirmed = comic
            ? DeleteConfirmation::confirm(&parent, "Delete comics", "The selected comics will be deleted from your disk. Are you sure?", { }, "comicDeleteConfirmation")
            : FolderDeleteConfirmation::confirm(&parent, path);
    QCOMPARE(confirmed, expected);
    timer.stop();
    QCOMPARE(dialogs, firstYes ? 2 : 1);
    QVERIFY2(error.isEmpty(), qPrintable(error));
}

QTEST_MAIN(FolderDeleteConfirmationTest)
#include "main.moc"
