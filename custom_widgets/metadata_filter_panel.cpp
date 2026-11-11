#include "metadata_filter_panel.h"

#include "comic_model.h"
#include "compact_disclosure_button.h"
#include "favorite_heart.h"
#include "metadata_chip_colors.h"
#include "metadata_value_chips.h"
#include "yacreader_global_gui.h"
#include "yacreader_table_view.h"

#include <QButtonGroup>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>

namespace {
class PinFilterButton : public QToolButton
{
public:
    explicit PinFilterButton(QWidget *parent) : QToolButton(parent)
    {
        setCheckable(true);
        setFixedSize(24, 24);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const auto color = palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText);
        painter.save();
        painter.setPen(QPen(color, 1.35, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.translate(QPointF(width() / 2.0, height() / 2.0));
        painter.scale(0.75, 0.75);
        painter.translate(-12, -12);
        QPainterPath pin;
        pin.moveTo(8, 4);
        pin.lineTo(16, 4);
        pin.lineTo(15, 9);
        pin.lineTo(18, 13);
        pin.lineTo(6, 13);
        pin.lineTo(9, 9);
        pin.closeSubpath();
        painter.drawPath(pin);
        painter.drawLine(QPointF(12, 13), QPointF(12, 20));
        if (!isChecked())
            painter.drawLine(QPointF(5, 4), QPointF(19, 20));
        painter.restore();
        if (hasFocus()) {
            QStyleOptionFocusRect focus;
            focus.initFrom(this);
            focus.rect = QRect(2, 2, 20, 20);
            style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
        }
    }
};

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

class CompactTabArrow : public QToolButton
{
public:
    CompactTabArrow(Qt::ArrowType direction, QWidget *parent) : QToolButton(parent)
    {
        setArrowType(direction);
        setCursor(Qt::PointingHandCursor);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        auto color = palette().color(QPalette::Active, QPalette::ButtonText);
        color.setAlphaF(isEnabled() && underMouse() ? 1.0 : 0.72);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        const QPointF center(width() / 2.0, height() / 2.0);
        const qreal direction = arrowType() == Qt::RightArrow ? 1 : -1;
        painter.drawPolygon(QPolygonF { center + QPointF(-2.5 * direction, -4),
                                        center + QPointF(2.5 * direction, 0),
                                        center + QPointF(-2.5 * direction, 4) });
    }
};

class FilterTabBar : public QTabBar
{
public:
    FilterTabBar()
    {
        setElideMode(Qt::ElideRight);
        setUsesScrollButtons(false);
    }
    QSize sizeHint() const override
    {
        // The preferred width must not depend on the width assigned by a layout.
        // Otherwise the tab widget feeds its allocated width back into the sidebar.
        QSize hint(0, 0);
        for (int i = 0; i < count(); ++i) {
            const auto tab = QTabBar::tabSizeHint(i);
            hint.rwidth() += tab.width();
            hint.setHeight(qMax(hint.height(), tab.height()));
        }
        return hint;
    }
    QSize minimumSizeHint() const override { return QSize(0, sizeHint().height()); }

protected:
    QSize tabSizeHint(int index) const override
    {
        return QSize(qMax(1, width() / qMax(1, count())), QTabBar::tabSizeHint(index).height());
    }
    void paintEvent(QPaintEvent *) override
    {
        QStylePainter painter(this);
        const auto draw = [&](int index) {
            QStyleOptionTab option;
            initStyleOption(&option, index);
            // Equal-width tabs can be narrower than the stylesheet's text hint.
            // Elide before asking the style to paint its centered label.
            const auto textRect = style()->subElementRect(QStyle::SE_TabBarTabText, &option, this);
            option.text = fontMetrics().elidedText(tabText(index), Qt::ElideRight, qMax(0, textRect.width()));
            painter.drawControl(QStyle::CE_TabBarTab, option);
        };
        for (int i = 0; i < count(); ++i)
            if (i != currentIndex())
                draw(i);
        if (currentIndex() >= 0)
            draw(currentIndex());
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

MetadataFilterTabs::MetadataFilterTabs(QWidget *parent) : QWidget(parent)
{
    setObjectName("metadataFilterTabs");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    strip = new QWidget(this);
    strip->setObjectName("metadataFilterTabStrip");
    strip->installEventFilter(this);
    layout->addWidget(strip);
    viewport = new QScrollArea(strip);
    viewport->setObjectName("metadataFilterTabViewport");
    viewport->setFrameShape(QFrame::NoFrame);
    viewport->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    viewport->viewport()->installEventFilter(this);
    bar = new FilterTabBar;
    bar->installEventFilter(this);
    viewport->setWidget(bar);
    viewport->setAutoFillBackground(false);
    viewport->viewport()->setAutoFillBackground(false);
    bar->setAutoFillBackground(false);
    previous = new CompactTabArrow(Qt::LeftArrow, strip);
    previous->setObjectName("metadataFilterPreviousTabs");
    previous->setArrowType(Qt::LeftArrow);
    previous->setIconSize(QSize(8, 8));
    previous->setAccessibleName(tr("Previous filter tab"));
    previous->setToolTip(previous->accessibleName());
    next = new CompactTabArrow(Qt::RightArrow, strip);
    next->setObjectName("metadataFilterNextTabs");
    next->setArrowType(Qt::RightArrow);
    next->setIconSize(QSize(8, 8));
    next->setAccessibleName(tr("Next filter tab"));
    next->setToolTip(next->accessibleName());
    connect(previous, &QToolButton::clicked, this, [this] { moveTabs(-1); });
    connect(next, &QToolButton::clicked, this, [this] { moveTabs(1); });
    pages = new QStackedWidget(this);
    layout->addWidget(pages, 1);
    connect(bar, &QTabBar::currentChanged, pages, &QStackedWidget::setCurrentIndex);
    connect(bar, &QTabBar::currentChanged, this, [this](int index) {
        if (index < first)
            first = index;
        else if (index >= first + 4)
            first = index - 3;
        updateStrip();
    });
    updateStrip();
}

int MetadataFilterTabs::addTab(QWidget *page, const QString &text)
{
    pages->addWidget(page);
    const int index = bar->addTab(text);
    updateStrip();
    return index;
}

int MetadataFilterTabs::count() const
{
    return bar->count();
}
int MetadataFilterTabs::currentIndex() const
{
    return bar->currentIndex();
}
void MetadataFilterTabs::setCurrentIndex(int index)
{
    bar->setCurrentIndex(index);
}
void MetadataFilterTabs::setTabToolTip(int index, const QString &text)
{
    bar->setTabToolTip(index, text);
}

void MetadataFilterTabs::moveTabs(int step)
{
    first = qBound(0, first + step, qMax(0, count() - 4));
    updateStrip();
}

void MetadataFilterTabs::updateStrip()
{
    if (!pages || !next)
        return;
    first = qBound(0, first, qMax(0, count() - 4));
    const int height = qMax(24, bar->sizeHint().height());
    if (strip->height() != height)
        strip->setFixedHeight(height);
    const int navigationWidth = qMin(36, qMax(2, strip->width() / 4));
    const int slot = qMax(1, (strip->width() - navigationWidth) / 4);
    const int tabsWidth = slot * 4;
    const int controlsWidth = qMax(2, strip->width() - tabsWidth);
    viewport->setGeometry(0, 0, tabsWidth, height);
    bar->resize(slot * qMax(4, count()), height);
    viewport->horizontalScrollBar()->setValue(first * slot);
    previous->setGeometry(tabsWidth, 0, controlsWidth / 2, height);
    next->setGeometry(tabsWidth + controlsWidth / 2, 0, controlsWidth - controlsWidth / 2, height);
    previous->setEnabled(first > 0);
    next->setEnabled(first + 4 < count());
}

bool MetadataFilterTabs::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        const auto movement = wheel->angleDelta().isNull() ? wheel->pixelDelta() : wheel->angleDelta();
        const int delta = movement.x() ? movement.x() : movement.y();
        if (delta)
            moveTabs(delta < 0 ? 1 : -1);
        event->accept();
        return true;
    }
    if (object == strip && (event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest || event->type() == QEvent::Show))
        updateStrip();
    if ((event->type() == QEvent::StyleChange || event->type() == QEvent::FontChange || event->type() == QEvent::LayoutRequest) && !stripUpdatePending) {
        stripUpdatePending = true;
        QTimer::singleShot(0, this, [this] {
            stripUpdatePending = false;
            updateStrip();
        });
    }
    return QWidget::eventFilter(object, event);
}

MetadataFilterPanel::MetadataFilterPanel(QWidget *parent, Host host) : QWidget(parent), host(host)
{
    if (!preferenceKey().isEmpty()) {
        QSettings settings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat);
        pinned = settings.value(preferenceKey(), true).toBool();
    }
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 4, 5, 6);
    layout->setSpacing(6);
    search = new QLineEdit(this);
    search->setObjectName("metadataFilterSearch");
    search->setPlaceholderText(tr("Search"));
    search->setClearButtonEnabled(true);
    layout->addWidget(search);
    layout->addSpacing(4);
    tabs = new MetadataFilterTabs(this);
    tabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Ignored);
    tabs->setMinimumHeight(54);
    const QStringList names { tr("tag"), tr("Work"), tr("Author"), tr("Convention"), tr("Series") };
    const QList<int> columns { ComicModel::Tags, ComicModel::Work, ComicModel::Author, ComicModel::Convention, -4 };
    for (int i = 0; i < columns.size(); ++i) {
        auto *area = new QScrollArea;
        area->setWidgetResizable(true);
        area->setFrameShape(QFrame::NoFrame);
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *page = new MetadataValueChips(columns[i]);
        page->setObjectName(QStringLiteral("metadataFilterChips%1").arg(i));
        area->setWidget(page);
        // QScrollArea::setWidget enables background filling on its child.
        // Let the sidebar show through both the content and the viewport.
        area->setAutoFillBackground(false);
        area->viewport()->setAutoFillBackground(false);
        page->setAutoFillBackground(false);
        chips.append(page);
        tabs->addTab(area, names[i]);
        tabs->setTabToolTip(i, names[i]);
        page->clicked = [this, i](const QString &value) {
            if (!model)
                return;
            auto filter = model->metadataFilter();
            auto &selection = i == 0 ? filter.tags : i == 1 ? filter.works
                    : i == 2                                ? filter.authors
                    : i == 3                                ? filter.conventions
                                                            : filter.seriesNames;
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
    pinButton = new PinFilterButton(controls);
    pinButton->setObjectName("metadataFilterAlwaysExpanded");
    pinButton->setToolTip(tr("Always expanded"));
    pinButton->setAccessibleName(tr("Always expanded"));
    pinButton->setChecked(pinned);
    controlLayout->addWidget(pinButton, 0, Qt::AlignVCenter);
    connect(pinButton, &QToolButton::clicked, this, [this](bool checked) {
        pinned = checked;
        expanded = false;
        if (!preferenceKey().isEmpty()) {
            QSettings settings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat);
            settings.setValue(preferenceKey(), pinned);
            settings.sync();
        }
        updatePresentation();
    });
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
    foldButton = new CompactDisclosureButton(this);
    foldButton->setObjectName("metadataFilterFold");
    foldButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    foldButton->setIconSize(QSize(8, 8));
    foldButton->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    foldButton->setCursor(Qt::PointingHandCursor);
    layout->insertWidget(0, foldButton);
    connect(foldButton, &QToolButton::clicked, this, [this] {
        expanded = !expanded;
        updatePresentation();
    });
    choicesScroll = new QScrollArea(this);
    choicesScroll->setObjectName("metadataFilterChoicesScroll");
    choicesScroll->setWidgetResizable(true);
    choicesScroll->setFrameShape(QFrame::NoFrame);
    choicesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    choicesScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    choicesScroll->setMinimumHeight(24);
    choicesBox = new QWidget;
    choicesBox->setObjectName("metadataFilterChoices");
    auto *choiceLayout = new QVBoxLayout(choicesBox);
    choiceLayout->setContentsMargins(0, 0, 0, 0);
    choiceLayout->setSpacing(6);
    choiceLayout->setSizeConstraint(QLayout::SetMinAndMaxSize);
    choicesScroll->setWidget(choicesBox);
    choicesScroll->setAutoFillBackground(false);
    choicesScroll->viewport()->setAutoFillBackground(false);
    choicesBox->setAutoFillBackground(false);
    layout->insertWidget(1, choicesScroll);
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
    addChoices({ tr("Doujinshi"), tr("Collected volume"), tr("Magazine"), tr("Artbook") }, { "doujinshi", "tankobon", "magazine", "artbook" }, &MetadataFilter::publicationType);
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

QString MetadataFilterPanel::preferenceKey() const
{
    if (host == Host::Sidebar)
        return QStringLiteral("libraryConfig/metadataFilterAlwaysExpandedSidebar");
    if (host == Host::SmartListEditor)
        return QStringLiteral("libraryConfig/metadataFilterAlwaysExpandedSmartEditor");
    return { };
}

QSize MetadataFilterPanel::sizeHint() const
{
    auto hint = QWidget::sizeHint();
    if (!choicesBox || !search || !tabs || !foldButton)
        return hint;
    const auto margins = layout()->contentsMargins();
    // Prefer enough room for all status rows and a useful tag viewport. This
    // intrinsic hint stays independent of the currently allocated scroll height.
    const int height = margins.top() + margins.bottom() + layout()->spacing() * 4 + 4 + search->sizeHint().height() + tabs->tabBar()->sizeHint().height() + 140 + ((pinned || expanded) ? choicesBox->sizeHint().height() : 0) + (!pinned ? foldButton->sizeHint().height() : 0);
    hint.setHeight(qMax(hint.height(), height));
    return hint;
}

void MetadataFilterPanel::updatePresentation()
{
    if (!foldButton || !choicesScroll || !pinButton || !tabs)
        return;
    foldButton->setVisible(!pinned);
    choicesScroll->setVisible(pinned || expanded);
    pinButton->setChecked(pinned);
    foldButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    foldButton->setProperty("expanded", expanded);
    QStringList selected;
    for (auto *button : choicesBox->findChildren<QPushButton *>())
        if (button->isCheckable() && button->isChecked())
            selected.append(button->text());
    const auto label = tr("Filter options") + (selected.isEmpty() ? QString() : QStringLiteral(" · ") + selected.join(QStringLiteral(" / ")));
    foldButton->setText(foldButton->fontMetrics().elidedText(label, Qt::ElideRight, qMax(30, width() - 42)));
    foldButton->setToolTip(label);
    foldButton->setAccessibleName(label);
    updateChoicesGeometry();
}

void MetadataFilterPanel::updateChoicesGeometry()
{
    const int tagMinimum = qMax(54, tabs->tabBar()->sizeHint().height() + fontMetrics().height() + 18);
    if (tabs->minimumHeight() != tagMinimum)
        tabs->setMinimumHeight(tagMinimum);
    const auto *boxLayout = qobject_cast<QVBoxLayout *>(layout());
    const int margins = boxLayout->contentsMargins().top() + boxLayout->contentsMargins().bottom();
    const int fixed = margins + search->sizeHint().height() + tabs->minimumHeight() + boxLayout->spacing() * 4 + 4 + (!pinned ? foldButton->sizeHint().height() : 0);
    const int available = qMax(24, height() - fixed);
    const int limit = qMax(24, qMin(choicesBox->sizeHint().height(), qMin(available, qRound(height() * 0.5))));
    if (choicesScroll->maximumHeight() != limit)
        choicesScroll->setMaximumHeight(limit);
}

void MetadataFilterPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updatePresentation();
}

