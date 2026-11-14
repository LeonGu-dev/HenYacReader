#include "comic_filename.h"
#include "comic_metadata_editor.h"
#include "metadata_picker.h"
#include "metadata_value_chips.h"
#include "staged_comic_rename.h"
#include "yacreader_cover_label.h"
#include "yacreader_global.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include <QTranslator>
#include <QVBoxLayout>

class ComicMetadataEditorTest : public QObject
{
    Q_OBJECT
private slots:
    void conventionsParseGenerateAndEditIndependently();
    void seriesAndRatingRemainDrafts();
    void translatedTitleTogglePreservesDraftUntilSave();
    void manyCharactersKeepTheInlineEditorInsideTheFrame();
    void openingAndSavingDoesNotRewriteMetadata();
    void savesSingleAuthorOnlyAfterExplicitChange();
    void restoresAnthologyAuthorsAfterAccidentalToggle();
    void originalsSynchronizeInBothDirections();
    void selectingAnIpAfterOriginalClearsTheOriginalCharacter();
    void batchTitleEditPreservesDifferentAuthorsAndTags();
    void statusesCanBeCleared();
    void storesMultipleCharactersAndTags();
    void rendersSinglePage();
    void acceptanceProfileIsolatesSettingsAndReaderConnection();
    void coverAspectRatioDoesNotChangePanelGeometry();
    void parsesStandardAndReorderedFilenames();
    void parsesIndependentAuthorsAndKeepsMissingFields();
    void generatesCanonicalNamesAndValidatesWindowsNames();
    void filenamePreviewRequiresExplicitConfirmation();
    void renamePreservesBytesRejectsCollisionsAndRollsBack();
    void favoriteHeartPreservesMixedValuesUntilEdited();
    void pickerStaysOpenSwitchesAndToggles();
    void pickerHandlesAuthorsAndOriginals();
    void pickerReloadsVocabularyForTheNextRecord();
    void manyPickerValuesScrollAndRender_data();
    void manyPickerValuesScrollAndRender();
    void blankClicksDismissWithoutChangingValues();
    void pickerControlsAndScrollDoNotDismiss();
};

namespace {
DescriptiveComicMetadata sample()
{
    DescriptiveComicMetadata metadata;
    metadata.title = QStringLiteral("漫画标题");
    metadata.translatedTitle = QStringLiteral("翻译标题");
    metadata.convention = "C107";
    metadata.writer = QStringLiteral("作者甲\n作者乙");
    metadata.isAnthology = true;
    metadata.circle = QStringLiteral("示例社团");
    metadata.work = QStringLiteral("示例作品");
    metadata.characters = QStringLiteral("角色甲\n角色乙");
    metadata.tags = QStringLiteral("日常\n冒险");
    metadata.color = true;
    metadata.censorship = "censored";
    metadata.publicationType = "doujinshi";
    metadata.translationStatus = "translated";
    metadata.translator = QStringLiteral("示例汉化组");
    metadata.synopsis = "Legacy synopsis";
    return metadata;
}
}

void ComicMetadataEditorTest::openingAndSavingDoesNotRewriteMetadata()
{
    ComicMetadataEditor editor;
    editor.setMetadata({ sample() });
    QVERIFY(editor.editedMetadata().isEmpty());
    QVERIFY(editor.findChildren<QTabWidget *>().isEmpty());
}

void ComicMetadataEditorTest::savesSingleAuthorOnlyAfterExplicitChange()
{
    ComicMetadataEditor editor;
    const auto original = sample();
    editor.setMetadata({ original });
    editor.findChild<QCheckBox *>("isAnthology")->setChecked(false);
    editor.findChild<QLineEdit *>("author")->setText("Replacement author");
    QCOMPARE(original.writer.toString(), QStringLiteral("作者甲\n作者乙"));
    auto saved = original;
    editor.editedMetadata().applyTo(saved);
    QCOMPARE(saved.writer.toString(), QString("Replacement author"));
    QVERIFY(!saved.isAnthology.toBool());
    QCOMPARE(saved.synopsis, original.synopsis);
}

void ComicMetadataEditorTest::restoresAnthologyAuthorsAfterAccidentalToggle()
{
    ComicMetadataEditor editor;
    editor.setMetadata({ sample() });
    auto anthology = editor.findChild<QCheckBox *>("isAnthology");
    anthology->setChecked(false);
    anthology->setChecked(true);
    auto saved = sample();
    editor.editedMetadata().applyTo(saved);
    QCOMPARE(saved.writer, sample().writer);
    QVERIFY(saved.isAnthology.toBool());
}

void ComicMetadataEditorTest::originalsSynchronizeInBothDirections()
{
    ComicMetadataEditor editor;
    editor.setMetadata({ sample() });
    emit editor.findChild<MetadataPicker *>("picker_work")->valueClicked(QStringLiteral("原创"));
    auto saved = sample();
    editor.editedMetadata().applyTo(saved);
    QCOMPARE(saved.work.toString(), QStringLiteral("原创"));
    QCOMPARE(saved.characters.toString(), QStringLiteral("原创"));

    editor.setMetadata({ sample() });
    editor.findChild<MetadataListEdit *>("characters")->findChild<QLineEdit *>("value")->setText(QStringLiteral("原创"));
    saved = sample();
    editor.editedMetadata().applyTo(saved);
    QCOMPARE(saved.work.toString(), QStringLiteral("原创"));
    QCOMPARE(saved.characters.toString(), QStringLiteral("原创"));
}

