#include "yacreader_cover_label.h"

#include "yacreader_global_gui.h"

#include <QPainter>

YACReader::CoverLabel::CoverLabel(QWidget *parent)
    : QLabel(parent)
{
    setAcceptDrops(true);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
}

QSize YACReader::CoverLabel::sizeHint() const
{
    return QSize(360, 575);
}

QSize YACReader::CoverLabel::minimumSizeHint() const
{
    return QSize(0, 0);
}

void YACReader::CoverLabel::paintEvent(QPaintEvent *)
{
    const auto image = pixmap();
    if (image.isNull())
        return;
    auto target = QRect(QPoint(), image.size().scaled(contentsRect().size(), Qt::KeepAspectRatio));
    target.moveCenter(contentsRect().center());
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawPixmap(target, image);
}

void YACReader::CoverLabel::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls() && !YACReader::imagePathFromMimeData(event->mimeData()).isEmpty()) {
        event->acceptProposedAction();
    }
}

void YACReader::CoverLabel::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void YACReader::CoverLabel::dropEvent(QDropEvent *event)
{
    QString path = YACReader::imagePathFromMimeData(event->mimeData());
    if (!path.isEmpty()) {
        emit imageDropped(path);
        event->acceptProposedAction();
    }
}
