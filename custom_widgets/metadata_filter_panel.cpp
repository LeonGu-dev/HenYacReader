#include "metadata_filter_panel.h"

#include "comic_model.h"
#include "favorite_heart.h"
#include "metadata_chip_colors.h"
#include "yacreader_table_view.h"

#include <QButtonGroup>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QTabBar>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <cmath>

namespace {
class LogicFilterButton : public QToolButton
{
public:
    explicit LogicFilterButton(QWidget *parent) : QToolButton(parent) { setAttribute(Qt::WA_Hover); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QPointF center = rect().center();
        const QRectF frame(center.x() - 9, center.y() - 9, 18, 18);
        const bool dark = palette().color(QPalette::Window).lightnessF() < 0.5;
        painter.setPen(Qt::NoPen);
        const QColor surface = dark
                ? QColor(isDown() ? "#515151" : underMouse() ? "#686868"
                                                             : "#5c5c5c")
                : QColor(isDown() ? "#dce0e5" : underMouse() ? "#f1f3f5"
                                                             : "#e9ebee");
        painter.setBrush(surface);
        painter.drawRoundedRect(frame, 3, 3);
        auto font = this->font();
        font.setPixelSize(12);
        painter.setFont(font);
        const QFontMetricsF metrics(font, this);
        const auto ink = metrics.tightBoundingRect(text());
        painter.setPen(palette().color(QPalette::ButtonText));
        painter.drawText(center - ink.center(), text());
    }
};

class FilterTabBar : public QTabBar
{
protected:
    QSize tabSizeHint(int index) const override
    {
        return QSize(qMax(1, width() / qMax(1, count())), QTabBar::tabSizeHint(index).height());
    }
};
class FilterTabs : public QTabWidget
{
public:
    FilterTabs(QWidget *parent) : QTabWidget(parent) { setTabBar(new FilterTabBar); }

protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QTabWidget::resizeEvent(event);
        tabBar()->setFixedWidth(contentsRect().width());
    }
};
class RatingFilterRow : public QAbstractButton
{
public:
    int rating;
    QColor starColor;
    explicit RatingFilterRow(int value, QWidget *parent) : QAbstractButton(parent), rating(value)
    {
        setCheckable(true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(qMax(34, StarRating().sizeHint().height() + 10));
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        if (isChecked())
            painter.fillRect(rect(), QColor("#b9ddf7"));
        StarRating(rating).paint(&painter, rect().adjusted(10, 0, -10, 0), palette(), StarRating::ReadOnly, starColor);
    }
};
}

class MetadataFilterChips : public QWidget
{
public:
    QStringList values;
    QSet<QString> selected;
    QString search;
    int column;
    QColor outline;
    std::function<void(const QString &)> clicked;
    explicit MetadataFilterChips(int column) : column(column)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        connect(&MetadataChipColors::instance(), &MetadataChipColors::colorsChanged, this, [this] { update(); });
    }
    void refresh()
    {
        layoutChips();
        update();
    }

protected:
    void resizeEvent(QResizeEvent *) override { layoutChips(); }
    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (event->button() != Qt::LeftButton)
            return;
        for (const auto &entry : rectangles)
            if (entry.first.contains(event->position().toPoint())) {
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
            painter.setPen(selected.contains(entry.second) ? QPen(outline, 2) : QPen(background.darker(110), 1));
            painter.setBrush(background);
            painter.drawRoundedRect(QRectF(entry.first), 6, 6);
            painter.setPen(MetadataChipColors::foregroundColor(background));
            painter.drawText(entry.first.adjusted(8, 0, -8, 0), Qt::AlignCenter,
                             metrics.elidedText(entry.second, Qt::ElideRight, entry.first.width() - 16));
        }
    }

private:
    QList<QPair<QRect, QString>> rectangles;
    void layoutChips()
    {
        rectangles.clear();
        const QFontMetricsF metrics(font(), this);
        const int height = qCeil(metrics.height()) + 12;
        const int available = qMax(1, width() - 12);
        int x = 6, y = 6;
        for (const auto &value : values) {
            if (!value.contains(search, Qt::CaseInsensitive))
                continue;
            const int chipWidth = qMin(available, qCeil(metrics.horizontalAdvance(value)) + 20);
            if (x > 6 && x + chipWidth > width() - 6) {
                x = 6;
                y += height + 6;
            }
            rectangles.append({ QRect(x, y, chipWidth, height), value });
            x += chipWidth + 6;
        }
        setMinimumHeight(rectangles.isEmpty() ? 12 : y + height + 6);
    }
};

