"""Generate a compact native-architecture regression baseline from probe outputs.

This script is for maintainers. Release validation should consume the frozen
baseline under docs/arm64-validation instead of regenerating it.
"""
from __future__ import annotations
import csv
import hashlib
import json
import math
import random
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def read_at(f, offset: int, fmt: str):
    f.seek(offset)
    data = f.read(struct.calcsize(fmt))
    if len(data) != struct.calcsize(fmt):
        raise ValueError(f'truncated binary at {offset}')
    return struct.unpack(fmt, data)


def matrix_baseline(path: Path, seed_text: str, sample_count: int = 192):
    with path.open('rb') as f:
        n, k = read_at(f, 0, '<QQ')
        freq_off = 16
        matrix_off = freq_off + k * 8
        freq_idx = sorted({0, 1 if k > 1 else 0, k // 4, k // 2, (3 * k) // 4, k - 2 if k > 1 else 0, k - 1})
        frequencies = [{'k': int(x), 'hz': read_at(f, freq_off + x * 8, '<d')[0]} for x in freq_idx]
        points = set()
        # Deliberately include corners, diagonals and broad frequency coverage.
        strategic_ports = sorted({0, n // 4, n // 2, (3 * n) // 4, n - 1})
        strategic_k = sorted({0, k // 8, k // 4, k // 2, (3 * k) // 4, (7 * k) // 8, k - 1})
        for i in strategic_ports:
            for j in strategic_ports:
                for q in strategic_k:
                    points.add((int(i), int(j), int(q)))
        seed = int(hashlib.sha256(seed_text.encode()).hexdigest()[:16], 16)
        rng = random.Random(seed)
        while len(points) < sample_count:
            points.add((rng.randrange(n), rng.randrange(n), rng.randrange(k)))
        samples = []
        for i, j, q in sorted(points)[:sample_count]:
            off = matrix_off + (((i * n + j) * k + q) * 16)
            re, im = read_at(f, off, '<dd')
            samples.append({'i': i, 'j': j, 'k': q, 'real': re, 'imag': im})
    return int(n), int(k), frequencies, samples


def tdr_baseline(folder: Path):
    rows = list(csv.DictReader((folder / 'index.csv').open(encoding='utf-8')))
    result = []
    for row in rows:
        index = int(row['index'])
        path = folder / f'{index}.bin'
        with path.open('rb') as f:
            nf = read_at(f, 0, '<Q')[0]
            freq_off = 8
            s_off = freq_off + nf * 8
            nt_off = s_off + nf * 16
            nt = read_at(f, nt_off, '<Q')[0]
            time_off = nt_off + 8
            z_off = time_off + nt * 8
            rho_off = z_off + nt * 8
            f_idx = sorted({0, nf // 4, nf // 2, (3 * nf) // 4, nf - 1})
            t_idx = sorted({0, nt // 8, nt // 4, nt // 2, (3 * nt) // 4, (7 * nt) // 8, nt - 1})
            fs = []
            for q in f_idx:
                hz = read_at(f, freq_off + q * 8, '<d')[0]
                re, im = read_at(f, s_off + q * 16, '<dd')
                fs.append({'k': int(q), 'hz': hz, 'real': re, 'imag': im})
            ts = []
            for q in t_idx:
                sec = read_at(f, time_off + q * 8, '<d')[0]
                z = read_at(f, z_off + q * 8, '<d')[0]
                rho = read_at(f, rho_off + q * 8, '<d')[0]
                stable = math.isfinite(z) and abs(1.0 - rho) >= 1e-4 and abs(z) < 1e9
                item = {'k': int(q), 'seconds': sec, 'rho': rho, 'stable_impedance': stable}
                if stable:
                    item['ohm'] = z
                elif not math.isfinite(z):
                    item['impedance_nonfinite'] = True
                ts.append(item)
        result.append({
            'index': index,
            'channel': row['channel'],
            'termination': int(row['termination']),
            'quality': row['quality'],
            'nf': int(row['nf']),
            'nt': int(row['nt']),
            'reference_ohm': float(row['reference']),
            'frequency_samples': fs,
            'time_samples': ts,
        })
    return result


def parse_metadata(path: Path):
    lines = path.read_text(encoding='utf-8').splitlines()
    result = {'labels': [], 'warnings': []}
    for line in lines:
        if line.startswith('format='):
            result['format'] = line.split('=', 1)[1]
        elif line.startswith('sha256='):
            result['probe_sha256'] = line.split('=', 1)[1]
        elif line.startswith('NOTE\t'):
            result['warnings'].append(line[5:])
        else:
            parts = line.split('\t', 2)
            if len(parts) >= 2 and parts[0].isdigit():
                result['labels'].append({'port': int(parts[0]), 'reference_ohm': float(parts[1]), 'label': parts[2] if len(parts) == 3 else ''})
    return result


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('--probe-root', type=Path, required=True)
    ap.add_argument('--samples', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    args = ap.parse_args()

    release = json.loads((ROOT / 'docs/release-1.0.4/SParamView_AllFiles_v1.0.4_Results.json').read_text(encoding='utf-8'))
    expected_by_name = {x['name']: x for x in release['files']}
    files = []
    for sample in sorted(args.samples.glob('*.S*P')):
        if sample.name not in expected_by_name:
            continue
        e = expected_by_name[sample.name]
        key = sample.stem
        base = args.probe_root / key
        probe = base / 'probe'
        n, k, frequencies, matrix_samples = matrix_baseline(probe / 'matrix.bin', sha256(sample))
        mapping = list(csv.DictReader((probe / 'candidates.csv').open(encoding='utf-8-sig')))
        if n != e['ports'] or k != e['points']:
            raise AssertionError((sample.name, n, k, e['ports'], e['points']))
        files.append({
            'name': sample.name,
            'bytes': sample.stat().st_size,
            'sha256': sha256(sample),
            'ports': n,
            'points': k,
            'metadata': parse_metadata(probe / 'metadata.txt'),
            'mapping': mapping,
            'frequency_samples': frequencies,
            'matrix_samples': matrix_samples,
            'tdr_forward': tdr_baseline(base / 'tdr_forward'),
            'tdr_reverse': tdr_baseline(base / 'tdr_reverse'),
            'workflow_expected': {
                'results': e['workflow']['results'],
                'metric_counts': e['workflow']['metric_counts'],
                'frequency_results_checked': e['workflow']['frequency_results_checked'],
                'xlsx_rows': e['workflow']['xlsx_rows'],
                'tdr_start_fraction': e['workflow']['tdr_start_fraction'],
            },
        })
    if len(files) != 7:
        raise SystemExit(f'Expected 7 known samples, got {len(files)}')
    output = {
        'schema': 1,
        'application': 'SParamView',
        'version': '1.0.4',
        'purpose': 'Frozen architecture-neutral numerical baseline for Windows ARM64 native validation.',
        'source': 'Known-good v1.0.4 core probe outputs generated after Release + ASan/UBSan 15/15 regression passes.',
        'tolerances': {
            'frequency_hz_abs': 1e-6,
            'complex_abs': 1e-11,
            'time_seconds_abs': 1e-18,
            'reflection_abs': 1e-9,
            'impedance_ohm_abs': 1e-4,
            'reference_ohm_abs': 1e-12,
        },
        'files': files,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2, ensure_ascii=False), encoding='utf-8')
    print(f'Wrote {args.output} ({args.output.stat().st_size} bytes, {len(files)} files)')


if __name__ == '__main__':
    main()
