#ifndef __PROPERTIES_DIALOG_H
#define __PROPERTIES_DIALOG_H

#include "comic_record.h"
#include "descriptive_comic_metadata.h"
#include "library_ids.h"

#include <QDialog>
#include <QImage>
#include <QPixmap>

#include <optional>

class QGridLayout;
class QLabel;
class QScrollArea;
class QDialogButtonBox;
class QPushButton;
class QToolButton;
class StagedCover;
class ComicMetadataEditor;
namespace YACReader {
class CoverLabel;
}

class PropertiesDialog : public QDialog
{
    Q_OBJECT
private:
    QWidget *mainWidget;
    QGridLayout *mainLayout;
    ComicMetadataEditor *metadataEditor;
    QScrollArea *metadataScroll;
    QWidget *coverBox;
    YACReader::CoverLabel *cover;
    QDialogButtonBox *buttonBox;
    QPushButton *closeButton;
    QPushButton *saveButton;
    QPushButton *nextButton;
    QPushButton *previousButton;
    QToolButton *showPreviousCoverPageButton;
    QToolButton *showNextCoverPageButton;
    QLabel *coverPageNumberLabel;
    QToolButton *resetCoverButton;
    QToolButton *loadCustomCoverImageButton;

    void createCoverBox();
    void createButtonBox();
    QList<ComicRecord> records;
    int currentComicIndex = 0;
    void updateCoverPageNumberLabel(int n);
    void loadComic(const ComicRecord &record);
    QPixmap coverOf(const ComicRecord &record) const;
    const ComicDB *lastComicWithFile() const;
    void updateButtons();
    DescriptiveMetadataPatch editedMetadata() const;
    bool applyCoverChange(ComicRecord &record, StagedCover &staged);
    bool showRecord(int index, int step);
    bool updateComics();
    void renameArchive(const QString &filename);

    bool sequentialEditing = false;
    bool coverChanged = false;
    bool updated = false;
    QImage customCover;

public:
    PropertiesDialog(QWidget *parent = nullptr);
    QString databasePath;
    QString basePath;
    QSize sizeHint() const override;
    void done(int result) override;

public slots:
    void setComics(QList<ComicRecord> records);
    bool setComicsForSequentialEditing(int currentComicIndex, QList<ComicRecord> records);
    void saveAndOpenPrevious();
    void saveAndOpenNext();
    void save();
    void saveAndClose();
    void setCover(const QPixmap &cover);
    void setMultipleCover();
    void loadNextCover();
    void loadPreviousCover();
    void resetCover();
    void loadCustomCoverImage();
    void loadCustomCoverImageFromPath(const QString &path);
    void setCoverPage(int pageNumber);
    void updateCoverBoxForMultipleComics();
    void updateCoverBoxForSingleComic();

signals:
    void coverChangedSignal(const QList<ComicInfoId> &ids);
    void placeholderCoverChanged(const QHash<PlaceholderId, std::optional<QString>> &coverKeys);
};
#endif
