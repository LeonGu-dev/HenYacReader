#ifndef METADATA_CHIP_COLORS_H
#define METADATA_CHIP_COLORS_H
#include <QColor>
#include <QHash>
#include <QObject>

class MetadataChipColors : public QObject
{
    Q_OBJECT
public:
    static MetadataChipColors &instance();
    static QColor defaultColor(int column, const QString &value);
    static QColor foregroundColor(const QColor &background);
    QColor color(int column, const QString &value);
    bool setColor(int column, const QString &value, const QColor &color);
    void reload();
signals:
    void colorsChanged();

private:
    explicit MetadataChipColors(QObject *parent);
    void loadProfile();
    static QString key(int column, const QString &value);
    QString settingsPath;
    QHash<QString, QColor> overrides;
};
#endif
