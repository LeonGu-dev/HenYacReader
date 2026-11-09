#ifndef METADATA_FILTER_H
#define METADATA_FILTER_H

#include "descriptive_comic_metadata.h"

#include <QSet>
#include <QStringList>

struct MetadataFilter {
    QSet<QString> tags, works, authors;
    QSet<int> ratings;
    bool matchAll = true;
    bool favoritesOnly = false;
    QString color, censorship, translation, publicationType;

    static QStringList values(const QVariant &value)
    {
        QStringList result;
        for (const auto &part : value.toString().split('\n')) {
            const auto text = part.trimmed();
            if (!text.isEmpty() && !result.contains(text))
                result.append(text);
        }
        return result;
    }
    bool active() const
    {
        return favoritesOnly || !tags.isEmpty() || !works.isEmpty() || !authors.isEmpty() || !ratings.isEmpty() || !color.isEmpty() || !censorship.isEmpty() || !translation.isEmpty() || !publicationType.isEmpty();
    }
    bool matches(const DescriptiveComicMetadata &metadata, int rating = 0) const
    {
        if (favoritesOnly && !metadata.favorite.toBool())
            return false;
        if (!ratings.isEmpty() && !ratings.contains(rating))
            return false;
        int selected = 0, matched = 0;
        const auto count = [&](const QSet<QString> &selection, const QVariant &field) {
            const auto present = values(field);
            for (const auto &value : selection) {
                ++selected;
                if (present.contains(value))
                    ++matched;
            }
        };
        count(tags, metadata.tags);
        count(works, metadata.work);
        count(authors, metadata.writer);
        if (selected && (matchAll ? matched != selected : matched == 0))
            return false;
        if (!color.isEmpty() && (metadata.color.isNull() || metadata.color.toBool() != (color == "color")))
            return false;
        return (censorship.isEmpty() || metadata.censorship.toString() == censorship) && (translation.isEmpty() || metadata.translationStatus.toString() == translation) && (publicationType.isEmpty() || metadata.publicationType.toString() == publicationType);
    }
};
#endif
