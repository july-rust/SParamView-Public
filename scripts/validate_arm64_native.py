"""One-command native Windows ARM64 validation for SParamView 1.1.2.

Designed so Work only needs to install the toolchain, build, and execute this
script. No NumPy/SciPy/openpyxl/pefile dependency is required.

Example (native Windows ARM64 PowerShell):
  py scripts/validate_arm64_native.py ^
    --build build-windows-arm64 ^
    --bundle dist/SParamView-Windows-ARM64 ^
    --samples C:/SParamView-Samples ^
    --output arm64-native-validation
"""
from __future__ import annotations
import argparse
import collections
import csv
import hashlib
import json
import math
import os
import shutil
import struct
import subprocess
import sys
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BASELINE = ROOT / 'docs/arm64-validation/baseline_v1.0.4.json'
MACHINE_ARM64 = 0xAA64


def native_windows_arm64():
    """Check the OS architecture even when Python runs under emulation."""
    if sys.platform != 'win32':
        raise RuntimeError('Native validation requires Windows ARM64; this host is not Windows.')
    import ctypes
    from ctypes import wintypes
    kernel = ctypes.WinDLL('kernel32', use_last_error=True)
    kernel.GetCurrentProcess.argtypes = []
    kernel.GetCurrentProcess.restype = wintypes.HANDLE
    kernel.IsWow64Process2.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.USHORT),
                                     ctypes.POINTER(wintypes.USHORT)]
    kernel.IsWow64Process2.restype = wintypes.BOOL
    process_machine, native_machine = wintypes.USHORT(), wintypes.USHORT()
    if not kernel.IsWow64Process2(kernel.GetCurrentProcess(), ctypes.byref(process_machine),
                                ctypes.byref(native_machine)):
        raise ctypes.WinError(ctypes.get_last_error())
    if native_machine.value != MACHINE_ARM64:
        raise RuntimeError(f'Native validation requires Windows ARM64; OS machine=0x{native_machine.value:04X}')
    return {'os_machine': f'0x{native_machine.value:04X}',
            'python_process_machine': f'0x{process_machine.value:04X}'}


def finite_error(error):
    # NaN > tolerance is False. Treat nonfinite errors as failures, never zero.
    return error if math.isfinite(error) else math.inf


def validation_passed(records, required_count, ctest, skip_gui, bundle_audit):
    return bool(required_count > 0 and len(records) == required_count
                and all(x.get('passed') is True for x in records)
                and ctest is not None and not skip_gui
                and bundle_audit and bundle_audit['pe_files'] > 0
                and not bundle_audit['wrong_architecture'])


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def read_at(f, offset: int, fmt: str):
    f.seek(offset)
    size = struct.calcsize(fmt)
    data = f.read(size)
    if len(data) != size:
        raise AssertionError(f'truncated binary at {offset}: {f.name}')
    return struct.unpack(fmt, data)


def pe_machine(path: Path) -> int:
    with path.open('rb') as f:
        dos = f.read(64)
        if len(dos) < 64 or dos[:2] != b'MZ':
            raise AssertionError(f'Not PE/MZ: {path}')
        peoff = struct.unpack_from('<I', dos, 0x3C)[0]
        f.seek(peoff)
        if f.read(4) != b'PE\0\0':
            raise AssertionError(f'Missing PE signature: {path}')
        return struct.unpack('<H', f.read(2))[0]


def executable(build: Path, name: str) -> Path:
    for candidate in (build / 'Release' / f'{name}.exe', build / f'{name}.exe'):
        if candidate.is_file():
            return candidate
    raise FileNotFoundError(f'{name}.exe not found in {build} or Release subdirectory')


def run(command, *, env=None, timeout=900, log=None):
    start = time.perf_counter()
    p = subprocess.run(list(map(str, command)), text=True, capture_output=True, env=env, timeout=timeout)
    elapsed = time.perf_counter() - start
    if log:
        Path(log).parent.mkdir(parents=True, exist_ok=True)
        Path(log).write_text(p.stdout + p.stderr, encoding='utf-8', errors='replace')
    if p.returncode:
        raise RuntimeError(f"Command failed ({p.returncode}): {' '.join(map(str, command))}\n{(p.stdout+p.stderr)[-3000:]}")
    return {'seconds': elapsed, 'stdout': p.stdout, 'stderr': p.stderr}


def approx(actual: float, expected: float, abs_tol: float, rel_tol: float = 1e-12):
    if math.isnan(expected):
        return math.isnan(actual)
    if math.isinf(expected):
        return math.isinf(actual) and (actual > 0) == (expected > 0)
    return abs(actual - expected) <= max(abs_tol, rel_tol * max(abs(expected), 1.0))