void ComicMetadataEditorTest::selectingAnIpAfterOriginalClearsTheOriginalCharacter()
{
    ComicMetadataEditor editor;
    editor.setMetadata({ sample() });
    emit editor.findChild<MetadataPicker *>("picker_work")->valueClicked(QStringLiteral("原创"));
    editor.findChild<QLineEdit *>("work")->setText("Another IP");
    auto saved = sample();
    editor.editedMetadata().applyTo(saved);
    QCOMPARE(saved.work.toString(), QString("Another IP"));
    QVERIFY(saved.characters.isNull());
}

void ComicMetadataEditorTest::batchTitleEditPreservesDifferentAuthorsAndTags()
{
    auto first = sample();
    auto second = sample();
    second.writer = "Another author";
    second.isAnthology = false;
    second.tags = "Another tag";
    ComicMetadataEditor editor;
    editor.setMetadata({ first, second });
    QVERIFY(editor.editedMetadata().isEmpty());
    editor.findChild<QLineEdit *>("title")->setText("Shared title");
    const auto patch = editor.editedMetadata();
    QVERIFY(!patch.contains(&DescriptiveComicMetadata::writer));
    QVERIFY(!patch.contains(&DescriptiveComicMetadata::tags));
    patch.applyTo(first);
    patch.applyTo(second);
    QCOMPARE(first.title, second.title);
    QCOMPARE(first.writer, sample().writer);
    QCOMPARE(second.writer.toString(), QString("Another author"));
}

void ComicMetadataEditorTest::statusesCanBeCleared()
{
    ComicMetadataEditor editor;
    editor.setMetadata({ sample() });
    for (const auto &key : { "color", "censorship", "translationStatus", "publicationType" })
        editor.findChild<QButtonGroup *>(key)->checkedButton()->click();
    auto saved = sample();
    editor.editedMetadata().applyTo(saved);
    QVERIFY(saved.color.isNull());
    QVERIFY(saved.censorship.isNull());
    QVERIFY(saved.translationStatus.isNull());
    QVERIFY(saved.publicationType.isNull());
}

void ComicMetadataEditorTest::storesMultipleCharactersAndTags()
{
    ComicMetadataEditor editor;
    editor.setMetadata({ sample() });
    for (const auto &key : { "characters", "tags" }) {
        auto list = editor.findChild<MetadataListEdit *>(key);
        list->findChildren<QLineEdit *>("value").last()->setText("Third value");
    }
    auto saved = sample();
    editor.editedMetadata().applyTo(saved);
    QCOMPARE(saved.getCharacters().size(), 3);
    QCOMPARE(saved.getTags().size(), 3);
}

void ComicMetadataEditorTest::rendersSinglePage()
{
    QTranslator translator;
    if (translator.load(qEnvironmentVariable("COMIC_EDITOR_TRANSLATION")))
        qApp->installTranslator(&translator);
    const auto previousPalette = qApp->palette();
    const auto restorePalette = qScopeGuard([&] { qApp->setPalette(previousPalette); });
    if (qEnvironmentVariable("COMIC_EDITOR_PREVIEW_MODE").contains("dark")) {
        auto palette = previousPalette;
        palette.setColor(QPalette::Window, QColor("#292929"));
        palette.setColor(QPalette::WindowText, QColor("#ededed"));
        palette.setColor(QPalette::Base, QColor("#353535"));
        palette.setColor(QPalette::Text, QColor("#ededed"));
        palette.setColor(QPalette::Button, QColor("#373737"));
        palette.setColor(QPalette::ButtonText, QColor("#ededed"));
        palette.setColor(QPalette::Midlight, QColor("#4b4b4b"));
        palette.setColor(QPalette::Mid, QColor("#777777"));
        palette.setColor(QPalette::PlaceholderText, QColor("#aaaaaa"));
        palette.setColor(QPalette::Highlight, QColor("#278ad0"));
        palette.setColor(QPalette::HighlightedText, QColor("#ffffff"));
        qApp->setPalette(palette);
    }
    QScrollArea window;
    auto editor = new ComicMetadataEditor;
    auto metadata = sample();
    metadata.seriesName = QStringLiteral("示例系列");
    if (qEnvironmentVariable("COMIC_EDITOR_PREVIEW_MODE").contains("empty"))
        metadata = DescriptiveComicMetadata();
    editor->setMetadata({ metadata });
    editor->setRatings({ 4 });
    editor->setFileName(QStringLiteral("[示例作者] 示例作品.zip"));
    window.setWidget(editor);
    window.setWidgetResizable(true);
    window.resize(730, 850);
    window.show();
    QCoreApplication::processEvents();
    QVERIFY(editor->findChildren<QTabWidget *>().isEmpty());
    const auto preview = qEnvironmentVariable("COMIC_EDITOR_PREVIEW_PATH");
    if (!preview.isEmpty())
        QVERIFY(window.grab().save(preview));
    qApp->removeTranslator(&translator);
}

void ComicMetadataEditorTest::acceptanceProfileIsolatesSettingsAndReaderConnection()
{
    const auto previous = qgetenv("YACREADER_DATA_DIR");
    const auto restore = qScopeGuard([&] {
        if (previous.isNull())
            qunsetenv("YACREADER_DATA_DIR");
        else
            qputenv("YACREADER_DATA_DIR", previous);
    });
    qunsetenv("YACREADER_DATA_DIR");
    const auto normalServer = YACReader::libraryLocalServerName();
    const auto first = QDir::temp().absoluteFilePath("comic-editor-profile-a");
    qputenv("YACREADER_DATA_DIR", first.toUtf8());
    QCOMPARE(YACReader::getCommonSettingsPath(), first);
    QCOMPARE(YACReader::getSettingsPath(), QDir(first).filePath(qApp->applicationName()));
    const auto firstServer = YACReader::libraryLocalServerName();
    QVERIFY(firstServer != normalServer);
    qputenv("YACREADER_DATA_DIR", QDir::temp().absoluteFilePath("comic-editor-profile-b").toUtf8());
    QVERIFY(firstServer != YACReader::libraryLocalServerName());
}

