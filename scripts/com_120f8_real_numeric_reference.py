#!/usr/bin/env python3
"""012_13 continuation: real final-120F8-profile independent numerical correlation.

Final *profile values* are standard-source verified. The historical 2019
measurement channels and 4x repeated FEXT are NOT standard final goldens.
NEXT/FEXT package routing is a source-cited reference adapter, not an
additional final 120F-8 numeric value. Never assert IEEE certification.
"""
from __future__ import annotations

import argparse
import contextlib
import csv
import hashlib
import io
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile

import com_120f8_config_contract as config
import com_3ck_c2c_real_reference as historical

SICOPR_COMMIT = historical.SICOPR_COMMIT
# IEEE P802.3ck public 2021 contribution, NOT the final released standard:
# https://www.ieee802.org/3/ck/public/adhoc/jun30_21/mellitz_3ck_adhoc_01_063021.pdf
# C2C COM configuration shows NEXT package [11 29;1.8 1.8] and
# FEXT package [13 31;1.8 1.8]. Final standard Annex 120F Table 120F-8
# establishes 11/29 RX and 13/31 TX package lengths.
AGGRESSOR_SOURCE = "IEEE_2021_WG_PUBLIC_C2C_ROUTING_CORROBORATION"
NEXT_PACKAGE = "[11 29;1.8 1.8]"
FEXT_PACKAGE = "[13 31;1.8 1.8]"


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise ValueError("012_13 NUMERIC_FAIL_CLOSED: " + reason)


def compute_csv_rows() -> tuple[list[list[str]], dict]:
    rows, provenance = config.config_rows()
    table = {row[0]: row[1] for row in rows}
    require(len(table) == len(rows) and
            provenance["mapped_source_leaves"] == 68 and
            table.get("NO_COM") == "1" and
            table.get("z_p (NEXT)") == "[0 0;0 0]" and
            table.get("z_p (FEXT)") == "[0 0;0 0]" and
            table.get("Port Order") == "[1 2 3 4]" and
            table.get("z_p (TX)") == "[13 31;1.8 1.8]" and
            table.get("z_p (RX)") == "[11 29;1.8 1.8]",
            "source verified 012_12 input contract drift")
    for row in rows:
        if row[0] == "NO_COM":
            row[1] = "0"
        elif row[0] == "z_p (NEXT)":
            row[1] = NEXT_PACKAGE
        elif row[0] == "z_p (FEXT)":
            row[1] = FEXT_PACKAGE
    require(dict(rows)["NO_COM"] == "0" and
            dict(rows)["z_p (NEXT)"] != "[0 0;0 0]" and
            dict(rows)["z_p (FEXT)"] != "[0 0;0 0]",
            "unqualified nonphysical guard remains")
    return rows, provenance


def write_csv(output: Path) -> dict:
    rows, source = compute_csv_rows()
    with output.open("w", newline="", encoding="utf-8") as handle:
        csv.writer(handle, lineterminator="\n").writerows(rows)
    return {
        "csv_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
        "final_source_profile": source["source_profile"],
        "mapped_numeric_leaves": source["mapped_source_leaves"],
        "package_routing_basis": AGGRESSOR_SOURCE,
        "aggressor_topology_published_final_qualified": False,
        "next_package": NEXT_PACKAGE,
        "fext_package": FEXT_PACKAGE,
        "no_com": False,
        "historical_only": True,
    }


def verified_inputs(loss: str, root: Path) -> tuple[list[Path], list[dict]]:
    require(loss in ("10", "16", "18"), "unexpected loss group")
    raw = historical.download(historical.CHANNEL_URL,
                              historical.CHANNEL_ZIP_SHA)
    files = []
    members = []
    for role, (member, digest) in zip(
        ("thru", "fext", "next"), historical.SOURCES[loss]
    ):
        payload = historical.verified_member(raw, member, digest)
        file = root / (role + ".s4p")
        file.write_bytes(payload)
        files.append(file)
        members.append({"role": role, "sha256": digest,
                        "original_member": member})
    return files, members