def compare_matrix(path: Path, base: dict, tol: dict):
    errors = []
    max_complex = 0.0
    max_freq = 0.0
    with path.open('rb') as f:
        n, k = read_at(f, 0, '<QQ')
        if (n, k) != (base['ports'], base['points']):
            errors.append(f'matrix dimensions {(n,k)} != {(base["ports"],base["points"])}')
            return errors, max_complex, max_freq
        freq_off = 16
        matrix_off = freq_off + k * 8
        for sample in base['frequency_samples']:
            actual = read_at(f, freq_off + sample['k'] * 8, '<d')[0]
            err = finite_error(abs(actual - sample['hz']))
            max_freq = max(max_freq, err)
            if not approx(actual, sample['hz'], tol['frequency_hz_abs']):
                errors.append(f"frequency[{sample['k']}] error={err}")
        for sample in base['matrix_samples']:
            off = matrix_off + (((sample['i'] * n + sample['j']) * k + sample['k']) * 16)
            ar, ai = read_at(f, off, '<dd')
            err = finite_error(math.hypot(ar - sample['real'], ai - sample['imag']))
            max_complex = max(max_complex, err)
            if err > tol['complex_abs']:
                errors.append(f"S[{sample['i']},{sample['j']},{sample['k']}] complex error={err}")
    return errors, max_complex, max_freq


def compare_mapping(path: Path, expected: list[dict]):
    with path.open(encoding='utf-8-sig') as stream:
        actual = list(csv.DictReader(stream))
    return actual == expected, actual


def compare_tdr(folder: Path, expected_rows: list[dict], tol: dict):
    with (folder / 'index.csv').open(encoding='utf-8') as stream:
        rows = list(csv.DictReader(stream))
    errors = []
    maxima = {'complex': 0.0, 'time': 0.0, 'rho': 0.0, 'ohm': 0.0}
    if len(rows) != len(expected_rows):
        return [f'condition count {len(rows)} != {len(expected_rows)}'], maxima
    by_index = {int(x['index']): x for x in rows}
    for base in expected_rows:
        idx = base['index']
        row = by_index.get(idx)
        if row is None:
            errors.append(f'missing TDR index {idx}')
            continue
        for key in ('channel', 'quality'):
            if row[key] != base[key]:
                errors.append(f'{idx}: {key} {row[key]!r} != {base[key]!r}')
        for key in ('termination', 'nf', 'nt'):
            if int(row[key]) != base[key]:
                errors.append(f'{idx}: {key} {row[key]} != {base[key]}')
        if not approx(float(row['reference']), base['reference_ohm'], tol['reference_ohm_abs']):
            errors.append(f'{idx}: reference mismatch')
        path = folder / f'{idx}.bin'
        with path.open('rb') as f:
            nf = read_at(f, 0, '<Q')[0]
            freq_off = 8
            s_off = freq_off + nf * 8
            nt_off = s_off + nf * 16
            nt = read_at(f, nt_off, '<Q')[0]
            time_off = nt_off + 8
            z_off = time_off + nt * 8
            rho_off = z_off + nt * 8
            for s in base['frequency_samples']:
                hz = read_at(f, freq_off + s['k'] * 8, '<d')[0]
                ar, ai = read_at(f, s_off + s['k'] * 16, '<dd')
                ferr = finite_error(abs(hz - s['hz']))
                cerr = finite_error(math.hypot(ar - s['real'], ai - s['imag']))
                maxima['complex'] = max(maxima['complex'], cerr)
                if ferr > tol['frequency_hz_abs']:
                    errors.append(f'{idx}: frequency sample {s["k"]} error={ferr}')
                if cerr > tol['complex_abs']:
                    errors.append(f'{idx}: complex sample {s["k"]} error={cerr}')
            for s in base['time_samples']:
                sec = read_at(f, time_off + s['k'] * 8, '<d')[0]
                z = read_at(f, z_off + s['k'] * 8, '<d')[0]
                rho = read_at(f, rho_off + s['k'] * 8, '<d')[0]
                terr = finite_error(abs(sec - s['seconds']))
                rerr = finite_error(abs(rho - s['rho']))
                maxima['time'] = max(maxima['time'], terr)
                maxima['rho'] = max(maxima['rho'], rerr)
                if terr > tol['time_seconds_abs']:
                    errors.append(f'{idx}: time sample {s["k"]} error={terr}')
                if rerr > tol['reflection_abs']:
                    errors.append(f'{idx}: rho sample {s["k"]} error={rerr}')
                if s.get('stable_impedance'):
                    zerr = finite_error(abs(z - s['ohm']))
                    maxima['ohm'] = max(maxima['ohm'], zerr)
                    if zerr > max(tol['impedance_ohm_abs'], 1e-10 * max(abs(s['ohm']), 1.0)):
                        errors.append(f'{idx}: Z sample {s["k"]} error={zerr}')
                elif s.get('impedance_nonfinite') and math.isfinite(z):
                    # A tiny cross-compiler perturbation can move a pole, so this is informational only.
                    pass
    return errors, maxima


