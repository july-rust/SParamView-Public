"""Dependency-free PE architecture check for Windows release bundles.

Usage:
  python scripts/verify_pe_machine.py <file-or-directory> ARM64 [report.json]
  python scripts/verify_pe_machine.py <file-or-directory> x64 [report.json]

Checks all .exe/.dll files recursively. This intentionally does not replace the
optional pefile-based import/export audit in scripts/audit_windows.py.
"""
from __future__ import annotations
import json
import struct
import sys
from pathlib import Path

MACHINES = {
    'X64': (0x8664, 'AMD64'),
    'AMD64': (0x8664, 'AMD64'),
    'ARM64': (0xAA64, 'ARM64'),
}


def pe_info(path: Path):
    with path.open('rb') as f:
        dos = f.read(64)
        if len(dos) < 64 or dos[:2] != b'MZ':
            raise ValueError('not an MZ executable')
        peoff = struct.unpack_from('<I', dos, 0x3C)[0]
        f.seek(peoff)
        sig = f.read(4)
        if sig != b'PE\0\0':
            raise ValueError('missing PE signature')
        coff = f.read(20)
        if len(coff) != 20:
            raise ValueError('truncated COFF header')
        machine, sections, timestamp, _ptr, _symbols, opt_size, characteristics = struct.unpack('<HHIIIHH', coff)
        optional = f.read(opt_size)
        if len(optional) != opt_size:
            raise ValueError('truncated optional header')
        magic = struct.unpack_from('<H', optional, 0)[0] if len(optional) >= 2 else None
        # Subsystem offset is 68 bytes into PE32/PE32+ optional header.
        subsystem = struct.unpack_from('<H', optional, 68)[0] if len(optional) >= 70 else None
        return {
            'machine': machine,
            'sections': sections,
            'timestamp': timestamp,
            'characteristics': characteristics,
            'optional_magic': magic,
            'subsystem': subsystem,
        }


def collect(target: Path):
    if target.is_file():
        return [target]
    return sorted(p for p in target.rglob('*') if p.is_file() and p.suffix.lower() in {'.exe', '.dll'})


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit(__doc__)
    target = Path(sys.argv[1]).resolve()
    arch = sys.argv[2].upper()
    if arch not in MACHINES:
        raise SystemExit('architecture must be x64/AMD64 or ARM64')
    expected, label = MACHINES[arch]
    files = collect(target)
    if not files:
        raise SystemExit(f'No .exe/.dll files found under {target}')
    report = {'expected': label, 'expected_machine': f'0x{expected:04X}', 'root': str(target), 'files': [], 'errors': []}
    for path in files:
        try:
            info = pe_info(path)
            row = {'file': str(path.relative_to(target) if target.is_dir() else path.name), **info,
                   'machine_hex': f"0x{info['machine']:04X}", 'ok': info['machine'] == expected}
            report['files'].append(row)
            if not row['ok']:
                report['errors'].append(f"{row['file']}: machine={row['machine_hex']} expected=0x{expected:04X}")
        except Exception as e:
            report['errors'].append(f'{path}: {e}')
    report['passed'] = not report['errors']
    if len(sys.argv) == 4:
        Path(sys.argv[3]).write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(f"PE architecture audit: {len(report['files'])} files, target={label}, errors={len(report['errors'])}")
    for e in report['errors'][:20]:
        print('ERROR:', e)
    raise SystemExit(0 if report['passed'] else 1)


if __name__ == '__main__':
    main()