def validate_external(root: Path) -> None:
    expected = root.resolve(strict=True)
    require((expected / "sicopr.py").is_file() and
            (expected / "octave" /
             "com_ieee8023_4p17p0_octave_compat.m").is_file(),
            "missing external reference source")
    status = subprocess.run(
        ["git", "-C", str(expected), "rev-parse", "HEAD"],
        capture_output=True, text=True, check=True, timeout=15)
    require(status.stdout.strip() == SICOPR_COMMIT,
            "independent reference SHA mismatch")


def actual_com_python(csv_file: Path, files: list[Path]) -> list[dict]:
    import sicopr
    import numpy as np
    require(sicopr.__file__ and
            getattr(sicopr, "COM_MATLAB_VERSION", None) is not None,
            "external reference import unavailable")
    sicopr.COM_MATLAB_VERSION = "4p17p0"
    n_fext, n_next, ordered = historical.reference_2019_c2c_inputs(files)
    require((n_fext, n_next) == (4, 0) and len(ordered) == 5,
            "source aggressor roles changed")
    with contextlib.redirect_stdout(io.StringIO()):
        result = sicopr._run_com(csv_file.name, n_fext, n_next,
                                 [str(file.name) for file in ordered],
                                 export_mat=False)
    records = result if isinstance(result, list) else [result]
    require(len(records) == 2, "reference returned wrong case count")
    out = []
    for case, item in enumerate(records, start=1):
        require(item is not None, "missing independent reference result")
        com = np.asarray(item.COM_dB)
        fom = np.asarray(item.FOM)
        require(com.size == fom.size == 1 and
                com.dtype.kind in "biufc" and fom.dtype.kind in "biufc",
                "invalid reference scalar type")
        c, f = complex(com.ravel()[0]), complex(fom.ravel()[0])
        require(all(math.isfinite(v) for v in
                    (c.real, c.imag, f.real, f.imag)) and
                abs(c.imag) <= 1e-12 and abs(f.imag) <= 1e-12,
                "complex or nonfinite Python COM/FOM")
        out.append({"case": case, "com_db": c.real, "fom_db": f.real})
    return out


def actual_com_octave(csv_file: Path, files: list[Path],
                      external: Path) -> list[dict]:
    import numpy as np
    import scipy.io
    from com_3ck_c2c_octave_check import (csv_to_octave_mat,
                                          finite_scalar_parts)
    mat = csv_file.parent / "verified-final-com.mat"
    csv_to_octave_mat(csv_file, mat)
    n_fext, n_next, ordered = historical.reference_2019_c2c_inputs(files)
    require((n_fext, n_next) == (4, 0), "unapproved source multiplicity")
    artifact = csv_file.parent / "com-octave-output.mat"
    quote = lambda value: "'" + str(value).replace("'", "''") + "'"
    args = ",".join(
        [quote(mat.name), str(n_fext), str(n_next)] +
        [quote(p.name) for p in ordered]
    )
    statement = (
        "more off; set(0,'defaultfigurevisible','off'); "
        "graphics_toolkit('gnuplot'); addpath(" +
        quote(external / "octave") + "); "
        "r=com_ieee8023_4p17p0_octave_compat(" + args + "); "
        "save('-v7'," + quote(artifact.name) + ",'r'); exit(0);"
    )
    proc = subprocess.run(
        ["octave-cli", "--no-gui", "--no-window-system",
         "--quiet", "--eval", statement],
        cwd=csv_file.parent, text=True, errors="replace",
        capture_output=True, timeout=1800, check=False,
    )
    require(proc.returncode == 0 and artifact.is_file(),
            "Octave failed: " + proc.stderr[-450:])
    data = scipy.io.loadmat(str(artifact), squeeze_me=True,
                            struct_as_record=False)["r"]
    records = np.asarray(data, dtype=object).ravel()
    require(len(records) == 2, "Octave did not return two cases")
    out = []
    for case, item in enumerate(records, start=1):
        com, com_im = finite_scalar_parts(item.COM_dB, "COM")
        fom, fom_im = finite_scalar_parts(item.FOM, "FOM")
        require(abs(com_im) <= 1e-12 and abs(fom_im) <= 1e-12,
                "complex Octave output not qualified")
        out.append({"case": case, "com_db": com, "fom_db": fom})
    return out


