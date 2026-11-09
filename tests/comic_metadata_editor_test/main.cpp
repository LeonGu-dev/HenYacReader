#include "comic_filename.h"
#include "comic_metadata_editor.h"
#include "staged_comic_rename.h"
#include "yacreader_cover_label.h"
#include "yacreader_global.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFile>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTranslator>
#include <QVBoxLayout>

class ComicMetadataEditorTest : public QObject
{
    Q_OBJECT
private slots:
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
};

namespace {
DescriptiveComicMetadata sample()
{
    DescriptiveComicMetadata metadata;
    metadata.title = QStringLiteral("漫画标题");
    metadata.translatedTitle = QStringLiteral("翻译标题");
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
    editor.findChild<QPushButton *>("original")->click();
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
    editor.findChild<QPushButton *>("original")->click();
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
        editor.findChild<QButtonGroup *>(key)->buttons().first()->click();
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
        list->findChildren<QPushButton *>("add").last()->click();
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
    QScrollArea window;
    auto editor = new ComicMetadataEditor;
    editor->setMetadata({ sample() });
    window.setWidget(editor);
    window.setWidgetResizable(true);
    window.resize(570, 760);
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
    QCOMPARE(generated, QStringLiteral("[示例社团 (作者)] 漫画标题 (示例作品) [日常] [冒险] [中国翻訳] [無修正] [示例汉化组]"));
    DescriptiveComicMetadata reparsed;
    ComicFilename::parse(generated + ".zip").metadata.applyTo(reparsed);
    QCOMPARE(reparsed.title, metadata.title);
    QCOMPARE(reparsed.writer, metadata.writer);
    QCOMPARE(reparsed.tags, metadata.tags);
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
    QVERIFY(editor.pendingFileName().isEmpty());
    editor.findChild<QLineEdit *>("newFilename")->setText(QStringLiteral("手动修改"));
    editor.findChild<QCheckBox *>("renameOnSave")->setChecked(true);
    QCOMPARE(editor.pendingFileName(), QStringLiteral("手动修改.ZIP"));
    const auto preview = qEnvironmentVariable("COMIC_FILENAME_PREVIEW_PATH");
    if (!preview.isEmpty()) {
        editor.resize(640, 900);
        editor.show();
        QCoreApplication::processEvents();
        QVERIFY(editor.grab().save(preview));
    }
    editor.setFileName("next.cbz");
    QVERIFY(editor.pendingFileName().isEmpty());
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

QTEST_MAIN(ComicMetadataEditorTest)
#include "main.moc"
