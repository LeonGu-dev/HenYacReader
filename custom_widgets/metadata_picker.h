#ifndef METADATA_PICKER_H
#define METADATA_PICKER_H
#include <QStringList>
#include <QWidget>
class QScrollArea;
class QLabel;
class QToolButton;
class QFrame;
class MetadataValueChips;

class MetadataPicker : public QWidget
{
    Q_OBJECT
public:
    MetadataPicker(QWidget *input, int colorColumn, QWidget *parent = nullptr);
    void setValues(QStringList values, const QString &firstValue = QString());
    void setSelected(const QStringList &values);
    void setExpanded(bool expanded);
    bool isExpanded() const;
signals:
    void toggleRequested();
    void valueClicked(const QString &value);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void updateAreaHeight();
    QToolButton *toggle;
    QFrame *entryFrame;
    QScrollArea *area;
    QLabel *empty;
    MetadataValueChips *chips;
    bool expanded = false;
    bool heightPending = false;
};
#endif
