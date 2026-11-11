#ifndef SAVE_FILTER_SMART_LIST_DIALOG_H
#define SAVE_FILTER_SMART_LIST_DIALOG_H

#include <QButtonGroup>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <optional>

class SaveFilterSmartListDialog : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(SaveFilterSmartListDialog)
public:
    explicit SaveFilterSmartListDialog(const std::optional<QString> &folderPath, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr("Save filter as smart list"));
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(10);
        layout->addWidget(new QLabel(tr("List name:"), this));
        nameEdit = new QLineEdit(this);
        nameEdit->setObjectName("smartListName");
        layout->addWidget(nameEdit);
        auto *row = new QGridLayout;
        row->setColumnStretch(0, 1);
        row->setHorizontalSpacing(16);
        row->setVerticalSpacing(6);
        auto *library = new QRadioButton(tr("Entire library"), this);
        library->setObjectName("smartListLibraryScope");
        folder = new QRadioButton(tr("Current folder"), this);
        folder->setObjectName("smartListFolderScope");
        auto *group = new QButtonGroup(this);
        group->setExclusive(true);
        for (auto *button : { library, folder }) {
            button->setCheckable(true);
            group->addButton(button);
        }
        row->addWidget(library, 0, 0, Qt::AlignTop);
        row->addWidget(folder, 1, 0, Qt::AlignTop);
        folder->setEnabled(folderPath.has_value());
        (folderPath ? folder : library)->setChecked(true);
        layout->addLayout(row);
        auto folderText = folderPath.value_or(QString()).trimmed();
        folderText.replace('\\', '/');
        folderText.replace(QRegularExpression("/+"), "/");
        if (folderText.isEmpty() || folderText == "/")
            folderText = tr("Library");
        auto *path = new QLabel(tr("Current folder: %1").arg(folderText), this);
        path->setObjectName("smartListFolderDescription");
        path->setWordWrap(true);
        path->setTextFormat(Qt::PlainText);
        layout->addWidget(path);
        if (!folderPath) {
            auto *reason = new QLabel(tr("Currently in a reading list; the current folder scope is unavailable."), this);
            reason->setObjectName("smartListFolderUnavailable");
            reason->setWordWrap(true);
            row->addWidget(reason, 2, 0, Qt::AlignTop);
        }
        auto *hint = new QLabel(tr("Tip: Smart filter lists are based on a folder or library, not a reading list."), this);
        hint->setWordWrap(true);
        layout->addWidget(hint);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
        auto *save = buttons->button(QDialogButtonBox::Save);
        save->setEnabled(false);
        connect(nameEdit, &QLineEdit::textChanged, this, [save](const QString &name) { save->setEnabled(!name.trimmed().isEmpty()); });
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
        resize(400, sizeHint().height());
        nameEdit->setFocus();
    }
    QString listName() const { return nameEdit->text().trimmed(); }
    bool currentFolderOnly() const { return folder->isChecked(); }

private:
    QLineEdit *nameEdit;
    QRadioButton *folder;
};
#endif
