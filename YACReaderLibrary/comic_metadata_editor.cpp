#include "comic_metadata_editor.h"

#include "comic_filename.h"
#include "draft_rating.h"
#include "favorite_heart.h"
#include "metadata_chip_colors.h"
#include "metadata_picker.h"
#include "metadata_value_chips.h"

#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QToolButton>
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

MetadataListEdit::MetadataListEdit(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *frame = new QFrame(this);
    frame->setObjectName("metadataTokenFrame");
    frame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setAutoFillBackground(true);
    frame->setBackgroundRole(QPalette::Base);
    auto *entry = new QVBoxLayout(frame);
    entry->setContentsMargins(0, 0, 0, 0);
    entry->setSpacing(0);
    tokens = new MetadataValueChips(-1, frame);
    tokens->setObjectName("selectedTokens");
    tokens->removable = true;
    tokens->clicked = [this](const QString &value) { toggleValue(value); };
    input = new QLineEdit(tokens);
    input->setObjectName("value");
    input->setFrame(false);
    input->installEventFilter(this);
    entry->addWidget(tokens);
    tokens->inlineEditor = input;
    layout->addWidget(frame);
    connect(input, &QLineEdit::textChanged, this, &MetadataListEdit::valuesEdited);
    connect(input, &QLineEdit::returnPressed, this, [this] {
        const auto value = input->text().trimmed();
        if (value.isEmpty())
            return;
        if (!selected.contains(value))
            selected.append(value);
        input->clear();
        refreshTokens();
        emit valuesEdited();
    });
    refreshTokens();
}
void MetadataListEdit::setColorColumn(int column)
{
    tokens->column = column;
    tokens->update();
}
void MetadataListEdit::refreshTokens()
{
    tokens->values = selected;
    tokens->show();
    tokens->refresh();
}
QStringList MetadataListEdit::values() const
{
    auto result = selected;
    const auto pending = input->text().trimmed();
    if (!pending.isEmpty() && !result.contains(pending))
        result.append(pending);
    return result;
}
void MetadataListEdit::setValues(const QStringList &values, bool mixed)
{
    QSignalBlocker blocker(input);
    selected = values;
    selected.removeDuplicates();
    input->clear();
    input->setPlaceholderText(mixed ? tr("Multiple values; edit to replace") : QString());
    refreshTokens();
}
bool MetadataListEdit::eventFilter(QObject *object, QEvent *event)
{
    if (object == input) {
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress)
            emit inputActivated();
        if (event->type() == QEvent::KeyPress && (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Return || static_cast<QKeyEvent *>(event)->key() == Qt::Key_Enter)) {
            emit input->returnPressed();
            return true; // Commit a token, not the dialog's default Save button.
        }
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Backspace && input->text().isEmpty() && !selected.isEmpty()) {
            selected.removeLast();
            refreshTokens();
            emit valuesEdited();
        }
    }
    return QWidget::eventFilter(object, event);
}
void MetadataListEdit::toggleValue(const QString &value)
{
    auto next = values();
    if (next.contains(value))
        next.removeAll(value);
    else
        next.append(value);
    setValues(next);
    emit valuesEdited();
}

