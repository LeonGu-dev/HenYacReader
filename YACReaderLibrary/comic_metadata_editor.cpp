#include "comic_metadata_editor.h"

#include "comic_filename.h"
#include "favorite_heart.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QString originalName()
{
    return QStringLiteral("原创");
}

QVariant textValue(const QString &text)
{
    const auto trimmed = text.trimmed();
    return trimmed.isEmpty() ? QVariant() : QVariant(trimmed);
}

QStringList splitValues(const QVariant &value)
{
    return value.toString().split('\n', Qt::SkipEmptyParts);
}
}

MetadataListEdit::MetadataListEdit(QWidget *parent)
    : QWidget(parent), rows(new QVBoxLayout)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    rows->setSpacing(4);
    layout->addLayout(rows);
    addRow();
}

void MetadataListEdit::addRow(const QString &value, bool mixed)
{
    auto row = new QWidget(this);
    auto layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto edit = new QLineEdit(value, row);
    edit->setObjectName("value");
    if (mixed)
        edit->setPlaceholderText(tr("Multiple values; edit to replace"));
    auto add = new QPushButton(QStringLiteral("＋"), row);
    add->setObjectName("add");
    add->setAccessibleName(tr("Add item"));
    add->setFixedWidth(32);
    auto remove = new QPushButton(QStringLiteral("－"), row);
    remove->setObjectName("remove");
    remove->setAccessibleName(tr("Remove item"));
    remove->setFixedWidth(32);
    layout->addWidget(edit, 1);
    layout->addWidget(add);
    layout->addWidget(remove);
    rows->addWidget(row);
    updateAddButtons();
    connect(edit, &QLineEdit::textChanged, this, &MetadataListEdit::valuesEdited);
    connect(add, &QPushButton::clicked, this, [this] {
        addRow();
        rows->itemAt(rows->count() - 1)->widget()->findChild<QLineEdit *>()->setFocus();
        emit valuesEdited();
    });
    connect(remove, &QPushButton::clicked, this, [this, row] {
        rows->removeWidget(row);
        row->hide();
        row->deleteLater();
        if (rows->count() == 0)
            addRow();
        updateAddButtons();
        emit valuesEdited();
    });
}

void MetadataListEdit::updateAddButtons()
{
    for (int i = 0; i < rows->count(); ++i)
        rows->itemAt(i)->widget()->findChild<QPushButton *>("add")->setVisible(i == rows->count() - 1);
}

QStringList MetadataListEdit::values() const
{
    QStringList result;
    for (int i = 0; i < rows->count(); ++i) {
        const auto text = rows->itemAt(i)->widget()->findChild<QLineEdit *>()->text().trimmed();
        if (!text.isEmpty() && !result.contains(text))
            result.append(text);
    }
    return result;
}

void MetadataListEdit::setValues(const QStringList &values, bool mixed)
{
    while (auto item = rows->takeAt(0)) {
        auto row = item->widget();
        row->hide();
        row->setParent(nullptr);
        row->deleteLater();
        delete item;
    }
    for (const auto &value : values)
        addRow(value);
    if (rows->count() == 0)
        addRow(QString(), mixed);
}

