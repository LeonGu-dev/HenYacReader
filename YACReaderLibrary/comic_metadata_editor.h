#ifndef COMIC_METADATA_EDITOR_H
#define COMIC_METADATA_EDITOR_H

#include "descriptive_comic_metadata.h"

#include <QMap>
#include <QSet>
#include <QWidget>

#include <functional>
#include <optional>

class QCheckBox;
class QPushButton;
class QButtonGroup;
class QLineEdit;
class QLabel;
class QVBoxLayout;
class FavoriteHeartButton;
class MetadataPicker;
class MetadataValueChips;
class DraftRating;

class MetadataListEdit : public QWidget
{
    Q_OBJECT
public:
    explicit MetadataListEdit(QWidget *parent = nullptr);
    QStringList values() const;
    void setValues(const QStringList &values, bool mixed = false);
    void toggleValue(const QString &value);
    void setColorColumn(int column);

signals:
    void valuesEdited();
    void inputActivated();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    void refreshTokens();
    QLineEdit *input;
    MetadataValueChips *tokens;
    QStringList selected;
};

class ComicMetadataEditor : public QWidget
{
    Q_OBJECT
public:
    explicit ComicMetadataEditor(QWidget *parent = nullptr);
    ~ComicMetadataEditor() override;
    void setMetadata(const QList<DescriptiveComicMetadata> &records);
    DescriptiveMetadataPatch editedMetadata() const;
    void setFileName(const QString &name);
    void setRatings(const QList<std::optional<int>> &values);
    std::optional<int> editedRating() const;
    void filenameRenamed(const QString &name);

signals:
    void renameRequested(const QString &filename);

public:
    using Vocabulary = QMap<QString, QStringList>;
    void setVocabularyProvider(std::function<Vocabulary()> provider);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

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
    QWidget *wrapPicker(const QString &key, QWidget *input);
    void openPicker(const QString &key);
    void refreshPickers();
    void chooseValue(const QString &key, const QString &value);
    QStringList pickerSelection(const QString &key) const;
    std::function<Vocabulary()> vocabularyProvider;
    Vocabulary vocabulary;
    bool vocabularyLoaded = false;
    QMap<QString, MetadataPicker *> pickers;
    QMap<QString, TextField> texts;
    QMap<QString, ChoiceField> choices;
    QLineEdit *singleAuthor = nullptr;
    QCheckBox *anthology;
    QCheckBox *seriesEnabled;
    QCheckBox *translatedEnabled;
    QWidget *translatedInput;
    QWidget *seriesInput;
    QLabel *seriesEmpty;
    DraftRating *rating;
    std::optional<int> pendingRating;
    QString currentFilename;

    FavoriteHeartButton *favorite;
    MetadataListEdit *authors = nullptr;
    MetadataListEdit *characters = nullptr;
    MetadataListEdit *tags = nullptr;
    QSet<QString> changed;
    bool loading = false;
    bool synchronizing = false;
    bool originalWork = false;
    QWidget *filenameBox;
    QLabel *sourceFileName;
    QWidget *renameBox;
    QLineEdit *newFileName;
    QWidget *renamePreview;
    QPushButton *renameButton;
    QString fileSuffix;
    DescriptiveComicMetadata loadedMetadata;
};

#endif