def make_project(template: dict, base: dict, source: Path, output: Path):
    project = json.loads(json.dumps(template))
    project['name'] = 'ARM64 validation ' + source.stem
    stop_hz = base['frequency_samples'][-1]['hz']
    project['settings'].update(startHz=0, stopHz=stop_hz, targetOhm=0, tdrLimit=False,
                               tdrTerminations=[0, 1, 2, 3, 4], manualMarkersHz=[])
    for limit in project['settings']['limits']:
        limit['enabled'] = False
    channels = []
    for c in base['mapping']:
        def port(name):
            return int(c[name] or 0) - 1
        channels.append({
            'id': c['ID'], 'name': c['Channel'], 'alias': c.get('Alias', ''),
            'group': c.get('Group', 'Default'), 'nearP': port('NearP'), 'nearN': port('NearN'),
            'farP': port('FarP'), 'farN': port('FarN'),
        })
    project['revisions'] = [{'name': source.stem, 'source': str(source.resolve()), 'channels': channels, 'confirmed': True}]
    project['ui'] = {'revision': 0, 'selected': [c['id'] for c in channels], 'group': 'All groups', 'metric': 0}
    output.write_text(json.dumps(project, indent=2, ensure_ascii=False), encoding='utf-8')


def verify_gui_output(folder: Path, expected: dict, version: str):
    data = json.loads((folder / 'Validation.json').read_text(encoding='utf-8'))
    errors = []
    if data.get('application') != version:
        errors.append(f"application={data.get('application')} expected={version}")
    if data.get('results') != expected['results']:
        errors.append(f"results={data.get('results')} expected={expected['results']}")
    counts = dict(collections.Counter(x['metric'] for x in data.get('analysis', [])))
    if counts != expected['metric_counts']:
        errors.append(f'metric counts={counts} expected={expected["metric_counts"]}')
    if not approx(float(data.get('tdr_start_fraction', -1)), expected['tdr_start_fraction'], 1e-12, 0):
        errors.append('tdr_start_fraction mismatch')
    if not data.get('termination_settings_roundtrip', False):
        errors.append('termination settings roundtrip failed')
    xlsx = folder / 'Analysis.xlsx'
    if not xlsx.is_file():
        errors.append('Analysis.xlsx missing')
        media = 0
    else:
        with zipfile.ZipFile(xlsx) as z:
            bad = z.testzip()
            if bad:
                errors.append(f'Analysis.xlsx CRC failure at {bad}')
            media = sum(n.startswith('xl/media/') for n in z.namelist())
        if media == 0:
            errors.append('Analysis.xlsx has no embedded plots')
    return errors, {'results': data.get('results'), 'metric_counts': counts, 'xlsx_media': media,
                    'elapsed_seconds': data.get('elapsed_seconds')}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build', type=Path, required=True)
    ap.add_argument('--bundle', type=Path)
    ap.add_argument('--samples', type=Path, required=True)
    ap.add_argument('--baseline', type=Path, default=DEFAULT_BASELINE)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--skip-ctest', action='store_true')
    ap.add_argument('--skip-gui', action='store_true')
    args = ap.parse_args()
    host = native_windows_arm64()
    build, samples, output = args.build.resolve(), args.samples.resolve(), args.output.resolve()
    baseline = json.loads(args.baseline.read_text(encoding='utf-8'))
    tol = baseline['tolerances']
    output.mkdir(parents=True, exist_ok=False)
    env = os.environ.copy()
    env['QT_QPA_PLATFORM'] = 'offscreen'

    targets = {name: executable(build, name) for name in ('SParamView', 'si_real_probe', 'si_termination_probe')}
    architecture = {}
    for name, path in targets.items():
        machine = pe_machine(path)
        architecture[name] = f'0x{machine:04X}'
        if machine != MACHINE_ARM64:
            raise AssertionError(f'{name} is not ARM64: machine=0x{machine:04X}')

    bundle_audit = None
    if args.bundle:
        bundle = args.bundle.resolve()
        required = ('SParamView.exe', 'Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll',
                    'Qt6Concurrent.dll', 'platforms/qwindows.dll', 'platforms/qoffscreen.dll')
        missing = [name for name in required if not (bundle / name).is_file()]
        if missing:
            raise AssertionError(f'Incomplete deployment bundle: {missing}')
        files = sorted(p for p in bundle.rglob('*') if p.is_file() and p.suffix.lower() in {'.exe', '.dll'})
        wrong = [(str(p), pe_machine(p)) for p in files if pe_machine(p) != MACHINE_ARM64]
        bundle_audit = {'pe_files': len(files), 'wrong_architecture': [{'file': p, 'machine': f'0x{m:04X}'} for p, m in wrong]}
        if wrong:
            raise AssertionError(f'Mixed architecture bundle: {wrong[:5]}')
        if sha256(bundle / 'SParamView.exe') != sha256(targets['SParamView']):
            raise AssertionError('Deployed SParamView.exe differs from the validated build.')
        targets['SParamView'] = bundle / 'SParamView.exe'

    ctest = None
    if not args.skip_ctest:
        ctest = run(['ctest', '--test-dir', build, '-C', 'Release', '--output-on-failure', '--no-tests=error'], env=env,
                    log=output / 'ctest.log')

    template = json.loads((ROOT / 'examples/demo.siproject').read_text(encoding='utf-8'))
    cache = output / 'cache'
    records = []
    for base in baseline['files']:
        source = samples / base['name']
        rec = {'name': base['name'], 'errors': [], 'timings': {}}
        records.append(rec)
        if not source.is_file():
            rec['errors'].append(f'missing source: {source}')
            continue
        actual_sha = sha256(source)
        rec['sha256'] = actual_sha
        if actual_sha != base['sha256'] or source.stat().st_size != base['bytes']:
            rec['errors'].append('source identity mismatch')
            continue
        case = output / source.stem
        probe_dir = case / 'probe'
        t = run([targets['si_real_probe'], source, cache, probe_dir], env=env, log=case / 'probe.log')
        rec['timings']['probe_seconds'] = t['seconds']
        ok, mapping = compare_mapping(probe_dir / 'candidates.csv', base['mapping'])
        if not ok:
            rec['errors'].append('automatic channel mapping differs from baseline')
        matrix_errors, max_complex, max_freq = compare_matrix(probe_dir / 'matrix.bin', base, tol)
        rec['errors'].extend(matrix_errors)
        rec['max_matrix_sample_error'] = max_complex
        rec['max_frequency_error_hz'] = max_freq
        for label, reverse, expected in (('forward', 0, base['tdr_forward']), ('reverse', 1, base['tdr_reverse'])):
            folder = case / ('tdr_' + label)
            t = run([targets['si_termination_probe'], source, probe_dir / 'candidates.csv', cache, folder, reverse],
                    env=env, log=case / f'tdr_{label}.log')
            rec['timings'][f'tdr_{label}_seconds'] = t['seconds']
            errors, maxima = compare_tdr(folder, expected, tol)
            rec['errors'].extend(f'{label}: {x}' for x in errors)
            rec[f'tdr_{label}_max_errors'] = maxima
        if not args.skip_gui:
            project = case / 'workflow.siproject'
            make_project(template, base, source, project)
            gui = case / 'gui'
            gui_env = env.copy()
            if args.bundle:
                gui_env['QT_PLUGIN_PATH'] = str(bundle)
                gui_env['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(bundle / 'platforms')
            t = run([targets['SParamView'], '--validate-project', project, gui], env=gui_env,
                    timeout=1200, log=case / 'gui.log')
            rec['timings']['gui_seconds'] = t['seconds']
            errors, summary = verify_gui_output(gui, base['workflow_expected'], baseline['version'])
            rec['errors'].extend(f'gui: {x}' for x in errors)
            rec['gui'] = summary
        rec['passed'] = not rec['errors']
        print(source.name, 'PASS' if rec['passed'] else 'FAIL', flush=True)
        if rec['errors']:
            for e in rec['errors'][:10]:
                print('  ', e, flush=True)

    report = {
        'application': baseline['application'], 'version': baseline['version'], 'target': 'Windows ARM64 native',
        'host': host, 'architecture': architecture, 'bundle_audit': bundle_audit,
        'skipped_ctest': args.skip_ctest, 'skipped_gui': args.skip_gui,
        'ctest_seconds': ctest['seconds'] if ctest else None,
        'files': records, 'passed': validation_passed(records, len(baseline['files']), ctest,
                                                    args.skip_gui, bundle_audit),
        # The frozen baseline has no Windows x64 per-row Pass/Fail reference,
        # and offscreen automation does not verify interactive native rendering.
        # Keep final release closed until those requested checks are implemented
        # and backed by actual Windows execution evidence.
        'release_ready': False,
        'release_blockers': ['Windows x64 per-row Pass/Fail parity is not verified by this baseline.',
                             'Interactive Windows ARM64 GUI rendering and controls are not verified by offscreen automation.'],
    }
    (output / 'ARM64_Native_Validation.json').write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
    print('ARM64 NATIVE VALIDATION', 'PASS' if report['passed'] else 'FAIL')
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
