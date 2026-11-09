"""Build an isolated favorite demo from the small existing acceptance library."""
from pathlib import Path
import re
import argparse
import shutil
import sqlite3
import struct

workspace = next(p for p in Path(__file__).resolve().parents if (p / '.build-tools').is_dir())
artifacts = workspace / 'artifacts' / 'table-v1'
parser = argparse.ArgumentParser()
parser.add_argument('--library', default='favorite-demo')
parser.add_argument('--profile', default='favorite-profile')
parser.add_argument('--title', default='最爱功能验收')
parser.add_argument('--launcher', default='启动最爱功能验收.cmd')
parser.add_argument('--executable', default='YACReaderLibrary-favorites.exe')
args = parser.parse_args()
for value in (args.library, args.profile, args.launcher, args.executable):
    if Path(value).name != value or value in ('.', '..'):
        raise SystemExit('Names must stay within the acceptance directory.')
destination = artifacts / args.library
profile = artifacts / args.profile / 'YACReaderLibrary'
if destination.exists() or profile.exists():
    raise SystemExit('Favorite demo already exists; refusing to overwrite it.')
shutil.copytree(artifacts / 'demo-library', destination)
database = destination / '.yacreaderlibrary' / 'library.ydb'
with sqlite3.connect(database) as db:
    first = db.execute('SELECT parentId,comicInfoId,fileName,path FROM comic ORDER BY id LIMIT 1').fetchone()
    source = destination / first[3].lstrip('/\\')
    copy_name = source.stem + '（相同内容副本）' + source.suffix
    shutil.copy2(source, destination / copy_name)
    db.execute('INSERT INTO comic(parentId,comicInfoId,fileName,path) VALUES(?,?,?,?)',
               (1, first[1], copy_name, '/' + copy_name))
    for table in ('comic_info', 'placeholder'):
        if 'favorite' not in {row[1] for row in db.execute(f'PRAGMA table_info({table})')}:
            db.execute(f'ALTER TABLE {table} ADD COLUMN favorite BOOLEAN DEFAULT 0')
        if 'publicationType' not in {row[1] for row in db.execute(f'PRAGMA table_info({table})')}:
            db.execute(f'ALTER TABLE {table} ADD COLUMN publicationType TEXT')
    db.execute('UPDATE comic_info SET favorite = 1 WHERE id IN (SELECT c.comicInfoId FROM comic c INNER JOIN comic_default_reading_list f ON f.comic_id=c.id WHERE f.default_reading_list_id=1)')
    db.execute('DELETE FROM comic_default_reading_list WHERE default_reading_list_id=1')
    db.execute('UPDATE comic_info SET favorite = 1 WHERE id IN (SELECT DISTINCT comicInfoId FROM comic ORDER BY id LIMIT 3)')
    ids = [row[0] for row in db.execute('SELECT id FROM comic_info ORDER BY id')]
    for index, comic_id in enumerate(ids):
        db.execute('UPDATE comic_info SET publicationType=? WHERE id=?',
                   (('doujinshi', 'tankobon', 'magazine', None)[index % 4], comic_id))
    db.execute("UPDATE db_info SET version='10.5.3'")
    db.commit()
    print('Favorite demo files:', db.execute('SELECT COUNT(*) FROM comic').fetchone()[0])
id_file = destination / '.yacreaderlibrary' / 'id'
if id_file.exists():
    id_file.unlink()

def qstring(text):
    data = text.encode('utf-16-be')
    return struct.pack('>I', len(data)) + data

registry = struct.pack('>I', 1) + qstring(args.title) + struct.pack('>i', 1) + qstring(destination.as_posix())
escaped = ''.join(f'\\x{byte:02x}' for byte in registry)
source_ini = artifacts / 'tag-scroll-profile' / 'YACReaderLibrary' / 'YACReaderLibrary.ini'
if not source_ini.exists():
    source_ini = artifacts / 'profile' / 'YACReaderLibrary' / 'YACReaderLibrary.ini'
ini = source_ini.read_text(encoding='utf-8')
ini = re.sub(r'^LIBRARIES=.*$', lambda _: 'LIBRARIES=@ByteArray(' + escaped + ')', ini, flags=re.M)
profile.mkdir(parents=True)
(profile / 'YACReaderLibrary.ini').write_text(ini, encoding='utf-8')
(artifacts / args.launcher).write_text(
    f'@echo off\nsetlocal\nset "YACREADER_DATA_DIR=%~dp0{args.profile}"\n'
    f'start "" "%~dp0{args.executable}"\nendlocal\n', encoding='ascii')
print('Created favorite demo:', destination)