void ComicMetadataEditorTest::coverAspectRatioDoesNotChangePanelGeometry()
{
    QWidget window;
    auto layout = new QHBoxLayout(&window);
    auto cover = new YACReader::CoverLabel;
    cover->setMaximumWidth(480);
    auto editor = new ComicMetadataEditor;
    editor->setMinimumWidth(520);
    editor->setMetadata({ sample() });
    layout->addWidget(cover, 2);
    layout->addWidget(editor, 3);
    window.resize(1040, 720);
    window.show();
    QCoreApplication::processEvents();
    const auto windowSize = window.size();
    const auto editorGeometry = editor->geometry();
    for (const auto &size : { QSize(600, 900), QSize(2400, 900), QSize(600, 900) }) {
        QPixmap image(size);
        image.fill(Qt::blue);
        cover->setPixmap(image);
        QCoreApplication::processEvents();
        QCOMPARE(window.size(), windowSize);
        QCOMPARE(editor->geometry(), editorGeometry);
        QVERIFY(cover->width() <= 480);
        QVERIFY(editor->width() >= 520);
        const auto rendered = cover->grab().toImage();
        const auto expected = size.scaled(cover->size(), Qt::KeepAspectRatio);
        QCOMPARE(rendered.pixelColor(rendered.width() / 2, rendered.height() / 2), QColor(Qt::blue));
        if (expected.height() < rendered.height() - 2)
            QVERIFY(rendered.pixelColor(rendered.width() / 2, 0) != QColor(Qt::blue));
        if (expected.width() < rendered.width() - 2)
            QVERIFY(rendered.pixelColor(0, rendered.height() / 2) != QColor(Qt::blue));
    }
}

void ComicMetadataEditorTest::parsesStandardAndReorderedFilenames()
{
    using M = DescriptiveComicMetadata;
    M metadata;
    const auto standard = QStringLiteral("[40010壱号 (40010試作型)] ハメたらもっとかわいいよ♡ [中国翻訳] [無修正] [Sky110036个人汉化].zip");
    ComicFilename::parse(standard).metadata.applyTo(metadata);
    QCOMPARE(metadata.circle.toString(), QStringLiteral("40010壱号"));
    QCOMPARE(metadata.writer.toString(), QStringLiteral("40010試作型"));
    QCOMPARE(metadata.title.toString(), QStringLiteral("ハメたらもっとかわいいよ♡"));
    QCOMPARE(metadata.translationStatus.toString(), QString("translated"));
    QCOMPARE(metadata.censorship.toString(), QString("uncensored"));
    QCOMPARE(metadata.translator.toString(), QStringLiteral("Sky110036个人汉化"));
    for (const auto &name : { QStringLiteral("(社团[作者甲、作者乙]) 标题 [無修正] (作品) (中国翻訳) [某汉化组].cbz"),
                              QStringLiteral("［社团（作者甲、作者乙）］ 标题 （作品） ［某汉化组］ ［中国翻訳］ ［無修正］.7z") }) {
        M value;
        ComicFilename::parse(name).metadata.applyTo(value);
        QCOMPARE(value.circle.toString(), QStringLiteral("社团"));
        QCOMPARE(value.writer.toString(), QStringLiteral("作者甲\n作者乙"));
        QVERIFY(value.isAnthology.toBool());
        QCOMPARE(value.work.toString(), QStringLiteral("作品"));
        QCOMPARE(value.title.toString(), QStringLiteral("标题"));
        QCOMPARE(value.translator.toString(), QStringLiteral("某汉化组"));
    }
}

void ComicMetadataEditorTest::parsesIndependentAuthorsAndKeepsMissingFields()
{
    auto metadata = sample();
    const auto parsed = ComicFilename::parse(QStringLiteral("[独立作者] 标题 (某作品) [AI翻译] [组名].rar"));
    parsed.metadata.applyTo(metadata);
    QCOMPARE(metadata.writer.toString(), QStringLiteral("独立作者"));
    QVERIFY(metadata.circle.isNull());
    QVERIFY(!metadata.isAnthology.toBool());
    QCOMPARE(metadata.translationStatus.toString(), QString("ai"));
    QCOMPARE(metadata.translator.toString(), QStringLiteral("组名"));
    QCOMPARE(metadata.characters, sample().characters);
    QCOMPARE(metadata.censorship, sample().censorship);
    QCOMPARE(metadata.translatedTitle, sample().translatedTitle);
    DescriptiveComicMetadata mixed;
    ComicFilename::parse(QStringLiteral("[無修正] (独立作者) 标题 [某作品] [某汉化组] (中国翻訳).zip")).metadata.applyTo(mixed);
    QCOMPARE(mixed.writer.toString(), QStringLiteral("独立作者"));
    QCOMPARE(mixed.work.toString(), QStringLiteral("某作品"));
    QCOMPARE(mixed.translationStatus.toString(), QString("translated"));
    auto minimal = ComicFilename::parse(QStringLiteral("普通标题.zip"));
    QVERIFY(!minimal.metadata.contains(&DescriptiveComicMetadata::writer));
    QVERIFY(!minimal.metadata.contains(&DescriptiveComicMetadata::censorship));
    QVERIFY(!ComicFilename::parse(QStringLiteral("标题 [不完整.zip")).notes.isEmpty());
}