MetadataFilterPanel::MetadataFilterPanel(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 4, 5, 6);
    layout->setSpacing(6);
    search = new QLineEdit(this);
    search->setObjectName("metadataFilterSearch");
    search->setPlaceholderText(tr("Search"));
    search->setClearButtonEnabled(true);
    layout->addWidget(search);
    layout->addSpacing(4);
    auto *tabs = new FilterTabs(this);
    const QStringList names { tr("tag"), tr("Work"), tr("Author") };
    const QList<int> columns { ComicModel::Tags, ComicModel::Work, ComicModel::Author };
    for (int i = 0; i < 3; ++i) {
        auto *area = new QScrollArea;
        area->setWidgetResizable(true);
        area->setFrameShape(QFrame::NoFrame);
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *page = new MetadataFilterChips(columns[i]);
        page->setObjectName(QStringLiteral("metadataFilterChips%1").arg(i));
        area->setWidget(page);
        // QScrollArea::setWidget enables background filling on its child.
        // Let the sidebar show through both the content and the viewport.
        area->setAutoFillBackground(false);
        area->viewport()->setAutoFillBackground(false);
        page->setAutoFillBackground(false);
        chips.append(page);
        tabs->addTab(area, names[i]);
        page->clicked = [this, i](const QString &value) {
            if (!model)
                return;
            auto filter = model->metadataFilter();
            auto &selection = i == 0 ? filter.tags : i == 1 ? filter.works
                                                            : filter.authors;
            if (selection.contains(value))
                selection.remove(value);
            else
                selection.insert(value);
            model->setMetadataFilter(filter);
        };
    }
    auto *ratingArea = new QScrollArea;
    ratingArea->setWidgetResizable(true);
    ratingArea->setFrameShape(QFrame::NoFrame);
    auto *ratingPage = new QWidget;
    auto *ratingLayout = new QVBoxLayout(ratingPage);
    ratingLayout->setContentsMargins(0, 6, 0, 6);
    ratingLayout->setSpacing(4);
    for (int rating = 5; rating >= 1; --rating) {
        auto *row = new RatingFilterRow(rating, ratingPage);
        row->setObjectName(QStringLiteral("metadataFilterRating%1").arg(rating));
        row->setAccessibleName(tr("%1 stars").arg(rating));
        ratingLayout->addWidget(row);
        connect(row, &QAbstractButton::clicked, this, [this, rating] {
            if (!model)
                return;
            auto filter = model->metadataFilter();
            if (filter.ratings.contains(rating))
                filter.ratings.remove(rating);
            else
                filter.ratings.insert(rating);
            model->setMetadataFilter(filter);
        });
        syncButtons.append([this, row, rating] {
            row->setChecked(model && model->metadataFilter().ratings.contains(rating));
            row->starColor = theme.comicsViewTable.starRatingColor;
            row->update();
        });
    }
    ratingLayout->addStretch();
    ratingArea->setWidget(ratingPage);
    ratingArea->setAutoFillBackground(false);
    ratingArea->viewport()->setAutoFillBackground(false);
    ratingPage->setAutoFillBackground(false);
    tabs->addTab(ratingArea, tr("Rating"));
    layout->addWidget(tabs, 1);
    connect(search, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (auto *page : chips) {
            page->search = text;
            page->refresh();
        }
    });
    controls = new QWidget(this);
    controls->setObjectName("metadataFilterHeaderControls");
    controls->hide();
    auto *controlLayout = new QHBoxLayout(controls);
    controlLayout->setContentsMargins(0, 0, 6, 0);
    controlLayout->setSpacing(0);
    auto *favorite = new FavoriteHeartButton(controls);
    favorite->setFixedSize(24, 24);
    favorite->setObjectName("metadataFilterFavoritesOnly");
    favorite->setToolTip(tr("Filter favorite comics"));
    favorite->setAccessibleName(tr("Filter favorite comics"));
    controlLayout->addWidget(favorite, 0, Qt::AlignVCenter);
    auto *logic = new LogicFilterButton(controls);
    logic->setObjectName("metadataFilterLogic");
    logic->setAutoRaise(false);
    logic->setFixedSize(24, 24);
    logic->setCursor(Qt::PointingHandCursor);
    controlLayout->addWidget(logic, 0, Qt::AlignVCenter);
    connect(favorite, &QAbstractButton::clicked, this, [this] {
        if (!model)
            return;
        auto filter = model->metadataFilter();
        filter.favoritesOnly = !filter.favoritesOnly;
        model->setMetadataFilter(filter);
    });
    connect(logic, &QToolButton::clicked, this, [this] {
        if (!model)
            return;
        auto filter = model->metadataFilter();
        filter.matchAll = !filter.matchAll;
        model->setMetadataFilter(filter);
    });
    syncButtons.append([this, favorite, logic] {
        const auto filter = model ? model->metadataFilter() : MetadataFilter();
        favorite->setChecked(filter.favoritesOnly);
        logic->setText(filter.matchAll ? tr("AND") : tr("OR"));
        logic->setToolTip(filter.matchAll ? tr("Switch to OR filtering") : tr("Switch to AND filtering"));
        logic->setAccessibleName(logic->toolTip());
    });
    auto *choicesBox = new QWidget(this);
    choicesBox->setObjectName("metadataFilterChoices");
    auto *choiceLayout = new QVBoxLayout(choicesBox);
    choiceLayout->setContentsMargins(0, 0, 0, 0);
    choiceLayout->setSpacing(6);
    layout->insertWidget(0, choicesBox);
    const auto addChoices = [this, choiceLayout](const QStringList &names, const QStringList &values, QString MetadataFilter::*field) {
        auto *row = new QHBoxLayout;
        row->setSpacing(4);
        for (int i = 0; i < names.size(); ++i) {
            auto *button = new QPushButton(names[i]);
            button->setCheckable(true);
            row->addWidget(button);
            const auto value = values[i];
            button->setObjectName("metadataFilter_" + value);
            connect(button, &QPushButton::clicked, this, [this, field, value] {
                if (!model)
                    return;
                auto filter = model->metadataFilter();
                filter.*field = filter.*field == value ? QString() : value;
                model->setMetadataFilter(filter);
            });
            syncButtons.append([this, button, field, value] { button->setChecked(model && model->metadataFilter().*field == value); });
        }
        choiceLayout->addLayout(row);
    };
    addChoices({ tr("Doujinshi"), tr("Collected volume"), tr("Magazine") }, { "doujinshi", "tankobon", "magazine" }, &MetadataFilter::publicationType);
    addChoices({ tr("Black and white"), tr("Color") }, { "bw", "color" }, &MetadataFilter::color);
    addChoices({ tr("Censored"), tr("Uncensored") }, { "censored", "uncensored" }, &MetadataFilter::censorship);
    addChoices({ tr("Chinese translation"), tr("AI translation"), tr("No translation") }, { "translated", "ai", "original" }, &MetadataFilter::translation);
    auto *reset = new QPushButton(tr("Reset all filters"), this);
    reset->setObjectName("metadataFilterReset");
    choiceLayout->addWidget(reset);
    connect(reset, &QPushButton::clicked, this, [this] {
        search->clear();
        if (model)
            model->setMetadataFilter({ });
    });
    initTheme(this);
    refresh();
}

