#ifndef STAGED_COMIC_RENAME_H
#define STAGED_COMIC_RENAME_H
#include <QString>

// Same-directory rename; finish only after the enclosing database transaction commits.
class StagedComicRename
{
public:
    StagedComicRename(const QString &source, const QString &destination);
    ~StagedComicRename();
    StagedComicRename(const StagedComicRename &) = delete;
    StagedComicRename &operator=(const StagedComicRename &) = delete;
    bool install(QString &error);
    bool restore(QString &error);
    void finish();

private:
    QString source;
    QString destination;
    bool installed = false;
};
#endif
