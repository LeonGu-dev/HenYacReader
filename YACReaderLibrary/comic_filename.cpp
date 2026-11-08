#include "comic_filename.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace {
QString normalized(QString text)
{
    text.replace(QChar(0xff08), '(').replace(QChar(0xff09), ')');
    text.replace(QChar(0xff3b), '[').replace(QChar(0xff3d), ']');
    return text;
}
bool opening(QChar c)
{
    return c == '(' || c == '[';
}
bool closing(QChar c)
{
    return c == ')' || c == ']';
}
QString safe(QString value)
{
    const QString illegal = QStringLiteral("<>:\"/\\|?*");
    const QString replacement = QStringLiteral("＜＞：＂／＼｜？＊");
    for (int i = 0; i < value.size(); ++i) {
        const int index = illegal.indexOf(value[i]);
        if (index >= 0)
            value[i] = replacement[index];
        else if (value[i].unicode() < 32)
            value[i] = ' ';
    }
    value = value.simplified();
    while (value.endsWith('.'))
        value.chop(1);
    return value.trimmed();
}
}

ComicFilename::Parsed ComicFilename::parse(const QString &fileName)
{
    using M = DescriptiveComicMetadata;
    Parsed result;
    const auto text = normalized(QFileInfo(fileName).completeBaseName());
    struct Group {
        QString value;
        bool round;
        int start;
        bool beforeTitle;
        bool used = false;
    };
    QList<Group> groups;
    QString title;
    for (int i = 0; i < text.size();) {
        if (!opening(text[i])) {
            title += text[i++];
            continue;
        }
        const int start = i++;
        int depth = 1;
        for (; i < text.size() && depth; ++i) {
            if (opening(text[i]))
                ++depth;
            if (closing(text[i]))
                --depth;
        }
        if (depth) {
            title += text.mid(start);
            result.notes << QStringLiteral("括号未闭合，保留在标题中，请核对。");
            break;
        }
        groups.append({ text.mid(start + 1, i - start - 2).trimmed(), text[start] == '(', start, title.trimmed().isEmpty() });
        title += ' ';
    }
    bool translated = false;
    for (auto &group : groups) {
        const auto value = group.value;
        if (value.contains(QRegularExpression(QStringLiteral("^(無修正|无修正|無修|无修|uncensored)$"), QRegularExpression::CaseInsensitiveOption))) {
            result.metadata.set(&M::censorship, "uncensored");
            group.used = true;
        } else if (value.contains(QRegularExpression(QStringLiteral("^(中国翻[訳译]|中國翻[譯訳]|中文翻[译譯]|中文|中[译譯]|Chinese|中[文国國].*AI.*|AI.*翻[译譯訳])$"), QRegularExpression::CaseInsensitiveOption))) {
            result.metadata.set(&M::translationStatus, value.contains("AI", Qt::CaseInsensitive) ? "ai" : "translated");
            translated = true;
            group.used = true;
        } else if (value == QStringLiteral("日文") || value == QStringLiteral("日语") || value == QStringLiteral("日本語")) {
            result.metadata.set(&M::translationStatus, "original");
            group.used = true;
        } else if (value == QStringLiteral("カラー") || value == QStringLiteral("彩色") || value == QStringLiteral("全彩")) {
            result.metadata.set(&M::color, true);
            group.used = true;
        } else if (value == QStringLiteral("黑白") || value == QStringLiteral("白黒")) {
            result.metadata.set(&M::color, false);
            group.used = true;
        }
    }
    // Status tags may precede the creator, but title suffixes are not authors.
    for (auto &creator : groups) {
        if (creator.used || !creator.beforeTitle || creator.value.contains(QRegularExpression("^(C[0-9]+|COMIC[0-9]+|コミケ.*)$", QRegularExpression::CaseInsensitiveOption)))
            continue;
        const auto nested = QRegularExpression("^(.+?)\\s*[\\[(]([^\\[\\]()]+)[\\])]$").match(creator.value);
        const auto author = nested.hasMatch() ? nested.captured(2).trimmed() : creator.value;
        result.metadata.set(&M::circle, nested.hasMatch() ? QVariant(nested.captured(1).trimmed()) : QVariant());
        const auto authors = author.split(QRegularExpression(QStringLiteral("\\s*(?:、|,|＆| & )\\s*")), Qt::SkipEmptyParts);
        result.metadata.set(&M::writer, authors.join('\n'));
        result.metadata.set(&M::isAnthology, authors.size() > 1);
        creator.used = true;
        break;
    }
    for (auto &group : groups) {
        if (!group.used && group.value.contains(QRegularExpression(QStringLiteral("汉化|漢化|翻[译譯訳]")))) {
            result.metadata.set(&M::translator, group.value);
            group.used = true;
            if (!translated) {
                result.metadata.set(&M::translationStatus, group.value.contains("AI", Qt::CaseInsensitive) ? "ai" : "translated");
                translated = true;
            }
            break;
        }
    }
    // With a translation marker the final square bracket is usually the translator.
    if (!result.metadata.contains(&M::translator) && translated && !groups.isEmpty()) {
        auto &last = groups.last();
        if (!last.used && !last.round) {
            result.metadata.set(&M::translator, last.value);
            last.used = true;
            result.notes << QStringLiteral("末尾括号暂按译者识别，请核对。");
        }
    }
    for (auto &group : groups) {
        if (!group.used && group.round) {
            result.metadata.set(&M::work, group.value);
            group.used = true;
            break;
        }
    }
    if (!result.metadata.contains(&M::work)) {
        for (auto &group : groups) {
            if (!group.used && !group.beforeTitle) {
                result.metadata.set(&M::work, group.value);
                group.used = true;
                result.notes << QStringLiteral("标题后的首个方括号暂按作品识别，请核对；它也可能是 tag。");
                break;
            }
        }
    }
    QStringList tags;
    for (const auto &group : groups)
        if (!group.used && !group.value.isEmpty())
            tags << group.value;
    if (!tags.isEmpty()) {
        result.metadata.set(&M::tags, tags.join('\n'));
        result.notes << QStringLiteral("未明确归类的括号内容已放入 tag，请核对作品及标签。");
    }
    title = title.simplified();
    if (!title.isEmpty())
        result.metadata.set(&M::title, title);
    else
        result.notes << QStringLiteral("未识别出标题，保留现有标题。");
    result.notes << QStringLiteral("仅填入识别到的字段；无「無修正」时不推断有码。角色和翻译标题保留原值。");
    return result;
}

