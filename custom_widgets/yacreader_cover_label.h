#ifndef DROP_LABEL_H
#define DROP_LABEL_H

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QMimeData>

namespace YACReader {

class CoverLabel : public QLabel
{
    Q_OBJECT

public:
    explicit CoverLabel(QWidget *parent = nullptr);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

signals:
    void imageDropped(const QString &path);
};

}

#endif // DROP_LABEL_H
