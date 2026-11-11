#ifndef EDIT_FILTER_SMART_LIST_DIALOG_H
#define EDIT_FILTER_SMART_LIST_DIALOG_H

#include "comic_model.h"
#include "data_base_management.h"
#include "metadata_filter_panel.h"

#include <QButtonGroup>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSqlQuery>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

// Owns a separate model: editing or cancelling never changes the main view's filter.
class EditFilterSmartListDialog : public QDialog
{
    Q_DECLARE_TR_FUNCTIONS(EditFilterSmartListDialog)
public:
    EditFilterSmartListDialog(const QString &databasePath, const QString &name, const QString &query, QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle(tr("Edit smart list"));
        selectedFolder = MetadataFilter::savedFolderId(query);
        QString connection;
        {
            auto db = DataBaseManagement::loadDatabase(databasePath);
            connection = db.connectionName();
            QSqlQuery rows(db);
            if (rows.exec("SELECT id, parentId, name, path FROM folder ORDER BY path, id"))
                while (rows.next()) {
                    const auto id = rows.value(0).toULongLong();
                    folders.insert(id, { rows.value(1).toULongLong(), rows.value(2).toString(), rows.value(3).toString() });
                }
        }
        QSqlDatabase::removeDatabase(connection);
        model = new ComicModel(this);
        model->setupFolderModelData(1, databasePath);
        model->setMetadataFilter(MetadataFilter::fromSavedQuery(query).value_or(MetadataFilter()));

        auto *outer = new QVBoxLayout(this);
        auto *scroll = new QScrollArea(this);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setObjectName("smartListEditorScroll");
        contents = new QWidget;
        auto *layout = new QVBoxLayout(contents);
        layout->setSizeConstraint(QLayout::SetMinAndMaxSize);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(10);
        layout->addWidget(new QLabel(tr("List name:"), contents));
        nameEdit = new QLineEdit(name, contents);
        nameEdit->setObjectName("editSmartListName");
        layout->addWidget(nameEdit);
        libraryScope = new QRadioButton(tr("Apply to library"), contents);
        libraryScope->setObjectName("editSmartListLibraryScope");
        folderScope = new QRadioButton(contents);
        folderScope->setObjectName("editSmartListFolderScope");
        auto *group = new QButtonGroup(this);
        group->addButton(libraryScope);
        group->addButton(folderScope);
        (selectedFolder ? folderScope : libraryScope)->setChecked(true);
        layout->addWidget(libraryScope);
        auto *folderRow = new QHBoxLayout;
        folderRow->addWidget(folderScope, 1);
        auto *choose = new QPushButton(tr("Choose folder"), contents);
        choose->setObjectName("editSmartListChooseFolder");
        choose->setAutoDefault(false);
        choose->setEnabled(!folders.isEmpty());
        folderRow->addWidget(choose);
        layout->addLayout(folderRow);
        invalidScope = new QLabel(tr("Choose a folder before saving."), contents);
        invalidScope->setObjectName("editSmartListInvalidScope");
        invalidScope->setWordWrap(true);
        layout->addWidget(invalidScope);
        auto *filterTitle = new QHBoxLayout;
        filterTitle->addWidget(new QLabel(tr("Filter"), contents), 1);
        auto *panel = new MetadataFilterPanel(contents, MetadataFilterPanel::Host::SmartListEditor);
        panel->setObjectName("smartListSharedFilterPanel");
        panel->layout()->setSizeConstraint(QLayout::SetMinAndMaxSize);
        panel->setModel(model);
        filterTitle->addWidget(panel->headerControls());
        panel->headerControls()->show();
        layout->addLayout(filterTitle);
        layout->addWidget(panel, 1);
        scroll->setWidget(contents);
        outer->addWidget(scroll, 1);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
        save = buttons->button(QDialogButtonBox::Save);
        outer->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        connect(nameEdit, &QLineEdit::textChanged, this, [this] { syncScope(); });
        connect(group, &QButtonGroup::buttonToggled, this, [this] { syncScope(); });
        connect(choose, &QPushButton::clicked, this, [this] { chooseFolder(); });
        syncScope();
        contents->installEventFilter(this);
        fitToContents();
    }

