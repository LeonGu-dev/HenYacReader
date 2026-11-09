#ifndef METADATA_CHIP_DELEGATE_H
#define METADATA_CHIP_DELEGATE_H
#include <QColor>
#include <QStyledItemDelegate>
class MetadataChipDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit MetadataChipDelegate(QObject *parent = nullptr);
    static QColor colorForValue(int column, const QString &value);
    QString valueAt(const QStyleOptionViewItem &option, const QModelIndex &index, const QPoint &point) const;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;
signals:
    // Reserved for the later filtering step; this version does not change the result set.
    void chipClicked(const QModelIndex &index, const QString &value);
};
#endif
