#ifndef METADATA_VALUE_CHIPS_H
#define METADATA_VALUE_CHIPS_H
#include "metadata_chip_colors.h"

#include <QMouseEvent>
#include <QPainter>
#include <QSet>
#include <QWidget>
#include <QtMath>

#include <functional>

// Shared chip geometry, colors and selection outline for filters and editors.
class MetadataValueChips : public QWidget
{
public:
    QStringList values;
    QSet<QString> selected;
    QString search;
    int column;
    bool removable = false;
    QWidget *inlineEditor = nullptr;
    QColor outline;
    std::function<void(const QString &)> clicked;
    explicit MetadataValueChips(int column, QWidget *parent = nullptr) : QWidget(parent), column(column)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        connect(&MetadataChipColors::instance(), &MetadataChipColors::colorsChanged, this, [this] { update(); });
    }
    void refresh()
    {
        layoutChips();
        update();
    }
    bool hasValueAt(const QPoint &point) const
    {
        for (const auto &entry : rectangles)
            if (entry.first.contains(point))
                return true;
        return false;
    }

protected:
    void resizeEvent(QResizeEvent *) override { layoutChips(); }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton)
            return;
        for (const auto &entry : rectangles)
            if (entry.first.contains(event->position().toPoint())) {
                if (clicked && (!removable || event->position().x() >= entry.first.right() - 20))
                    clicked(entry.second);
                return;
            }
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QFontMetricsF metrics(font(), this);
        for (const auto &entry : rectangles) {
            const auto background = MetadataChipColors::instance().color(column, entry.second);
            painter.setPen(selected.contains(entry.second) ? QPen(outline.isValid() ? outline : palette().color(QPalette::Window).lightnessF() > 0.5 ? QColor(Qt::black)
                                                                                                                                                     : QColor(Qt::white),
                                                                  2)
                                                           : QPen(background.darker(110), 1));
            painter.setBrush(background);
            painter.drawRoundedRect(QRectF(entry.first), 6, 6);
            painter.setPen(MetadataChipColors::foregroundColor(background));
            painter.drawText(entry.first.adjusted(8, 0, removable ? -24 : -8, 0), Qt::AlignCenter,
                             metrics.elidedText(entry.second, Qt::ElideRight, entry.first.width() - (removable ? 32 : 16)));
            if (removable)
                painter.drawText(QRect(entry.first.right() - 21, entry.first.top(), 18, entry.first.height()), Qt::AlignCenter, QStringLiteral("×"));
        }
    }

private:
    QList<QPair<QRect, QString>> rectangles;
    void layoutChips()
    {
        rectangles.clear();
        const QFontMetricsF metrics(font(), this);
        const int inset = inlineEditor ? 3 : 6;
        const int height = qMax(qCeil(metrics.height()) + (inlineEditor ? 6 : 12), inlineEditor ? qMax(inlineEditor->sizeHint().height(), inlineEditor->minimumSizeHint().height()) : 0);
        const int available = qMax(1, width() - 2 * inset);
        int x = inset, y = inset;
        for (const auto &value : values) {
            if (!value.contains(search, Qt::CaseInsensitive))
                continue;
            const int chipWidth = qMin(available, qCeil(metrics.horizontalAdvance(value)) + (removable ? 36 : 20));
            if (x > inset && x + chipWidth > width() - inset) {
                x = inset;
                y += height + 6;
            }
            rectangles.append({ QRect(x, y, chipWidth, height), value });
            x += chipWidth + 6;
        }
        if (inlineEditor) {
            if (x > inset && x + 76 > width() - inset) {
                x = inset;
                y += height + 4;
            }
            inlineEditor->setGeometry(x, y, qMax(1, width() - inset - x), height);
        }
        const int contentHeight = rectangles.isEmpty() && !inlineEditor ? 12 : y + height + inset;
        if (inlineEditor) {
            if (minimumHeight() != contentHeight || maximumHeight() != contentHeight)
                setFixedHeight(contentHeight);
        } else
            setMinimumHeight(contentHeight);
    }
};

#endif
