#ifndef STAR_RATING_H
#define STAR_RATING_H
#include <QMetaType>
#include <QPainter>
#include <QPolygonF>

#include <cmath>
class StarRating
{
public:
    enum EditMode { Editable,
                    ReadOnly };

    StarRating(int starCount = 1, int maxStarCount = 5);

    void paint(QPainter *painter, const QRect &rect,
               const QPalette &palette, EditMode mode) const;
    void paint(QPainter *painter, const QRect &rect,
               const QPalette &palette, EditMode mode, QColor color) const;
    void paintSelected(QPainter *painter, const QRect &rect,
                       const QPalette &palette, EditMode mode, QColor color) const;
    void paintSelected(QPainter *painter, const QRect &rect,
                       const QPalette &palette, EditMode mode) const;
    QSize sizeHint() const;
    int starCount() const { return myStarCount; }
    int maxStarCount() const { return myMaxStarCount; }
    void setStarCount(int starCount) { myStarCount = starCount; }
    void setMaxStarCount(int maxStarCount) { myMaxStarCount = maxStarCount; }

private:
    QPolygonF starPolygon;
    QPolygonF diamondPolygon;
    int myStarCount;
    int myMaxStarCount;
};
Q_DECLARE_METATYPE(StarRating);

inline constexpr int PaintingScaleFactor = 20;

inline StarRating::StarRating(int starCount, int maxStarCount)
{
    myStarCount = starCount;
    myMaxStarCount = maxStarCount;

    int numVertex = 5;
    double pi = 3.14159265359;
    double angle = 3.14159265359 / numVertex;

    float rOuter = 0.3f;
    float rInner = 0.15f;
    for (int i = 0; i < 2 * numVertex; i++) {
        double r = (i & 1) == 0 ? rOuter : rInner;
        starPolygon << QPointF(0.5 + cos((i * angle) - pi / 2) * r, 0.5 + sin((i * angle) - pi / 2) * r);
    }

    diamondPolygon << QPointF(0.4, 0.5) << QPointF(0.5, 0.4)
                   << QPointF(0.6, 0.5) << QPointF(0.5, 0.6)
                   << QPointF(0.4, 0.5);
}

inline QSize StarRating::sizeHint() const
{
    return PaintingScaleFactor * QSize(myMaxStarCount, 1);
}

inline void StarRating::paint(QPainter *painter, const QRect &rect,
                              const QPalette &palette, EditMode mode) const
{
    paint(painter, rect, palette, mode, QColor("#e9be0f"));
}

inline void StarRating::paint(QPainter *painter, const QRect &rect,
                              const QPalette &palette, EditMode mode, QColor color) const
{
    Q_UNUSED(palette)
    painter->save();

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);

    QBrush brush(color);
    painter->setBrush(brush);

    int yOffset = (rect.height() - PaintingScaleFactor) / 2;
    painter->translate(rect.x(), rect.y() + yOffset);
    painter->scale(PaintingScaleFactor, PaintingScaleFactor);

    for (int i = 0; i < myMaxStarCount; ++i) {
        if (i < myStarCount) {
            painter->drawPolygon(starPolygon, Qt::WindingFill);
        } else if (mode == Editable) {
            painter->drawEllipse(QPointF(0.5, 0.5), 0.08, 0.08);
        }
        painter->translate(1.0, 0.0);
    }

    painter->restore();
}

inline void StarRating::paintSelected(QPainter *painter, const QRect &rect,
                                      const QPalette &palette, EditMode mode, QColor color) const
{
    Q_UNUSED(palette)
    Q_UNUSED(mode)
    painter->save();

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);

    QBrush star(color);
    QBrush dot(QColor("#ffffff"));

    int yOffset = (rect.height() - PaintingScaleFactor) / 2;
    painter->translate(rect.x(), rect.y() + yOffset);
    painter->scale(PaintingScaleFactor, PaintingScaleFactor);

    for (int i = 0; i < myMaxStarCount; ++i) {
        if (i < myStarCount) {
            painter->setBrush(star);
            painter->drawPolygon(starPolygon, Qt::WindingFill);
        } else {
            painter->setBrush(dot);
            painter->drawEllipse(QPointF(0.5, 0.5), 0.08, 0.08);
        }
        painter->translate(1.0, 0.0);
    }

    painter->restore();
}

inline void StarRating::paintSelected(QPainter *painter, const QRect &rect,
                                      const QPalette &palette, EditMode mode) const
{
    paintSelected(painter, rect, palette, mode, QColor("#ffffff"));
}

#endif
