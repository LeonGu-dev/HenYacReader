import html
import re
from pathlib import Path

path = Path(__file__).resolve().parents[2] / 'YACReaderLibrary/yacreaderlibrary_zh_CN.ts'
translations = {
    'ComicMetadataEditor': {
        'Parse tags from filename': '从文件名解析标签', 'Generate filename': '生成文件名',
        'Current archive filename': '当前压缩包文件名',
        'Edit the filename without its extension': '可以修改新文件名；扩展名保持不变',
        'Rename this archive when saving (keep extension)': '保存时重命名压缩包（保留扩展名）',
        'Check the preview, then enable rename on save. Extension: %1. Translated title, characters and color stay in the library.': '请核对预览并勾选保存时重命名。扩展名：%1。翻译标题、角色和彩色/黑白仅保存在书库中。',
        'Title': '标题', 'Translated title': '翻译标题', 'Anthology': '合同本',
        'Author': '作者', 'Circle': '社团', 'Work / IP': '作品',
        'Original work': '原创', 'Characters': '角色', 'Tags': 'tag',
        'Color / black and white': '彩色/黑白', 'Color': '彩色',
        'Black and white': '黑白', 'Censorship': '无修正', 'Censored': '有码',
        'Uncensored': '无修', 'Translation': '翻译', 'Japanese': '日文',
        'Chinese translation': '中译', 'AI translation': 'AI 翻译',
        'Translator': '译者', 'Not set': '未设置',
        'Multiple values; edit to replace': '多个值（修改后覆盖）',
        'Multiple values; unchanged': '多个值（保持不变）',
    },
    'PropertiesDialog': {
        'Rename is only available for one existing archive.': '只能为单个现存的压缩包生成新文件名。',
    },
    'MetadataListEdit': {
        'Add item': '添加一项', 'Remove item': '移除这一项',
        'Multiple values; edit to replace': '多个值（修改后覆盖）',
    },
    'SearchFieldRegistry': {
        'Translated comic title': '漫画的翻译标题', 'Creator circle': '作者所属社团',
        'Source work or IP': '原作或 IP 作品',
        'Translator or translation group': '译者或汉化组',
        'Censorship status': '修正状态', 'Translation status': '翻译状态',
        'Multiple-author anthology': '多作者合同本',
    },
}
source = path.read_text(encoding='utf-8')
counts = {}

def context(match):
    block = match.group(0)
    name_match = re.search(r'<name>(.*?)</name>', block)
    if not name_match or name_match[1] not in translations:
        return block
    name = name_match[1]
    mapping = translations[name]
    counts[name] = 0
    def message(match):
        message_block = match.group(0)
        source_match = re.search(r'<source>(.*?)</source>', message_block, re.S)
        key = html.unescape(source_match[1]) if source_match else ''
        if key not in mapping:
            return message_block
        counts[name] += 1
        translated = '<translation>' + html.escape(mapping[key], quote=False) + '</translation>'
        return re.sub(r'<translation\b[^>]*(?:/>|>.*?</translation>)', lambda _: translated, message_block, flags=re.S)
    return re.sub(r'<message\b[^>]*>.*?</message>', message, block, flags=re.S)

updated = re.sub(r'<context>.*?</context>', context, source, flags=re.S)
for name, mapping in translations.items():
    if counts.get(name) != len(mapping):
        raise RuntimeError(f'Missing extracted translations for {name}: {counts.get(name)} / {len(mapping)}')
path.write_text(updated, encoding='utf-8', newline='\n')
print('Updated Simplified Chinese editor translations:', counts)
