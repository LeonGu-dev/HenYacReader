#include "staged_comic_rename.h"

#include "comic_filename.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

namespace {
bool renameFile(const QString &source, const QString &destination, QString &error)
{
    if (source == destination)
        return true;
#ifdef Q_OS_WIN
    if (source.compare(destination, Qt::CaseInsensitive) == 0) {
        const auto temporary = source + "." + QUuid::createUuid().toString(QUuid::Id128) + ".tmp";
        QFile file(source);
        if (!file.rename(temporary)) {
            error = file.errorString();
            return false;
        }
        QFile staged(temporary);
        if (staged.rename(destination))
            return true;
        error = staged.errorString();
        if (!staged.rename(source))
            error += QStringLiteral("；恢复失败，文件位于：") + temporary;
        return false;
    }
#endif
    QFile file(source);
    if (file.rename(destination))
        return true;
    error = file.errorString();
    return false;
}
}
StagedComicRename::StagedComicRename(const QString &source, const QString &destination)
    : source(QFileInfo(source).absoluteFilePath()), destination(QFileInfo(destination).absoluteFilePath()) { }
StagedComicRename::~StagedComicRename()
{
    QString error;
    restore(error);
}
bool StagedComicRename::install(QString &error)
{
    if (source == destination)
        return true;
    error = ComicFilename::invalidNameReason(QFileInfo(destination).fileName());
    if (!error.isEmpty())
        return false;
    if (QFileInfo(source).absolutePath() != QFileInfo(destination).absolutePath()) {
        error = QStringLiteral("只能修改当前目录下的文件名。");
        return false;
    }
    if (!QFileInfo(source).isFile() || QFileInfo(source).isSymLink()) {
        error = QStringLiteral("原文件不存在或为符号链接，未执行重命名。");
        return false;
    }
    if (!renameFile(source, destination, error))
        return false;
    installed = true;
    return true;
}
bool StagedComicRename::restore(QString &error)
{
    if (!installed)
        return true;
    if (!renameFile(destination, source, error)) {
        error = QStringLiteral("无法恢复原文件名，请检查：") + destination + "\n" + error;
        return false;
    }
    installed = false;
    return true;
}
void StagedComicRename::finish()
{
    installed = false;
}