QString ComicFilename::generate(const DescriptiveComicMetadata &metadata)
{
    QStringList parts;
    auto authors = metadata.writer.toString().split('\n', Qt::SkipEmptyParts);
    for (auto &author : authors)
        author = safe(author);
    const auto circle = safe(metadata.circle.toString());
    const auto writer = authors.join(QStringLiteral("、"));
    if (!circle.isEmpty() || !writer.isEmpty())
        parts << '[' + (circle.isEmpty() ? writer : writer.isEmpty() ? circle
                                                                     : circle + " (" + writer + ')') +
                        ']';
    parts << safe(metadata.title.toString());
    const auto work = safe(metadata.work.toString());
    if (!work.isEmpty())
        parts << '(' + work + ')';
    for (const auto &tag : metadata.tags.toString().split('\n', Qt::SkipEmptyParts))
        if (!safe(tag).isEmpty())
            parts << '[' + safe(tag) + ']';
    const auto translation = metadata.translationStatus.toString();
    if (translation == "translated")
        parts << QStringLiteral("[中国翻訳]");
    else if (translation == "ai")
        parts << QStringLiteral("[AI翻译]");
    if (metadata.censorship.toString() == "uncensored")
        parts << QStringLiteral("[無修正]");
    const auto translator = safe(metadata.translator.toString());
    if (!translator.isEmpty())
        parts << '[' + translator + ']';
    parts.removeAll(QString());
    return parts.join(' ');
}

QString ComicFilename::invalidNameReason(const QString &name)
{
    if (name.isEmpty() || name.startsWith('.'))
        return QStringLiteral("文件名不能为空。");
    if (name.size() > 255)
        return QStringLiteral("文件名超过 255 个字符，请缩短。");
    if (name.endsWith('.') || name.endsWith(' '))
        return QStringLiteral("文件名不能以空格或句点结尾。");
    if (name.contains(QRegularExpression("[<>:\"/\\\\|?*\\x00-\\x1f]")))
        return QStringLiteral("文件名不能包含路径分隔符或 <>:\"/\\|?* 等字符。");
    const auto stem = name.section('.', 0, 0).trimmed();
    if (stem.contains(QRegularExpression("^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$", QRegularExpression::CaseInsensitiveOption)))
        return QStringLiteral("该文件名为 Windows 保留名称。");
    return { };
}
