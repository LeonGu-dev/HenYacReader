#include "properties_dialog.h"

#include "comic_filename.h"
#include "comic_metadata_editor.h"
#include "cover_utils.h"
#include "data_base_management.h"
#include "db_helper.h"
#include "initial_comic_info_extractor.h"
#include "placeholder_cover.h"
#include "staged_comic_rename.h"
#include "staged_cover.h"
#include "yacreader_cover_label.h"
#include "yacreader_global_gui.h"

#include <QApplication>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSet>
#include <QToolButton>
#include <QUuid>

#include <memory>

using namespace YACReader;

PropertiesDialog::PropertiesDialog(QWidget *parent)
    : QDialog(parent)
{
    createCoverBox();
    createButtonBox();

    auto rootLayout = new QGridLayout;

    cover = new YACReader::CoverLabel();
    connect(cover, &YACReader::CoverLabel::imageDropped, this, &PropertiesDialog::loadCustomCoverImageFromPath);

    mainLayout = new QGridLayout;
    metadataEditor = new ComicMetadataEditor;
    connect(metadataEditor, &ComicMetadataEditor::renameRequested, this, &PropertiesDialog::renameArchive);
    metadataEditor->setVocabularyProvider([this] {
        ComicMetadataEditor::Vocabulary values;
        if (databasePath.isEmpty())
            return values;
        const auto file = QDir(databasePath).absoluteFilePath("library.ydb");
        if (!QFileInfo::exists(file))
            return values;
        const auto connection = QStringLiteral("metadata-picker-") + QUuid::createUuid().toString();
        {
            // A separate read-only connection cannot replace a save/import
            // transaction that may still be active while focus changes.
            auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
            db.setDatabaseName(file);
            db.setConnectOptions("QSQLITE_OPEN_READONLY;QSQLITE_BUSY_TIMEOUT=1000");
            if (db.open())
                values = DBHelper::metadataVocabulary(db);
        }
        QSqlDatabase::removeDatabase(connection);
        return values;
    });
    metadataScroll = new QScrollArea;
    metadataScroll->setFrameShape(QFrame::NoFrame);
    metadataScroll->setWidgetResizable(true);
    metadataScroll->setWidget(metadataEditor);
    mainLayout->addWidget(metadataScroll, 0, 0);
    mainLayout->addWidget(buttonBox, 1, 0, Qt::AlignBottom);

    mainWidget = new QWidget(this);
    mainWidget->setAutoFillBackground(true);
    mainWidget->setLayout(mainLayout);
    mainWidget->setMinimumWidth(520);

    auto coverPanel = new QWidget(this);
    coverPanel->setMaximumWidth(480);
    auto coverLayout = new QVBoxLayout(coverPanel);
    coverLayout->setContentsMargins(0, 0, 0, 0);
    coverLayout->setSpacing(0);
    coverLayout->addWidget(cover, 1);
    coverLayout->addWidget(coverBox);

    rootLayout->setHorizontalSpacing(0);
    rootLayout->setVerticalSpacing(0);

    rootLayout->addWidget(coverPanel, 0, 0);
    rootLayout->addWidget(mainWidget, 0, 1);

    rootLayout->setColumnStretch(0, 2);
    rootLayout->setColumnStretch(1, 3);

    this->setLayout(rootLayout);

    this->setContentsMargins(0, 0, 0, 0);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    setModal(true);

    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

QSize PropertiesDialog::sizeHint() const
{
    // Keep a stable default across comics, with room for the complete editor.
    // Screen geometry uses logical pixels, so the cap also respects Windows DPI scaling.
    int height = 960;
    const auto *display = parentWidget() ? parentWidget()->screen() : screen();
    if (display)
        height = qMin(height, qMax(1, display->availableGeometry().height() - 64));
    return QSize(1040, height);
}

void PropertiesDialog::createCoverBox()
{
    coverBox = new QWidget;

    auto layout = new QHBoxLayout;

    QLabel *label = new QLabel(tr("Cover page"));
    label->setStyleSheet("QLabel {color: white; font-weight:bold; font-size:14px;}");
    layout->addWidget(label, 0, Qt::AlignVCenter);
    layout->addStretch();

    showPreviousCoverPageButton = new QToolButton();
    showPreviousCoverPageButton->setIcon(QIcon(":/images/metadata_dialog/previousCoverPage.svg"));
    showPreviousCoverPageButton->setToolTip(tr("Load previous page as cover"));
    showPreviousCoverPageButton->setStyleSheet("QToolButton {border:none;}");
    showNextCoverPageButton = new QToolButton();
    showNextCoverPageButton->setIcon(QIcon(":/images/metadata_dialog/nextCoverPage.svg"));
    showNextCoverPageButton->setToolTip(tr("Load next page as cover"));
    showNextCoverPageButton->setStyleSheet("QToolButton {border:none;}");

    resetCoverButton = new QToolButton();
    resetCoverButton->setIcon(QIcon(":/images/metadata_dialog/resetCover.svg"));
    resetCoverButton->setToolTip(tr("Reset cover to the default image"));
    resetCoverButton->setStyleSheet("QToolButton {border:none;}");

    loadCustomCoverImageButton = new QToolButton();
    loadCustomCoverImageButton->setIcon(QIcon(":/images/metadata_dialog/loadCustomCover.svg"));
    loadCustomCoverImageButton->setToolTip(tr("Load custom cover image"));
    loadCustomCoverImageButton->setStyleSheet("QToolButton {border:none;}");

    coverPageNumberLabel = new QLabel("-");

    coverPageNumberLabel->setStyleSheet("QLabel {color: white; font-weight:bold; font-size:14px;}");

    layout->addWidget(showPreviousCoverPageButton, 0, Qt::AlignVCenter);
    layout->addSpacing(5);
    layout->addWidget(coverPageNumberLabel, 0, Qt::AlignVCenter);
    layout->addSpacing(5);
    layout->addWidget(showNextCoverPageButton, 0, Qt::AlignVCenter);
    layout->addSpacing(5);
    layout->addWidget(resetCoverButton, 0, Qt::AlignVCenter);
    layout->addSpacing(5);
    layout->addWidget(loadCustomCoverImageButton, 0, Qt::AlignVCenter);

    layout->setSpacing(0);

    coverBox->setLayout(layout);

    coverBox->setFixedHeight(28);
    layout->setContentsMargins(5, 2, 5, 2);

    QPalette p(palette());
    p.setColor(QPalette::Window, QColor(0, 0, 0, 200));
    coverBox->setAutoFillBackground(true);
    coverBox->setPalette(p);

    connect(showPreviousCoverPageButton, &QAbstractButton::clicked, this, &PropertiesDialog::loadPreviousCover);
    connect(showNextCoverPageButton, &QAbstractButton::clicked, this, &PropertiesDialog::loadNextCover);
    connect(resetCoverButton, &QAbstractButton::clicked, this, &PropertiesDialog::resetCover);
    connect(loadCustomCoverImageButton, &QAbstractButton::clicked, this, &PropertiesDialog::loadCustomCoverImage);
}

void PropertiesDialog::createButtonBox()
{
    buttonBox = new QDialogButtonBox;

    closeButton = buttonBox->addButton(QDialogButtonBox::Close);
    saveButton = buttonBox->addButton(QDialogButtonBox::Save);
    previousButton = buttonBox->addButton("<", QDialogButtonBox::ButtonRole::NoRole);
    nextButton = buttonBox->addButton(">", QDialogButtonBox::ButtonRole::NoRole);

    connect(closeButton, &QAbstractButton::clicked, this, &PropertiesDialog::close);
    connect(saveButton, &QAbstractButton::clicked, this, &PropertiesDialog::saveAndClose);
    connect(previousButton, &QAbstractButton::clicked, this, &PropertiesDialog::saveAndOpenPrevious);
    connect(nextButton, &QAbstractButton::clicked, this, &PropertiesDialog::saveAndOpenNext);
}

QImage blurred(const QImage &image, const QRect &rect, int radius, bool alphaOnly = false)
{
    int tab[] = { 14, 10, 8, 6, 5, 5, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2 };
    int alpha = (radius < 1) ? 16 : ((radius > 17) ? 1 : tab[radius - 1]);

    QImage result = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    int r1 = rect.top();
    int r2 = rect.bottom();
    int c1 = rect.left();
    int c2 = rect.right();

    int bpl = result.bytesPerLine();
    int rgba[4];
    unsigned char *p;

    int i1 = 0;
    int i2 = 3;

    if (alphaOnly)
        i1 = i2 = (QSysInfo::ByteOrder == QSysInfo::BigEndian ? 0 : 3);

    for (int col = c1; col <= c2; col++) {
        p = result.scanLine(r1) + col * 4;
        for (int i = i1; i <= i2; i++)
            rgba[i] = p[i] << 4;

        p += bpl;
        for (int j = r1; j < r2; j++, p += bpl)
            for (int i = i1; i <= i2; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }

    for (int row = r1; row <= r2; row++) {
        p = result.scanLine(row) + c1 * 4;
        for (int i = i1; i <= i2; i++)
            rgba[i] = p[i] << 4;

        p += 4;
        for (int j = c1; j < c2; j++, p += 4)
            for (int i = i1; i <= i2; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }

    for (int col = c1; col <= c2; col++) {
        p = result.scanLine(r2) + col * 4;
        for (int i = i1; i <= i2; i++)
            rgba[i] = p[i] << 4;

        p -= bpl;
        for (int j = r1; j < r2; j++, p -= bpl)
            for (int i = i1; i <= i2; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }

    for (int row = r1; row <= r2; row++) {
        p = result.scanLine(row) + c2 * 4;
        for (int i = i1; i <= i2; i++)
            rgba[i] = p[i] << 4;

        p -= 4;
        for (int j = c1; j < c2; j++, p -= 4)
            for (int i = i1; i <= i2; i++)
                p[i] = (rgba[i] += ((p[i] << 4) - rgba[i]) * alpha / 16) >> 4;
    }

    return result;
}

void PropertiesDialog::loadComic(const ComicRecord &record)
{
    customCover = QImage();
    const auto *comicInfo = recordInfo(record);
    const auto *comic = recordComic(record);
    coverChanged = false;
    showPreviousCoverPageButton->setEnabled(comic != nullptr);
    showNextCoverPageButton->setEnabled(comic != nullptr);
    resetCoverButton->setEnabled(comic != nullptr);
    loadCustomCoverImageButton->setEnabled(true);
    updateCoverPageNumberLabel(comicInfo && !comicInfo->coverPage.isNull() ? comicInfo->coverPage.toInt() : 1);
    updateCoverBoxForSingleComic();
    if (comic && !QFileInfo(basePath + comic->file.path).exists()) {
        QMessageBox::warning(this, tr("Not found"), tr("Comic not found. You should update your library."));
        showPreviousCoverPageButton->setDisabled(true);
        showNextCoverPageButton->setDisabled(true);
    }
    setWindowTitle(tr("Edit comic information"));
    setCover(coverOf(record));
    metadataEditor->setMetadata({ recordMetadata(record) });
    metadataEditor->setRatings({ recordInfo(record) ? std::optional<int>(recordInfo(record)->rating) : std::nullopt });
    metadataEditor->setFileName(comic ? recordFileName(record) : QString());
}

QPixmap PropertiesDialog::coverOf(const ComicRecord &record) const
{
    if (const auto *comic = recordComic(record))
        return comic->info.getCover(basePath);

    QImage image;
    if (const auto *placeholder = recordPlaceholder(record)) {
        if (placeholder->coverKey)
            image = QImage(LibraryPaths::placeholderCoverPathFromDataPath(databasePath, *placeholder->coverKey));
    } else {
        image = QImage(LibraryPaths::coverPath(basePath, recordInfo(record)->hash));
    }
    if (image.isNull())
        image = QImage(":/images/unavailableCover.png");
    return QPixmap::fromImage(image.convertToFormat(QImage::Format_Grayscale8));
}

const ComicDB *PropertiesDialog::lastComicWithFile() const
{
    for (auto it = records.crbegin(); it != records.crend(); ++it) {
        if (const auto *comic = recordComic(*it))
            return comic;
    }
    return nullptr;
}

void PropertiesDialog::updateButtons()
{
    if (sequentialEditing) {
        previousButton->setDisabled(currentComicIndex == 0);
        nextButton->setDisabled(currentComicIndex == records.length() - 1);
        previousButton->setHidden(false);
        nextButton->setHidden(false);
    } else {
        previousButton->setHidden(true);
        nextButton->setHidden(true);
    }
}

void PropertiesDialog::setComics(QList<ComicRecord> records)
{
    if (records.isEmpty())
        return;
    sequentialEditing = false;
    updated = false;
    currentComicIndex = 0;
    this->records = records;
    loadComic(records.first());
    updateButtons();
    if (records.length() > 1) {
        updateCoverBoxForMultipleComics();
        resetCoverButton->setEnabled(lastComicWithFile() != nullptr);
        setWindowTitle(tr("Edit selected comics information"));
        setMultipleCover();
    }
    QList<DescriptiveComicMetadata> metadata;
    for (const auto &record : records)
        metadata.append(recordMetadata(record));
    metadataEditor->setMetadata(metadata);
    QList<std::optional<int>> ratings;
    for (const auto &record : records)
        ratings.append(recordInfo(record) ? std::optional<int>(recordInfo(record)->rating) : std::nullopt);
    metadataEditor->setRatings(ratings);
    if (records.length() > 1)
        metadataEditor->setFileName(QString());
}

bool PropertiesDialog::setComicsForSequentialEditing(int currentComicIndex, QList<ComicRecord> records)
{
    sequentialEditing = true;

    updated = false;
    coverChanged = false;
    customCover = QImage();

    this->records = records;
    this->currentComicIndex = currentComicIndex;

    return showRecord(currentComicIndex, 1);
}

bool PropertiesDialog::showRecord(int index, int step)
{
    for (; index >= 0 && index < records.length(); index += step) {
        std::optional<ComicRecord> loaded;
        QString connectionName = "";
        {
            QSqlDatabase db = DataBaseManagement::loadDatabase(databasePath);
            loaded = DBHelper::loadRecord(records[index], db);
            connectionName = db.connectionName();
        }
        QSqlDatabase::removeDatabase(connectionName);

        if (loaded) {
            records[index] = *loaded;
            currentComicIndex = index;
            loadComic(records[index]);
            updateButtons();
            return true;
        }

        QMessageBox::information(this, tr("Comic skipped"), tr("A comic was removed from the library or now has a different file, so it is skipped."));
    }
    return false;
}

DescriptiveMetadataPatch PropertiesDialog::editedMetadata() const
{
    return metadataEditor->editedMetadata();
}

bool PropertiesDialog::applyCoverChange(ComicRecord &record, StagedCover &staged)
{
    auto &info = *recordInfo(record);
    const auto *comic = recordComic(record);

    if (customCover.isNull()) {
        if (comic == nullptr)
            return false;

        // sequential editing keeps the chosen page; the reset of several comics goes back to the first page
        const int page = sequentialEditing ? coverPageNumberLabel->text().toInt() : 1;
        info.coverPage = sequentialEditing ? QVariant(page) : QVariant();

        InitialComicInfoExtractor ie(basePath + comic->file.path, staged.stagedPath(), page);
        ie.extract();
        if (!staged.isStaged())
            return false;

        if (ie.getOriginalCoverSize().second > 0) {
            info.originalCoverSize = QString("%1x%2").arg(ie.getOriginalCoverSize().first).arg(ie.getOriginalCoverSize().second);
            info.coverSizeRatio = static_cast<float>(ie.getOriginalCoverSize().first) / ie.getOriginalCoverSize().second;
        }
        info.lastTimeCoverSet = QDateTime::currentSecsSinceEpoch();
        info.usesExternalCover = false;
    } else {
        if (!staged.stage(customCover))
            return false;
        info.coverPage = QVariant();

        auto width = customCover.width();
        auto height = customCover.height();
        info.originalCoverSize = QString("%1x%2").arg(width).arg(height);
        info.coverSizeRatio = static_cast<float>(width) / height;
        info.lastTimeCoverSet = QDateTime::currentSecsSinceEpoch();
        info.usesExternalCover = true;
    }
    return true;
}

void PropertiesDialog::renameArchive(const QString &filename)
{
    const int index = sequentialEditing ? currentComicIndex : records.size() == 1 ? 0
                                                                                  : -1;
    const auto *comic = index >= 0 && index < records.size() ? recordComic(records[index]) : nullptr;
    auto error = ComicFilename::invalidNameReason(filename);
    if (!comic || !error.isEmpty()) {
        QMessageBox::warning(this, tr("Rename failed"), comic ? error : tr("Rename is only available for one existing archive."));
        return;
    }
    if (QFileInfo(filename).suffix().compare(QFileInfo(comic->file.fileName).suffix(), Qt::CaseInsensitive) != 0) {
        QMessageBox::warning(this, tr("Rename failed"), tr("Keep the archive extension unchanged."));
        return;
    }
    const auto relative = comic->file.path.left(comic->file.path.lastIndexOf('/') + 1) + filename;
    if (relative == comic->file.path)
        return;
    const auto oldPath = comic->file.path;
    StagedComicRename rename(basePath + oldPath, basePath + relative);
    bool success = false;
    QString connection;
    {
        auto db = DataBaseManagement::loadDatabase(databasePath);
        connection = db.connectionName();
        if (db.transaction()) {
            const auto stored = DBHelper::loadRecord(records[index], db);
            const auto *current = stored ? recordComic(*stored) : nullptr;
            if (current && current->file.path == oldPath && rename.install(error)) {
                success = DBHelper::moveComic(current->file.id.value(), current->file.folderId.value(), filename, relative, db) && db.commit();
            }
            if (!success)
                db.rollback();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    if (!success) {
        QString rollbackError;
        if (!rename.restore(rollbackError))
            error += "\n" + rollbackError;
        QMessageBox::warning(this, tr("Rename failed"), tr("The file could not be renamed.") + (error.isEmpty() ? QString() : "\n" + error));
        return;
    }
    rename.finish();
    auto *saved = recordComic(records[index]);
    saved->file.fileName = filename;
    saved->file.path = relative;
    metadataEditor->filenameRenamed(filename);
    updated = true;
}

bool PropertiesDialog::updateComics()
{
    const auto metadata = editedMetadata();
    const int begin = sequentialEditing ? currentComicIndex : 0;
    const int end = sequentialEditing ? currentComicIndex + 1 : records.length();

    const auto rating = metadataEditor->editedRating();
    bool refused = false;
    bool failed = false;
    bool wrote = false;
    // the records change only after the commit, so a failed save keeps them, the files, and the database as they were
    QList<std::pair<int, ComicRecord>> savedRecords;
    QList<StagedCover> stagedCovers;
    QList<ComicInfoId> changedCovers;
    // a placeholder's new cover is a new file; the old files go after the commit, the new ones on a failure
    QHash<PlaceholderId, std::optional<QString>> storedPlaceholderCovers;
    QList<std::pair<PlaceholderId, QString>> newPlaceholderCovers;
    QString connectionName = "";
    {
        QSqlDatabase db = DataBaseManagement::loadDatabase(databasePath);
        if (!db.transaction()) {
            QMessageBox::warning(this, tr("Changes not saved"), tr("The changes could not be saved."));
            return false;
        }

        for (int i = begin; i < end && !refused; i++) {
            const auto stored = DBHelper::loadRecord(records[i], db);
            refused = !stored;
            if (const auto *placeholder = stored ? recordPlaceholder(*stored) : nullptr)
                storedPlaceholderCovers.insert(placeholder->id, placeholder->coverKey);
        }

        if (!refused) {
            bool written = true;
            // each comic_info or placeholder is written once, also when several rows show it
            QSet<ComicInfoId> writtenIds;
            QSet<PlaceholderId> writtenPlaceholders;
            for (int i = begin; i < end; i++) {
                auto record = records[i];
                if (auto *placeholderRecord = std::get_if<ComicRecordCase::Placeholder>(&record)) {
                    auto &placeholder = placeholderRecord->placeholder;
                    if (writtenPlaceholders.contains(placeholder.id))
                        continue;
                    writtenPlaceholders.insert(placeholder.id);

                    auto patch = metadata;
                    if (!patch.isEmpty()) {
                        written = DBHelper::updatePlaceholderMetadata(placeholder.id, patch, db) && written;
                        patch.applyTo(placeholder);
                        wrote = true;
                    }

                    // a cover page or a reset needs a file
                    if (coverChanged && !customCover.isNull()) {
                        const auto key = savePlaceholderCover(databasePath, customCover);
                        written = key && DBHelper::updatePlaceholderCover(placeholder.id, key, db) && written;
                        if (key) {
                            newPlaceholderCovers.append({ placeholder.id, *key });
                            placeholder.coverKey = key;
                        }
                        wrote = true;
                    }

                    savedRecords.append({ i, std::move(record) });
                    continue;
                }

                auto &info = *recordInfo(record);
                if (writtenIds.contains(info.id))
                    continue;
                writtenIds.insert(info.id);

                auto patch = metadata;

                if (!patch.isEmpty()) {
                    written = DBHelper::updateComicInfoMetadata(info.id, patch, db) && written;
                    patch.applyTo(info);
                    info.edited = true;
                    wrote = true;
                }

                if (rating) {
                    written = DBHelper::updateComicRating(info.id, *rating, db) && written;
                    info.rating = *rating;
                    wrote = true;
                }
                if (coverChanged) {
                    StagedCover staged(LibraryPaths::coverPath(basePath, info.hash));
                    if (applyCoverChange(record, staged)) {
                        written = DBHelper::updateComicInfoCover(info, db) && written;
                        info.edited = true;
                        stagedCovers.append(staged);
                        changedCovers.append(info.id);
                        wrote = true;
                    }
                }

                savedRecords.append({ i, std::move(record) });
            }

            written = written && std::all_of(stagedCovers.begin(), stagedCovers.end(), [](StagedCover &staged) { return staged.install(); });
            if (written && db.commit()) {
                for (auto &staged : stagedCovers)
                    staged.finish();
                for (const auto &[id, key] : std::as_const(newPlaceholderCovers)) {
                    if (const auto old = storedPlaceholderCovers.value(id))
                        DBHelper::removePlaceholderCovers(databasePath, { *old });
                }
            } else {
                db.rollback();
                for (auto &staged : stagedCovers)
                    staged.restore();
                for (const auto &[id, key] : std::as_const(newPlaceholderCovers))
                    DBHelper::removePlaceholderCovers(databasePath, { key });
                newPlaceholderCovers.clear();
                failed = true;
            }
        } else {
            db.rollback();
        }
        connectionName = db.connectionName();
    }
    QSqlDatabase::removeDatabase(connectionName);

    if (refused) {
        QMessageBox::warning(this, tr("Changes not saved"), tr("A comic that you are editing was removed from the library or now has a different file. Your changes were not saved."));
        return false;
    }
    if (failed) {
        QMessageBox::warning(this, tr("Changes not saved"), tr("The changes could not be saved."));
        return false;
    }

    for (auto &[index, record] : savedRecords)
        records[index] = std::move(record);
    updated = updated || wrote;

    if (!changedCovers.isEmpty())
        emit coverChangedSignal(changedCovers);
    if (!newPlaceholderCovers.isEmpty()) {
        QHash<PlaceholderId, std::optional<QString>> coverKeys;
        for (const auto &[id, key] : std::as_const(newPlaceholderCovers))
            coverKeys.insert(id, key);
        emit placeholderCoverChanged(coverKeys);
    }
    return true;
}

void PropertiesDialog::setMultipleCover()
{
    QPixmap last = coverOf(records.last());
    last = last.scaledToHeight(575, Qt::SmoothTransformation);

    auto coverImage = QPixmap::fromImage(blurred(last.toImage(), QRect(0, 0, last.width(), last.height()), 15));

    cover->setPixmap(coverImage);
}

void PropertiesDialog::setCover(const QPixmap &coverI)
{
    cover->setPixmap(coverI);
}

void PropertiesDialog::saveAndOpenPrevious()
{
    if (!updateComics())
        return;

    if (!showRecord(currentComicIndex - 1, -1)) {
        close();
        return;
    }

    repaint();
}

void PropertiesDialog::saveAndOpenNext()
{
    if (!updateComics())
        return;

    if (!showRecord(currentComicIndex + 1, 1)) {
        close();
        return;
    }

    repaint();
}

void PropertiesDialog::saveAndClose()
{
    if (!updateComics())
        return;

    close();
}

void PropertiesDialog::done(int result)
{
    if (updated && result != QDialog::Accepted)
        emit accepted();
    updated = false;
    coverChanged = false;
    customCover = QImage();
    metadataEditor->setFileName(QString());
    QDialog::done(result);
}

void PropertiesDialog::updateCoverPageNumberLabel(int n)
{
    coverPageNumberLabel->setText(QString::number(n));
    coverPageNumberLabel->adjustSize();
}

void PropertiesDialog::loadNextCover()
{
    const auto *comic = recordComic(records[currentComicIndex]);
    if (comic == nullptr)
        return;
    const auto &info = comic->info;
    int current = coverPageNumberLabel->text().toInt();
    int next;

    if (current == info.numPages.toInt())
        next = 1;
    else
        next = current + 1;

    setCoverPage(next);

    coverChanged = next != info.coverPage;
}

void PropertiesDialog::loadPreviousCover()
{
    const auto *comic = recordComic(records[currentComicIndex]);
    if (comic == nullptr)
        return;
    const auto &info = comic->info;
    int current = coverPageNumberLabel->text().toInt();
    int previous;

    if (current == 1)
        previous = info.numPages.toInt();
    else
        previous = current - 1;

    setCoverPage(previous);

    coverChanged = previous != info.coverPage.toInt();
}

void PropertiesDialog::resetCover()
{
    if (sequentialEditing) {
        setCoverPage(1);
        coverChanged = true; // it could be that the cover is a custom cover, so we need to always update it
    } else {
        const auto *comic = lastComicWithFile();
        if (comic == nullptr)
            return;

        InitialComicInfoExtractor ie(basePath + comic->file.path, "", 1);
        ie.extract();
        auto cover = ie.getCover();
        cover = cover.scaledToHeight(575, Qt::SmoothTransformation);

        auto coverImage = QPixmap::fromImage(blurred(cover.toImage(), QRect(0, 0, cover.width(), cover.height()), 15));

        this->cover->setPixmap(coverImage);

        customCover = QImage();
        coverChanged = true;
    }
}

void PropertiesDialog::loadCustomCoverImage()
{
    auto path = YACReader::imageFileLoader(this);
    loadCustomCoverImageFromPath(path);
}

void PropertiesDialog::loadCustomCoverImageFromPath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    const QImage image(path);
    if (image.isNull()) {
        QMessageBox::warning(this, tr("Invalid cover"), tr("The image is invalid."));
        return;
    }

    customCover = image;
    coverChanged = true;

    const bool deleted = (sequentialEditing || records.length() == 1) && recordComic(records[currentComicIndex]) == nullptr;
    setCover(QPixmap::fromImage(deleted ? customCover.convertToFormat(QImage::Format_Grayscale8) : customCover));
}

void PropertiesDialog::setCoverPage(int pageNumber)
{
    const auto *comic = recordComic(records[currentComicIndex]);
    if (comic == nullptr)
        return;

    customCover = QImage();

    updateCoverPageNumberLabel(pageNumber);
    InitialComicInfoExtractor ie(basePath + comic->file.path, "", pageNumber);
    ie.extract();
    setCover(ie.getCover());
    repaint();
}

void PropertiesDialog::updateCoverBoxForMultipleComics()
{
    showPreviousCoverPageButton->hide();
    showNextCoverPageButton->hide();
    coverPageNumberLabel->hide();
}

void PropertiesDialog::updateCoverBoxForSingleComic()
{
    showPreviousCoverPageButton->show();
    showNextCoverPageButton->show();
    coverPageNumberLabel->show();
}
