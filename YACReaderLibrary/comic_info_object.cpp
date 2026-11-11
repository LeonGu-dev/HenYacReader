#include "comic_info_object.h"

#include "overloaded.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QLocale>

ComicInfoObject::ComicInfoObject(QObject *parent)
    : QObject(parent), favorite(false)
{
}

ComicInfoObject::ComicInfoObject(const ComicRecord &record, bool isFavorite, QObject *parent)
    : QObject(parent), record(record), fileName(recordFileName(record)), isAvailable(recordComic(record) != nullptr), favorite(isFavorite)
{
}

qulonglong ComicInfoObject::id() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->id.value();
}

bool ComicInfoObject::read() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->read;
}

bool ComicInfoObject::edited() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->edited;
}

QString ComicInfoObject::hash() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->hash;
}

int ComicInfoObject::rating() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->rating;
}

bool ComicInfoObject::hasBeenOpened() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->hasBeenOpened;
}

int ComicInfoObject::currentPage() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->currentPage;
}

int ComicInfoObject::bookmark1() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->bookmark1;
}

int ComicInfoObject::bookmark2() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->bookmark2;
}

int ComicInfoObject::bookmark3() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->bookmark3;
}

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
int ComicInfoObject::brightness() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->brightness;
}

int ComicInfoObject::contrast() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->contrast;
}

int ComicInfoObject::gamma() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->gamma;
}
QT_WARNING_POP

QVariant ComicInfoObject::title() const
{
    return recordMetadata(record).title;
}

QVariant ComicInfoObject::coverPage() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->coverPage;
}

QVariant ComicInfoObject::numPages() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->numPages;
}

QVariant ComicInfoObject::number() const
{
    return recordMetadata(record).number;
}

QVariant ComicInfoObject::isBis() const
{
    return recordMetadata(record).isBis;
}

QVariant ComicInfoObject::count() const
{
    return recordMetadata(record).count;
}

QVariant ComicInfoObject::volume() const
{
    return recordMetadata(record).volume;
}

QVariant ComicInfoObject::storyArc() const
{
    return recordMetadata(record).storyArc;
}

QVariant ComicInfoObject::arcNumber() const
{
    return recordMetadata(record).arcNumber;
}

QVariant ComicInfoObject::arcCount() const
{
    return recordMetadata(record).arcCount;
}

QVariant ComicInfoObject::genere() const
{
    return recordMetadata(record).genere;
}

QVariant ComicInfoObject::writer() const
{
    return recordMetadata(record).writer;
}

QVariant ComicInfoObject::penciller() const
{
    return recordMetadata(record).penciller;
}

QVariant ComicInfoObject::inker() const
{
    return recordMetadata(record).inker;
}

QVariant ComicInfoObject::colorist() const
{
    return recordMetadata(record).colorist;
}

QVariant ComicInfoObject::letterer() const
{
    return recordMetadata(record).letterer;
}

QVariant ComicInfoObject::coverArtist() const
{
    return recordMetadata(record).coverArtist;
}

QVariant ComicInfoObject::date() const
{
    return recordMetadata(record).date;
}

QVariant ComicInfoObject::publisher() const
{
    return recordMetadata(record).publisher;
}

QVariant ComicInfoObject::format() const
{
    return recordMetadata(record).format;
}

QVariant ComicInfoObject::color() const
{
    return recordMetadata(record).color;
}

QVariant ComicInfoObject::ageRating() const
{
    return recordMetadata(record).ageRating;
}

QVariant ComicInfoObject::synopsis() const
{
    return recordMetadata(record).synopsis;
}

QVariant ComicInfoObject::characters() const
{
    return recordMetadata(record).characters;
}

QVariant ComicInfoObject::notes() const
{
    return recordMetadata(record).notes;
}

QVariant ComicInfoObject::comicVineID() const
{
    return recordMetadata(record).comicVineID;
}

QVariant ComicInfoObject::lastTimeOpened() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->lastTimeOpened;
}

QVariant ComicInfoObject::coverSizeRatio() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->coverSizeRatio;
}

QVariant ComicInfoObject::originalCoverSize() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->originalCoverSize;
}

QVariant ComicInfoObject::added() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->added;
}

QVariant ComicInfoObject::type() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->type;
}

QVariant ComicInfoObject::editor() const
{
    return recordMetadata(record).editor;
}

QVariant ComicInfoObject::imprint() const
{
    return recordMetadata(record).imprint;
}

QVariant ComicInfoObject::teams() const
{
    return recordMetadata(record).teams;
}