ComicMetadataEditor::ComicMetadataEditor(QWidget *parent)
    : QWidget(parent)
{
    using M = DescriptiveComicMetadata;
    auto layout = new QVBoxLayout(this);
    filenameBox = new QWidget(this);
    filenameBox->setVisible(false);
    auto filenameLayout = new QVBoxLayout(filenameBox);
    filenameLayout->setContentsMargins(0, 0, 0, 0);
    sourceFileName = new QLabel(filenameBox);
    sourceFileName->setObjectName("sourceFilename");
    sourceFileName->setTextFormat(Qt::PlainText);
    sourceFileName->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sourceFileName->setWordWrap(true);
    sourceFileName->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto filenameFont = sourceFileName->font();
    if (filenameFont.pointSizeF() > 0)
        filenameFont.setPointSizeF(filenameFont.pointSizeF() + 2);
    else
        filenameFont.setPixelSize(filenameFont.pixelSize() + 2);
    sourceFileName->setFont(filenameFont);
    sourceFileName->setToolTip(tr("Current archive filename"));
    filenameLayout->addWidget(sourceFileName);
    auto parse = new QPushButton(tr("Parse tags from filename"), filenameBox);
    parse->setObjectName("parseFilename");
    filenameLayout->addWidget(parse);
    renameBox = new QWidget(this);
    renameBox->hide();
    auto renameLayout = new QVBoxLayout(renameBox);
    renameLayout->setContentsMargins(0, 0, 0, 0);
    auto generate = new QPushButton(tr("Generate filename"), renameBox);
    generate->setObjectName("generateFilename");
    renameLayout->addWidget(generate);
    auto preview = new QWidget(renameBox);
    preview->setVisible(false);
    auto previewLayout = new QVBoxLayout(preview);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    newFileName = new QLineEdit(preview);
    newFileName->setObjectName("newFilename");
    newFileName->setToolTip(tr("Edit the filename without its extension"));
    previewLayout->addWidget(newFileName);
    renameOnSave = new QCheckBox(tr("Rename this archive when saving (keep extension)"), preview);
    renameOnSave->setObjectName("renameOnSave");
    previewLayout->addWidget(renameOnSave);
    renameLayout->addWidget(preview);
    auto notes = new QLabel(renameBox);
    notes->setObjectName("filenameNotes");
    notes->setWordWrap(true);
    notes->hide();
    renameLayout->addWidget(notes);
    layout->addWidget(filenameBox);
    connect(parse, &QPushButton::clicked, this, [this] {
        using M = DescriptiveComicMetadata;
        const auto parsed = ComicFilename::parse(sourceFileName->text());
        auto values = loadedMetadata;
        editedMetadata().applyTo(values);
        parsed.metadata.applyTo(values);
        for (auto it = texts.begin(); it != texts.end(); ++it)
            if (parsed.metadata.contains(it->member))
                it->edit->setText((values.*(it->member)).toString());
        if (parsed.metadata.contains(&M::writer)) {
            anthology->setChecked(values.isAnthology.toBool());
            singleAuthor->setText(values.writer.toString().section('\n', 0, 0));
            authors->setValues(splitValues(values.writer));
            changed.insert("writer");
            changed.insert("isAnthology");
        }
        if (parsed.metadata.contains(&M::tags)) {
            tags->setValues(splitValues(values.tags));
            changed.insert("tags");
        }
        for (auto it = choices.begin(); it != choices.end(); ++it) {
            if (!parsed.metadata.contains(it->member))
                continue;
            for (auto button : it->edit->buttons())
                if (button->property("value") == values.*(it->member))
                    button->setChecked(true);
            changed.insert(it.key());
        }
    });
    connect(generate, &QPushButton::clicked, this, [this, preview, notes] {
        auto metadata = loadedMetadata;
        editedMetadata().applyTo(metadata);
        newFileName->setText(ComicFilename::generate(metadata));
        renameOnSave->setChecked(false);
        preview->show();
        newFileName->setFocus();
        notes->setText(tr("Check the preview, then enable rename on save. Extension: %1. Translated title, characters and color stay in the library.").arg(fileSuffix));
        notes->show();
    });
    auto form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setSpacing(10);
    layout->addLayout(form);
    layout->addStretch();
    layout->addWidget(renameBox);
    const auto text = [this, form](const QString &key, const QString &label, Member member) {
        auto edit = new QLineEdit(this);
        edit->setObjectName(key);
        texts.insert(key, { edit, member });
        form->addRow(label, edit);
        connect(edit, &QLineEdit::textChanged, this, [this, key] {
            if (!loading)
                changed.insert(key);
        });
        return edit;
    };
    text("title", tr("Title"), &M::title);
    text("translatedTitle", tr("Translated title"), &M::translatedTitle);
    favorite = new FavoriteHeartButton(this);
    favorite->setObjectName("favorite");
    favorite->setAccessibleName(tr("Favorite"));
    auto *favoriteBox = new QWidget(this);
    auto *favoriteLayout = new QHBoxLayout(favoriteBox);
    favoriteLayout->setContentsMargins(0, 0, 0, 0);
    favoriteLayout->addWidget(favorite);
    favoriteLayout->addStretch();
    form->addRow(tr("Favorite"), favoriteBox);
    connect(favorite, &QAbstractButton::clicked, this, [this] {
        favorite->mixed = false;
        favorite->setToolTip(QString());
        favorite->update();
        if (!loading)
            changed.insert("favorite");
    });

    auto authorBox = new QWidget(this);
    auto authorLayout = new QVBoxLayout(authorBox);
    authorLayout->setContentsMargins(0, 0, 0, 0);
    anthology = new QCheckBox(tr("Anthology"), authorBox);
    anthology->setObjectName("isAnthology");
    singleAuthor = new QLineEdit(authorBox);
    singleAuthor->setObjectName("author");
    authors = new MetadataListEdit(authorBox);
    authors->setObjectName("authors");
    authorLayout->addWidget(anthology);
    authorLayout->addWidget(singleAuthor);
    authorLayout->addWidget(authors);
    form->addRow(tr("Author"), authorBox);
    connect(singleAuthor, &QLineEdit::textChanged, this, [this] {
        if (!loading)
            changed.insert("writer");
    });
    connect(authors, &MetadataListEdit::valuesEdited, this, [this] {
        if (!loading)
            changed.insert("writer");
    });
    connect(anthology, &QCheckBox::checkStateChanged, this, [this] {
        if (!loading && anthology->checkState() != Qt::PartiallyChecked) {
            if (anthology->isChecked() && authors->values().size() <= 1)
                authors->setValues({ singleAuthor->text() });
            if (!anthology->isChecked())
                singleAuthor->setText(authors->values().value(0));
            changed.insert("writer");
            changed.insert("isAnthology");
        }
        updateAuthorMode();
    });

    text("circle", tr("Circle"), &M::circle);
    auto work = text("work", tr("Work / IP"), &M::work);
    auto original = new QPushButton(tr("Original work"), this);
    original->setObjectName("original");
    form->addRow(QString(), original);
    characters = new MetadataListEdit(this);
    characters->setObjectName("characters");
    form->addRow(tr("Characters"), characters);
    tags = new MetadataListEdit(this);
    tags->setObjectName("tags");
    form->addRow(tr("Tags"), tags);
    connect(work, &QLineEdit::textChanged, this, &ComicMetadataEditor::syncOriginalFromWork);
    connect(original, &QPushButton::clicked, this, [this, work] {
        work->setText(originalName());
        syncOriginalFromWork();
    });
    connect(characters, &MetadataListEdit::valuesEdited, this, [this] {
        if (!loading) {
            changed.insert("characters");
            syncOriginalFromCharacters();
        }
    });
    connect(tags, &MetadataListEdit::valuesEdited, this, [this] {
        if (!loading)
            changed.insert("tags");
    });

    const auto choice = [this, form](const QString &key, const QString &label, Member member, const QList<QPair<QString, QVariant>> &options) {
        auto box = new QWidget(this);
        auto buttons = new QHBoxLayout(box);
        buttons->setContentsMargins(0, 0, 0, 0);
        auto group = new QButtonGroup(box);
        group->setObjectName(key);
        const auto addOption = [box, buttons, group](const QString &label, const QVariant &value) {
            auto button = new QPushButton(label, box);
            button->setCheckable(true);
            button->setProperty("value", value);
            group->addButton(button);
            buttons->addWidget(button);
        };
        addOption(tr("Not set"), QVariant());
        for (const auto &option : options)
            addOption(option.first, option.second);
        form->addRow(label, box);
        choices.insert(key, { group, member });
        connect(group, &QButtonGroup::buttonClicked, this, [this, key] {
            if (!loading)
                changed.insert(key);
        });
    };
    choice("color", tr("Color / black and white"), &M::color, { { tr("Color"), true }, { tr("Black and white"), false } });
    choice("censorship", tr("Censorship"), &M::censorship, { { tr("Censored"), QStringLiteral("censored") }, { tr("Uncensored"), QStringLiteral("uncensored") } });
    choice("publicationType", tr("Publication type"), &M::publicationType, { { tr("Doujinshi"), QStringLiteral("doujinshi") }, { tr("Collected volume"), QStringLiteral("tankobon") }, { tr("Magazine"), QStringLiteral("magazine") } });
    choice("translationStatus", tr("Translation"), &M::translationStatus, { { tr("Japanese"), QStringLiteral("original") }, { tr("Chinese translation"), QStringLiteral("translated") }, { tr("AI translation"), QStringLiteral("ai") } });
    text("translator", tr("Translator"), &M::translator);
    updateAuthorMode();
}

