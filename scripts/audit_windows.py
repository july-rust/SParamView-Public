"""Static PE architecture/import audit for SParamView Windows bundles.
Usage: python audit_windows.py <bundle-dir> <report.json> [x64|ARM64]
Requires pefile. Actual Windows execution must still be tested natively.
"""
import json
import sys
from pathlib import Path
import pefile

root = Path(sys.argv[1])
out = Path(sys.argv[2])
arch = (sys.argv[3] if len(sys.argv) > 3 else 'x64').upper()
MACHINES = {'X64': (0x8664, 'PE32+ AMD64'), 'ARM64': (0xAA64, 'PE32+ ARM64')}
if arch not in MACHINES:
    raise SystemExit('architecture must be x64 or ARM64')
expected_machine, architecture_label = MACHINES[arch]

paths = {p.name.lower(): p for p in root.rglob('*') if p.suffix.lower() in ('.exe', '.dll')}
pes = {n: pefile.PE(str(p), max_symbol_exports=100000) for n, p in paths.items()}
exports = {n: {s.name for s in getattr(getattr(p, 'DIRECTORY_ENTRY_EXPORT', None), 'symbols', []) if s.name} for n, p in pes.items()}
missing, system, files = [], set(), []
for name, pe in pes.items():
    if pe.FILE_HEADER.Machine != expected_machine:
        raise AssertionError(f'{name}: machine=0x{pe.FILE_HEADER.Machine:04x}, expected=0x{expected_machine:04x} ({arch})')
    dependencies = []
    for entry in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []):
        dll = entry.dll.decode().lower()
        dependencies.append(dll)
        if dll not in pes:
            system.add(dll)
            continue
        for symbol in entry.imports:
            if symbol.name and symbol.name not in exports[dll]:
                missing.append(dict(file=name, dll=dll, missing_symbol=symbol.name.decode()))
    files.append(dict(file=name, dependencies=dependencies))
report = dict(architecture=architecture_label, machine=f'0x{expected_machine:04X}', files=files,
              missing_bundled_symbols=missing, system_dlls=sorted(system),
              scope='Static import and exported-symbol audit; native Windows execution still required')
out.write_text(json.dumps(report, indent=2), encoding='utf-8')
print(f'{len(files)} PE files, architecture={architecture_label}, {len(missing)} missing bundled symbols')
if missing:
    raise SystemExit(1)