QVariant ComicInfoObject::locations() const
{
    return recordMetadata(record).locations;
}

QVariant ComicInfoObject::series() const
{
    return recordMetadata(record).series;
}

QVariant ComicInfoObject::alternateSeries() const
{
    return recordMetadata(record).alternateSeries;
}

QVariant ComicInfoObject::alternateNumber() const
{
    return recordMetadata(record).alternateNumber;
}

QVariant ComicInfoObject::alternateCount() const
{
    return recordMetadata(record).alternateCount;
}

QVariant ComicInfoObject::languageISO() const
{
    return recordMetadata(record).languageISO;
}

QVariant ComicInfoObject::seriesGroup() const
{
    return recordMetadata(record).seriesGroup;
}

QVariant ComicInfoObject::mainCharacterOrTeam() const
{
    return recordMetadata(record).mainCharacterOrTeam;
}

QVariant ComicInfoObject::review() const
{
    return recordMetadata(record).review;
}

QVariant ComicInfoObject::tags() const
{
    return recordMetadata(record).tags;
}

QVariant ComicInfoObject::imageFiltersJson() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->imageFiltersJson;
}

QVariant ComicInfoObject::lastTimeImageFiltersSet() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->lastTimeImageFiltersSet;
}

QVariant ComicInfoObject::lastTimeCoverSet() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->lastTimeCoverSet;
}

QVariant ComicInfoObject::usesExternalCover() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->usesExternalCover;
}

QVariant ComicInfoObject::lastTimeMetadataSet() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->lastTimeMetadataSet;
}

bool ComicInfoObject::isFavorite() const
{
    return favorite;
}

void ComicInfoObject::setRead(bool read)
{
    auto *info = recordInfo(record);
    if (info != nullptr && read != info->read) {
        info->read = read;
        emit readChanged();
    }
}

void ComicInfoObject::setRating(int rating)
{
    auto *info = recordInfo(record);
    if (info != nullptr && rating != info->rating) {
        info->rating = rating;
        emit ratingChanged();
    }
}

void ComicInfoObject::setFavorite(bool isFavorite)
{
    if (isFavorite != favorite) {
        favorite = isFavorite;
        emit favoriteChanged();
    }
}

QStringList ComicInfoObject::getWriters() const
{
    return recordMetadata(record).getWriters();
}

QStringList ComicInfoObject::getPencillers() const
{
    return recordMetadata(record).getPencillers();
}

QStringList ComicInfoObject::getInkers() const
{
    return recordMetadata(record).getInkers();
}

QStringList ComicInfoObject::getColorists() const
{
    return recordMetadata(record).getColorists();
}

QStringList ComicInfoObject::getLetterers() const
{
    return recordMetadata(record).getLetterers();
}

QStringList ComicInfoObject::getCoverArtists() const
{
    return recordMetadata(record).getCoverArtists();
}

QStringList ComicInfoObject::getEditors() const
{
    return recordMetadata(record).getEditors();
}

QStringList ComicInfoObject::getImprint() const
{
    return recordMetadata(record).getImprint();
}

QStringList ComicInfoObject::getCharacters() const
{
    return recordMetadata(record).getCharacters();
}

QStringList ComicInfoObject::getTeams() const
{
    return recordMetadata(record).getTeams();
}

QStringList ComicInfoObject::getLocations() const
{
    return recordMetadata(record).getLocations();
}

QStringList ComicInfoObject::getTags() const
{
    return recordMetadata(record).getTags();
}

QString ComicInfoObject::getTypeString() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    return info->getTypeString();
}

QString ComicInfoObject::getStoryArcInfoString() const
{
    return recordMetadata(record).getStoryArcInfoString();
}

QString ComicInfoObject::getAlternateSeriesString() const
{
    return recordMetadata(record).getAlternateSeriesString();
}

QString ComicInfoObject::getTitleOrFileName() const
{
    return orDash(titleOrFileName(recordMetadata(record), fileName));
}

qulonglong ComicInfoObject::getFileSize() const
{
    const auto *info = recordInfo(record);
    if (info == nullptr)
        return { };
    // the size is encoded in the hash after the SHA-1
    return info->hash.right(info->hash.length() - 40).toLongLong();
}

QString ComicInfoObject::getTitleIncludingNumber() const
{
    return orDash(titleIncludingNumber(recordMetadata(record), fileName));
}

QString ComicInfoObject::getInfoTitle() const
{
    return orDash(infoTitle(recordMetadata(record), fileName));
}

