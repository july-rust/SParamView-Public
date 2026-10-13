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
    if ref and (ref['object']['type'] != 'commit' or ref['object']['sha'] != commit):
        raise RuntimeError('Existing release tag points to another object; refusing replacement')
    if existing and not existing['draft']:
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

    output = Path('release-assets')
    output.mkdir(exist_ok=True)
    for arch in ('x64', 'ARM64'):
        folder = Path('package/windows-' + arch.lower())
        check_provenance(json.loads((folder/'build-provenance.json').read_text(encoding='utf-8-sig')), version, commit)
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
    print(final['html_url'])


if __name__ == '__main__':
    main()