ComicMetadataEditor::ComicMetadataEditor(QWidget *parent)
    : QWidget(parent)
{
    using M = DescriptiveComicMetadata;
    setObjectName("comicMetadataEditor");
    // Local, palette-driven controls override platform-specific native line edits
    // without changing the library's global theme or introducing fixed theme colors.
    setStyleSheet(QStringLiteral(R"(
        QWidget#comicMetadataEditor QLineEdit {
            background: palette(base); color: palette(text);
            border: 1px solid palette(midlight); border-radius: 5px;
            min-height: 18px; padding: 3px 7px;
        }
        QWidget#comicMetadataEditor QLineEdit:focus { border-color: palette(highlight); }
        QWidget#comicMetadataEditor QFrame#metadataPickerEntry,
        QWidget#comicMetadataEditor QFrame#metadataTokenFrame {
            background: palette(base); border: 1px solid palette(midlight); border-radius: 5px;
        }
        QWidget#comicMetadataEditor QFrame#metadataPickerEntry[focused="true"] { border-color: palette(highlight); }
        QWidget#comicMetadataEditor QFrame#metadataPickerEntry QLineEdit {
            border: none; background: transparent; padding: 1px 4px;
        }
        QWidget#comicMetadataEditor QFrame#metadataPickerEntry QFrame#metadataTokenFrame {
            border: none; background: transparent;
        }
        QWidget#comicMetadataEditor QPushButton {
            background: palette(button); color: palette(button-text);
            border: 1px solid palette(midlight); border-radius: 5px;
            padding: 3px 12px; min-height: 18px;
        }
        QWidget#comicMetadataEditor QPushButton:hover { border-color: palette(mid); }
        QWidget#comicMetadataEditor QPushButton:checked {
            background: palette(highlight); color: palette(highlighted-text); border-color: palette(highlight);
        }
        QWidget#comicMetadataEditor QPushButton:disabled { color: palette(mid); }
        QWidget#comicMetadataEditor QPushButton#parseFilename,
        QWidget#comicMetadataEditor QPushButton#generateFilename,
        QWidget#comicMetadataEditor QPushButton#renameFile {
            background: rgba(128, 128, 128, 26); border-color: rgba(128, 128, 128, 90);
        }
        QWidget#comicMetadataEditor QPushButton#parseFilename:hover,
        QWidget#comicMetadataEditor QPushButton#generateFilename:hover,
        QWidget#comicMetadataEditor QPushButton#renameFile:hover { background: rgba(128, 128, 128, 45); }
        QWidget#comicMetadataEditor QToolButton#metadataHelp {
            background: transparent; color: palette(window-text);
            border: 1px solid palette(mid); border-radius: 7px; padding: 0;
        }
        QWidget#comicMetadataEditor QCheckBox { spacing: 6px; }
        QWidget#comicMetadataEditor QCheckBox::indicator { width: 14px; height: 14px; }
        QWidget#comicMetadataEditor QFrame#metadataDivider {
            border: none; background: rgba(128, 128, 128, 110); min-height: 1px; max-height: 1px;
        }
    )"));
    const auto help = [](const QString &key, const QString &tip, QWidget *parent) {
        auto *button = new QToolButton(parent);
        button->setObjectName("metadataHelp");
        button->setProperty("helpKey", key);
        button->setText(QStringLiteral("?"));
        button->setFixedSize(15, 15);
        button->setAutoRaise(true);
        auto font = button->font();
        if (font.pointSizeF() > 0)
            font.setPointSizeF(qMax(7.0, font.pointSizeF() - 1));
        button->setFont(font);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        return button;
    };
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 12);
    layout->setSpacing(10);
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
    filenameLayout->addWidget(parse, 0, Qt::AlignLeft);
    renameBox = new QWidget(this);
    renameBox->hide();
    auto renameLayout = new QVBoxLayout(renameBox);
    renameLayout->setContentsMargins(0, 0, 0, 0);
    auto generate = new QPushButton(tr("Generate filename"), renameBox);
    generate->setObjectName("generateFilename");
    auto *generateHeading = new QHBoxLayout;
    generateHeading->setSpacing(6);
    generateHeading->addWidget(generate);
    generateHeading->addWidget(help("generateFilename", tr("Generate a formatted filename from the tags, then rename the file."), renameBox));
    generateHeading->addStretch();
    renameLayout->addLayout(generateHeading);
    renamePreview = new QWidget(renameBox);
    renamePreview->setObjectName("renamePreview");
    renamePreview->hide();
    auto *previewLayout = new QVBoxLayout(renamePreview);
    previewLayout->setContentsMargins(0, 0, 0, 0);
    auto *heading = new QHBoxLayout;
    heading->addWidget(new QLabel(tr("New filename"), renamePreview));
    heading->addStretch();
    auto *closePreview = new QToolButton(renamePreview);
    closePreview->setObjectName("closeFilenamePreview");
    closePreview->setText(QStringLiteral("×"));
    closePreview->setAutoRaise(true);
    closePreview->setFixedSize(22, 22);
    closePreview->setAccessibleName(tr("Close filename preview"));
    heading->addWidget(closePreview);
    previewLayout->addLayout(heading);
    auto *renameRow = new QHBoxLayout;
    newFileName = new QLineEdit(renamePreview);
    newFileName->setObjectName("newFilename");
    newFileName->setProperty("metadataPickerKey", QString());
    renameButton = new QPushButton(tr("Rename file"), renamePreview);
    renameButton->setObjectName("renameFile");
    renameRow->addWidget(newFileName, 1);
    renameRow->addWidget(renameButton);
    previewLayout->addLayout(renameRow);
    renameLayout->addWidget(renamePreview);
    connect(closePreview, &QToolButton::clicked, this, [this] {
        renamePreview->hide();
        newFileName->clear();
    });
    connect(renameButton, &QPushButton::clicked, this, [this] {
        if (!newFileName->text().trimmed().isEmpty())
            emit renameRequested(newFileName->text());
    });
    connect(newFileName, &QLineEdit::textChanged, this, [this] {
        renameButton->setEnabled(!newFileName->text().trimmed().isEmpty() && newFileName->text() != currentFilename);
    });
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
                button->setChecked(button->property("value") == values.*(it->member));
            changed.insert(it.key());
        }
    });
    connect(parse, &QPushButton::clicked, this, &ComicMetadataEditor::refreshPickers);
    connect(generate, &QPushButton::clicked, this, [this] {
        auto metadata = loadedMetadata;
        editedMetadata().applyTo(metadata);
        newFileName->setText(ComicFilename::generate(metadata) + fileSuffix);
        renamePreview->show();
        newFileName->setFocus();
    });
    auto form = new QVBoxLayout;
    form->setAlignment(Qt::AlignTop);
    form->setSpacing(8);
    layout->addLayout(form);
    layout->addStretch();
    layout->addWidget(renameBox);
    const auto text = [this, form, help](const QString &key, const QString &label, Member member) {
        auto edit = new QLineEdit(this);
        edit->setObjectName(key);
        texts.insert(key, { edit, member });
        edit->setProperty("metadataPickerKey", key);
        auto *field = key == "title" || key == "translatedTitle" ? static_cast<QWidget *>(edit) : wrapPicker(key, edit);
        auto *box = new QWidget(this);
        auto *column = new QVBoxLayout(box);
        column->setContentsMargins(0, 0, 0, 0);
        column->setSpacing(3);
        column->setAlignment(Qt::AlignTop);
        box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        auto *heading = new QHBoxLayout;
        heading->addWidget(new QLabel(label, box));
        const auto tip = key == "seriesName" ? tr("This work is part of a serialized comic series.")
                : key == "convention"        ? tr("Comic Market, Reitaisai, etc.")
                : key == "work"              ? tr("The original work that this fan comic is based on.")
                                             : QString();
        if (!tip.isEmpty()) {
            heading->addSpacing(2);
            heading->addWidget(help(key, tip, box));
        }
        if (key == "translatedTitle") {
            translatedEnabled = new QCheckBox(tr("Enabled"), box);
            translatedEnabled->setObjectName("showTranslatedTitle");
            translatedInput = field;
            heading->addSpacing(14);
            heading->addWidget(translatedEnabled);
            connect(translatedEnabled, &QCheckBox::checkStateChanged, this, [this] {
                translatedInput->setVisible(translatedEnabled->isChecked());
                if (!loading)
                    changed.insert("translatedTitleEnabled");
            });
        }
        if (key == "seriesName") {
            seriesEnabled = new QCheckBox(tr("Series work"), box);
            seriesEnabled->setObjectName("isSeries");
            heading->addSpacing(14);
            heading->addWidget(seriesEnabled);
            seriesInput = field;
            seriesEmpty = new QLabel(tr("Not in a series"), box);
            seriesEmpty->setForegroundRole(QPalette::PlaceholderText);
            connect(seriesEnabled, &QCheckBox::checkStateChanged, this, [this] {
                seriesInput->setVisible(seriesEnabled->isChecked());
                seriesEmpty->setVisible(!seriesEnabled->isChecked());
                if (!seriesEnabled->isChecked())
                    pickers["seriesName"]->setExpanded(false);
                if (!loading)
                    changed.insert("seriesName");
            });
        }
        heading->addStretch();
        column->addLayout(heading);
        column->addWidget(field);
        if (key == "seriesName")
            column->addWidget(seriesEmpty);
        form->addWidget(box);
        connect(edit, &QLineEdit::textChanged, this, [this, key] {
            if (!loading) {
                changed.insert(key);
                if (key == "translatedTitle" && !texts[key].edit->text().trimmed().isEmpty())
                    translatedEnabled->setChecked(true);
            }
            refreshPickers();
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
    rating = new DraftRating(favoriteBox);
    rating->setObjectName("rating");
    rating->changed = [this](int value) { if (!loading) pendingRating = value; };
    favoriteLayout->addWidget(new QLabel(tr("Rating"), favoriteBox));
    favoriteLayout->addWidget(rating);
    favoriteLayout->addSpacing(24);
    favoriteLayout->addWidget(new QLabel(tr("Favorite"), favoriteBox));
    favoriteLayout->addWidget(favorite);
    favoriteLayout->addStretch();
    form->addWidget(favoriteBox);
    connect(favorite, &QAbstractButton::clicked, this, [this] {
        favorite->mixed = false;
        favorite->setToolTip(QString());
        favorite->update();
        if (!loading)
            changed.insert("favorite");
    });

    const auto divider = [form, this] {
        auto *line = new QFrame(this);
        line->setObjectName("metadataDivider");
        line->setFixedHeight(1);
        form->addWidget(line);
    };
    text("seriesName", tr("Series"), &M::seriesName);
    divider();
    auto authorBox = new QWidget(this);
    auto authorLayout = new QVBoxLayout(authorBox);
    authorLayout->setContentsMargins(0, 0, 0, 0);
    authorLayout->setSpacing(3);
    authorLayout->setAlignment(Qt::AlignTop);
    authorBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    anthology = new QCheckBox(tr("Anthology"), authorBox);
    anthology->setObjectName("isAnthology");
    singleAuthor = new QLineEdit(authorBox);
    singleAuthor->setObjectName("author");
    singleAuthor->setProperty("metadataPickerKey", "writer");
    authors = new MetadataListEdit(authorBox);
    authors->setObjectName("authors");
    authors->setColorColumn(MetadataChipColors::columnForField("author"));
    auto *authorHeading = new QHBoxLayout;
    authorHeading->addWidget(new QLabel(tr("Author"), authorBox));
    authorHeading->addSpacing(14);
    authorHeading->addWidget(anthology);
    authorHeading->addWidget(help("isAnthology", tr("This work is a collaboration between multiple artists."), authorBox));
    authorHeading->addStretch();
    authorLayout->addLayout(authorHeading);
    auto authorInputs = new QWidget(authorBox);
    auto authorInputsLayout = new QVBoxLayout(authorInputs);
    authorInputsLayout->setContentsMargins(0, 0, 0, 0);
    authorInputsLayout->setAlignment(Qt::AlignTop);
    authorInputsLayout->addWidget(singleAuthor);
    authorInputsLayout->addWidget(authors);
    authorLayout->addWidget(wrapPicker("writer", authorInputs));
    connect(authors, &MetadataListEdit::inputActivated, this, [this] { openPicker("writer"); });
    form->addWidget(authorBox);
    connect(singleAuthor, &QLineEdit::textChanged, this, [this] {
        if (!loading)
            changed.insert("writer");
        refreshPickers();
    });
    connect(authors, &MetadataListEdit::valuesEdited, this, [this] {
        if (!loading)
            changed.insert("writer");
        refreshPickers();
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
        refreshPickers();
    });

    text("convention", tr("Convention"), &M::convention);
    text("circle", tr("Circle"), &M::circle);
    auto *circleRow = form->takeAt(form->count() - 1);
    auto *conventionRow = form->takeAt(form->count() - 1);
    auto *pair = new QWidget(this);
    auto *pairLayout = new QHBoxLayout(pair);
    pairLayout->setContentsMargins(0, 0, 0, 0);
    pairLayout->addWidget(circleRow->widget(), 1);
    pairLayout->addWidget(conventionRow->widget(), 1);
    delete circleRow;
    delete conventionRow;
    form->addWidget(pair);
    divider();
    const auto labeled = [form, this](const QString &label, QWidget *field) {
        auto *box = new QWidget(this);
        auto *layout = new QVBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        box->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        layout->setSpacing(3);
        layout->setAlignment(Qt::AlignTop);
        layout->addWidget(new QLabel(label, box));
        layout->addWidget(field);
        form->addWidget(box);
    };
    auto work = text("work", tr("Work / IP"), &M::work);
    text("translator", tr("Translator"), &M::translator);
    auto *translatorRow = form->takeAt(form->count() - 1);
    auto *workRow = form->takeAt(form->count() - 1);
    auto *workPair = new QWidget(this);
    auto *workPairLayout = new QHBoxLayout(workPair);
    workPairLayout->setContentsMargins(0, 0, 0, 0);
    workPairLayout->addWidget(workRow->widget(), 1, Qt::AlignTop);
    workPairLayout->addWidget(translatorRow->widget(), 1, Qt::AlignTop);
    delete workRow;
    delete translatorRow;
    form->addWidget(workPair);
    characters = new MetadataListEdit(this);
    characters->setObjectName("characters");
    characters->setColorColumn(MetadataChipColors::columnForField("character"));
    labeled(tr("Characters"), wrapPicker("characters", characters));
    connect(characters, &MetadataListEdit::inputActivated, this, [this] { openPicker("characters"); });
    tags = new MetadataListEdit(this);
    tags->setObjectName("tags");
    tags->setColorColumn(MetadataChipColors::columnForField("tag"));
    labeled(tr("Tags"), wrapPicker("tags", tags));
    connect(tags, &MetadataListEdit::inputActivated, this, [this] { openPicker("tags"); });
    connect(work, &QLineEdit::textChanged, this, &ComicMetadataEditor::syncOriginalFromWork);
    connect(characters, &MetadataListEdit::valuesEdited, this, [this] {
        if (!loading) {
            changed.insert("characters");
            syncOriginalFromCharacters();
        }
        refreshPickers();
    });
    connect(tags, &MetadataListEdit::valuesEdited, this, [this] {
        if (!loading)
            changed.insert("tags");
        refreshPickers();
    });

    divider();
    const auto choice = [this, form](const QString &key, const QString &label, Member member, const QList<QPair<QString, QVariant>> &options) {
        auto box = new QWidget(this);
        auto buttons = new QHBoxLayout(box);
        buttons->setContentsMargins(0, 0, 0, 0);
        auto group = new QButtonGroup(box);
        group->setObjectName(key);
        const auto addOption = [box, buttons, group](const QString &label, const QVariant &value) {
            auto button = new QPushButton(label, box);
            button->setCheckable(true);
            button->setMinimumWidth(68);
            button->setProperty("value", value);
            group->addButton(button);
            buttons->addWidget(button);
        };
        group->setExclusive(false);
        for (const auto &option : options)
            addOption(option.first, option.second);
        buttons->addStretch();
        auto *row = new QHBoxLayout;
        auto *caption = new QLabel(label, this);
        caption->setFixedWidth(72);
        caption->setWordWrap(true);
        row->addWidget(caption);
        row->addWidget(box, 1);
        form->addLayout(row);
        choices.insert(key, { group, member });
        connect(group, &QButtonGroup::buttonClicked, this, [this, key, group](QAbstractButton *clicked) {
            for (auto *button : group->buttons())
                if (button != clicked)
                    button->setChecked(false);
            if (!loading)
                changed.insert(key);
        });
    };
    choice("color", tr("Color / black and white"), &M::color, { { tr("Color"), true }, { tr("Black and white"), false } });
    choice("censorship", tr("Censorship"), &M::censorship, { { tr("Censored"), QStringLiteral("censored") }, { tr("Uncensored"), QStringLiteral("uncensored") } });
    choice("publicationType", tr("Publication type"), &M::publicationType, { { tr("Doujinshi"), QStringLiteral("doujinshi") }, { tr("Collected volume"), QStringLiteral("tankobon") }, { tr("Magazine"), QStringLiteral("magazine") }, { tr("Artbook"), QStringLiteral("artbook") } });
    choice("translationStatus", tr("Translation"), &M::translationStatus, { { tr("Japanese"), QStringLiteral("original") }, { tr("Chinese translation"), QStringLiteral("translated") }, { tr("AI translation"), QStringLiteral("ai") } });
    updateAuthorMode();
    // Scope application-level mouse observation to this editor's window, so
    // blank scroll viewports and margins outside the editor are covered too.
    qApp->installEventFilter(this);
}

ComicMetadataEditor::~ComicMetadataEditor()
{
    if (qApp)
        qApp->removeEventFilter(this);
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
    vocabularyLoaded = false;
    vocabulary.clear();
    for (auto *picker : pickers)
        picker->setExpanded(false);
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
        it->edit->setExclusive(false);
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
    seriesEnabled->setTristate(false);
    const auto firstSeries = records.first().seriesName.toString();
    const bool mixedSeries = std::any_of(records.begin(), records.end(), [&](const auto &record) { return record.seriesName.toString() != firstSeries; });
    seriesEnabled->setTristate(mixedSeries);
    seriesEnabled->setCheckState(mixedSeries ? Qt::PartiallyChecked : firstSeries.isEmpty() ? Qt::Unchecked
                                                                                            : Qt::Checked);
    seriesInput->setVisible(seriesEnabled->checkState() == Qt::Checked);
    seriesEmpty->setVisible(seriesEnabled->checkState() != Qt::Checked);
    bool mixedTranslated;
    const auto translated = common(&DescriptiveComicMetadata::translatedTitle, mixedTranslated).toString();
    translatedEnabled->setTristate(mixedTranslated);
    translatedEnabled->setCheckState(mixedTranslated ? Qt::PartiallyChecked : translated.isEmpty() ? Qt::Unchecked
                                                                                                   : Qt::Checked);
    translatedInput->setVisible(translatedEnabled->checkState() == Qt::Checked);
    pendingRating.reset();
    changed.clear();
    refreshPickers();
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
    refreshPickers();
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
    refreshPickers();
}

void ComicMetadataEditor::setVocabularyProvider(std::function<Vocabulary()> provider)
{
    vocabularyProvider = std::move(provider);
    vocabularyLoaded = false;
    vocabulary.clear();
    for (auto *picker : pickers)
        picker->setExpanded(false);
}

QWidget *ComicMetadataEditor::wrapPicker(const QString &key, QWidget *input)
{
    const auto field = key == "writer" ? QStringLiteral("author") : key == "characters" ? QStringLiteral("character")
            : key == "tags"                                                             ? QStringLiteral("tag")
                                                                                        : key;
    auto *picker = new MetadataPicker(input, MetadataChipColors::columnForField(field), this);
    picker->setObjectName("picker_" + key);
    pickers.insert(key, picker);
    connect(picker, &MetadataPicker::toggleRequested, this, [this, key, picker] {
        if (picker->isExpanded())
            picker->setExpanded(false);
        else
            openPicker(key);
    });
    connect(picker, &MetadataPicker::valueClicked, this, [this, key](const QString &value) { chooseValue(key, value); });
    return picker;
}

QStringList ComicMetadataEditor::pickerSelection(const QString &key) const
{
    if (key == "tags")
        return tags->values();
    if (key == "characters")
        return characters->values();
    if (key == "writer")
        return anthology->checkState() == Qt::Checked ? authors->values() : QStringList { singleAuthor->text().trimmed() };
    return { texts.value(key).edit->text().trimmed() };
}

void ComicMetadataEditor::openPicker(const QString &key)
{
    if (loading)
        return;
    for (auto it = pickers.begin(); it != pickers.end(); ++it)
        if (it.key() != key)
            it.value()->setExpanded(false);
    auto *picker = pickers.value(key);
    if (!picker || picker->isExpanded())
        return;
    if (!vocabularyLoaded) {
        vocabulary = vocabularyProvider ? vocabularyProvider() : Vocabulary();
        vocabularyLoaded = true;
    }
    picker->setValues(vocabulary.value(key) + pickerSelection(key),
                      key == "work" || key == "characters" ? originalName() : QString());
    refreshPickers();
    picker->setExpanded(true);
}

void ComicMetadataEditor::refreshPickers()
{
    if (!singleAuthor || !characters || !tags)
        return;
    for (auto it = pickers.begin(); it != pickers.end(); ++it)
        it.value()->setSelected(pickerSelection(it.key()));
}

void ComicMetadataEditor::chooseValue(const QString &key, const QString &value)
{
    if (key == "tags")
        tags->toggleValue(value);
    else if (key == "characters") {
        if (value != originalName() && characters->values().contains(originalName())) {
            characters->setValues({ value });
            emit characters->valuesEdited();
        } else
            characters->toggleValue(value);
    } else if (key == "writer") {
        if (anthology->checkState() == Qt::Checked)
            authors->toggleValue(value);
        else
            singleAuthor->setText(singleAuthor->text().trimmed() == value ? QString() : value);
    } else if (auto *edit = texts.value(key).edit)
        edit->setText(edit->text().trimmed() == value ? QString() : value);
    refreshPickers();
}

bool ComicMetadataEditor::eventFilter(QObject *object, QEvent *event)
{
    auto *widget = qobject_cast<QWidget *>(object);
    if (event->type() == QEvent::MouseButtonPress && isVisible() && widget && widget->window() == window()) {
        const bool anyOpen = std::any_of(pickers.cbegin(), pickers.cend(), [](auto *picker) { return picker->isExpanded(); });
        if (anyOpen) {
            const auto global = static_cast<QMouseEvent *>(event)->globalPosition().toPoint();
            // Inspect the actual child under the click, including when Qt
            // propagates an ignored mouse press to a viewport or parent.
            auto *hit = widget->childAt(widget->mapFromGlobal(global));
            if (!hit)
                hit = widget;
            bool interactive = false;
            for (auto *ancestor = hit; ancestor; ancestor = ancestor->parentWidget()) {
                if (qobject_cast<QAbstractButton *>(ancestor) || qobject_cast<QLineEdit *>(ancestor) || qobject_cast<QAbstractSlider *>(ancestor) || qobject_cast<QAbstractSpinBox *>(ancestor) || qobject_cast<QComboBox *>(ancestor) || qobject_cast<QAbstractItemView *>(ancestor)) {
                    interactive = true;
                    break;
                }
                if (auto *chips = dynamic_cast<MetadataValueChips *>(ancestor)) {
                    interactive = chips->hasValueAt(chips->mapFromGlobal(global));
                    break;
                }
                if (ancestor == window())
                    break;
            }
            if (!interactive)
                for (auto *picker : pickers)
                    picker->setExpanded(false);
        }
    }
    if (widget && isAncestorOf(widget) && qobject_cast<QLineEdit *>(widget) && widget->dynamicPropertyNames().contains("metadataPickerKey") && (event->type() == QEvent::FocusIn || event->type() == QEvent::MouseButtonPress))
        openPicker(object->property("metadataPickerKey").toString());
    return QWidget::eventFilter(object, event);
}

DescriptiveMetadataPatch ComicMetadataEditor::editedMetadata() const
{
    using M = DescriptiveComicMetadata;
    DescriptiveMetadataPatch patch;
    for (auto it = texts.cbegin(); it != texts.cend(); ++it) {
        if (changed.contains(it.key()))
            patch.set(it->member, it.key() == "seriesName" && !seriesEnabled->isChecked() ? QVariant() : textValue(it->edit->text()));
    }
    for (auto it = choices.cbegin(); it != choices.cend(); ++it) {
        if (changed.contains(it.key()))
            patch.set(it->member, it->edit->checkedButton() ? it->edit->checkedButton()->property("value") : QVariant());
    }
    if (changed.contains("translatedTitleEnabled") && !translatedEnabled->isChecked())
        patch.set(&M::translatedTitle, QVariant());
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

void ComicMetadataEditor::setRatings(const QList<std::optional<int>> &values)
{
    pendingRating.reset();
    const bool enabled = !values.isEmpty() && std::all_of(values.begin(), values.end(), [](auto value) { return value.has_value(); });
    rating->setEnabled(enabled);
    const int first = values.isEmpty() ? 0 : values.first().value_or(0);
    rating->setValue(first, std::any_of(values.begin(), values.end(), [first](auto value) { return value.value_or(0) != first; }));
}
std::optional<int> ComicMetadataEditor::editedRating() const
{
    return pendingRating;
}
void ComicMetadataEditor::setFileName(const QString &name)
{
    currentFilename = name;
    filenameBox->setVisible(!name.isEmpty());
    renameBox->setVisible(!name.isEmpty());
    sourceFileName->setText(name);
    fileSuffix = name.isEmpty() ? QString() : "." + QFileInfo(name).suffix();
    newFileName->clear();
    renamePreview->hide();
}
void ComicMetadataEditor::filenameRenamed(const QString &name)
{
    // Do not reset the unsaved metadata or rating draft after an independent rename.
    setFileName(name);
}