void MetadataFilterPanel::setModel(ComicModel *value)
{
    model = value;
    connect(model, &QAbstractItemModel::modelReset, this, &MetadataFilterPanel::refresh);
    connect(model, &QAbstractItemModel::dataChanged, this, [this] { refresh(); });
    refresh();
}

void MetadataFilterPanel::refresh()
{
    const auto values = model ? model->metadataFilterValues() : QMap<int, QStringList>();
    const auto filter = model ? model->metadataFilter() : MetadataFilter();
    for (int i = 0; i < 3; ++i) {
        chips[i]->values = values.value(i);
        chips[i]->selected = i == 0 ? filter.tags : i == 1 ? filter.works
                                                           : filter.authors;
        // Keep an active condition reachable if its final comic was edited/deleted.
        for (const auto &value : chips[i]->selected)
            if (!chips[i]->values.contains(value))
                chips[i]->values.append(value);
        chips[i]->values.sort(Qt::CaseInsensitive);
        chips[i]->outline = outline;
        chips[i]->refresh();
    }
    for (const auto &sync : syncButtons)
        sync();
    const bool enabled = model != nullptr && !model->getCurrentPath().isEmpty();
    setEnabled(enabled);
    controls->setEnabled(enabled);
}

void MetadataFilterPanel::applyTheme(const Theme &theme)
{
    const auto background = theme.sidebar.backgroundColor;
    outline = background.lightnessF() > 0.5 ? QColor(Qt::black) : QColor(Qt::white);
    auto colors = palette();
    colors.setColor(QPalette::Window, background);
    colors.setColor(QPalette::Base, background);
    colors.setColor(QPalette::WindowText, outline);
    colors.setColor(QPalette::Text, outline);
    colors.setColor(QPalette::ButtonText, outline);
    setPalette(colors);
    controls->setPalette(colors);
    controls->setStyleSheet(QStringLiteral("QToolButton { color:%1; }").arg(outline.name()));
    const auto surface = background.lightnessF() > 0.5 ? background.darker(104) : background.lighter(135);
    const auto border = background.lightnessF() > 0.5 ? background.darker(125) : background.lighter(190);
    setStyleSheet(QStringLiteral(
                          "MetadataFilterPanel { background:%1; }"
                          "MetadataFilterPanel QLineEdit { background:%1; color:%2; border:1px solid %3; border-radius:4px; padding:4px; }"
                          "MetadataFilterPanel QTabWidget::pane { border:0; background:transparent; }"
                          "MetadataFilterPanel QStackedWidget { background:transparent; }"
                          "MetadataFilterPanel QTabBar::tab { background:%4; color:%2; border:1px solid %3; padding:5px 12px; }"
                          "MetadataFilterPanel QTabBar::tab:selected { background:%1; border-bottom-color:%1; }"
                          "MetadataFilterPanel QPushButton { background:%4; color:%2; border:1px solid %3; border-radius:4px; padding:4px 3px; }"
                          "MetadataFilterPanel QPushButton:checked { background:%5; color:%6; border-color:%5; }"
                          "MetadataFilterPanel QScrollArea { border:0; background:transparent; }")
                          .arg(background.name(), outline.name(), border.name(), surface.name(),
                               palette().color(QPalette::Highlight).name(), palette().color(QPalette::HighlightedText).name()));
    refresh();
}