void ComicMetadataEditor::updateAuthorMode()
{
    const bool multiple = anthology->checkState() == Qt::Checked;
    singleAuthor->setVisible(!multiple);
    authors->setVisible(multiple);
}

void ComicMetadataEditor::setMetadata(const QList<DescriptiveComicMetadata> &records)
{
    if (records.isEmpty())
        return;
    QScopedValueRollback<bool> guard(loading, true);
    loadedMetadata = records.first();
    const auto common = [&records](Member member, bool &mixed) {
        const auto value = records.first().*member;
        mixed = std::any_of(records.begin(), records.end(), [&](const auto &record) { return record.*member != value; });
        return mixed ? QVariant() : value;
    };
    for (auto it = texts.begin(); it != texts.end(); ++it) {
        bool mixed;
        it->edit->setText(common(it->member, mixed).toString());
        it->edit->setPlaceholderText(mixed ? tr("Multiple values; edit to replace") : QString());
    }
    for (auto it = choices.begin(); it != choices.end(); ++it) {
        bool mixed;
        const auto value = common(it->member, mixed);
        it->edit->setExclusive(false);
        for (auto button : it->edit->buttons()) {
            const auto option = button->property("value");
            button->setChecked(!mixed && (value.isNull() ? option.isNull() : option == value));
            button->setToolTip(mixed ? tr("Multiple values; unchanged") : QString());
        }
        it->edit->setExclusive(true);
    }
    bool mixed;
    const bool firstFavorite = records.first().favorite.toBool();
    mixed = std::any_of(records.begin(), records.end(), [firstFavorite](const auto &record) { return record.favorite.toBool() != firstFavorite; });
    favorite->setChecked(!mixed && firstFavorite);
    favorite->mixed = mixed;
    favorite->setToolTip(mixed ? tr("Multiple values; unchanged") : QString());
    favorite->update();
    const auto writers = splitValues(common(&DescriptiveComicMetadata::writer, mixed));
    authors->setValues(writers, mixed);
    singleAuthor->setText(writers.value(0));
    singleAuthor->setPlaceholderText(mixed ? tr("Multiple values; edit to replace") : QString());
    const auto isAnthology = [](const auto &record) {
        return record.isAnthology.isNull() ? splitValues(record.writer).size() > 1 : record.isAnthology.toBool();
    };
    const bool firstAnthology = isAnthology(records.first());
    const bool mixedAnthology = std::any_of(records.begin(), records.end(), [&](const auto &record) { return isAnthology(record) != firstAnthology; });
    anthology->setTristate(mixedAnthology);
    anthology->setCheckState(mixedAnthology ? Qt::PartiallyChecked : firstAnthology ? Qt::Checked
                                                                                    : Qt::Unchecked);
    updateAuthorMode();
    const auto characterValues = common(&DescriptiveComicMetadata::characters, mixed);
    characters->setValues(splitValues(characterValues), mixed);
    const auto tagValues = common(&DescriptiveComicMetadata::tags, mixed);
    tags->setValues(splitValues(tagValues), mixed);
    originalWork = texts["work"].edit->text().trimmed() == originalName();
    changed.clear();
}

