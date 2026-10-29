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
        check_provenance(json.loads((folder/'build-provenance.json').read_text(encoding='utf-8-sig')), version, commit)
        # Fail closed if an old flat layout leaks test/CLI binaries to the root.
        top_level_exes = sorted(p.name for p in folder.glob('*.exe'))
        if top_level_exes != ['SParamView.exe']:
            raise RuntimeError(f'Unexpected root executables in {arch}: {top_level_exes}')
        for relative in ('tools/si_cli.exe', 'tests/si_v130_touchstone_tests.exe'):
            if not (folder / relative).is_file():
                raise RuntimeError(f'Missing relocated Windows executable ({arch}): {relative}')
        target = output/f'SParamView_Windows_{arch}_v{version}.zip'
        with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for path in sorted(folder.rglob('*')):
                if path.is_file():
                    add_file(archive, path, path.relative_to(folder))
    import shutil
    for extension in ('zip', 'dmg'):
        name = f'SParamView_macOS_AppleSilicon_arm64_v{version}.{extension}'
        shutil.copyfile(Path('package/macos')/name, output/name)
    with zipfile.ZipFile(output/f'SParamView_macOS_AppleSilicon_arm64_v{version}.zip') as archive:
        check_provenance(json.loads(archive.read('SParamView.app/Contents/Resources/build-provenance.json')), version, commit)
    for language in ('EN', 'KO'):
        shutil.copyfile(f'docs/USER_GUIDE_{language}.md', output/f'SParamView_User_Guide_{language}_v{version}.md')
    shutil.copyfile('README_FIRST.txt', output/f'README_FIRST_v{version}.txt')
    with zipfile.ZipFile(output/f'SParamView_v{version}_Validation_Evidence.zip','w',zipfile.ZIP_DEFLATED) as archive:
        evidence = list(Path('package/evidence').rglob('*'))
        evidence += list(Path('docs/validation-v124').rglob('*'))
        for path in sorted(evidence):
            if path.is_file() and path.suffix.lower() in ('.json','.log','.txt','.md'):
                add_file(archive, path, path)
    hashes = {path.name: sha(path) for path in sorted(output.iterdir()) if path.is_file()}
    status = dict(application='SParamView', version=version, release_commit=commit,
                  tag=tag, workflow_run=os.environ['GITHUB_RUN_ID'],
                  platforms=['Windows x64','Windows ARM64','macOS Apple Silicon arm64'],
                  automated_platform_gates='PASS', gates=gates, asset_sha256=hashes,
                  limitations=['macOS ad-hoc signed; not notarized', 'Quick TDR remains Preview',
                               'Automated GUI tests do not establish physical display or instrument acceptance'])
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
    main()