void ComicMetadataEditorTest::generatesCanonicalNamesAndValidatesWindowsNames()
{
    auto metadata = sample();
    metadata.writer = QStringLiteral("作者");
    metadata.censorship = "uncensored";
    const auto generated = ComicFilename::generate(metadata);
    QCOMPARE(generated, QStringLiteral("(C107) [示例社团 (作者)] 漫画标题 (示例作品) [中国翻訳] [無修正] [示例汉化组]"));
    DescriptiveComicMetadata reparsed;
    ComicFilename::parse(generated + ".zip").metadata.applyTo(reparsed);
    QCOMPARE(reparsed.title, metadata.title);
    QCOMPARE(reparsed.convention, metadata.convention);
    QCOMPARE(reparsed.writer, metadata.writer);
    QVERIFY(reparsed.tags.isNull());
    QCOMPARE(reparsed.work, metadata.work);
    metadata.title = QStringLiteral("标题: A/B?");
    QVERIFY(ComicFilename::invalidNameReason(ComicFilename::generate(metadata) + ".zip").isEmpty());
    for (const auto &name : { "../a.zip", "a/b.zip", "a\\b.zip", "CON.zip", "bad?.zip", "a.zip ", "a.zip.", ".zip" })
        QVERIFY2(!ComicFilename::invalidNameReason(name).isEmpty(), name);
}

void ComicMetadataEditorTest::filenamePreviewRequiresExplicitConfirmation()
{
    QTranslator translator;
    if (translator.load(qEnvironmentVariable("COMIC_EDITOR_TRANSLATION")))
        qApp->installTranslator(&translator);
    ComicMetadataEditor editor;
    editor.setMetadata({ sample() });
    editor.setFileName(QStringLiteral("[独立作者] 解析标题 [中国翻訳] [无修正] [某汉化组].ZIP"));
    editor.findChild<QPushButton *>("parseFilename")->click();
    auto metadata = sample();
    editor.editedMetadata().applyTo(metadata);
    QCOMPARE(metadata.title.toString(), QStringLiteral("解析标题"));
    QCOMPARE(metadata.writer.toString(), QStringLiteral("独立作者"));
    QVERIFY(metadata.circle.isNull());
    QCOMPARE(editor.findChild<QButtonGroup *>("censorship")->checkedButton()->property("value").toString(), QString("uncensored"));
    editor.findChild<QPushButton *>("generateFilename")->click();
    QVERIFY(!editor.findChild<QWidget *>("renamePreview")->isHidden());
    editor.findChild<QLineEdit *>("newFilename")->setText(QStringLiteral("手动修改"));
    QSignalSpy rename(&editor, &ComicMetadataEditor::renameRequested);
    editor.findChild<QLineEdit *>("newFilename")->setText(QStringLiteral("手动修改.ZIP"));
    editor.findChild<QPushButton *>("renameFile")->click();
    QCOMPARE(rename.count(), 1);
    QCOMPARE(rename.first().first().toString(), QStringLiteral("手动修改.ZIP"));
    editor.findChild<QToolButton *>("closeFilenamePreview")->click();
    QVERIFY(editor.findChild<QWidget *>("renamePreview")->isHidden());
    const auto preview = qEnvironmentVariable("COMIC_FILENAME_PREVIEW_PATH");
    if (!preview.isEmpty()) {
        editor.resize(640, 900);
        editor.show();
        QCoreApplication::processEvents();
        QVERIFY(editor.grab().save(preview));
    }
    editor.setFileName("next.cbz");
    QVERIFY(editor.findChild<QWidget *>("renamePreview")->isHidden());
}

void ComicMetadataEditorTest::renamePreservesBytesRejectsCollisionsAndRollsBack()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto source = directory.filePath("old.zip");
    const auto target = directory.filePath(QStringLiteral("新名字.zip"));
    const QByteArray bytes("archive bytes unchanged");
    {
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
    }
    QString error;
    {
        StagedComicRename rename(source, target);
        QVERIFY2(rename.install(error), qPrintable(error));
        QVERIFY(!QFile::exists(source));
        QFile file(target);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), bytes);
    }
    QVERIFY(QFile::exists(source));
    QVERIFY(!QFile::exists(target));
    {
        QFile file(target);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("do not overwrite");
    }
    {
        StagedComicRename collision(source, target);
        QVERIFY(!collision.install(error));
        QVERIFY(QFile::exists(source));
        QFile file(target);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("do not overwrite"));
    }
    QVERIFY(QFile::remove(target));
    {
        StagedComicRename rename(source, target);
        QVERIFY(rename.install(error));
        rename.finish();
    }
    QVERIFY(QFile::exists(target));
    QVERIFY(!QFile::exists(source));
    {
        StagedComicRename rename(target, directory.filePath(QStringLiteral("新名字.ZIP")));
        QVERIFY2(rename.install(error), qPrintable(error));
        QVERIFY(rename.restore(error));
    }
}

