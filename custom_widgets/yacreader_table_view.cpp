#include "yacreader_table_view.h"

#include "QsLog.h"
#include "comic_model.h"
#include "cover_utils.h"
#include "favorite_heart.h"
#include "metadata_chip_colors.h"
#include "metadata_chip_delegate.h"
#include "yacreader_global_gui.h"

#include <QApplication>
#include <QBuffer>
#include <QColorDialog>
#include <QDrag>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QResizeEvent>

namespace {
class FavoriteDelegate : public QStyledItemDelegate
{
public:
    explicit FavoriteDelegate(QObject *parent) : QStyledItemDelegate(parent) { }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        auto item = option;
        initStyleOption(&item, index);
        item.text.clear();
        (item.widget ? item.widget->style() : QApplication::style())->drawControl(QStyle::CE_ItemViewItem, &item, painter, item.widget);
        paintFavoriteHeart(*painter, option.rect, index.data(ComicModel::FavoriteRole).toBool());
    }
};
}

YACReaderTableView::YACReaderTableView(QWidget *parent)
    : QTableView(parent), editing(false), myeditor(0)
{
    setAlternatingRowColors(true);
    verticalHeader()->setAlternatingRowColors(true);

    setContextMenuPolicy(Qt::ActionsContextMenu);

    setShowGrid(false);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);

    // comicView->horizontalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setSectionsClickable(false);
    // comicView->verticalHeader()->setResizeMode(QHeaderView::ResizeToContents);
    verticalHeader()->setDefaultSectionSize(qMax(32, fontMetrics().height() + 14));
    verticalHeader()->setSectionsClickable(false); // TODO comportamiento anómalo

    setCornerButtonEnabled(false);

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    setItemDelegateForColumn(ComicModel::Rating, new YACReaderRatingDelegate(this));
    setItemDelegateForColumn(ComicModel::Favorite, new FavoriteDelegate(this));
    setItemDelegateForColumn(ComicModel::State, new YACReaderComicStateDelegate(this));
    for (const auto column : { ComicModel::Work, ComicModel::Characters, ComicModel::Tags, ComicModel::Convention })
        setItemDelegateForColumn(column, new MetadataChipDelegate(this));
    setTextElideMode(Qt::ElideRight);
    setWordWrap(false);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    setMouseTracking(true);

    // drag: if the default drag is enabled there is no way for setting a custom image
    // TODO report bug/suggestion
    // setDragEnabled(true);
    // setDragDropMode(QAbstractItemView::DragDrop);
    setAcceptDrops(true);

    initTheme(this);
}

void YACReaderTableView::applyTheme(const Theme &theme)
{
    setStyleSheet(theme.comicsViewTable.tableViewQSS);
}

void YACReaderTableView::mouseMoveEvent(QMouseEvent *event)
{

    QModelIndex mi = indexAt(event->pos());
    viewport()->setCursor(mi.isValid() && (mi.column() == ComicModel::Favorite || !mi.data(ComicModel::ChipValuesRole).toStringList().isEmpty()) ? Qt::PointingHandCursor : Qt::ArrowCursor);
    if (mi.isValid()) {
        QList<QModelIndex> selectedIndexes = this->selectedIndexes();
        if (selectedIndexes.contains(mi)) {
            if (mi.column() == ComicModel::Rating) {
                if (!editing) {
                    editing = true;
                    currentIndexEditing = mi;
                    edit(mi);
                    myeditor = indexWidget(mi);
                } else if (mi.row() != currentIndexEditing.row())
                    closeRatingEditor();
            } else
                closeRatingEditor();
        } else
            closeRatingEditor();
    } else
        closeRatingEditor();

    // are we in a drag action??
    if (event->buttons() & Qt::LeftButton) {
        int distance = (event->pos() - startDragPos).manhattanLength();
        if (distance >= QApplication::startDragDistance())
            performDrag();
    }

    // disabled mouseMoveEvent in the parent class
}
void YACReaderTableView::mousePressEvent(QMouseEvent *event)
{
    QTableView::mousePressEvent(event);
    QModelIndex mi = indexAt(event->pos());
    if (event->button() == Qt::LeftButton && mi.isValid() && mi.column() == ComicModel::Favorite) {
        closeRatingEditor();
        if (auto *comics = qobject_cast<ComicModel *>(model()))
            comics->setFavorite(mi, !mi.data(ComicModel::FavoriteRole).toBool());
        return;
    }
    if (mi.isValid()) {
        QList<QModelIndex> selectedIndexes = this->selectedIndexes();
        if (selectedIndexes.contains(mi)) {
            if (mi.column() == ComicModel::Rating) {
                if (!editing) {
                    editing = true;
                    currentIndexEditing = mi;
                    edit(mi);
                    myeditor = indexWidget(mi);
                }
                return;
            }
        }
    }

    // this could be the origin of a new drag acction
    if (event->button() == Qt::LeftButton) {
        startDragPos = event->pos();
    }
}
void YACReaderTableView::leaveEvent(QEvent *event)
{
    closeRatingEditor();
    viewport()->unsetCursor();
    event->accept();
}

