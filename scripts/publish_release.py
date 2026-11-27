"""Publish only validated packages for one exact commit; never clobber assets."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile


def sha(path):
    with open(path, 'rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def gh(*args, payload=None):
    command = ['gh', *args]
    if payload is not None:
        command += ['--input', '-']
    result = subprocess.run(command, input=None if payload is None else json.dumps(payload),
                            text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(result.stderr.strip())
    return result.stdout


def api(path, payload=None, missing=False):
    try:
        return json.loads(gh('api', path, payload=payload))
    except RuntimeError as error:
        if missing and 'HTTP 404' in str(error):
            return None
        raise


def add_file(archive, path, name):
    # Download/extraction times must not change same-run retry archive bytes.
    entry = zipfile.ZipInfo(str(name).replace('\\', '/'), (2000, 1, 1, 0, 0, 0))
    entry.compress_type = zipfile.ZIP_DEFLATED
    entry.external_attr = 0o100644 << 16
    archive.writestr(entry, path.read_bytes(), compresslevel=6)


def check_provenance(data, version, commit):
    if data.get('version') != version or data.get('commit') != commit:
        raise RuntimeError('Package provenance does not match release version and build commit')



def stage_windows_packages(folder, output, version, arch):
    """Same-source full QA archive and slim user archive, reproducible ZIPs."""
    folder, output = Path(folder), Path(output)
    output.mkdir(parents=True, exist_ok=True)
    required = ('SParamView.exe', 'tools/si_cli.exe', 'tests/si_v130_touchstone_tests.exe',
                'Run_Windows_Verification.cmd', 'platforms/qwindows.dll', 'build-provenance.json',
                'LICENSE', 'COPYRIGHT.txt')
    for name in required:
        if not (folder / name).is_file():
            raise RuntimeError('Missing Windows bundle member: ' + name)
    if sorted(p.name for p in folder.glob('*.exe')) != ['SParamView.exe']:
        raise RuntimeError('Unexpected Windows root executable')
    provenance = json.loads((folder/'build-provenance.json').read_text(encoding='utf-8-sig'))
    check_provenance(provenance, version, os.environ['GITHUB_SHA'])
    entries = []
    user = output/f'SParamView_Windows_{arch}_v{version}.zip'
    qa = output/f'SParamView_Windows_{arch}_QA_v{version}.zip'
    with zipfile.ZipFile(user, 'w') as u, zipfile.ZipFile(qa, 'w') as q:
        for path in sorted(folder.rglob('*')):
            if not path.is_file():
                continue
            rel = path.relative_to(folder)
            excluded = rel.parts[0] == 'tests' or rel.as_posix() == 'Run_Windows_Verification.cmd'
            entries.append(dict(path=rel.as_posix(), bytes=path.stat().st_size,
                                sha256=sha(path), classification='QA_ONLY' if excluded else 'RETAINED'))
            add_file(q, path, rel)
            if not excluded:
                add_file(u, path, rel)
    manifest = dict(version=version, commit=provenance['commit'], platform=arch,
                    files=entries, packages={p.name:dict(bytes=p.stat().st_size, sha256=sha(p))
                                             for p in (user, qa)},
                    full_uncompressed_bytes=sum(e['bytes'] for e in entries),
                    user_uncompressed_bytes=sum(e['bytes'] for e in entries if e['classification']=='RETAINED'))
    (output/'inventory.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    return manifest


def verify_windows_candidate(folder, version, commit, arch):
    folder = Path(folder)
    inventory = json.loads((folder/'inventory.json').read_text(encoding='utf-8'))
    check_provenance(inventory, version, commit)
    if inventory['platform'] != arch:
        raise RuntimeError('Wrong Windows candidate architecture')
    user = folder/f'SParamView_Windows_{arch}_v{version}.zip'
    qa = folder/f'SParamView_Windows_{arch}_QA_v{version}.zip'
    entries = {e['path']:e for e in inventory['files']}
    for package, full in ((user, False), (qa, True)):
        expected = inventory['packages'][package.name]
        if expected['sha256'] != sha(package) or expected['bytes'] != package.stat().st_size:
            raise RuntimeError('Windows package digest mismatch: '+package.name)
        wanted = {n for n,e in entries.items() if full or e['classification']=='RETAINED'}
        with zipfile.ZipFile(package) as z:
            if len(z.namelist()) != len(wanted) or set(z.namelist()) != wanted:
                raise RuntimeError('Windows package member mismatch: '+package.name)
            for name in wanted:
                if hashlib.sha256(z.read(name)).hexdigest() != entries[name]['sha256']:
                    raise RuntimeError('Windows member digest mismatch: '+name)
            check_provenance(json.loads(z.read('build-provenance.json').decode('utf-8-sig')), version, commit)
            if not full and any(n.startswith('tests/') or n=='Run_Windows_Verification.cmd' for n in wanted):
                raise RuntimeError('QA files leaked into user package')
            if 'SParamView.exe' not in wanted or 'tools/si_cli.exe' not in wanted:
                raise RuntimeError('Required user executable missing')
    return user, qa


def smoke_windows_candidate(folder, version, arch, evidence):
    """Run the exact user ZIP, without SDK/build-tree Qt search paths."""
    import platform
    import shutil
    folder, evidence = Path(folder).resolve(), Path(evidence).resolve()
    evidence.mkdir(parents=True, exist_ok=True)
    user, _ = verify_windows_candidate(folder, version, os.environ['GITHUB_SHA'], arch)
    machine = platform.machine().lower()
    expected = {'x64':('amd64','x86_64'), 'ARM64':('arm64','aarch64')}[arch]
    if os.name != 'nt' or machine not in expected:
        raise RuntimeError('Native Windows architecture required: '+machine)
    with tempfile.TemporaryDirectory(prefix='sparamview-user-') as temp:
        root = Path(temp)/'app'
        with zipfile.ZipFile(user) as archive:
            archive.extractall(root)
        env = {k:v for k,v in os.environ.items() if not k.upper().startswith(('QT_', 'QML'))}
        system = Path(os.environ['SystemRoot'])
        env['PATH'] = os.pathsep.join(map(str,(root, system/'System32',system)))
        env['QT_QPA_PLATFORM'] = 'windows'
        env['LOCALAPPDATA'] = str(Path(temp)/'local')
        env['APPDATA'] = str(Path(temp)/'roaming')
        env['USERPROFILE'] = str(Path(temp)/'profile')
        for name in ('LOCALAPPDATA','APPDATA','USERPROFILE'):
            Path(env[name]).mkdir()
        command = [str(root/'SParamView.exe'), '--selftest', str(root/'examples'), str(evidence/'selftest')]
        with (evidence/'user-smoke.log').open('wb') as log:
            result = subprocess.run(command, cwd=root, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=180)
        receipt = dict(commit=os.environ['GITHUB_SHA'], version=version, platform=arch, machine=machine,
                       package=user.name, sha256=sha(user), exit=result.returncode,
                       test='native windows platform, extracted user ZIP, SDK paths removed')
        (evidence/'user-package-smoke.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
        if result.returncode:
            raise RuntimeError('User ZIP selftest failed')


def write_validation_evidence(destination, roots):
    # Retain executable evidence, including GUI screenshots and CSV inputs/
    # outputs. Disposable office profiles are not verification evidence.
    extensions = {'.json', '.log', '.txt', '.md', '.png', '.csv', '.tsv', '.ods', '.sha256'}
    with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED) as archive:
        for root in roots:
            for path in sorted(root.rglob('*')):
                if (path.is_file() and path.suffix.lower() in extensions
                        and 'profile' not in path.relative_to(root).parts):
                    add_file(archive, path, path)


def retire_v124_after_v124a_published(endpoint, repo, version, commit, expected_assets):
    """Remove the superseded official release only after complete v1.2.4a publication.

    The exact old tag/commit is pinned to avoid ever deleting an unrelated
    release. This is intentionally not called for draft/incomplete packages.
    """
    if version != '1.2.4a' or repo != 'july-rust/SParamView-Public':
        return
    replacement = api(f'{endpoint}/releases/tags/v1.2.4a')
    new_ref = api(f'{endpoint}/git/ref/tags/v1.2.4a')
    remote_names = {asset['name'] for asset in replacement.get('assets', [])}
    if (replacement.get('draft') or replacement.get('prerelease')
            or replacement.get('tag_name') != 'v1.2.4a'
            or replacement.get('target_commitish') != commit
            or new_ref.get('object', {}).get('sha') != commit
            or remote_names != set(expected_assets)):
        raise RuntimeError('Refusing to remove v1.2.4: replacement release not fully verified')

    old = api(f'{endpoint}/releases/tags/v1.2.4', missing=True)
    old_ref = api(f'{endpoint}/git/ref/tags/v1.2.4', missing=True)
    if old and (old.get('id') != 411355784 or old.get('tag_name') != 'v1.2.4'):
        raise RuntimeError('Refusing to delete an unexpected legacy release')
    if old_ref and (old_ref.get('object', {}).get('type') != 'commit'
                    or old_ref['object']['sha'] !=
                    '92fe88c83fd1171c8171b04660c95bfebc723264'):
        raise RuntimeError('Refusing to delete a legacy tag that moved')
    if old:
        gh('api', '--method', 'DELETE', f'{endpoint}/releases/{old["id"]}')
    if old_ref:
        gh('api', '--method', 'DELETE', f'{endpoint}/git/refs/tags/v1.2.4')
    if (api(f'{endpoint}/releases/tags/v1.2.4', missing=True)
            or api(f'{endpoint}/git/ref/tags/v1.2.4', missing=True)):
        raise RuntimeError('Legacy v1.2.4 removal not complete')


def validated_ci_only_v124a(endpoint, repo, version, existing, ref, commit):
    """Keep the published v1.2.4a tag/assets untouched for CI-only fixes.

    A new program binary requires a NEW release version. The compare endpoint
    must prove that no runtime, algorithm, packaging or user-document files
    changed since the exact published commit.
    """
    old_commit = '139b05fb112aec3aba4637a32eec48d1cc796672'
    if (repo != 'july-rust/SParamView-Public' or version != '1.2.4a'
            or not existing or existing.get('id') != 411430037
            or existing.get('tag_name') != 'v1.2.4a'
            or existing.get('target_commitish') != old_commit
            or not ref or ref.get('object', {}).get('type') != 'commit'
            or ref.get('object', {}).get('sha') != old_commit):
        return False
    comparison = api(f'{endpoint}/compare/{old_commit}...{commit}')
    allowed = {
        '.github/workflows/release-3platform.yml',
        'tests/open_tests.cpp',
        'scripts/publish_release.py',
        'tests/publish_release_tests.py',
        # Repository-only current-version documentation; existing release
        # attachments (including guides, checksums and binaries) remain pinned.
        'README.md',
        'README_EN.md',
        'README_KO.md',
        'docs/USER_GUIDE_EN.md',
        'docs/USER_GUIDE_KO.md',
        'docs/Revision_History.md',
        'docs/Release_1.2.4a.md',
        'docs/images/README.md',
    }
    changes = comparison.get('files', [])
    return (comparison.get('status') == 'ahead' and bool(changes)
            and all(item.get('status') == 'modified'
                    and item.get('filename') in allowed for item in changes)
            and len(changes) == len({item['filename'] for item in changes}))



def validated_docs_only_v130(endpoint, repo, version, existing, ref, commit):
    """Allow strictly repository-only documentation maintenance for a tagged v1.3.0.

    This preserves the published tag, build commit, executable assets and SHA-256
    evidence. Documentation in main may be clarified without silently
    replacing already-released files.
    """
    released_commit = 'dd4354367c49dc25912ed83bbeb41b284e29f616'
    if (repo != 'july-rust/SParamView-Public' or version != '1.3.0'
            or not existing or existing.get('id') != 427408966
            or existing.get('draft') or existing.get('prerelease')
            or existing.get('tag_name') != 'v1.3.0'
            or existing.get('target_commitish') != released_commit
            or not ref or ref.get('object', {}).get('type') != 'commit'
            or ref.get('object', {}).get('sha') != released_commit):
        return False
    comparison = api(f'{endpoint}/compare/{released_commit}...{commit}')
    allowed = {
        'README.md', 'README_EN.md', 'README_KO.md', 'README_FIRST.txt',
        'docs/USER_GUIDE_EN.md', 'docs/USER_GUIDE_KO.md',
        'docs/Revision_History.md', 'docs/Release_1.3.0.md',
        'docs/Temporary_Cache_Cleanup.md', 'docs/images/README.md',
        # This one-time publication-guard implementation and its tests may
        # differ from the already-published immutable source tag.
        'scripts/publish_release.py', 'tests/publish_release_tests.py',
    }
    files = comparison.get('files', [])
    return (comparison.get('status') == 'ahead' and bool(files)
            and all(item.get('status') == 'modified'
                    and item.get('filename') in allowed for item in files)
            and len(files) == len({item['filename'] for item in files}))



def validated_ci_only_v131(endpoint, repo, version, existing, ref, commit):
    """Validate repository-only release documentation gate maintenance.

    A tagged v1.3.1 release is immutable. Only the specific CI check, its
    regression test, and this publication guard may change on main without
    introducing a new application version. No executable or release asset
    may be replaced.
    """
    released_commit = '54f0fd40173dd60f452096832adcfb736d21b4f1'
    if (repo != 'july-rust/SParamView-Public' or version != '1.3.1'
            or not existing or existing.get('id') != 427525263
            or existing.get('draft') or existing.get('prerelease')
            or existing.get('tag_name') != 'v1.3.1'
            or existing.get('target_commitish') != released_commit
            or not ref or ref.get('object', {}).get('type') != 'commit'
            or ref.get('object', {}).get('sha') != released_commit):
        return False
    comparison = api(f'{endpoint}/compare/{released_commit}...{commit}')
    allowed = {
        '.github/workflows/release-3platform.yml',
        'scripts/publish_release.py',
        'tests/publish_release_tests.py',
    }
    files = comparison.get('files', [])
    return (comparison.get('status') == 'ahead' and bool(files)
            and all(item.get('status') == 'modified'
                    and item.get('filename') in allowed for item in files)
            and len(files) == len({item['filename'] for item in files}))

def main():
    version, commit, repo = (os.environ[k] for k in
                             ('SPARAMVIEW_VERSION', 'GITHUB_SHA', 'GITHUB_REPOSITORY'))
    if os.environ.get('GITHUB_REF') != 'refs/heads/main' or os.environ.get('GITHUB_EVENT_NAME') != 'push':
        raise RuntimeError('Official publication requires a validated main push')
    actual = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    if actual != commit:
        raise RuntimeError('Checkout and build commit differ')
    tag = 'v' + version
    endpoint = f'repos/{repo}'
    existing = api(f'{endpoint}/releases/tags/{tag}', missing=True)
    ref = api(f'{endpoint}/git/ref/tags/{tag}', missing=True)
    if ref and (ref['object']['type'] != 'commit' or ref['object']['sha'] != commit) and (not existing or existing['draft']):
        raise RuntimeError('Existing release tag points to another object; refusing replacement')
    ci_only_maintenance = False
    if existing and not existing['draft']:
        ci_only_maintenance = (
            validated_ci_only_v124a(endpoint, repo, version, existing, ref, commit)
            or validated_docs_only_v130(endpoint, repo, version,
                                        existing, ref, commit)
            or validated_ci_only_v131(endpoint, repo, version,
                                      existing, ref, commit)
        )
        if not ci_only_maintenance:
            raise RuntimeError('This official release already exists; use a new version. No asset was changed.')
    if existing and not ref:
        raise RuntimeError('Existing draft has no pinned tag; manual inspection required')

    jobs = api(f'{endpoint}/actions/runs/{os.environ["GITHUB_RUN_ID"]}/jobs?per_page=100')['jobs']
    required = ['Version and release consistency', 'Core quality gates', 'Windows x64',
                'Windows ARM64', 'Native Windows ARM64 packaged-binary gate', 'macOS Apple Silicon arm64']
    gates = {}
    for name in required:
        matching = [job for job in jobs if job['name'] == name]
        if len(matching) != 1 or matching[0]['conclusion'] != 'success':
            raise RuntimeError('Required gate did not succeed: ' + name)
        gates[name] = dict(conclusion='success', url=matching[0]['html_url'])

    if ci_only_maintenance:
        print('Validated repository-only maintenance of a published SParamView release. '
              'Existing tag, release ID, source commit and SHA-256 assets remain unchanged.')
        return

    output = Path('release-assets')
    output.mkdir(exist_ok=True)
    for arch in ('x64', 'ARM64'):
        folder = Path('package/windows-' + arch.lower())
        user, qa = verify_windows_candidate(folder, version, commit, arch)
        evidence_root = Path('package/evidence')
        receipts = [json.loads(p.read_text(encoding='utf-8')) for p in evidence_root.rglob('user-package-smoke.json')]
        matching = [r for r in receipts if r.get('platform') == arch]
        if len(matching) != 1 or matching[0].get('commit') != commit or matching[0].get('version') != version or matching[0].get('sha256') != sha(user) or matching[0].get('exit') != 0:
            raise RuntimeError('Missing exact user ZIP native execution receipt: '+arch)
        import shutil
        shutil.copyfile(user, output/user.name)
        shutil.copyfile(qa, output/qa.name)
    import shutil
    mac_receipts = list(Path('package/evidence').rglob('mac-user-package-smoke.json'))
    if len(mac_receipts) != 1:
        raise RuntimeError('Missing unique macOS ZIP/DMG native execution receipt')
    mac = json.loads(mac_receipts[0].read_text(encoding='utf-8'))
    check_provenance(mac, version, commit)
    if mac.get('machine') != 'arm64' or mac.get('zip_exit') != 0 or mac.get('dmg_exit') != 0:
        raise RuntimeError('macOS ZIP/DMG native execution failed')
    for extension in ('zip', 'dmg'):
        name = f'SParamView_macOS_AppleSilicon_arm64_v{version}.{extension}'
        source = Path('package/macos')/name
        expected = mac['packages'][name]
        if expected['sha256'] != sha(source) or expected['bytes'] != source.stat().st_size:
            raise RuntimeError('macOS package receipt mismatch: '+name)
        shutil.copyfile(source, output/name)
    with zipfile.ZipFile(output/f'SParamView_macOS_AppleSilicon_arm64_v{version}.zip') as archive:
        check_provenance(json.loads(archive.read('SParamView.app/Contents/Resources/build-provenance.json')), version, commit)
    for language in ('EN', 'KO'):
        shutil.copyfile(f'docs/USER_GUIDE_{language}.md', output/f'SParamView_User_Guide_{language}_v{version}.md')
    shutil.copyfile('README_FIRST.txt', output/f'README_FIRST_v{version}.txt')
    write_validation_evidence(output/f'SParamView_v{version}_Validation_Evidence.zip',
                              [Path('package/evidence'), Path('docs/validation-v124')])
    hashes = {path.name: sha(path) for path in sorted(output.iterdir()) if path.is_file()}
    status = dict(application='SParamView', version=version, release_commit=commit,
                  tag=tag, workflow_run=os.environ['GITHUB_RUN_ID'],
                  platforms=['Windows x64','Windows ARM64','macOS Apple Silicon arm64'],
                  automated_platform_gates='PASS', gates=gates, asset_sha256=hashes,
                  limitations=['macOS ad-hoc signed; not notarized', 'Quick TDR remains Preview',
                               'Excel desktop round-trip UNVERIFIED: execution environment unavailable',
                               'Windows/macOS Calc interactive import/save dialogs UNVERIFIED; see per-platform results for API round-trip',
                               'Injected bad_alloc is not real OOM; arbitrary callback exceptions are not exhaustively verified',
                               'Automated GUI tests do not establish physical display or instrument acceptance',
                               'COM deferred; generic ILD/ICN and simulated NRZ Eye are engineering references, not compliance or measured BER',
                               'Private S36P final-package workflows, exhaustive HiDPI, long-duration memory and runtime localization inventory not certified'])
    status_path = output/f'SParamView_v{version}_Release_Status_FINAL.json'
    status_path.write_text(json.dumps(status, indent=2)+'\n', encoding='utf-8')
    for path in list(output.iterdir()):
        path.with_name(path.name+'.sha256').write_text(f'{sha(path)}  {path.name}\n')
    if not ref:
        api(f'{endpoint}/git/refs', dict(ref='refs/tags/'+tag,sha=commit))
    if not existing:
        existing = api(f'{endpoint}/releases',dict(tag_name=tag,target_commitish=commit,
            name=f'SParamView {tag} — Windows x64 + Windows ARM64 + macOS Apple Silicon',
            body=Path(f'docs/Release_{version}.md').read_text(encoding='utf-8'),draft=True,prerelease=False))
    assets = {item['name']:item for item in existing['assets']}
    for path in sorted(output.iterdir()):
        if path.name in assets:
            with tempfile.TemporaryDirectory() as temp:
                gh('release','download',tag,'--repo',repo,'--pattern',path.name,'--dir',temp)
                if sha(Path(temp)/path.name) != sha(path):
                    raise RuntimeError('Retry asset differs; draft retained without overwriting: '+path.name)
        else:
            gh('release','upload',tag,'--repo',repo,str(path))
    final = api(f'{endpoint}/releases/{existing["id"]}')
    remote = {item['name']:item for item in final['assets']}
    for path in output.iterdir():
        item = remote.get(path.name)
        if not item or item['size'] != path.stat().st_size or item.get('digest') != 'sha256:'+sha(path):
            raise RuntimeError('Uploaded asset verification failed: '+path.name)
    if api(f'{endpoint}/git/ref/tags/{tag}')['object']['sha'] != commit:
        raise RuntimeError('Tag moved during publication')
    gh('api','--method','PATCH',f'{endpoint}/releases/{existing["id"]}',
       payload=dict(draft=False,prerelease=False,make_latest='true'))
    retire_v124_after_v124a_published(endpoint, repo, version, commit,
                                     {path.name for path in output.iterdir()})
    print(final['html_url'])


if __name__ == '__main__':
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == '--stage-windows':
        stage_windows_packages(Path(sys.argv[2]), Path(sys.argv[3]), sys.argv[4], sys.argv[5])
    elif len(sys.argv) > 1 and sys.argv[1] == '--smoke-windows':
        smoke_windows_candidate(Path(sys.argv[2]), sys.argv[3], sys.argv[4], Path(sys.argv[5]))
    else:
        main()
