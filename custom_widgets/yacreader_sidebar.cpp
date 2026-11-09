#include "yacreader_sidebar.h"

#include "metadata_filter_panel.h"
#include "yacreader_folders_view.h"
#include "yacreader_global.h"
#include "yacreader_global_gui.h"
#include "yacreader_library_list_widget.h"
#include "yacreader_reading_lists_view.h"
#include "yacreader_titled_toolbar.h"

#include <QCoreApplication>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>

#include <utility>

namespace {
class SidebarSectionHeader : public QWidget
{
public:
    explicit SidebarSectionHeader(QSplitterHandle *handle) : QWidget(handle), handle(handle) { }

protected:
    void mousePressEvent(QMouseEvent *event) override { forward(event); }
    void mouseMoveEvent(QMouseEvent *event) override { forward(event); }
    void mouseReleaseEvent(QMouseEvent *event) override { forward(event); }

private:
    QSplitterHandle *handle;
    void forward(QMouseEvent *event)
    {
        QMouseEvent forwarded(event->type(), handle->mapFromGlobal(event->globalPosition().toPoint()),
                              event->globalPosition(), event->button(), event->buttons(), event->modifiers(), event->pointingDevice());
        QCoreApplication::sendEvent(handle, &forwarded);
        event->setAccepted(forwarded.isAccepted());
    }
};
}

YACReaderSideBar::YACReaderSideBar(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    settings = new QSettings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat, this); // TODO unificar la creación del fichero de config con el servidor
    settings->beginGroup("libraryConfig");

    // widgets
    foldersView = new YACReaderFoldersView;
    readingListsView = new YACReaderReadingListsView;
    selectedLibrary = new YACReaderLibraryListWidget;

    // Titles will be set from theme in applyTheme
    librariesTitle = new YACReaderTitledToolBar("");
    foldersTitle = new YACReaderTitledToolBar("");
    readingListsTitle = new YACReaderTitledToolBar("", this);
    filterTitle = new YACReaderTitledToolBar("", this);

    splitter = new QSplitter(this);
    splitter->setOrientation(Qt::Vertical);

    selectedLibrary->setContextMenuPolicy(Qt::ActionsContextMenu);
    selectedLibrary->setAttribute(Qt::WA_MacShowFocusRect, false);
    selectedLibrary->setFocusPolicy(Qt::NoFocus);

    // layout
    QVBoxLayout *l = new QVBoxLayout;

    l->setContentsMargins(0, 0, 0, 0);

    // LIBRARIES-------------------------------------------------------
    l->addSpacing(5);

    l->addWidget(librariesTitle);

    l->addSpacing(4);
    auto sep1 = new YACReaderSideBarSeparator(this);
    separators.append(sep1);
    l->addWidget(sep1);
    l->addSpacing(3);

    l->addWidget(selectedLibrary);
    l->addSpacing(11);

    // END LIBRARIES---------------------------------------------------

    // FOLDERS---------------------------------------------------------
    QWidget *foldersContainer = new QWidget(this);
    QVBoxLayout *foldersLayout = new QVBoxLayout;
    foldersLayout->setContentsMargins(0, 0, 0, 0);
    foldersLayout->setSpacing(0);

    auto sep2 = new YACReaderSideBarSeparator(this);
    separators.append(sep2);
    foldersLayout->addWidget(sep2);
    foldersLayout->addSpacing(4);

    foldersLayout->addWidget(foldersTitle);

    foldersLayout->addSpacing(4);
    auto sep3 = new YACReaderSideBarSeparator(this);
    separators.append(sep3);
    foldersLayout->addWidget(sep3);
    foldersLayout->addSpacing(4);

    foldersLayout->addWidget(foldersView);
    foldersLayout->addSpacing(6);

    foldersContainer->setLayout(foldersLayout);
    splitter->addWidget(foldersContainer);
    // END FOLDERS------------------------------------------------------

    // FILTERS: its header lives in the splitter handle so the whole row is draggable.
    filterPanel = new MetadataFilterPanel(this);
    splitter->addWidget(filterPanel);
    splitter->addWidget(readingListsView);
    foldersContainer->setMinimumHeight(96);
    filterPanel->setMinimumHeight(280);
    readingListsView->setMinimumHeight(120);
    splitter->setChildrenCollapsible(false);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    splitter->setStretchFactor(2, 1);
    splitter->setHandleWidth(39);
    splitter->setObjectName("folderFilterSplitter");
    filterTitle->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto addSectionHeader = [this](int index, YACReaderTitledToolBar *title, QWidget *controls, const QString &objectName) {
        auto *handle = splitter->handle(index);
        auto *header = new SidebarSectionHeader(handle);
        header->setObjectName(objectName);
        auto *headerLayout = new QVBoxLayout(header);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(0);
        auto *topLine = new YACReaderSideBarSeparator(header);
        auto *bottomLine = new YACReaderSideBarSeparator(header);
        topLine->setAttribute(Qt::WA_TransparentForMouseEvents);
        bottomLine->setAttribute(Qt::WA_TransparentForMouseEvents);
        separators.append(topLine);
        separators.append(bottomLine);
        headerLayout->addWidget(topLine);
        headerLayout->addSpacing(4);
        auto *titleRow = new QHBoxLayout;
        titleRow->setSpacing(0);
        titleRow->addWidget(title, 1);
        if (controls)
            titleRow->addWidget(controls);
        headerLayout->addLayout(titleRow);
        headerLayout->addSpacing(4);
        headerLayout->addWidget(bottomLine);
        headerLayout->addSpacing(4);
        auto *handleLayout = new QVBoxLayout(handle);
        handleLayout->setContentsMargins(0, 0, 0, 0);
        handleLayout->addWidget(header);
        handle->setCursor(Qt::SplitVCursor);
    };
    addSectionHeader(1, filterTitle, filterPanel->headerControls(), "metadataFilterHeader");
    filterPanel->headerControls()->show();
    addSectionHeader(2, readingListsTitle, nullptr, "readingListsHeader");

    l->addWidget(splitter);
    l->setSpacing(0);

    setLayout(l);

    splitter->setSizes({ 180, 420, 180 });
    if (settings->contains("sidebarFolderFilterReadingListsSplitterV3"))
        splitter->restoreState(settings->value("sidebarFolderFilterReadingListsSplitterV3").toByteArray());
    connect(splitter, &QSplitter::splitterMoved, this, [this] {
        settings->setValue("sidebarFolderFilterReadingListsSplitterV3", splitter->saveState());
        settings->sync();
    });

    initTheme(this);
}

void YACReaderSideBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.fillRect(0, 0, width(), height(), theme.sidebar.backgroundColor);
}

void YACReaderSideBar::closeEvent(QCloseEvent *event)
{
    QWidget::closeEvent(event);

    settings->setValue("sidebarFolderFilterReadingListsSplitterV3", splitter->saveState());
}

QSize YACReaderSideBar::sizeHint() const
{
    return QSize(275, 200);
}

void YACReaderSideBar::applyTheme(const Theme &theme)
{
    splitter->setStyleSheet(theme.sidebar.splitterQSS);

    // Titles are per-instance, toolbars handle their own colors via Themable
    auto applyCase = [&](const QString &s) { return theme.sidebar.uppercaseLabels ? s.toUpper() : s; };
    librariesTitle->setTitle(applyCase(QObject::tr("Libraries")));
    foldersTitle->setTitle(applyCase(QObject::tr("Folders")));
    filterTitle->setTitle(applyCase(QCoreApplication::translate("MetadataFilterPanel", "Filter")));
    readingListsTitle->setTitle(applyCase(QObject::tr("Reading Lists")));

    for (auto separator : std::as_const(separators)) {
        separator->setColor(theme.sidebar.sectionSeparatorColor);
    }

    update(); // Trigger repaint for background color
}

YACReaderSideBarSeparator::YACReaderSideBarSeparator(QWidget *parent)
    : QWidget(parent), separatorColor(QColor("#575757"))
{
    setFixedHeight(1);
}

void YACReaderSideBarSeparator::setColor(const QColor &color)
{
    separatorColor = color;
    update();
}

void YACReaderSideBarSeparator::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);

    painter.fillRect(5, 0, width() - 10, height(), separatorColor);
}