def compare(py: list[dict], octave: list[dict]) -> list[dict]:
    require(len(py) == len(octave) == 2,
            "both independent implementations must evaluate both cases")
    out = []
    for idx, (p, o) in enumerate(zip(py, octave), start=1):
        require(p["case"] == o["case"] == idx,
                "mismatched package case identities")
        cd = abs(p["com_db"] - o["com_db"])
        fd = abs(p["fom_db"] - o["fom_db"])
        require(math.isfinite(cd) and math.isfinite(fd) and
                cd <= 1e-4 and fd <= 1e-4,
                "independent numerical COM/FOM disagreement")
        out.append({"package_test": idx, "python_com_db": p["com_db"],
                    "octave_com_db": o["com_db"], "delta_com_db": cd,
                    "python_fom_db": p["fom_db"],
                    "octave_fom_db": o["fom_db"], "delta_fom_db": fd,
                    "historical_2019_input": True,
                    "final_profile_candidate": True,
                    "normative_ieee_pass_fail": None})
    return out


def execute(loss: str, external: Path, output: Path) -> int:
    import sicopr
    validate_external(external)
    require(Path(sicopr.__file__).resolve() ==
            (external / "sicopr.py").resolve(),
            "loaded third-party COM source not pinned checkout")
    with tempfile.TemporaryDirectory(prefix="012_13-numeric-") as tmp:
        root = Path(tmp)
        config_csv = root / "final_profile_candidate.csv"
        contract = write_csv(config_csv)
        files, members = verified_inputs(loss, root)
        original = Path.cwd()
        try:
            os.chdir(root)
            python_cases = actual_com_python(config_csv, files)
            octave_cases = actual_com_octave(config_csv, files, external)
        finally:
            os.chdir(original)
        case_results = compare(python_cases, octave_cases)
    receipt = {
        "status": "FINAL_PROFILE_HISTORICAL_CHANNEL_NUMERIC_CORRELATION",
        "channel_loss_label_db": loss,
        "independent_python_octave_correlation_pass": True,
        "config": contract,
        "reference_engine_commit": SICOPR_COMMIT,
        "historical_channel_archive_sha256": historical.CHANNEL_ZIP_SHA,
        "channel_members": members,
        "cases": case_results,
        "final_ieee_certification": False,
        "official_ieee_matlab_reference_executed": False,
        "native_sparamview_cpp_com_evaluated": False,
        "final_standard_golden_qualified": False,
        "limitation": "Final 120F-8 profile values on 2019 historical S4P; "
                      "NEXT/FEXT package routing is 2021 IEEE WG corroborated, "
                      "not independently certified as a final IEEE golden.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    print("012_13 REAL COM/FOM:", loss, case_results, flush=True)
    return 0


if __name__ == "__main__":
    cli = argparse.ArgumentParser()
    cli.add_argument("--channel", required=True, choices=["10", "16", "18"])
    cli.add_argument("--external", type=Path, required=True)
    cli.add_argument("--output", type=Path, required=True)
    a = cli.parse_args()
    try:
        raise SystemExit(execute(a.channel, a.external.resolve(),
                                 a.output.resolve()))
    except (OSError, ValueError, TypeError, RuntimeError,
            subprocess.TimeoutExpired) as exc:
        print("012_13_FINAL_PROFILE_REAL_NUMERIC_BLOCKED:",
              type(exc).__name__, str(exc)[:700],
              file=sys.stderr, flush=True)
        raise SystemExit(2)