void ComicMetadataEditorTest::favoriteHeartPreservesMixedValuesUntilEdited()
{
    auto first = sample();
    auto second = first;
    first.favorite = true;
    second.favorite = false;
    ComicMetadataEditor editor;
    editor.setMetadata({ first, second });
    QVERIFY(editor.editedMetadata().isEmpty());
    auto *heart = editor.findChild<QAbstractButton *>("favorite");
    QVERIFY(heart);
    QVERIFY(!heart->isChecked());
    heart->click();
    auto patch = editor.editedMetadata();
    QVERIFY(patch.contains(&DescriptiveComicMetadata::favorite));
    patch.applyTo(first);
    patch.applyTo(second);
    QVERIFY(first.favorite.toBool());
    QVERIFY(second.favorite.toBool());
    editor.setMetadata({ first });
    QVERIFY(heart->isChecked());
    heart->click();
    editor.editedMetadata().applyTo(first);
    QVERIFY(!first.favorite.toBool());
    QCOMPARE(first.title, second.title);
    second.favorite = QVariant();
    editor.setMetadata({ first, second });
    QVERIFY(heart->toolTip().isEmpty());
    QVERIFY(editor.editedMetadata().isEmpty());
}

void ComicMetadataEditorTest::conventionsParseGenerateAndEditIndependently()
{
    using M = DescriptiveComicMetadata;
    for (const auto &event : { QStringLiteral("c107"), QStringLiteral("例大祭") }) {
        M metadata;
        ComicFilename::parse('(' + event + QStringLiteral(") [社团 (作者)] 标题 (作品) [中国翻訳].zip")).metadata.applyTo(metadata);
        QCOMPARE(metadata.convention.toString(), event);
        QCOMPARE(metadata.circle.toString(), QStringLiteral("社团"));
        QCOMPARE(metadata.writer.toString(), QStringLiteral("作者"));
        QCOMPARE(metadata.work.toString(), QStringLiteral("作品"));
        QVERIFY(ComicFilename::generate(metadata).startsWith('(' + event + ") ["));
        M roundTrip;
        ComicFilename::parse(ComicFilename::generate(metadata) + ".zip").metadata.applyTo(roundTrip);
        QCOMPARE(roundTrip.convention, metadata.convention);
    }
    M wide;
    ComicFilename::parse(QStringLiteral("（C107）［独立作者］ 标题 （作品）.7z")).metadata.applyTo(wide);
    QCOMPARE(wide.convention.toString(), QString("C107"));
    QCOMPARE(wide.writer.toString(), QStringLiteral("独立作者"));
    auto absent = ComicFilename::parse(QStringLiteral("[独立作者] 标题 (作品).zip"));
    QVERIFY(!absent.metadata.contains(&M::convention));
    absent.metadata.applyTo(wide);
    QCOMPARE(wide.convention.toString(), QString("C107"));
    ComicMetadataEditor editor;
    editor.setMetadata({ wide });
    auto *field = editor.findChild<QLineEdit *>("convention");
    QVERIFY(field);
    QCOMPARE(field->text(), QString("C107"));
    QVERIFY(editor.editedMetadata().isEmpty());
    field->setText(QStringLiteral("例大祭"));
    editor.editedMetadata().applyTo(wide);
    QCOMPARE(wide.convention.toString(), QStringLiteral("例大祭"));
    field->clear();
    editor.editedMetadata().applyTo(wide);
    QVERIFY(wide.convention.toString().isEmpty());
}

void ComicMetadataEditorTest::pickerStaysOpenSwitchesAndToggles()
{
    ComicMetadataEditor editor;
    int loads = 0;
    editor.setVocabularyProvider([&] {
        ++loads;
        return ComicMetadataEditor::Vocabulary { { "tags", { "Alpha", "Beta", "Existing" } }, { "work", { "Work A", "Work B" } } };
    });
    auto metadata = sample();
    metadata.tags = "Existing";
    metadata.work = "Work A";
    editor.setMetadata({ metadata });
    editor.resize(700, 850);
    editor.show();
    auto *tags = editor.findChild<MetadataListEdit *>("tags");
    auto *tagPicker = editor.findChild<MetadataPicker *>("picker_tags");
    auto *workPicker = editor.findChild<MetadataPicker *>("picker_work");
    QTest::mouseClick(tags->findChild<QLineEdit *>("value"), Qt::LeftButton);
    QVERIFY(tagPicker->isExpanded());
    QVERIFY(editor.editedMetadata().isEmpty());
    QCOMPARE(loads, 1);
    auto *chips = static_cast<MetadataValueChips *>(tagPicker->findChild<QWidget *>("pickerChips"));
    QCoreApplication::processEvents();
    // Actual mouse selection of the first, alphabetically sorted chip.
    QTest::mouseClick(chips, Qt::LeftButton, Qt::NoModifier, QPoint(12, 12));
    QVERIFY(tags->values().contains("Alpha"));
    QVERIFY(tags->values().contains("Existing"));
    QVERIFY(chips->selected.contains("Alpha"));
    QVERIFY(tagPicker->isExpanded());
    auto saved = metadata;
    editor.editedMetadata().applyTo(saved);
    QCOMPARE(saved.tags.toString(), QString("Existing\nAlpha"));
    QCOMPARE(metadata.tags.toString(), QString("Existing"));
    QTest::mouseClick(chips, Qt::LeftButton, Qt::NoModifier, QPoint(12, 12));
    QCOMPARE(tags->values(), QStringList({ "Existing" }));
    QTest::mouseClick(editor.findChild<QLineEdit *>("work"), Qt::LeftButton);
    QVERIFY(!tagPicker->isExpanded());
    QVERIFY(workPicker->isExpanded());
    emit workPicker->valueClicked("Work B");
    QCOMPARE(editor.findChild<QLineEdit *>("work")->text(), QString("Work B"));
    QVERIFY(workPicker->isExpanded());
    workPicker->findChild<QToolButton *>("pickerToggle")->click();
    QVERIFY(!workPicker->isExpanded());
    QTest::mouseClick(editor.findChild<QLineEdit *>("work"), Qt::LeftButton);
    QVERIFY(workPicker->isExpanded());
    QCOMPARE(loads, 1);
    QTest::mouseClick(editor.findChild<QLineEdit *>("title"), Qt::LeftButton);
    QVERIFY(!workPicker->isExpanded());
}

