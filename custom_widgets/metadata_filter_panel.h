#ifndef METADATA_FILTER_PANEL_H
#define METADATA_FILTER_PANEL_H

#include "themable.h"

#include <QWidget>

#include <functional>

class ComicModel;
class QLineEdit;
class MetadataFilterChips;

class MetadataFilterPanel : public QWidget, protected Themable
{
    Q_OBJECT
public:
    explicit MetadataFilterPanel(QWidget *parent = nullptr);
    void setModel(ComicModel *model);
    QWidget *headerControls() const { return controls; }

protected:
    void applyTheme(const Theme &theme) override;

private:
    void refresh();
    ComicModel *model = nullptr;
    QLineEdit *search;
    QWidget *controls;
    QList<MetadataFilterChips *> chips;
    QList<std::function<void()>> syncButtons;
    QColor outline;
};
#endif
