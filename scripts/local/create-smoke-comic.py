"""Create a three-page comic fixture without external dependencies."""

import struct
import zlib
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile


def chunk(kind, data):
    return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))


def page(number, color):
    width, height = 600, 800
    digits = {1: ['010', '110', '010', '010', '111'],
              2: ['111', '001', '111', '100', '111'],
              3: ['111', '001', '111', '001', '111']}
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        for x in range(width):
            pixel = (245, 245, 245)
            if 30 < x < 570 and 30 < y < 770:
                pixel = color
            if 180 <= x < 420 and 200 <= y < 600:
                if digits[number][(y - 200) // 80][(x - 180) // 80] == '1':
                    pixel = (255, 255, 255)
            rows.extend(pixel)
    header = struct.pack('!2I5B', width, height, 8, 2, 0, 0, 0)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(rows)) + chunk(b'IEND', b'')


source_root = Path(__file__).resolve().parents[2]
workspace = source_root.parents[1] if (source_root.parents[1] / '.build-tools').is_dir() else source_root
destination = workspace / 'artifacts' / 'smoke-test' / 'Baseline Test.cbz'
destination.parent.mkdir(parents=True, exist_ok=True)
with ZipFile(destination, 'w', ZIP_DEFLATED) as archive:
    for index, color in enumerate([(50, 100, 170), (40, 140, 110), (180, 100, 60)], 1):
        archive.writestr(f'{index:03}.png', page(index, color))
print(destination)
