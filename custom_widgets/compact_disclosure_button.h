#ifndef COMPACT_DISCLOSURE_BUTTON_H
#define COMPACT_DISCLOSURE_BUTTON_H
#include <QPainter>
#include <QStyleOptionToolButton>
#include <QToolButton>

// Fixed small chevrons, with an explicit gap before a disclosure label.
class CompactDisclosureButton : public QToolButton
{
public:
    explicit CompactDisclosureButton(QWidget *parent = nullptr) : QToolButton(parent) { }
    QSize sizeHint() const override
    {
        return QSize(toolButtonStyle() == Qt::ToolButtonTextBesideIcon ? fontMetrics().horizontalAdvance(text()) + 30 : 22,
                     qMax(24, fontMetrics().height() + 8));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QStyleOptionToolButton option;
        initStyleOption(&option);
        option.text.clear();
        option.icon = QIcon();
        option.arrowType = Qt::NoArrow;
        option.features &= ~QStyleOptionToolButton::Arrow;
        QPainter painter(this);
        style()->drawComplexControl(QStyle::CC_ToolButton, &option, &painter, this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto foreground = parentWidget() ? parentWidget()->palette().color(QPalette::WindowText) : palette().color(QPalette::ButtonText);
        painter.setPen(QPen(foreground, 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        const bool label = toolButtonStyle() == Qt::ToolButtonTextBesideIcon;
        const QPointF center(label ? 9 : width() / 2.0, height() / 2.0);
        QPolygonF arrow;
        if (arrowType() == Qt::RightArrow)
            arrow = { center + QPointF(-2, -4), center + QPointF(2, 0), center + QPointF(-2, 4) };
        else if (arrowType() == Qt::UpArrow)
            arrow = { center + QPointF(-4, 2), center + QPointF(0, -2), center + QPointF(4, 2) };
        else
            arrow = { center + QPointF(-4, -2), center + QPointF(0, 2), center + QPointF(4, -2) };
        painter.drawPolyline(arrow);
        if (label) {
            painter.setFont(font());
            painter.drawText(rect().adjusted(22, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter, text());
        }
    }
};
#endif