// Display fallback only; never stored in the comic's metadata.
QString ComicInfoObject::orDash(const QString &title)
{
    return title.isEmpty() ? QStringLiteral("-") : title;
}

bool ComicInfoObject::available() const
{
    return isAvailable;
}

bool ComicInfoObject::canSetReadState() const
{
    return recordInfo(record) != nullptr;
}

QVariantMap ComicInfoObject::metadata() const
{
    QVariantMap values;
    for (const auto &column : DescriptiveComicMetadata::columns())
        values.insert(QString::fromLatin1(column.name), recordMetadata(record).*column.member);
    return values;
}

QVariantList ComicInfoObject::detailRows() const
{
    using M = DescriptiveComicMetadata;
    struct Field {
        const char *key;
        const char *label;
        QVariant M::*member;
        const char *chipField;
        bool multiple;
        bool compactHalf = false;
        int compactOrder = 100;
        int compactLimit = 0;
    };
    // Add/reorder descriptive rows here. Empty values and wrapping are handled
    // by the shared delegate, independently of the number of metadata fields.
    static const Field fields[] {
        { "writer", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Author"), &M::writer, "", true, true, 0 },
        { "circle", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Circle"), &M::circle, "", false, true, 1 },
        { "convention", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Convention"), &M::convention, "convention", false, true, 3 },
        { "work", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Work / IP"), &M::work, "work", false, true, 2 },
        { "characters", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Characters"), &M::characters, "character", true, false, 4 },
        { "tags", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Tags"), &M::tags, "tag", true, false, 5, 4 },
        { "seriesName", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Series"), &M::seriesName, "seriesName", false, false, 7 },
        { "translator", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Translator"), &M::translator, "", false, false, 6 },
    };
    const auto &metadata = recordMetadata(record);
    QVariantList rows;
    for (const auto &field : fields) {
        const auto value = metadata.*field.member;
        QStringList values = field.multiple ? value.toString().split('\n', Qt::SkipEmptyParts) : QStringList { value.toString() };
        for (auto &text : values)
            text = text.trimmed();
        values.removeAll(QString());
        if (values.isEmpty())
            continue;
        rows.append(QVariantMap {
                { "key", QString::fromLatin1(field.key) },
                { "label", QCoreApplication::translate("ComicMetadataEditor", field.label) },
                { "values", values },
                { "chipField", QString::fromLatin1(field.chipField) },
                { "compactHalf", field.compactHalf },
                { "compactOrder", field.compactOrder },
                { "compactLimit", field.compactLimit },
                { "note", field.member == &M::writer && metadata.isAnthology.toBool() ? QCoreApplication::translate("ComicMetadataEditor", "Anthology") : QString() },
        });
    }
    return rows;
}

QStringList ComicInfoObject::statusBadges() const
{
    using M = DescriptiveComicMetadata;
    struct Choice {
        QVariant M::*member;
        const char *value;
        const char *label;
    };
    static const Choice choices[] {
        { &M::publicationType, "doujinshi", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Doujinshi") },
        { &M::publicationType, "tankobon", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Collected volume") },
        { &M::publicationType, "magazine", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Magazine") },
        { &M::publicationType, "artbook", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Artbook") },
        { &M::color, "true", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Color") },
        { &M::color, "false", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Black and white") },
        { &M::censorship, "censored", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Censored") },
        { &M::censorship, "uncensored", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Uncensored") },
        { &M::translationStatus, "original", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Japanese") },
        { &M::translationStatus, "translated", QT_TRANSLATE_NOOP("ComicMetadataEditor", "Chinese translation") },
        { &M::translationStatus, "ai", QT_TRANSLATE_NOOP("ComicMetadataEditor", "AI translation") },
    };
    QStringList result;
    const auto &metadata = recordMetadata(record);
    for (const auto &choice : choices) {
        const auto value = metadata.*choice.member;
        if (!value.isNull() && (choice.member == &M::color ? (value.toBool() ? QStringLiteral("true") : QStringLiteral("false")) : value.toString()) == QLatin1String(choice.value))
            result.append(QCoreApplication::translate("ComicMetadataEditor", choice.label));
    }
    return result;
}

QVariantMap ComicInfoObject::fileDetails() const
{
    return {
        { "name", fileName },
        { "path", recordPath(record) },
        { "pages", numPages() },
        { "size", getFileSize() > 0 ? QLocale().formattedDataSize(getFileSize()) : QString() },
    };
}
