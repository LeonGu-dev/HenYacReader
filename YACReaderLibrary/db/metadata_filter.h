#ifndef METADATA_FILTER_H
#define METADATA_FILTER_H

#include "descriptive_comic_metadata.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>

#include <algorithm>
#include <optional>
#include <utility>

struct MetadataFilter {
    QSet<QString> tags, works, authors, conventions, seriesNames;
    QSet<int> ratings;
    bool matchAll = true;
    bool favoritesOnly = false;
    QString color, censorship, translation, publicationType;

    static QString savedPrefix() { return QStringLiteral("myyacreader:metadata-filter:v3:"); }
    static bool isSavedQuery(const QString &query) { return query.startsWith(savedPrefix()) || query.startsWith(QStringLiteral("myyacreader:metadata-filter:v2:")) || query.startsWith(QStringLiteral("myyacreader:metadata-filter:v1:")); }
    static std::optional<qulonglong> savedFolderId(const QString &query)
    {
        const auto object = QJsonDocument::fromJson(query.mid(savedPrefix().size()).toUtf8()).object();
        if (!object.contains("folderId"))
            return std::nullopt;
        return object["folderId"].toString().toULongLong();
    }
    QString savedQuery(std::optional<qulonglong> folderId = std::nullopt) const
    {
        const auto strings = [](const QSet<QString> &values) {
            auto ordered = values.values();
            ordered.sort();
            return QJsonArray::fromStringList(ordered);
        };
        QJsonArray stars;
        auto ordered = ratings.values();
        std::sort(ordered.begin(), ordered.end());
        for (int rating : ordered)
            stars.append(rating);
        QJsonObject object { { "tags", strings(tags) }, { "works", strings(works) }, { "authors", strings(authors) }, { "conventions", strings(conventions) }, { "seriesNames", strings(seriesNames) }, { "ratings", stars }, { "matchAll", matchAll }, { "favoritesOnly", favoritesOnly }, { "color", color }, { "censorship", censorship }, { "translation", translation }, { "publicationType", publicationType } };
        if (folderId)
            object["folderId"] = QString::number(*folderId);
        return savedPrefix() + QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
    }
    static std::optional<MetadataFilter> fromSavedQuery(const QString &query)
    {
        if (!isSavedQuery(query))
            return std::nullopt;
        const auto document = QJsonDocument::fromJson(query.mid(savedPrefix().size()).toUtf8());
        if (!document.isObject())
            return std::nullopt;
        const auto object = document.object();
        if (object.contains("folderId")) {
            bool valid = false;
            const auto id = object["folderId"].toString().toULongLong(&valid);
            if (!object["folderId"].isString() || !valid || !id)
                return std::nullopt;
        }
        MetadataFilter result;
        for (const auto &entry : { std::pair { "tags", &result.tags }, { "works", &result.works }, { "authors", &result.authors } }) {
            if (!object.value(entry.first).isArray())
                return std::nullopt;
            for (const auto &value : object.value(entry.first).toArray()) {
                if (!value.isString())
                    return std::nullopt;
                entry.second->insert(value.toString());
            }
        }
        for (const auto &entry : { std::pair { "conventions", &result.conventions }, std::pair { "seriesNames", &result.seriesNames } }) {
            if (!object.contains(entry.first))
                continue;
            if (!object.value(entry.first).isArray())
                return std::nullopt;
            for (const auto &value : object.value(entry.first).toArray()) {
                if (!value.isString())
                    return std::nullopt;
                entry.second->insert(value.toString());
            }
        }
        if (!object["ratings"].isArray() || !object["matchAll"].isBool() || !object["favoritesOnly"].isBool())
            return std::nullopt;
        for (const auto &value : object["ratings"].toArray()) {
            const int rating = value.toInt(-1);
            if (!value.isDouble() || rating < 1 || rating > 5 || value.toDouble() != rating)
                return std::nullopt;
            result.ratings.insert(rating);
        }
        result.matchAll = object["matchAll"].toBool();
        result.favoritesOnly = object["favoritesOnly"].toBool();
        result.color = object["color"].toString();
        result.censorship = object["censorship"].toString();
        result.translation = object["translation"].toString();
        result.publicationType = object["publicationType"].toString();
        return result;
    }

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
        return favoritesOnly || !tags.isEmpty() || !works.isEmpty() || !authors.isEmpty() || !conventions.isEmpty() || !seriesNames.isEmpty() || !ratings.isEmpty() || !color.isEmpty() || !censorship.isEmpty() || !translation.isEmpty() || !publicationType.isEmpty();
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
        count(conventions, metadata.convention);
        count(seriesNames, metadata.seriesName);
        if (selected && (matchAll ? matched != selected : matched == 0))
            return false;
        if (!color.isEmpty() && (metadata.color.isNull() || metadata.color.toBool() != (color == "color")))
            return false;
        return (censorship.isEmpty() || metadata.censorship.toString() == censorship) && (translation.isEmpty() || metadata.translationStatus.toString() == translation) && (publicationType.isEmpty() || metadata.publicationType.toString() == publicationType);
    }
};
#endif
