#ifndef FAVORITE_HEART_H
#define FAVORITE_HEART_H

#include <QAbstractButton>
#include <QPainter>
#include <QPainterPath>

inline void paintFavoriteHeart(QPainter &painter, const QRect &area, bool favorite, bool mixed = false)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal size = qMin(area.width(), area.height()) * 0.66;
    painter.translate(area.center().x() - size / 2, area.center().y() - size / 2);
    painter.scale(size / 24, size / 24);
    QPainterPath heart;
    heart.moveTo(12, 22);
    heart.cubicTo(9, 19, 1, 13, 1, 7);
    heart.cubicTo(1, 0, 9, -1, 12, 5);
    heart.cubicTo(15, -1, 23, 0, 23, 7);
    heart.cubicTo(23, 13, 15, 19, 12, 22);
    painter.setPen(Qt::NoPen);
    painter.setBrush(favorite ? QColor("#e54d62") : QColor("#969696"));
    painter.drawPath(heart);
    if (mixed) {
        painter.setPen(QPen(Qt::white, 2));
        painter.drawLine(QPointF(6, 11), QPointF(18, 11));
    }
    painter.restore();
}

class FavoriteHeartButton : public QAbstractButton
{
public:
    bool mixed = false;
    explicit FavoriteHeartButton(QWidget *parent = nullptr) : QAbstractButton(parent)
    {
        setCheckable(true);
        setFixedSize(32, 30);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        paintFavoriteHeart(painter, rect(), isChecked(), mixed);
    }
};
#endif
