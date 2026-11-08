#ifndef COMIC_METADATA_EDITOR_H
#define COMIC_METADATA_EDITOR_H

#include "descriptive_comic_metadata.h"

#include <QMap>
#include <QSet>
#include <QWidget>

class QCheckBox;
class QButtonGroup;
class QLineEdit;
class QLabel;
class QVBoxLayout;

class MetadataListEdit : public QWidget
{
    Q_OBJECT
public:
    explicit MetadataListEdit(QWidget *parent = nullptr);
    QStringList values() const;
    void setValues(const QStringList &values, bool mixed = false);

signals:
    void valuesEdited();

private:
    void addRow(const QString &value = QString(), bool mixed = false);
    void updateAddButtons();
    QVBoxLayout *rows;
};

class ComicMetadataEditor : public QWidget
{
    Q_OBJECT
public:
    explicit ComicMetadataEditor(QWidget *parent = nullptr);
    void setMetadata(const QList<DescriptiveComicMetadata> &records);
    DescriptiveMetadataPatch editedMetadata() const;
    void setFileName(const QString &name);
    QString pendingFileName() const;

private:
    using Member = QVariant DescriptiveComicMetadata::*;
    struct TextField {
        QLineEdit *edit;
        Member member;
    };
    struct ChoiceField {
        QButtonGroup *edit;
        Member member;
    };
    void syncOriginalFromWork();
    void syncOriginalFromCharacters();
    void updateAuthorMode();
    QMap<QString, TextField> texts;
    QMap<QString, ChoiceField> choices;
    QLineEdit *singleAuthor;
    QCheckBox *anthology;
    MetadataListEdit *authors;
    MetadataListEdit *characters;
    MetadataListEdit *tags;
    QSet<QString> changed;
    bool loading = false;
    bool synchronizing = false;
    bool originalWork = false;
    QWidget *filenameBox;
    QLabel *sourceFileName;
    QWidget *renameBox;
    QLineEdit *newFileName;
    QCheckBox *renameOnSave;
    QString fileSuffix;
    DescriptiveComicMetadata loadedMetadata;
};

#endif
