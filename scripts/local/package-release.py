"""Create a verified Windows ZIP and checksums from an audited release payload."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import zipfile


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def package(payload, output, version):
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Expected an x.y.z version')
    payload = payload.resolve(strict=True)
    source = Path(__file__).resolve().parents[2]
    if version != (source / 'VERSION').read_text('utf-8').strip():
        raise ValueError('Package version does not match source VERSION')
    if subprocess.check_output(['git', 'status', '--porcelain'], cwd=source, text=True).strip():
        raise ValueError('Commit the verified source before packaging a release')
    commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=source, text=True).strip()
    output.mkdir(parents=True, exist_ok=True)
    name = f'HenYacReader-{version}-windows-x64'
    archive = output / (name + '.zip')
    manifest = output / (name + '.manifest.json')
    checksums = output / (name + '.sha256')
    if any(path.exists() for path in (archive, manifest, checksums)):
        raise FileExistsError('Use a fresh output directory; existing releases are not overwritten')
    subprocess.run([sys.executable, str(source / 'scripts/local/check-reference-isolation.py'),
                    '--source', str(source), '--payload', str(payload), '--manifest', str(manifest)], check=True)
    metadata = json.loads(manifest.read_text('utf-8'))
    metadata.update({'project': 'HenYacReader', 'version': version, 'source_commit': commit})
    manifest.write_text(json.dumps(metadata, ensure_ascii=False, indent=2) + '\n', 'utf-8')
    records = metadata['files']
    with zipfile.ZipFile(archive, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as zipped:
        for record in records:
            zipped.write(payload / record['path'], name + '/' + record['path'])
    # Read every compressed entry back, rather than trusting a successful write.
    with zipfile.ZipFile(archive) as zipped:
        if set(zipped.namelist()) != {name + '/' + record['path'] for record in records}:
            raise ValueError('ZIP file list does not match the audited payload')
        for record in records:
            data = zipped.read(name + '/' + record['path'])
            if len(data) != record['bytes'] or hashlib.sha256(data).hexdigest() != record['sha256']:
                raise ValueError('ZIP entry failed verification: ' + record['path'])
    checksums.write_text(f'{sha256(archive)}  {archive.name}\n{sha256(manifest)}  {manifest.name}\n', 'utf-8')
    print(json.dumps({'archive': str(archive.resolve()), 'bytes': archive.stat().st_size,
                      'sha256': sha256(archive), 'manifest': str(manifest.resolve()),
                      'checksums': str(checksums.resolve())}, ensure_ascii=False))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--payload', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--version', required=True)
    args = parser.parse_args()
    package(args.payload, args.output, args.version)
