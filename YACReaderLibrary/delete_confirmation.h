#ifndef DELETE_CONFIRMATION_H
#define DELETE_CONFIRMATION_H

#include <QCoreApplication>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>

namespace DeleteConfirmation {
inline bool ask(QWidget *parent, const QString &title, const QString &message, const QString &details, const char *objectName)
{
    QMessageBox box(QMessageBox::Warning, title, message, QMessageBox::Yes | QMessageBox::No, parent);
    box.setObjectName(QString::fromLatin1(objectName));
    box.setOption(QMessageBox::Option::DontUseNativeDialog);
    box.setTextFormat(Qt::PlainText);
    if (!details.isEmpty())
        box.setInformativeText(details);
    box.setDefaultButton(QMessageBox::No);
    box.setEscapeButton(QMessageBox::No);
    if (auto *label = box.findChild<QLabel *>("qt_msgbox_label")) {
        auto palette = label->palette();
        palette.setColor(QPalette::WindowText, box.palette().color(QPalette::Window).lightnessF() < 0.5 ? QColor("#f16a72") : QColor("#c93742"));
        label->setPalette(palette);
        label->setStyleSheet(QStringLiteral("color: %1;").arg(palette.color(QPalette::WindowText).name()));
    }
    return box.exec() == QMessageBox::Yes;
}

inline bool confirm(QWidget *parent, const QString &title, const QString &message, const QString &details = { }, const char *objectName = "deleteConfirmation")
{
    if (!ask(parent, title, message, details, objectName))
        return false;
    return ask(parent, title, QCoreApplication::translate("DeleteConfirmation", "This operation cannot be undone. Are you really sure?"), { }, objectName);
}
}
#endif