void YACReaderTableView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (indexAt(event->pos()).column() == ComicModel::Favorite) {
        event->accept();
        return;
    }
    QTableView::mouseDoubleClickEvent(event);
}

void YACReaderTableView::performDrag()
{
    QLOG_DEBUG() << "performDrag";
    QDrag *drag = new QDrag(this);
    drag->setMimeData(model()->mimeData(selectionModel()->selectedRows()));
    drag->setPixmap(theme.comicsViewToolbar.openInYACReaderIcon.pixmap(18, 18)); // TODO add better image

    /*Qt::DropAction dropAction =*/drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
}

void YACReaderTableView::dragEnterEvent(QDragEnterEvent *event)
{
    const auto imagePath = event->mimeData()->hasUrls() ? YACReader::droppedImagePath(event->mimeData()->urls()) : QString();
    if (!imagePath.isEmpty()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }

    QTableView::dragEnterEvent(event);

    if (model()->canDropMimeData(event->mimeData(), event->proposedAction(), 0, 0, QModelIndex()))
        event->acceptProposedAction();
    QLOG_DEBUG() << "drag enter table";
}

void YACReaderTableView::dragMoveEvent(QDragMoveEvent *event)
{
    const auto imagePath = event->mimeData()->hasUrls() ? YACReader::droppedImagePath(event->mimeData()->urls()) : QString();
    if (!imagePath.isEmpty() && indexAt(event->position().toPoint()).isValid()) {
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }

    QTableView::dragMoveEvent(event);

    if (model()->canDropMimeData(event->mimeData(), event->proposedAction(), 0, 0, QModelIndex()))
        event->acceptProposedAction();
    QLOG_DEBUG() << "dragMoveEvent table";
}

void YACReaderTableView::dropEvent(QDropEvent *event)
{
    const auto imagePath = event->mimeData()->hasUrls() ? YACReader::droppedImagePath(event->mimeData()->urls()) : QString();
    const auto imageIndex = indexAt(event->position().toPoint());
    if (!imagePath.isEmpty() && imageIndex.isValid()) {
        emit customCoverDropped(imagePath, imageIndex.siblingAtColumn(0));
        event->setDropAction(Qt::CopyAction);
        event->accept();
        return;
    }

    if (!model()->canDropMimeData(event->mimeData(), event->proposedAction(), 0, 0, QModelIndex())) {
        event->ignore();
        return;
    }

    const QPoint position = event->position().toPoint();
    const QModelIndex destination = indexAt(position);
    int destinationRow = -1;
    if (destination.isValid()) {
        destinationRow = destination.row();
        if (position.y() >= visualRect(destination).center().y())
            ++destinationRow;
    }

    if (model()->dropMimeData(event->mimeData(), Qt::MoveAction, destinationRow, 0, QModelIndex())) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    } else {
        event->ignore();
    }
    QLOG_DEBUG() << "drop on table";
}

void YACReaderTableView::closeRatingEditor()
{
    editing = false;
    if (myeditor != 0)
        closeEditor(myeditor, QAbstractItemDelegate::NoHint);
    myeditor = 0;
}

void YACReaderTableView::closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint)
{
    editing = false;
    myeditor = 0;
    QTableView::closeEditor(editor, hint);
}
void YACReaderTableView::commitData(QWidget *editor)
{
    // TODO
    StarEditor *starEditor = qobject_cast<StarEditor *>(editor);
    if (starEditor->getShouldCommitData())
        emit comicRated(((StarEditor *)editor)->starRating().starCount(), currentIndexEditing);
}

void YACReaderTableView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
}

//------------------------------------------------------------------------------
// YACReaderComicStateDelegate---------------------------------------------------
//------------------------------------------------------------------------------
void YACReaderComicStateDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    auto tableView = qobject_cast<YACReaderTableView *>(parent());
    if (tableView == nullptr)
        return;

    const auto kind = index.data(ComicModel::KindRole).toString();
    QIcon icon;
    if (kind == QStringLiteral("DeletedComicInList"))
        icon = tableView->deletedComicIcon();
    else if (kind == QStringLiteral("PlaceholderInList"))
        icon = tableView->placeholderComicIcon();
    else
        return;

    const int size = 12;
    const QRect iconRect(option.rect.center() - QPoint(size / 2 - 1, size / 2 - 1), QSize(size, size));
    icon.paint(painter, iconRect, Qt::AlignCenter, (option.state & QStyle::State_Selected) ? QIcon::Selected : QIcon::Normal);
}

QSize YACReaderComicStateDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    return { 20, QStyledItemDelegate::sizeHint(option, index).height() };
}