void ComicMetadataEditorTest::pickerHandlesAuthorsAndOriginals()
{
    ComicMetadataEditor editor;
    auto metadata = sample();
    metadata.writer = "Author A";
    metadata.isAnthology = false;
    editor.setMetadata({ metadata });
    auto *picker = editor.findChild<MetadataPicker *>("picker_writer");
    emit picker->valueClicked("Author B");
    QCOMPARE(editor.findChild<QLineEdit *>("author")->text(), QString("Author B"));
    editor.findChild<QCheckBox *>("isAnthology")->setChecked(true);
    emit picker->valueClicked("Author C");
    QCOMPARE(editor.findChild<MetadataListEdit *>("authors")->values(), QStringList({ "Author B", "Author C" }));
    emit picker->valueClicked("Author B");
    QCOMPARE(editor.findChild<MetadataListEdit *>("authors")->values(), QStringList({ "Author C" }));
    auto *work = editor.findChild<MetadataPicker *>("picker_work");
    auto *characters = editor.findChild<MetadataPicker *>("picker_characters");
    QVERIFY(!editor.findChild<QPushButton *>("original"));
    // A built-in first choice is available even when the library has none.
    work->findChild<QToolButton *>("pickerToggle")->click();
    auto *workChips = static_cast<MetadataValueChips *>(work->findChild<QWidget *>("pickerChips"));
    QCOMPARE(workChips->values.first(), QStringLiteral("原创"));
    QCOMPARE(workChips->values.count(QStringLiteral("原创")), 1);
    emit work->valueClicked(QStringLiteral("原创"));
    characters->findChild<QToolButton *>("pickerToggle")->click();
    auto *characterChips = static_cast<MetadataValueChips *>(characters->findChild<QWidget *>("pickerChips"));
    QCOMPARE(characterChips->values.first(), QStringLiteral("原创"));
    QCOMPARE(characterChips->values.count(QStringLiteral("原创")), 1);
    QCOMPARE(editor.findChild<MetadataListEdit *>("characters")->values(), QStringList({ QStringLiteral("原创") }));
    emit characters->valueClicked("Character A");
    QCOMPARE(editor.findChild<MetadataListEdit *>("characters")->values(), QStringList({ "Character A" }));
    QVERIFY(editor.findChild<QLineEdit *>("work")->text().isEmpty());
    emit characters->valueClicked(QStringLiteral("原创"));
    QCOMPARE(editor.findChild<QLineEdit *>("work")->text(), QStringLiteral("原创"));
    auto saved = metadata;
    editor.editedMetadata().applyTo(saved);
    QVERIFY(saved.isAnthology.toBool());
    QCOMPARE(saved.writer.toString(), QString("Author C"));
}

void ComicMetadataEditorTest::pickerReloadsVocabularyForTheNextRecord()
{
    ComicMetadataEditor editor;
    QString candidate = "Library A";
    int calls = 0;
    editor.setVocabularyProvider([&] { ++calls; return ComicMetadataEditor::Vocabulary { { "tags", { candidate } } }; });
    auto first = sample(), second = sample();
    first.tags = "One";
    second.tags = "Two";
    editor.setMetadata({ first, second });
    auto *picker = editor.findChild<MetadataPicker *>("picker_tags");
    picker->findChild<QToolButton *>("pickerToggle")->click();
    QVERIFY(picker->isExpanded());
    QVERIFY(editor.editedMetadata().isEmpty());
    auto *chips = static_cast<MetadataValueChips *>(picker->findChild<QWidget *>("pickerChips"));
    QVERIFY(chips->selected.isEmpty());
    emit picker->valueClicked(candidate);
    auto a = first, b = second;
    editor.editedMetadata().applyTo(a);
    editor.editedMetadata().applyTo(b);
    QCOMPARE(a.tags.toString(), candidate);
    QCOMPARE(b.tags.toString(), candidate);
    candidate = "Library B";
    editor.setMetadata({ second });
    QVERIFY(!picker->isExpanded());
    QVERIFY(editor.editedMetadata().isEmpty());
    picker->findChild<QToolButton *>("pickerToggle")->click();
    QCOMPARE(calls, 2);
    QVERIFY(chips->values.contains("Library B"));
    QVERIFY(!chips->values.contains("Library A"));
    QVERIFY(chips->selected.contains("Two"));
}

void ComicMetadataEditorTest::manyPickerValuesScrollAndRender_data()
{
    QTest::addColumn<bool>("dark");
    QTest::newRow("light") << false;
    QTest::newRow("dark") << true;
}

