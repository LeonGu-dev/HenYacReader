#include "metadata_chip_colors.h"

#include "comic_model.h"
#include "yacreader_global_gui.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

#include <cmath>

MetadataChipColors::MetadataChipColors(QObject *parent) : QObject(parent) { }
MetadataChipColors &MetadataChipColors::instance()
{
    static auto store = new MetadataChipColors(qApp);
    return *store;
}
QString MetadataChipColors::key(int column, const QString &value)
{
    const auto field = column == ComicModel::Work ? QStringLiteral("work") : column == ComicModel::Characters ? QStringLiteral("character")
            : column == ComicModel::Tags                                                                      ? QStringLiteral("tag")
            : column == ComicModel::Author                                                                    ? QStringLiteral("author")
            : column == ComicModel::Convention                                                                ? QStringLiteral("convention")
            : column == -2                                                                                    ? QStringLiteral("circle")
            : column == -4                                                                                    ? QStringLiteral("seriesName")
            : column == -3                                                                                    ? QStringLiteral("translator")
                                                                                                              : QString();
    if (field.isEmpty() || value.isEmpty())
        return { };
    return field + '/' + QString::fromLatin1(QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex());
}
void MetadataChipColors::loadProfile()
{
    const auto path = YACReader::getSettingsPath() + "/YACReaderLibrary.ini";
    if (path == settingsPath)
        return;
    settingsPath = path;
    overrides.clear();
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup("metadataChipColors");
    for (const auto &key : settings.allKeys()) {
        const QColor color(settings.value(key).toString());
        if (color.isValid())
            overrides.insert(key, color);
    }
}
QColor MetadataChipColors::defaultColor(int column, const QString &value)
{
    const auto bytes = QCryptographicHash::hash((QString::number(column) + ':' + value).toUtf8(), QCryptographicHash::Sha256);
    const int hue = (quint8(bytes.at(0)) * 256 + quint8(bytes.at(1))) % 360;
    return QColor::fromHsl(hue, 125 + quint8(bytes.at(2)) % 35, 211);
}
QColor MetadataChipColors::foregroundColor(const QColor &background)
{
    const auto linear = [](qreal component) { return component <= 0.04045 ? component / 12.92 : std::pow((component + 0.055) / 1.055, 2.4); };
    const auto luminance = 0.2126 * linear(background.redF()) + 0.7152 * linear(background.greenF()) + 0.0722 * linear(background.blueF());
    return luminance > 0.179 ? QColor(Qt::black) : QColor(Qt::white);
}
QColor MetadataChipColors::color(int column, const QString &value)
{
    loadProfile();
    return overrides.value(key(column, value), defaultColor(column, value));
}
bool MetadataChipColors::setColor(int column, const QString &value, const QColor &color)
{
    const auto colorKey = key(column, value);
    if (colorKey.isEmpty() || !color.isValid())
        return false;
    loadProfile();
    if (!QDir().mkpath(QFileInfo(settingsPath).absolutePath()))
        return false;
    auto opaque = color.toRgb();
    opaque.setAlpha(255);
    QSettings settings(settingsPath, QSettings::IniFormat);
    settings.setValue("metadataChipColors/" + colorKey, opaque.name(QColor::HexRgb));
    settings.sync();
    if (settings.status() != QSettings::NoError)
        return false;
    overrides.insert(colorKey, opaque);
    emit colorsChanged();
    return true;
}
void MetadataChipColors::reload()
{
    settingsPath.clear();
    loadProfile();
    emit colorsChanged();
}

QVariantMap MetadataChipColors::colorsForField(const QString &field, const QString &value)
{
    const auto background = color(columnForField(field), value);
    return { { "background", background }, { "foreground", foregroundColor(background) } };
}

int MetadataChipColors::columnForField(const QString &field)
{
    static const QHash<QString, int> columns {
        { "work", ComicModel::Work },
        { "character", ComicModel::Characters },
        { "tag", ComicModel::Tags },
        { "author", ComicModel::Author },
        { "convention", ComicModel::Convention },
        { "circle", -2 },
        { "translator", -3 },
        { "seriesName", -4 },
    };
    return columns.value(field, -1);
}
