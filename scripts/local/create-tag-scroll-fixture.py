"""Create an isolated, portable tag-scroll demo beside the existing UI demo."""
from pathlib import Path
import shutil
import sqlite3
import struct

workspace = next(p for p in Path(__file__).resolve().parents if (p / '.build-tools').is_dir())
artifacts = workspace / 'artifacts' / 'table-v1'
source = artifacts / 'demo-library'
destination = artifacts / 'tag-scroll-demo'
profile = artifacts / 'tag-scroll-profile' / 'YACReaderLibrary'
if destination.exists() or profile.exists():
    raise SystemExit('Scroll fixture already exists; refusing to overwrite it.')
shutil.copytree(source, destination)
database = destination / '.yacreaderlibrary' / 'library.ydb'
with sqlite3.connect(database) as db:
    ids = [row[0] for row in db.execute('SELECT id FROM comic_info WHERE id IN (SELECT comicInfoId FROM comic) ORDER BY id')]
    assert ids, 'Source demo has no comics'
    tags = []
    for number in range(1, 181):
        if number % 4 == 0:
            tags.append(f'{number:03d} English-tag-{number}')
        elif number % 4 == 1:
            tags.append(f'{number:03d} 日常标签')
        elif number % 4 == 2:
            tags.append(f'{number:03d} 用于测试换行的较长标签')
        else:
            tags.append(f'{number:03d} 数字{number * 12345}')
    for index, comic_id in enumerate(ids):
        db.execute('UPDATE comic_info SET tags = ?, rating = ? WHERE id = ?',
                   ('\n'.join(tags[index::len(ids)]), index % 5 + 1, comic_id))
    db.commit()
id_file = destination / '.yacreaderlibrary' / 'id'
if id_file.exists():
    id_file.unlink()  # The new library gets its own UUID during profile migration.

def qstring(text):
    encoded = text.encode('utf-16-be')
    return struct.pack('>I', len(encoded)) + encoded

# Use the supported legacy QMap<QString, QPair<int, QString>> settings format.
# YACReaderLibraries::load migrates it to the current format on first launch.
registry = struct.pack('>I', 1) + qstring('多标签滚动验收') + struct.pack('>i', 1) + qstring(destination.as_posix())
escaped = ''.join(f'\\x{byte:02x}' for byte in registry)
profile.mkdir(parents=True)
(profile / 'YACReaderLibrary.ini').write_text(
    f'[General]\nLIBRARIES=@ByteArray({escaped})\n\n'
    '[libraryConfig]\nSERVER_ON=false\nUI_LANGUAGE=zh_CN\nCOMICS_VIEW_STATUS=0\n',
    encoding='utf-8')
(artifacts / '启动多标签滚动验收.cmd').write_text(
    '@echo off\nsetlocal\nset "YACREADER_DATA_DIR=%~dp0tag-scroll-profile"\n'
    'start "" "%~dp0YACReaderLibrary-filter-ui.exe"\nendlocal\n', encoding='ascii')
print(f'Created {len(tags)} tags across {len(ids)} comics: {destination}')
