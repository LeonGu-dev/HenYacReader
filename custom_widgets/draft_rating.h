#ifndef DRAFT_RATING_H
#define DRAFT_RATING_H

#include "star_rating.h"

#include <QAbstractButton>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QWidget>

#include <functional>

// Pure UI state. This widget never reads a model or writes to a database.
class DraftRating : public QWidget
{
public:
    explicit DraftRating(QWidget *parent = nullptr) : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        auto *layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        for (int n = 1; n <= 5; ++n) {
            auto *button = new Star(this, n);
            button->setObjectName(QStringLiteral("rating%1").arg(n));
            button->setAccessibleName(QString::number(n));
            button->setFixedSize(26, 30);
            button->setFocusPolicy(Qt::StrongFocus);
            layout->addWidget(button);
            connect(button, &QAbstractButton::clicked, this, [this, n] {
                value = !mixed && value == n ? 0 : n;
                mixed = false;
                refresh();
                if (changed)
                    changed(value);
            });
        }
        layout->addStretch();
    }
    void setValue(int rating, bool multiple = false)
    {
        value = qBound(0, rating, 5);
        mixed = multiple;
        refresh();
    }
    int rating() const { return value; }
    std::function<void(int)> changed;

private:
    class Star : public QAbstractButton
    {
    public:
        Star(DraftRating *owner, int number) : QAbstractButton(owner), owner(owner), number(number) { }

    protected:
        void paintEvent(QPaintEvent *) override
        {
            QPainter painter(this);
            const auto inactive = palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::PlaceholderText);
            const auto color = isEnabled() && !owner->mixed && number <= owner->value ? QColor("#e9be0f") : inactive;
            StarRating(1, 1).paint(&painter, rect().adjusted(3, 0, -3, 0), palette(), StarRating::ReadOnly, color);
        }

    private:
        DraftRating *owner;
        int number;
    };
    void refresh()
    {
        for (auto *button : findChildren<QAbstractButton *>()) {
            button->setToolTip(mixed ? QCoreApplication::translate("ComicMetadataEditor", "Multiple values; unchanged") : QString::number(value));
            button->update();
        }
    }
    int value = 0;
    bool mixed = false;
};
#endif
