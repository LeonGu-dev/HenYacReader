#ifndef FOLDER_DELETE_CONFIRMATION_H
#define FOLDER_DELETE_CONFIRMATION_H

#include "delete_confirmation.h"

namespace FolderDeleteConfirmation {
inline bool confirm(QWidget *parent, const QString &path)
{
    return DeleteConfirmation::confirm(parent,
                                       QCoreApplication::translate("LibraryWindow", "Delete folder"),
                                       QCoreApplication::translate("LibraryWindow", "The selected folder and all its contents will be deleted from your disk. Are you sure?"),
                                       QCoreApplication::translate("FolderDeleteConfirmation", "Folder: %1").arg(path),
                                       "folderDeleteConfirmation");
}
}
#endif