void ComicMetadataEditorTest::manyPickerValuesScrollAndRender()
{
    QFETCH(bool, dark);
    QTranslator translator;
    if (translator.load(qEnvironmentVariable("COMIC_EDITOR_TRANSLATION")))
        qApp->installTranslator(&translator);
    const auto restoreTranslator = qScopeGuard([&] { qApp->removeTranslator(&translator); });
    ComicMetadataEditor editor;
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
    editor.setFont(QFont("Microsoft YaHei", 10));
    QStringList values;
    for (int n = 0; n < 100; ++n)
        values.append(QString("%1 English-tag-%2").arg(n + 1, 3, 10, QChar('0')).arg(n));
    editor.setVocabularyProvider([values] { return ComicMetadataEditor::Vocabulary { { "tags", values } }; });
    auto metadata = sample();
    metadata.tags = values[0] + '\n' + values[1];
    editor.setMetadata({ metadata });
    auto palette = editor.palette();
    for (auto role : { QPalette::Window, QPalette::Button })
        palette.setColor(role, QColor(dark ? "#292929" : "#f5f5f5"));
    palette.setColor(QPalette::Base, QColor(dark ? "#343434" : "#ffffff"));
    for (auto role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
        palette.setColor(role, QColor(dark ? "#ededed" : "#222222"));
    editor.setPalette(palette);
    editor.setAutoFillBackground(true);
    editor.resize(700, 1100);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    auto *picker = editor.findChild<MetadataPicker *>("picker_tags");
    QTest::mouseClick(picker->findChild<QToolButton *>("pickerToggle"), Qt::LeftButton);
    auto *area = picker->findChild<QScrollArea *>("pickerArea");
    QTRY_VERIFY(area->verticalScrollBar()->maximum() > 0);
    QVERIFY(area->height() <= 180);
    auto *chips = static_cast<MetadataValueChips *>(picker->findChild<QWidget *>("pickerChips"));
    QCOMPARE(chips->selected.size(), 2);
    const auto preview = qEnvironmentVariable("METADATA_PICKER_PREVIEW_DIR");
    if (!preview.isEmpty()) {
        QDir().mkpath(preview);
        QTest::qWait(100);
        QVERIFY(editor.grab().save(preview + '/' + QTest::currentDataTag() + ".png"));
    }
    area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum());
    QVERIFY(area->verticalScrollBar()->value() > 0);
    QVERIFY(picker->isExpanded());
    QVERIFY(editor.editedMetadata().isEmpty());
}

void ComicMetadataEditorTest::blankClicksDismissWithoutChangingValues()
{
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *editor = new ComicMetadataEditor(&window);
    auto *blank = new QWidget(&window);
    blank->setFixedHeight(65);
    layout->addWidget(editor);
    layout->addWidget(blank);
    editor->setMetadata({ sample() });
    window.resize(700, 950);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto *picker = editor->findChild<MetadataPicker *>("picker_tags");
    auto *tags = editor->findChild<MetadataListEdit *>("tags");
    QTest::mouseClick(tags->findChild<QLineEdit *>("value"), Qt::LeftButton);
    QVERIFY(picker->isExpanded());
    emit picker->valueClicked("New tag");
    const auto selected = tags->values();
    auto before = sample();
    editor->editedMetadata().applyTo(before);
    // Blank space in the window, outside the editor, must also dismiss.
    QTest::mouseClick(blank, Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
    QVERIFY(!picker->isExpanded());
    QCOMPARE(tags->values(), selected);
    auto after = sample();
    editor->editedMetadata().applyTo(after);
    QCOMPARE(after.tags, before.tags);
    picker->findChild<QToolButton *>("pickerToggle")->click();
    QVERIFY(picker->isExpanded());
    QCoreApplication::processEvents();
    // Padding inside the chip area is blank, rather than a tag hit target.
    auto *chips = picker->findChild<QWidget *>("pickerChips");
    QTest::mouseClick(chips, Qt::LeftButton, Qt::NoModifier, QPoint(1, 1));
    QVERIFY(!picker->isExpanded());
    picker->findChild<QToolButton *>("pickerToggle")->click();
    QVERIFY(picker->isExpanded());
    QWidget otherWindow;
    otherWindow.resize(150, 100);
    otherWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&otherWindow));
    QTest::mouseClick(&otherWindow, Qt::LeftButton, Qt::NoModifier, QPoint(20, 20));
    QVERIFY(picker->isExpanded());
    QCOMPARE(tags->values(), selected);
}

void ComicMetadataEditorTest::pickerControlsAndScrollDoNotDismiss()
{
    ComicMetadataEditor editor;
    QStringList candidates;
    for (int n = 0; n < 80; ++n)
        candidates.append(QString("Tag-%1").arg(n, 3, 10, QChar('0')));
    editor.setVocabularyProvider([candidates] { return ComicMetadataEditor::Vocabulary { { "tags", candidates } }; });
    auto metadata = sample();
    metadata.tags = "Tag-000";
    editor.setMetadata({ metadata });
    editor.resize(700, 1000);
    editor.show();
    QVERIFY(QTest::qWaitForWindowExposed(&editor));
    auto *tags = editor.findChild<MetadataListEdit *>("tags");
    auto *picker = editor.findChild<MetadataPicker *>("picker_tags");
    QTest::mouseClick(tags->findChild<QLineEdit *>("value"), Qt::LeftButton);
    QVERIFY(picker->isExpanded());
    auto *chips = picker->findChild<QWidget *>("pickerChips");
    QCoreApplication::processEvents();
    QTest::mouseClick(chips, Qt::LeftButton, Qt::NoModifier, QPoint(12, 12));
    QVERIFY(picker->isExpanded());
    QVERIFY(!tags->values().contains("Tag-000"));
    auto *input = tags->findChild<QLineEdit *>("value");
    input->setText("Typed tag");
    QTest::keyClick(input, Qt::Key_Return);
    QVERIFY(picker->isExpanded());
    QVERIFY(tags->values().contains("Typed tag"));
    QTest::keyClick(input, Qt::Key_Backspace);
    QVERIFY(!tags->values().contains("Typed tag"));
    QVERIFY(picker->isExpanded());
    auto *area = picker->findChild<QScrollArea *>("pickerArea");
    QTRY_VERIFY(area->verticalScrollBar()->maximum() > 0);
    auto *scroll = area->verticalScrollBar();
    QTest::mouseClick(scroll, Qt::LeftButton, Qt::NoModifier, QPoint(scroll->width() / 2, scroll->height() - 3));
    QVERIFY(picker->isExpanded());
    QTest::mouseClick(editor.findChild<QLineEdit *>("work"), Qt::LeftButton);
    QVERIFY(!picker->isExpanded());
    auto *work = editor.findChild<MetadataPicker *>("picker_work");
    QVERIFY(work->isExpanded());
    QTest::mouseClick(work->findChild<QToolButton *>("pickerToggle"), Qt::LeftButton);
    QVERIFY(!work->isExpanded());
}

