"""Run isolated production-entry-point probes; fatal exits are never GUI PASSes."""
import argparse, hashlib, json, os, pathlib, platform, subprocess, time
p = argparse.ArgumentParser()
p.add_argument('probe', type=pathlib.Path)
p.add_argument('output', type=pathlib.Path)
p.add_argument('--platform', default='offscreen')
a = p.parse_args()
a.probe = a.probe.resolve(); a.output = a.output.resolve()
a.output.mkdir(parents=True, exist_ok=True)
results = []

def run(mode, *, native=False, reuse=None):
    folder = reuse or a.output / mode
    folder.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.update(SPARAMVIEW_QA_SCENARIO=mode, SPARAMVIEW_QA_ROOT=str(folder),
               QT_QPA_PLATFORM=a.platform if native else 'offscreen')
    env.pop('SPARAMVIEW_QA_CLICK', None)
    if native: env['SPARAMVIEW_QA_CLICK'] = '1'
    if mode == 'qt-init': env['QT_QPA_PLATFORM'] = 'sparamview-nonexistent-qa'
    command = [str(a.probe)] + (['QA-invalid-file'] if mode == 'file-open' else [])
    started = time.monotonic()
    with (folder / (mode + '.log')).open('wb') as log:
        child = subprocess.Popen(command, env=env, stdout=log, stderr=subprocess.STDOUT)
        try:
            if mode == 'hold':
                deadline = time.monotonic() + 20
                while not (folder / 'ready').exists() and child.poll() is None and time.monotonic() < deadline:
                    time.sleep(.05)
                if not (folder / 'ready').exists(): raise RuntimeError('No readiness handshake')
                child.kill()
            code = child.wait(timeout=30)
        except Exception:
            child.kill(); child.wait(); raise
    text = (folder / (mode + '.log')).read_text(errors='replace')
    runtime_path = folder / 'runtime.json'
    runtime = json.loads(runtime_path.read_text()) if runtime_path.exists() else {}
    cache = pathlib.Path(runtime['cache']) / 'v3' if runtime else None
    remaining = sorted(str(x.relative_to(cache)) for x in cache.rglob('*')) if cache and cache.is_dir() else []
    if mode == 'normal': ok = code == 0 and not remaining
    elif mode == 'pre-app': ok = code == 1 and 'before GUI was available' in text
    elif mode == 'qt-init': ok = code != 0 and 'platform plugin' in text
    elif mode == 'fatal': ok = code != 0 and 'QA deliberate Qt fatal' in text and bool(remaining)
    elif mode == 'timer-unhandled': ok = code != 0 and 'QA deliberate unguarded timer exception' in text
    elif mode == 'hold': ok = code != 0 and bool(remaining)
    else: ok = code == 1 and 'SParamView startup failed:' in text and not remaining
    if mode == 'cache-block': ok = ok and cache.read_bytes() == b'QA SENTINEL'
    if native and mode not in ('normal',):
        dialog_path = folder / 'dialog.json'
        dialog = json.loads(dialog_path.read_text()) if dialog_path.exists() else {}
        ok = ok and dialog.get('valid', False) and runtime.get('platform') == a.platform
    results.append(dict(case=mode, status='PASS' if ok else 'FAIL', exit=code,
                        command=command, platform=runtime.get('platform'), native=native,
                        elapsed=time.monotonic()-started, remaining=remaining,
                        expected=('nonzero fatal/kill; recover next launch' if mode in ('fatal','timer-unhandled','hold','qt-init')
                                  else 'exit 0 and cleanup' if mode == 'normal' else 'exit 1; guarded failure'),
                        log=str(folder / (mode+'.log'))))
    if cache and cache.is_file(): cache.unlink() # remove only the exact fixture checked above
    return folder

try:
    for case in ['pre-app','qt-init','initialization','bad-alloc','cache-block','file-open','normal']:
        run(case)
    for case in ['fatal','timer-unhandled','hold']:
        folder = run(case)
        run('normal', reuse=folder)
    if a.platform not in ('offscreen','minimal'):
        for case in ['initialization','bad-alloc','cache-block','file-open','normal']:
            run(case, native=True, reuse=a.output / ('native-'+case))
finally:
    manifest = dict(os=platform.platform(), machine=platform.machine(),
                    source_commit=os.environ.get('GITHUB_SHA','local-working-tree'),
                    executable=str(a.probe), sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest(),
                    limits=['fault-injected bad_alloc, not actual OOM',
                            'timer-unhandled intentionally demonstrates unguarded callback abort',
                            'native Qt event injection, not physical desktop input'], results=results)
    (a.output/'results.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
for row in results: print(row['case'], row['native'], row['status'], row['exit'])
raise SystemExit(0 if results and all(r['status']=='PASS' for r in results) else 1)