bool MetadataFilterPanel::event(QEvent *event)
{
    const bool handled = QWidget::event(event);
    if (event->type() == QEvent::LayoutRequest && !presentationPending) {
        presentationPending = true;
        QTimer::singleShot(0, this, [this] {
            presentationPending = false;
            updatePresentation();
        });
    }
    return handled;
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
    for (int i = 0; i < chips.size(); ++i) {
        chips[i]->values = values.value(i);
        chips[i]->selected = i == 0 ? filter.tags : i == 1 ? filter.works
                : i == 2                                   ? filter.authors
                : i == 3                                   ? filter.conventions
                                                           : filter.seriesNames;
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
    updatePresentation();
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
                          "MetadataFilterPanel QToolButton#metadataFilterFold { background:transparent; color:%2; border:0; border-bottom:1px solid %3; padding:3px 0; }"
                          "MetadataFilterPanel QTabWidget::pane { border:0; background:transparent; }"
                          "MetadataFilterPanel QStackedWidget { background:transparent; }"
                          "MetadataFilterPanel QTabBar::tab { background:%4; color:%2; border:1px solid %3; padding:5px 12px; }"
                          "MetadataFilterPanel QTabBar::tab:selected { background:%1; border-bottom-color:%1; }"
                          "MetadataFilterPanel QToolButton#metadataFilterPreviousTabs, MetadataFilterPanel QToolButton#metadataFilterNextTabs { background:transparent; color:%2; border:0; padding:0; }"
                          "MetadataFilterPanel QToolButton#metadataFilterPreviousTabs:disabled, MetadataFilterPanel QToolButton#metadataFilterNextTabs:disabled { color:%3; }"
                          "MetadataFilterPanel QPushButton { background:%4; color:%2; border:1px solid %3; border-radius:4px; padding:4px 3px; }"
                          "MetadataFilterPanel QPushButton:checked { background:%5; color:%6; border-color:%5; }"
                          "MetadataFilterPanel QScrollArea { border:0; background:transparent; }")
                          .arg(background.name(), outline.name(), border.name(), surface.name(),
                               palette().color(QPalette::Highlight).name(), palette().color(QPalette::HighlightedText).name()));
    refresh();
}
