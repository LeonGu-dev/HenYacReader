import html
import re
from pathlib import Path

path = Path(__file__).resolve().parents[2] / 'YACReaderLibrary/yacreaderlibrary_zh_CN.ts'
translations = {
    'MetadataPicker': {
        'No entries in this library yet': '当前书库尚无已录入内容',
        'Collapse choices': '收起候选', 'Expand choices': '展开候选',
    },
    'FolderDeleteConfirmation': {
        'Folder: %1': '文件夹：%1',
    },
    'DeleteConfirmation': {
        'This operation cannot be undone. Are you really sure?': '该操作无法撤销，你真的确定吗？',
    },
    'CurrentComicBanner': {'Continue reading': '继续阅读'},
    'ComicInfoView': {
        'Collapse': '收起', '+%1 more': '+%1 展开',
        'Read': '已读', 'Unread': '未读', 'Page %1 / %2': '第 %1 / %2 页',
        'Not started · %1 pages': '未开始 · %1 页',
        'No comic information entered': '尚未录入漫画信息',
        'File information': '文件信息', '%1 pages': '%1 页', 'Filename': '文件名', 'Location': '位置',
    },
    'LibraryWindow': {
        'Edit smart list...': '编辑智能列表…',
        'Changes could not be saved': '无法保存修改',
    },
    'LibraryWindowMenus': {
        'Advanced repair': '高级修复', 'No reading lists': '暂无普通阅读列表',
    },
    'EditFilterSmartListDialog': {
        'Edit smart list': '编辑智能列表', 'List name:': '列表名称：',
        'Apply to library': '作用于库', 'Choose folder': '选择文件夹',
        'Choose a folder before saving.': '请选择文件夹后再保存。',
        'Filter': '筛选', 'Folder unavailable': '文件夹已失效',
        'Library root': '书库根目录', 'Apply to folder: %1': '作用于文件夹：%1', 'None': '无',
    },
    'SaveFilterSmartListDialog': {
        'Save filter as smart list': '保存筛选为智能列表',
        'List name:': '列表名称：',
        'Entire library': '作用于整个书库',
        'Current folder': '作用于当前文件夹',
        'Current folder: %1': '当前文件夹：%1',
        'Library': '书库',
        'Currently in a reading list; the current folder scope is unavailable.': '当前位于阅读列表，无法作用于当前文件夹',
        'Tip: Smart filter lists are based on a folder or library, not a reading list.': '提示：筛选智能列表基于文件夹或库，无法基于阅读列表',
    },
    'LibraryWindowActions': {
        'Check and repair library database...': '检查并修复数据库…',
        'Repair covers and page counts...': '修复封面与页数…',
        'Re-extract missing covers and invalid page counts from local comic files': '从本地漫画文件重新提取缺失封面和无效页数',
        'Rescan comic metadata': '重新扫描漫画元数据',
        'Read metadata embedded in local comic files; existing fields may be overwritten': '读取本地漫画文件中内嵌的元数据，可能覆盖已有字段',
        'Add to reading list': '添加到阅读列表',
        'Save filter as smart list': '保存筛选为智能列表',
        'Save filter as reading list': '保存筛选为普通列表',
    },
    'ReadingListManagementCoordinator': {
        'Save filter as smart list': '保存筛选为智能列表',
        'Save filter as reading list': '保存筛选为普通列表',
        'There are no comics to save.': '当前没有可保存的漫画。',
        'The list could not be saved.': '无法保存列表。',
        'List name:': '列表名称：',
    },
    'QObject': {'Reading Lists': '阅读列表'},
    'MetadataFilterTabs': {'Previous filter tab': '向左移动标签', 'Next filter tab': '向右移动标签'},
    'MetadataFilterPanel': {
        'Series': '系列',
        'Artbook': '插画集',
        'Always expanded': '始终展开', 'Filter options': '筛选项',
        'Convention': '即卖会',
        'Doujinshi': '同人志', 'Collected volume': '单行本', 'Magazine': '杂志',
        'Reset all filters': '重置所有筛选',
        'Rating': '评分', '%1 stars': '%1星',
        'Filter': '筛选', 'Search': '搜索', 'tag': 'tag', 'Work': '作品', 'Author': '作者',
        'AND': '与', 'OR': '或', 'Filter favorite comics': '筛选最爱漫画',
        'Switch to OR filtering': '调整筛选逻辑为或筛选', 'Switch to AND filtering': '调整筛选逻辑为与筛选',
        'Black and white': '黑白', 'Color': '彩色', 'Censored': '有码', 'Uncensored': '无修正',
        'Chinese translation': '中译', 'AI translation': 'AI翻译', 'No translation': '无翻译',
    },
    'ComicMetadataEditor': {
        'This work is part of a serialized comic series.': '作品为系列连载漫画',
        'This work is a collaboration between multiple artists.': '作品为多画师合作漫画',
        'Comic Market, Reitaisai, etc.': 'Comic Market/例大祭等',
        'The original work that this fan comic is based on.': '同人漫画的原作品',
        'Generate a formatted filename from the tags, then rename the file.': '根据标签，生成并重命名格式化的文件名',
        'Enabled': '启用',
        'Series': '系列', 'Series work': '系列作', 'Not in a series': '未加入系列', 'Rating': '评分',
        'New filename': '新文件名', 'Close filename preview': '关闭文件名预览', 'Rename file': '重命名文件',
        'Artbook': '插画集',
        'Convention': '即卖会',
        'Publication type': '类型', 'Doujinshi': '同人志', 'Collected volume': '单行本', 'Magazine': '杂志',
        'Favorite': '最爱',
        'Parse tags from filename': '从文件名解析标签', 'Generate filename': '生成文件名',
        'Current archive filename': '当前压缩包文件名',
        'Title': '标题', 'Translated title': '标题翻译', 'Anthology': '合同本',
        'Author': '作者', 'Circle': '社团', 'Work / IP': '作品',
        'Characters': '角色', 'Tags': 'tag',
        'Color / black and white': '彩色/黑白', 'Color': '彩色',
        'Black and white': '黑白', 'Censorship': '无修正', 'Censored': '有码',
        'Uncensored': '无修', 'Translation': '翻译', 'Japanese': '日文',
        'Chinese translation': '中译', 'AI translation': 'AI 翻译',
        'Translator': '译者',
        'Multiple values; edit to replace': '多个值（修改后覆盖）',
        'Multiple values; unchanged': '多个值（保持不变）',
    },
    'PropertiesDialog': {
        'Rename failed': '重命名失败', 'Keep the archive extension unchanged.': '请保持压缩文件扩展名不变。',
        'The file could not be renamed.': '无法重命名文件。',
        'Rename is only available for one existing archive.': '只能为单个现存的压缩包生成新文件名。',
    },
    'ComicModel': {
        'The comic information could not be cleared. No selected comics were changed.': '无法清除漫画信息，所选漫画均未修改。',
        'Favorites could not be saved. No selected comics were changed.': '无法保存最爱状态，所选漫画均未修改。',
        'The selected comics could not be added to the reading list.': '无法将所选漫画添加到阅读列表。',
        'The rating could not be saved. The previous rating was kept.': '无法保存评分，已保留原评分。',
        'Convention': '即卖会',
        'Reading progress': '阅读进度', 'Author': '作者', 'Work / IP': '作品',
        'Characters': '角色', 'Tags': 'tag', 'Color / black and white': '彩色/黑白',
        'Censorship': '无修正', 'Translation': '翻译', 'Not set': '未设置',
        'Color': '彩色', 'Black and white': '黑白', 'Censored': '有码',
        'Uncensored': '无修', 'Japanese': '日文', 'Chinese translation': '中译',
        'AI translation': 'AI 翻译',
    },
    'YACReaderTableView': { 'Change color': '更改颜色', 'The color could not be saved.': '颜色无法保存。' },
    'ClassicComicsView': { 'Restore default columns': '恢复默认列' },
    'MetadataListEdit': {
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
