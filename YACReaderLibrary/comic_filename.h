#ifndef COMIC_FILENAME_H
#define COMIC_FILENAME_H
#include "descriptive_comic_metadata.h"

#include <QString>

namespace ComicFilename {
struct Parsed {
    DescriptiveMetadataPatch metadata;
    QStringList notes;
};
Parsed parse(const QString &fileName);
QString generate(const DescriptiveComicMetadata &metadata);
QString invalidNameReason(const QString &name);
}
#endif