void ComicMetadataEditorTest::seriesAndRatingRemainDrafts()
{
    ComicMetadataEditor editor;
    editor.setMetadata({ sample() });
    editor.setRatings({ 2, 4 });
    editor.resize(620, 1000);
    editor.show();
    auto *toggle = editor.findChild<QCheckBox *>("isSeries");
    auto *picker = editor.findChild<MetadataPicker *>("picker_seriesName");
    QVERIFY(toggle && picker);
    QVERIFY(picker->isHidden());
    toggle->setChecked(true);
    QVERIFY(!picker->isHidden());
    emit picker->valueClicked("Series A");
    emit picker->valueClicked("Series B");
    auto metadata = sample();
    editor.editedMetadata().applyTo(metadata);
    QCOMPARE(metadata.seriesName.toString(), QString("Series B"));
    toggle->setChecked(false);
    toggle->setChecked(true);
    QCOMPARE(editor.findChild<QLineEdit *>("seriesName")->text(), QString("Series B"));
    QVERIFY(!editor.editedRating());
    editor.findChild<QAbstractButton *>("rating3")->click();
    QCOMPARE(editor.editedRating(), std::optional<int>(3));
    editor.findChild<QAbstractButton *>("rating3")->click();
    QCOMPARE(editor.editedRating(), std::optional<int>(0));
    editor.setRatings({ std::nullopt });
    QVERIFY(!editor.findChild<QAbstractButton *>("rating3")->isEnabled());
    QVERIFY(!editor.editedRating());
    toggle->setChecked(false);
    editor.editedMetadata().applyTo(metadata);
    QVERIFY(metadata.seriesName.isNull());
}

void ComicMetadataEditorTest::translatedTitleTogglePreservesDraftUntilSave()
{
    ComicMetadataEditor editor;
    auto original = sample();
    editor.setMetadata({ original });
    auto *toggle = editor.findChild<QCheckBox *>("showTranslatedTitle");
    auto *input = editor.findChild<QLineEdit *>("translatedTitle");
    QVERIFY(toggle->isChecked());
    toggle->setChecked(false);
    QVERIFY(input->isHidden());
    QCOMPARE(input->text(), original.translatedTitle.toString());
    auto cleared = original;
    editor.editedMetadata().applyTo(cleared);
    QVERIFY(cleared.translatedTitle.isNull());
    toggle->setChecked(true);
    QCOMPARE(input->text(), original.translatedTitle.toString());
    original.translatedTitle = QVariant();
    editor.setMetadata({ original });
    QVERIFY(!toggle->isChecked());
    QVERIFY(input->isHidden());
    QVERIFY(editor.editedMetadata().isEmpty());
    toggle->setChecked(true);
    input->setText("New translation");
    editor.editedMetadata().applyTo(original);
    QCOMPARE(original.translatedTitle.toString(), QString("New translation"));
}

void ComicMetadataEditorTest::manyCharactersKeepTheInlineEditorInsideTheFrame()
{
    QScrollArea window;
    auto *editor = new ComicMetadataEditor;
    auto metadata = sample();
    QStringList names;
    for (int n = 0; n < 24; ++n)
        names.append(QString::fromUtf8("角色-Long-name-%1").arg(n));
    metadata.characters = names.join('\n');
    editor->setMetadata({ metadata });
    window.setWidget(editor);
    window.setWidgetResizable(true);
    window.resize(620, 700);
    window.show();
    auto *characters = editor->findChild<MetadataListEdit *>("characters");
    auto *tokens = characters->findChild<QWidget *>("selectedTokens");
    auto *input = characters->findChild<QLineEdit *>("value");
    QVERIFY(tokens && input);
    for (const auto width : { 520, 760, 520 }) {
        window.resize(width, 700);
        QTest::qWait(30);
        QVERIFY(tokens->width() > editor->width() * 0.8);
        QVERIFY(input->geometry().bottom() < tokens->height());
        QVERIFY(input->geometry().right() < tokens->width());
        QVERIFY(tokens->mapTo(characters, QPoint(0, tokens->height())).y() <= characters->height());
    }
    input->setText("Another character");
    QTest::keyClick(input, Qt::Key_Return);
    QCOMPARE(characters->values().size(), 25);
}

QTEST_MAIN(ComicMetadataEditorTest)
#include "main.moc"