    QString listName() const { return nameEdit->text().trimmed(); }
    QString savedQuery() const { return model->metadataFilter().savedQuery(folderScope->isChecked() ? selectedFolder : std::nullopt); }
    void accept() override
    {
        if (save->isEnabled())
            QDialog::accept();
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object == contents && event->type() == QEvent::LayoutRequest && !fitPending) {
            fitPending = true;
            QTimer::singleShot(0, this, [this] {
                fitPending = false;
                fitToContents();
            });
        }
        return QDialog::eventFilter(object, event);
    }
    void showEvent(QShowEvent *event) override
    {
        QDialog::showEvent(event);
        lastHint = { };
        fitToContents();
    }

private:
    struct FolderEntry {
        qulonglong parent;
        QString name, path;
    };
    QMap<qulonglong, FolderEntry> folders;
    std::optional<qulonglong> selectedFolder;
    ComicModel *model;
    QLineEdit *nameEdit;
    QRadioButton *libraryScope;
    QRadioButton *folderScope;
    QLabel *invalidScope;
    QPushButton *save;
    QWidget *contents;
    QSize lastHint;
    bool fitPending = false;

    QString folderName(qulonglong id) const
    {
        if (!folders.contains(id))
            return tr("Folder unavailable");
        auto path = folders.value(id).path;
        path.replace('\\', '/');
        path.replace(QRegularExpression("/+"), "/");
        return path.isEmpty() || path == "/" ? tr("Library root") : path;
    }
    void syncScope()
    {
        const bool scoped = folderScope->isChecked();
        folderScope->setText(tr("Apply to folder: %1").arg(scoped && selectedFolder ? folderName(*selectedFolder) : tr("None")));
        const bool valid = !scoped || (selectedFolder && folders.contains(*selectedFolder));
        invalidScope->setVisible(!valid);
        save->setEnabled(valid && !listName().isEmpty());
    }
    void fitToContents()
    {
        const auto hint = contents->sizeHint();
        if (hint == lastHint)
            return;
        lastHint = hint;
        const auto bounds = screen()->availableGeometry().size() * 0.9;
        setMaximumSize(bounds);
        resize(qMin(qMax(520, hint.width() + 40), bounds.width()), qMin(hint.height() + 80, bounds.height()));
    }
    void chooseFolder()
    {
        QDialog picker(this);
        picker.setWindowTitle(tr("Choose folder"));
        auto *layout = new QVBoxLayout(&picker);
        auto *tree = new QTreeWidget(&picker);
        tree->setObjectName("smartListFolderTree");
        tree->setHeaderHidden(true);
        tree->setSelectionMode(QAbstractItemView::SingleSelection);
        QMap<qulonglong, QTreeWidgetItem *> items;
        for (auto it = folders.cbegin(); it != folders.cend(); ++it) {
            auto *item = new QTreeWidgetItem;
            item->setText(0, it->path.isEmpty() || it->path == "/" || it->parent == it.key() ? tr("Library root") : it->name);
            item->setData(0, Qt::UserRole, QVariant::fromValue(it.key()));
            item->setToolTip(0, folderName(it.key()));
            items.insert(it.key(), item);
        }
        for (auto it = folders.cbegin(); it != folders.cend(); ++it) {
            auto *item = items.value(it.key());
            if (items.contains(it->parent) && it->parent != it.key())
                items.value(it->parent)->addChild(item);
            else
                tree->addTopLevelItem(item);
        }
        tree->expandAll();
        if (selectedFolder && items.contains(*selectedFolder))
            tree->setCurrentItem(items.value(*selectedFolder));
        layout->addWidget(tree);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &picker);
        auto *ok = buttons->button(QDialogButtonBox::Ok);
        ok->setEnabled(tree->currentItem() != nullptr);
        connect(tree, &QTreeWidget::currentItemChanged, &picker, [ok](QTreeWidgetItem *item) { ok->setEnabled(item != nullptr); });
        connect(buttons, &QDialogButtonBox::accepted, &picker, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &picker, &QDialog::reject);
        connect(tree, &QTreeWidget::itemDoubleClicked, &picker, [&picker] { picker.accept(); });
        layout->addWidget(buttons);
        picker.resize(420, 440);
        if (picker.exec() == QDialog::Accepted && tree->currentItem()) {
            selectedFolder = tree->currentItem()->data(0, Qt::UserRole).toULongLong();
            folderScope->setChecked(true);
            syncScope();
        }
    }
};
#endif