//------------------------------------------------------------------------------
// YACReaderRatingDelegate-------------------------------------------------------
//------------------------------------------------------------------------------
void YACReaderRatingDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                    const QModelIndex &index) const
{
    int rating = index.data(ComicModel::RatingRole).toInt();

    StarRating starRating(rating);

    // Get colors from parent table view
    QColor ratingColor(0xE9BE0F);
    QColor ratingSelectedColor(Qt::white);
    if (auto tableView = qobject_cast<YACReaderTableView *>(parent())) {
        ratingColor = tableView->starRatingColor();
        ratingSelectedColor = tableView->starRatingSelectedColor();
    }

    QStyledItemDelegate::paint(painter, option, index);

    if (!(option.state & QStyle::State_Editing)) {
        if (option.state & QStyle::State_Selected)
            starRating.paintSelected(painter, option.rect, option.palette,
                                     StarRating::ReadOnly, ratingSelectedColor);
        else
            starRating.paint(painter, option.rect, option.palette,
                             StarRating::ReadOnly, ratingColor);
    }
}

QSize YACReaderRatingDelegate::sizeHint(const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const
{
    Q_UNUSED(option)
    int rating = index.data(ComicModel::RatingRole).toInt();
    StarRating starRating(rating);
    return starRating.sizeHint();
}

QWidget *YACReaderRatingDelegate::createEditor(QWidget *parent,
                                               const QStyleOptionViewItem &option,
                                               const QModelIndex &index) const
{
    Q_UNUSED(option)
    Q_UNUSED(index)
    StarEditor *editor = new StarEditor(parent);
    connect(editor, &StarEditor::editingFinished,
            this, &YACReaderRatingDelegate::sendCloseEditor);
    connect(editor, &StarEditor::commitData,
            this, &YACReaderRatingDelegate::sendCommitData);
    return editor;
}

void YACReaderRatingDelegate::setEditorData(QWidget *editor,
                                            const QModelIndex &index) const
{
    int rating = index.data(ComicModel::RatingRole).toInt();

    StarRating starRating(rating);

    StarEditor *starEditor = qobject_cast<StarEditor *>(editor);
    starEditor->setStarRating(starRating);
}

void YACReaderRatingDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                           const QModelIndex &index) const
{
    QStyledItemDelegate::setModelData(editor, model, index);
}

void YACReaderRatingDelegate::sendCommitData()
{
    StarEditor *editor = qobject_cast<StarEditor *>(sender());
    emit commitData(editor);
}
void YACReaderRatingDelegate::sendCloseEditor()
{
    StarEditor *editor = qobject_cast<StarEditor *>(sender());
    emit closeEditor(editor);
}

//-------------------------------------------------------------------------------
// StarRating---------------------------------------------------------------------
//-------------------------------------------------------------------------------

//-------------------------------------------------------------------------------
// StarEditor---------------------------------------------------------------------
//-------------------------------------------------------------------------------

StarEditor::StarEditor(QWidget *parent)
    : QWidget(parent), shouldCommitData(false)
{
    // setMouseTracking(true);
    // setAutoFillBackground(true);
}

QSize StarEditor::sizeHint() const
{
    return myStarRating.sizeHint();
}

void StarEditor::paintEvent(QPaintEvent *)
{
    /*
        QPainter painter(this);
        myStarRating.paintSelected(&painter, rect(), this->palette(),
                StarRating::Editable,QColor("#615f59"));*/
}

void StarEditor::mouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event)
    /*int star = starAtPosition(event->x());

        if (star != myStarRating.starCount() && star != -1) {
                myStarRating.setStarCount(star);
                update();
        }*/
}
void StarEditor::leaveEvent(QEvent *event)
{
    emit editingFinished();
    QWidget::leaveEvent(event);
}

void StarEditor::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int star = starAtPosition(event->position().x());

        if (star != myStarRating.starCount() && star != -1) {
            myStarRating.setStarCount(star);
            shouldCommitData = true;
            emit commitData();
        }
    }
}

int StarEditor::starAtPosition(int x)
{
    int star = (x / (myStarRating.sizeHint().width() / myStarRating.maxStarCount())) + 1;
    if (star <= 0 || star > myStarRating.maxStarCount())
        return -1;

    return star;
}

bool YACReaderTableView::addChipColorAction(QMenu &menu, const QPoint &viewportPosition)
{
    const auto index = indexAt(viewportPosition);
    if (!index.isValid())
        return false;
    const auto delegate = qobject_cast<MetadataChipDelegate *>(itemDelegateForColumn(index.column()));
    if (!delegate)
        return false;
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = visualRect(index);
    const auto value = delegate->valueAt(option, index, viewportPosition);
    if (value.isEmpty())
        return false;
    auto action = new QAction(tr("Change color"), &menu);
    action->setObjectName("changeChipColor");
    const auto before = menu.actions().isEmpty() ? nullptr : menu.actions().first();
    menu.insertAction(before, action);
    menu.insertSeparator(before);
    // Close the popup before entering the native modal color picker.
    connect(action, &QAction::triggered, this, [this, column = index.column(), value] {
        const auto selected = requestChipColor(MetadataChipDelegate::colorForValue(column, value));
        if (!selected.isValid()) return;
        if (!MetadataChipColors::instance().setColor(column, value, selected))
            QMessageBox::warning(this, tr("Change color"), tr("The color could not be saved.")); }, Qt::QueuedConnection);
    return true;
}
QColor YACReaderTableView::requestChipColor(const QColor &initial)
{
    // Default options deliberately retain the native Windows color dialog.
    return QColorDialog::getColor(initial, this, tr("Change color"));
}
