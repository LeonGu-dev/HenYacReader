#ifndef METADATA_FILTER_PANEL_H
#define METADATA_FILTER_PANEL_H

#include "themable.h"

#include <QWidget>

#include <functional>

class ComicModel;
class QLineEdit;
class QScrollArea;
class QToolButton;
class QTabBar;
class QStackedWidget;
class MetadataValueChips;

class MetadataFilterTabs : public QWidget
{
    Q_OBJECT
public:
    explicit MetadataFilterTabs(QWidget *parent = nullptr);
    int addTab(QWidget *page, const QString &text);
    int count() const;
    void setCurrentIndex(int index);
    int currentIndex() const;
    void setTabToolTip(int index, const QString &text);
    QTabBar *tabBar() const { return bar; }
    int firstVisibleTab() const { return first; }

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void updateStrip();
    void moveTabs(int step);
    QWidget *strip = nullptr;
    QScrollArea *viewport = nullptr;
    QTabBar *bar = nullptr;
    QStackedWidget *pages = nullptr;
    QToolButton *previous = nullptr;
    QToolButton *next = nullptr;
    int first = 0;
    bool stripUpdatePending = false;
};

class MetadataFilterPanel : public QWidget, protected Themable
{
    Q_OBJECT
public:
    enum class Host { Standalone,
                      Sidebar,
                      SmartListEditor };
    explicit MetadataFilterPanel(QWidget *parent = nullptr, Host host = Host::Standalone);
    void setModel(ComicModel *model);
    QWidget *headerControls() const { return controls; }
    QSize sizeHint() const override;

protected:
    void applyTheme(const Theme &theme) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

private:
    void refresh();
    void updatePresentation();
    void updateChoicesGeometry();
    QString preferenceKey() const;
    Host host;
    bool pinned = true;
    bool expanded = false;
    bool presentationPending = false;
    ComicModel *model = nullptr;
    QLineEdit *search = nullptr;
    QWidget *controls = nullptr;
    QWidget *choicesBox = nullptr;
    QScrollArea *choicesScroll = nullptr;
    QToolButton *pinButton = nullptr;
    QToolButton *foldButton = nullptr;
    MetadataFilterTabs *tabs = nullptr;
    QList<MetadataValueChips *> chips;
    QList<std::function<void()>> syncButtons;
    QColor outline;
};
#endif
