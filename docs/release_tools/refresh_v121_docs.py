"""Refresh v1.2.1 release documentation while preserving application bytes."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import plistlib
import shutil
import subprocess
import tempfile
import zipfile
from datetime import datetime, timezone

VERSION = '1.2.1'
ROOT_DOCS = ('README.md', 'README_EN.md', 'README_KO.md', 'README_FIRST.txt',
             'LICENSE', 'COPYRIGHT.txt')


def digest(data):
    return hashlib.sha256(data).hexdigest()


def documentation(root):
    files = {name: (root / name).read_bytes() for name in ROOT_DOCS}
    for path in (root / 'docs').rglob('*'):
        if path.is_file() and 'release_tools' not in path.relative_to(root).parts:
            files[path.relative_to(root).as_posix()] = path.read_bytes()
    assert b'1.2.1' in files['README_FIRST.txt']
    for name, data in files.items():
        if name.endswith(('.txt', '.md')) and not name.startswith('docs/Release_'):
            if b'1.1.5' in data:
                raise RuntimeError(f'Stale current-documentation version: {name}')
    return files


def is_documentation(name):
    return name in ROOT_DOCS or name.startswith('docs/')


def zip_payload(path):
    with zipfile.ZipFile(path) as archive:
        if archive.testzip() is not None:
            raise RuntimeError(f'Corrupt ZIP: {path}')
        return {info.filename: digest(archive.read(info))
                for info in archive.infolist()
                if not info.is_dir() and not is_documentation(info.filename)}


def refresh_windows(original, output, docs):
    before = zip_payload(original)
    with zipfile.ZipFile(original) as source, zipfile.ZipFile(output, 'w') as target:
        for entry in source.infolist():
            if not is_documentation(entry.filename):
                target.writestr(entry, source.read(entry))
        for name, data in sorted(docs.items()):
            target.writestr(name, data, compress_type=zipfile.ZIP_DEFLATED)
    after = zip_payload(output)
    if before != after:
        raise RuntimeError('Non-documentation Windows package files changed')
    with zipfile.ZipFile(output) as archive:
        for name, data in docs.items():
            if archive.read(name) != data:
                raise RuntimeError(f'Windows documentation mismatch: {name}')
    return {'non_documentation_files_unchanged': True,
            'verified_file_count': len(before)}


def tree_payload(root):
    result = {}
    for path in root.rglob('*'):
        relative = path.relative_to(root).as_posix()
        if path.is_symlink():
            result[relative] = {'symlink': os.readlink(path)}
        elif path.is_file():
            result[relative] = {'sha256': digest(path.read_bytes()),
                                'mode': path.stat().st_mode & 0o777}
    return result


def write_docs(root, docs):
    for name, data in docs.items():
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def verify_original(path):
    checksum = path.with_name(path.name + '.sha256')
    expected = checksum.read_text().split()[0]
    if digest(path.read_bytes()) != expected:
        raise RuntimeError(f'Original release checksum mismatch: {path.name}')


def refresh_macos(original_zip, original_dmg, output_zip, output_dmg, docs):
    with tempfile.TemporaryDirectory() as temporary:
        staging = Path(temporary) / 'staging'
        staging.mkdir()
        subprocess.run(['ditto', '-x', '-k', str(original_zip), str(staging)], check=True)
        app = staging / 'SParamView.app'
        if not app.is_dir():
            raise RuntimeError('Missing SParamView.app in macOS ZIP')
        before = tree_payload(app)
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
        # The ZIP and DMG must contain the same original application.
        mounted = None
        try:
            raw = subprocess.check_output(['hdiutil', 'attach', '-readonly', '-nobrowse',
                                           '-plist', str(original_dmg)])
            entities = plistlib.loads(raw)['system-entities']
            mounted = next(Path(e['mount-point']) for e in entities if 'mount-point' in e)
            if tree_payload(mounted / 'SParamView.app') != before:
                raise RuntimeError('Original macOS ZIP and DMG application mismatch')
        finally:
            if mounted is not None:
                subprocess.run(['hdiutil', 'detach', str(mounted)], check=True)
        # Put documentation beside the signed app; its contents stay untouched.
        write_docs(staging, docs)
        if tree_payload(app) != before:
            raise RuntimeError('Signed macOS application changed')
        subprocess.run(['codesign', '--verify', '--deep', '--strict', str(app)], check=True)
        subprocess.run(['ditto', '-c', '-k', '--sequesterRsrc',
                        str(staging), str(output_zip)], check=True)
        subprocess.run(['hdiutil', 'create', '-volname', f'SParamView {VERSION}',
                        '-srcfolder', str(staging), '-ov', '-format', 'UDZO',
                        str(output_dmg)], check=True)
        # Reopen both output formats and verify the app and every document.
        verify_zip = Path(temporary) / 'verify-zip'
        subprocess.run(['ditto', '-x', '-k', str(output_zip), str(verify_zip)], check=True)
        if tree_payload(verify_zip / 'SParamView.app') != before:
            raise RuntimeError('Repacked macOS ZIP application mismatch')
        for name, data in docs.items():
            if (verify_zip / name).read_bytes() != data:
                raise RuntimeError(f'macOS ZIP documentation mismatch: {name}')
        mounted = None
        try:
            raw = subprocess.check_output(['hdiutil', 'attach', '-readonly', '-nobrowse',
                                           '-plist', str(output_dmg)])
            mounted = next(Path(e['mount-point']) for e in plistlib.loads(raw)['system-entities']
                           if 'mount-point' in e)
            if tree_payload(mounted / 'SParamView.app') != before:
                raise RuntimeError('Repacked macOS DMG application mismatch')
            for name, data in docs.items():
                if (mounted / name).read_bytes() != data:
                    raise RuntimeError(f'macOS DMG documentation mismatch: {name}')
            subprocess.run(['codesign', '--verify', '--deep', '--strict',
                            str(mounted / 'SParamView.app')], check=True)
        finally:
            if mounted is not None:
                subprocess.run(['hdiutil', 'detach', str(mounted)], check=True)
        return {'signed_app_unchanged': True, 'codesign_verification': 'PASS',
                'zip_and_dmg_application_match': True,
                'verified_file_count': len(before)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--originals', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    if f'project(SParamView VERSION {VERSION} ' not in (root / 'CMakeLists.txt').read_text():
        raise RuntimeError('This refresh is limited to the v1.2.1 release')
    docs = documentation(root)
    output = args.output.resolve()
    originals = args.originals.resolve()
    output.mkdir(parents=True, exist_ok=True)
    names = [f'SParamView_Windows_x64_v{VERSION}.zip',
             f'SParamView_Windows_ARM64_v{VERSION}.zip',
             f'SParamView_macOS_AppleSilicon_arm64_v{VERSION}.zip',
             f'SParamView_macOS_AppleSilicon_arm64_v{VERSION}.dmg']
    for name in names:
        verify_original(originals / name)
    audit = {'version': VERSION, 'documentation_commit': os.environ['GITHUB_SHA'],
             'documentation_updated_utc': datetime.now(timezone.utc).isoformat(),
             'application_rebuilt': False, 'packages': {}}
    for name in names[:2]:
        audit['packages'][name] = refresh_windows(originals / name, output / name, docs)
    mac_audit = refresh_macos(originals / names[2], originals / names[3],
                             output / names[2], output / names[3], docs)
    for name in names[2:]:
        audit['packages'][name] = mac_audit.copy()
    for name in names:
        old_sha = digest((originals / name).read_bytes())
        new_sha = digest((output / name).read_bytes())
        audit['packages'][name].update(original_sha256=old_sha, updated_sha256=new_sha)
        (output / (name + '.sha256')).write_text(f'{new_sha}  {name}\n')
    audit_name = f'SParamView_v{VERSION}_Documentation_Refresh.json'
    (output / audit_name).write_text(json.dumps(audit, indent=2) + '\n')
    status_name = f'SParamView_v{VERSION}_Release_Status_FINAL.json'
    status = json.loads((originals / status_name).read_text())
    status.update(documentation_commit=audit['documentation_commit'],
                  documentation_updated_utc=audit['documentation_updated_utc'],
                  application_binaries_unchanged=True, documentation_refresh_audit=audit_name)
    (output / status_name).write_text(json.dumps(status, indent=2) + '\n')
    for language in ('KO', 'EN'):
        shutil.copyfile(root / f'docs/USER_GUIDE_{language}.md',
                        output / f'SParamView_User_Guide_{language}_v{VERSION}.md')
    shutil.copyfile(root / 'README_FIRST.txt', output / f'README_FIRST_v{VERSION}.txt')
    print(json.dumps(audit, indent=2))


if __name__ == '__main__':
    main()