void ComicMetadataEditor::syncOriginalFromWork()
{
    if (loading || synchronizing)
        return;
    QScopedValueRollback<bool> guard(synchronizing, true);
    const bool original = texts["work"].edit->text().trimmed() == originalName();
    if (original) {
        characters->setValues({ originalName() });
        changed.insert("characters");
    } else if (originalWork && characters->values() == QStringList { originalName() }) {
        characters->setValues({ });
        changed.insert("characters");
    }
    originalWork = original;
}

void ComicMetadataEditor::syncOriginalFromCharacters()
{
    if (loading || synchronizing)
        return;
    QScopedValueRollback<bool> guard(synchronizing, true);
    if (characters->values().contains(originalName())) {
        texts["work"].edit->setText(originalName());
        characters->setValues({ originalName() });
        originalWork = true;
    } else if (originalWork) {
        texts["work"].edit->clear();
        originalWork = false;
    }
}

DescriptiveMetadataPatch ComicMetadataEditor::editedMetadata() const
{
    using M = DescriptiveComicMetadata;
    DescriptiveMetadataPatch patch;
    for (auto it = texts.cbegin(); it != texts.cend(); ++it) {
        if (changed.contains(it.key()))
            patch.set(it->member, textValue(it->edit->text()));
    }
    for (auto it = choices.cbegin(); it != choices.cend(); ++it) {
        if (changed.contains(it.key()) && it->edit->checkedButton())
            patch.set(it->member, it->edit->checkedButton()->property("value"));
    }
    if (changed.contains("writer") || changed.contains("isAnthology")) {
        const bool multiple = anthology->checkState() == Qt::Checked;
        patch.set(&M::writer, textValue(multiple ? authors->values().join('\n') : singleAuthor->text()));
        patch.set(&M::isAnthology, multiple);
    }
    if (changed.contains("characters"))
        patch.set(&M::characters, textValue(characters->values().join('\n')));
    if (changed.contains("tags"))
        patch.set(&M::tags, textValue(tags->values().join('\n')));
    if (changed.contains("favorite"))
        patch.set(&M::favorite, favorite->isChecked());
    return patch;
}

void ComicMetadataEditor::setFileName(const QString &name)
{
    filenameBox->setVisible(!name.isEmpty());
    renameBox->setVisible(!name.isEmpty());
    sourceFileName->setText(name);
    fileSuffix = name.isEmpty() ? QString() : "." + QFileInfo(name).suffix();
    newFileName->clear();
    newFileName->parentWidget()->hide();
    renameOnSave->setChecked(false);
    renameBox->findChild<QLabel *>("filenameNotes")->hide();
}

QString ComicMetadataEditor::pendingFileName() const
{
    return renameOnSave->isChecked() && !sourceFileName->text().isEmpty() ? newFileName->text() + fileSuffix : QString();
}
