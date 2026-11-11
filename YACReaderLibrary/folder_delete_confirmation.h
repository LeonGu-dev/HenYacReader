#ifndef FOLDER_DELETE_CONFIRMATION_H
#define FOLDER_DELETE_CONFIRMATION_H

#include <QCoreApplication>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>

namespace FolderDeleteConfirmation {
inline bool ask(QWidget *parent, const QString &message, const QString &path = { })
{
    QMessageBox box(QMessageBox::Warning,
                    QCoreApplication::translate("LibraryWindow", "Delete folder"),
                    message, QMessageBox::Yes | QMessageBox::No, parent);
    box.setObjectName("folderDeleteConfirmation");
    box.setOption(QMessageBox::Option::DontUseNativeDialog);
    box.setTextFormat(Qt::PlainText);
    if (!path.isEmpty())
        box.setInformativeText(QCoreApplication::translate("FolderDeleteConfirmation", "Folder: %1").arg(path));
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

inline bool confirm(QWidget *parent, const QString &path)
{
    if (!ask(parent, QCoreApplication::translate("LibraryWindow", "The selected folder and all its contents will be deleted from your disk. Are you sure?"), path))
        return false;
    return ask(parent, QCoreApplication::translate("FolderDeleteConfirmation", "This operation cannot be undone. Are you really sure?"));
}
}
#endif
