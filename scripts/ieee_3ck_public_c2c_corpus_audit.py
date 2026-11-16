#!/usr/bin/env python3
"""Read-only SHA inventory of IEEE P802.3ck publicly contributed C2C channels.

This is *not* a 802.3ck-2022 normative COM numerical oracle. Do not copy
third-party channels to the source tree, GitHub artifacts or release assets.
"""
from __future__ import annotations

import hashlib
import io
import json
import re
import sys
import urllib.request
import zipfile

URL = "https://www.ieee802.org/3/ck/public/tools/c2c/rabinovich_3ck_informal_08162019.zip"
MAX_ARCHIVE = 256_000_000
MAX_MEMBER = 350_000_000
MAX_ENTRIES = 2500
MAX_SAMPLES = 12
SNP = re.compile(r"\.s(\d+)p$", re.I)

def inspect(data: bytes) -> dict:
    if len(data) > MAX_ARCHIVE:
        raise ValueError("IEEE C2C ZIP exceeds download quota")
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        entries = z.infolist()
        if len(entries) > MAX_ENTRIES:
            raise ValueError("Excessive archive entry count")
        touchstone = []
        ignored_archives = []
        names = set()
        for entry in entries:
            name = entry.filename.replace("\\", "/")
            if name.startswith("/") or any(
                part in ("..", "") for part in name.split("/")
            ) or (entry.external_attr >> 16) & 0o170000 == 0o120000:
                raise ValueError("Unsafe archive member")
            if name in names:
                raise ValueError("Duplicate ZIP entry")
            names.add(name)
            if entry.is_dir():
                continue
            if entry.file_size > MAX_MEMBER:
                raise ValueError("Excessive expanded data file")
            ext = SNP.search(name)
            if ext:
                touchstone.append((name, int(ext.group(1)), entry))
            elif name.lower().endswith((".zip", ".7z", ".rar")):
                ignored_archives.append(name)
        touchstone.sort(key=lambda item: item[0].lower())
        fingerprints = []
        for name, ports, entry in touchstone[:MAX_SAMPLES]:
            h = hashlib.sha256()
            count = 0
            # Stream only a fixed number of source S-parameter members:
            # no extracted paths and no third-party bytes in result.
            with z.open(entry) as stream:
                while part := stream.read(65536):
                    count += len(part)
                    if count > MAX_MEMBER:
                        raise ValueError("Excessive Touchstone member")
                    h.update(part)
            if count != entry.file_size:
                raise ValueError("Incomplete Touchstone member read")
            fingerprints.append({
                "archive_member_path": name,
                "physical_port_count": ports,
                "uncompressed_bytes": count,
                "sha256": h.hexdigest(),
            })
        four_port = [x for x in touchstone if x[1] == 4]
        report = {
            "source_url": URL,
            "source_author": "IEEE 802.3ck public contribution (Keysight, 2019)",
            "download_bytes": len(data),
            "archive_sha256": hashlib.sha256(data).hexdigest(),
            "archive_member_count": len(entries),
            "touchstone_count": len(touchstone),
            "physical_four_port_count": len(four_port),
            "physical_ports_histogram": {
                str(p): sum(q == p for _,q,_ in touchstone)
                for p in sorted({t[1] for t in touchstone})
            },
            "fingerprints": fingerprints,
            "nested_archives_ignored": ignored_archives[:25],
            "candidate_unique_four_port_files": len(four_port),
            "normative_8023ck_final_profile_qualified": False,
            "computed_com_goldens": 0,
            "qualified_independent_same_phy_cases": 0,
            "third_party_bytes_uploaded": False,
            "qualification_status": "PUBLIC_CHANNEL_CORPUS_DISCOVERY_ONLY",
            "limitations": (
                "Public contributed 2019 exploratory channels are inputs, "
                "not independent reference COM results. Their file labels "
                "do not prove IEEE 802.3ck-2022 120F Table120F-8 qualification "
                "or that each channel's aggressors are correctly paired. "
                "No numeric COM Test1/Test2, jitter/ISI/noise/EQ, pass/fail."
            ),
        }
    return report

def run() -> int:
    request = urllib.request.Request(
        URL, headers={"User-Agent":"SParamView-012_09-readonly-source-audit/1.0"}
    )
    with urllib.request.urlopen(request, timeout=90) as stream:
        data = stream.read(MAX_ARCHIVE+1)
    report=inspect(data)
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0

if __name__ == "__main__":
    try:
        raise SystemExit(run())
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        print("IEEE_3CK_C2C_FIXTURE_PROVENANCE_BLOCKED: "
              f"{type(exc).__name__}: {exc}", file=sys.stderr)
        raise SystemExit(2)
