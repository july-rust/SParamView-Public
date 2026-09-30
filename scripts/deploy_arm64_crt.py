"""Select native ARM64 CRT DLLs and reject missing runtime dependencies.

Requires pefile==2024.8.26. Check without copying by omitting --bundle.
The SDK folder can contain an x64 vcruntime140_1.dll; it is omitted only
when no selected ARM64 binary imports it. The bundle architecture gate
in build_windows.ps1 remains mandatory.
"""
from __future__ import annotations
import argparse
import re
import shutil
from pathlib import Path
import pefile

ARM64 = 0xAA64
X64 = 0x8664
CRT_NAME = re.compile(r"^(?:msvcp|vcruntime|concrt|vccorlib)\d.*\.dll$", re.I)


def inspect(path):
    with pefile.PE(str(path), fast_load=True) as pe:
        pe.parse_data_directories(directories=[
            pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"],
            pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT"],
            pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"],
        ])
        dependencies = set()
        for table in ("DIRECTORY_ENTRY_IMPORT", "DIRECTORY_ENTRY_DELAY_IMPORT"):
            for entry in getattr(pe, table, []):
                dependencies.add(entry.dll.decode("ascii").lower())
        exports = getattr(pe, "DIRECTORY_ENTRY_EXPORT", None)
        for symbol in getattr(exports, "symbols", []):
            if symbol.forwarder:
                module = symbol.forwarder.decode("ascii").rsplit(".", 1)[0].lower()
                dependencies.add(module if module.endswith(".dll") else module + ".dll")
        return pe.FILE_HEADER.Machine, dependencies


def deploy(runtime_dir, bundle=None):
    selected, omitted = {}, []
    for path in sorted(runtime_dir.iterdir()):
        if not path.is_file() or path.suffix.lower() != ".dll":
            continue
        machine, dependencies = inspect(path)
        name = path.name.lower()
        if machine == ARM64:
            selected[name] = (path, dependencies)
        elif name == "vcruntime140_1.dll" and machine == X64:
            omitted.append(path.name)
        else:
            raise ValueError(f"Unexpected CRT architecture: {path.name}=0x{machine:04X}")
    required = {"msvcp140.dll", "vcruntime140.dll"}
    if not required <= selected.keys():
        raise ValueError(f"Missing ARM64 CRT: {sorted(required - selected.keys())}")

    binaries = dict(selected)
    if bundle is not None:
        if not (bundle / "SParamView.exe").is_file():
            raise ValueError("Bundle must contain SParamView.exe")
        for path in sorted(bundle.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in {".exe", ".dll"}:
                continue
            machine, dependencies = inspect(path)
            if machine != ARM64:
                raise ValueError(f"Non-ARM64 bundle file: {path}=0x{machine:04X}")
            # Both existing bundle files and the replacement CRT are checked.
            binaries[str(path)] = (path, dependencies)

    for path, dependencies in binaries.values():
        missing = sorted(d for d in dependencies if CRT_NAME.match(d) and d not in selected)
        if missing:
            raise ValueError(f"{path.name} requires unavailable ARM64 CRT: {missing}")

    # Validate all dependencies before writing anything to the bundle.
    if bundle is not None:
        for path, _ in selected.values():
            shutil.copy2(path, bundle / path.name)
    print(f"ARM64 CRT checked: {len(selected)} DLLs; omitted x64 files: {omitted}")
    return sorted(selected)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runtime_dir", type=Path)
    parser.add_argument("--bundle", type=Path)
    args = parser.parse_args()
    deploy(args.runtime_dir, args.bundle)


if __name__ == "__main__":
    main()
