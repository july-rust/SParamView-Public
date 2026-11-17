#!/usr/bin/env python3
"""012_13: execute a *candidate* 120F-8 numerical reference on historical C2C.

The final approved 120F-8 values are SHA/source locked; separately attributed
2020 IEEE P802.3ck draft aggressor packages are an EXPLICIT fixture choice.
Neither this combination nor a SiCoPR correlation is a PHY certificate.
No IEEE-licensed documents, third-party source or S4P leave ephemeral runners.
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

import com_120f8_config_contract as final
import com_3ck_c2c_real_reference as historical

SICOPR_COMMIT = historical.SICOPR_COMMIT
ROLE_SOURCE = "https://www.ieee802.org/3/ck/public/20_03/sun_3ck_01_0320.pdf"
# Source slide 10, draft table 93A-1. These are NOT Table 120F-8 leaves.
# Physical TX-side FEXT aggressor has TX package; RX-side NEXT has RX package.
NEXT_PACKAGE = "[11 29;1.8 1.8]"
FEXT_PACKAGE = "[13 31;1.8 1.8]"
# Draft COM package group delay (93A.3) is an adapter choice, not a verified
# IEEE 802.3ck Table 120F-8 numeric leaf.
PACKAGE_TAU_NS_PER_MM = 0.006141
SOURCE_CHANNELS = ("10", "16", "18")


def require(ok: bool, msg: str) -> None:
    if not ok:
        raise ValueError("012_13 FINAL_REFERENCE_FAIL_CLOSED: " + msg)


def configure(root: Path) -> tuple[Path, dict]:
    original_rows, audit = final.config_rows()
    require(audit["mapped_source_leaves"] == 68 and
            audit["status"] == "STATIC_CONTRACT_PASS" and
            audit["reference_config_no_com"] and
            audit["next_fext_zero_package_sentinels"] and
            audit["aggressor_package_and_s4p_compliance"] == "NOT_QUALIFIED",
            "final normative parameter source is unverified")
    rows = [list(row) for row in original_rows]
    vals = {row[0]: row[1] for row in rows}
    require(len(vals) == len(rows), "duplicate 4.17 CSV parameter")
    require(vals["z_p (TX)"] == "[13 31;1.8 1.8]" and
            vals["z_p (RX)"] == "[11 29;1.8 1.8]" and
            vals["z_p (NEXT)"] == "[0 0;0 0]" and
            vals["z_p (FEXT)"] == "[0 0;0 0]" and
            vals["NO_COM"] == "1" and
            vals["T_r"] == "0.0075" and vals["f_b"] == "53.125",
            "source input or safety-sentinel contract changed")
    replacement = {
        "z_p (NEXT)": NEXT_PACKAGE,
        "z_p (FEXT)": FEXT_PACKAGE,
        "NO_COM": "0",
    }
    applied = set()
    for row in rows:
        if row[0] in replacement:
            applied.add(row[0])
            row[1] = replacement[row[0]]
    require(applied == set(replacement), "incomplete explicit model overlay")
    # The official 120F-8 parameter table does not enumerate these
    # source-code operational controls. The physical aggressor selection
    # is historical-source-specific, not a normative parameter substitution.
    rows.extend([
        ["package_tl_tau", str(PACKAGE_TAU_NS_PER_MM)],
        ["TDR", "0"], ["ERL", "0"], ["ERL_ONLY", "0"],
        ["CSV_REPORT", "0"], ["DISPLAY_WINDOW", "0"],
        ["SAVE_FIGURES", "0"],
    ])
    require(len({x[0] for x in rows}) == len(rows), "extra control duplicates")
    result = root / "120f8_candidate_93a_draft_aggressors.csv"
    with result.open("w", encoding="utf-8", newline="") as f:
        csv.writer(f, lineterminator="\n").writerows(rows)
    digest = historical.sha(result.read_bytes())
    old = io.StringIO(newline="")
    csv.writer(old, lineterminator="\n").writerows(original_rows)
    return result, {
        "source_120f8_68_leaves_verified": True,
        "immutable_base_csv_sha256": hashlib.sha256(old.getvalue().encode()).hexdigest(),
        "effective_csv_sha256": digest,
        "source_profile": audit["source_profile"],
        "external_engine_matlab_4p17_source_commit": final.REFERENCE_COMMIT,
        "historical_aggressor_model_source": ROLE_SOURCE,
        "historical_aggressor_model_source_status": "DRAFT_2020_NOT_NORMATIVE_FINAL",
        "historical_fixture_fext_count": 4,
        "historical_fixture_fext_distinct_originals": 1,
        "historical_fixture_next_count": 0,
        "effective_next_package": NEXT_PACKAGE,
        "effective_fext_package": FEXT_PACKAGE,
        "package_tau_adapter_ns_per_mm": PACKAGE_TAU_NS_PER_MM,
        "run_com_not_safety_disabled": True,
        "normative_phy_pass_fail": None,
        "source_s4p_files_redistributed": False,
    }


def validate_external(repo: Path) -> Path:
    root = repo.resolve(strict=True)
    sha = subprocess.run(["git", "-C", str(root), "rev-parse", "HEAD"],
                         capture_output=True, text=True, timeout=15, check=True)
    require(sha.stdout.strip() == SICOPR_COMMIT,
            "independent code revision not SHA pinned")
    status = subprocess.run(["git", "-C", str(root), "status",
                             "--porcelain", "--untracked-files=no"],
                            capture_output=True, text=True, timeout=15, check=True)
    require(status.stdout.strip() == "", "external COM source modified")
    require((root/"sicopr.py").is_file() and
            (root/"octave/com_ieee8023_4p17p0_octave_compat.m").is_file(),
            "4p17p0 Python or Octave independent implementation missing")
    return root


def result_pair(raw) -> list[dict]:
    records = raw if isinstance(raw, list) else [raw]
    require(len(records) == 2, "exactly Test1 and Test2 required")
    result = []
    for case, rec in enumerate(records, 1):
        require(rec is not None, "missing COM reference output")
        # Reject complex results; no silent complex-to-real conversion.
        import numpy as np
        def finite_real(name: str) -> float:
            arr = np.asarray(getattr(rec, name))
            require(arr.size == 1 and arr.dtype.kind in "biufc",
                    "missing or nonnumeric " + name)
            num = complex(arr.reshape(-1)[0])
            require(math.isfinite(num.real) and math.isfinite(num.imag) and
                    abs(num.imag) <= 1e-12, "nonreal or invalid " + name)
            return float(num.real)
        result.append({
            "package_case": case,
            "reference_com_db": finite_real("COM_dB"),
            "reference_fom_db": finite_real("FOM"),
            "sampling_phase_itick": int(rec.itick),
        })
    return result


def run_python(cfg: Path, inputs: list[Path], root: Path, ext: Path) -> list[dict]:
    sys.path.insert(0, str(ext))
    try:
        import sicopr
        require(Path(sicopr.__file__).resolve() == (ext/"sicopr.py").resolve(),
                "Python code not loaded from SHA-pinned checkout")
        require(getattr(sicopr, "COM_MATLAB_VERSION", None) is not None,
                "reference version selector unavailable")
        sicopr.COM_MATLAB_VERSION = "4p17p0"
        original = Path.cwd()
        num_fext, num_next, ordered = historical.reference_2019_c2c_inputs(inputs)
        require((num_fext, num_next, len(ordered)) == (4, 0, 5),
                "wrong historical aggressor population")
        try:
            os.chdir(root)
            with open(root/"python_reference_console.log", "w") as log:
                with contextlib.redirect_stdout(log):
                    observed = sicopr._run_com(str(cfg), num_fext, num_next,
                                               [str(p) for p in ordered],
                                               export_mat=False)
        finally:
            os.chdir(original)
        return result_pair(observed)
    finally:
        sys.path.remove(str(ext))


def run_octave(cfg: Path, inputs: list[Path], root: Path, ext: Path) -> list[dict]:
    from com_3ck_c2c_octave_check import csv_to_octave_mat, finite_scalar_parts
    import numpy as np
    import scipy.io
    mat = root/"candidate.mat"
    conversion = csv_to_octave_mat(cfg, mat)
    require(conversion["source_csv_sha256"] == historical.sha(cfg.read_bytes()),
            "Octave converter changed final CSV")
    nf, nn, ordered = historical.reference_2019_c2c_inputs(inputs)
    require((nf, nn, len(ordered)) == (4, 0, 5), "wrong aggressor population")
    quote = lambda text: "'" + str(text).replace("'", "''") + "'"
    argument = ",".join([quote(mat.name),str(nf),str(nn)] +
                        [quote(p.name) for p in ordered])
    statement = (
        "more off; set(0,'defaultfigurevisible','off');"
        "graphics_toolkit('gnuplot');"
        "addpath(" + quote(ext/"octave") + ");"
        "r=com_ieee8023_4p17p0_octave_compat(" + argument + ");"
        "save('-v7'," + quote("reference_octave.mat") + ",'r');exit(0);"
    )
    env = os.environ.copy()
    env.update({"OPENBLAS_NUM_THREADS":"1","OMP_NUM_THREADS":"1",
                "MKL_NUM_THREADS":"1","MPLBACKEND":"Agg"})
    try:
        proc = subprocess.run(["octave-cli", "--no-gui", "--no-window-system",
                               "--quiet", "--eval", statement],
                              cwd=root, env=env, capture_output=True,
                              text=True, errors="replace", timeout=2400,
                              check=False)
    except subprocess.TimeoutExpired as ex:
        raise ValueError("Octave 40-minute numerical budget exceeded") from ex
    require(proc.returncode == 0 and
            (root/"reference_octave.mat").is_file(),
            "Octave execution failed: " + proc.stderr[-500:])
    observed = scipy.io.loadmat(str(root/"reference_octave.mat"),
                                squeeze_me=True, struct_as_record=False)["r"]
    cases = np.asarray(observed, dtype=object).ravel()
    require(len(cases) == 2, "Octave did not produce two package tests")
    result = []
    for idx, case in enumerate(cases, 1):
        com, com_imag = finite_scalar_parts(case.COM_dB, "COM_dB")
        fom, fom_imag = finite_scalar_parts(case.FOM, "FOM")
        require(com_imag == 0 and fom_imag == 0,
                "complex Octave COM/FOM is not a valid Golden")
        result.append({"package_case":idx, "reference_com_db":com,
                       "reference_fom_db":fom})
    return result


def run(channel: str, mode: str, external: Path, output: Path) -> dict:
    require(channel in SOURCE_CHANNELS, "unqualified source channel")
    ext = validate_external(external)
    with tempfile.TemporaryDirectory(prefix="ck-final-120f8-reference-") as temp:
        root = Path(temp)
        _, inputs, source = historical.prepare(channel, root)
        require(len(source) == 3 and
                [x["role"] for x in source] == ["thru", "fext", "next"],
                "three historical real IEEE source roles not verified")
        orientation = historical.audit_physical_thru_orientation(inputs[0])
        historical.verified_historical_port_order(channel, orientation)
        cfg, settings = configure(root)
        if mode == "python":
            cases = run_python(cfg, inputs, root, ext)
        else:
            cases = run_octave(cfg, inputs, root, ext)
    receipt = {
        "status":"FINAL_120F8_PARAMETER_SET_HISTORICAL_AGGRESSOR_CANDIDATE",
        "engine": mode,
        "independent_code_commit": SICOPR_COMMIT,
        "historical_channel_loss_label_db": channel,
        "historical_ieee_channel_zip_sha256": historical.CHANNEL_ZIP_SHA,
        "historical_ieee_source_sha256_members": source,
        "verified_original_thru_port_order": [1, 2, 3, 4],
        "settings":settings, "package_cases":cases,
        "final_ieee_normative_phy_qualified":False,
        "native_sparamview_cpp_qualified":False,
        "official_ieee_matlab_executed":False,
        "license_source_files_uploaded":False,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(receipt, indent=2, sort_keys=True,
                                 allow_nan=False)+"\n",encoding="utf-8")
    return receipt


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--channel", choices=SOURCE_CHANNELS, required=True)
    p.add_argument("--mode", choices=("python","octave"),required=True)
    p.add_argument("--external", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    x = p.parse_args()
    try:
        result = run(x.channel, x.mode, x.external, x.output)
        print("012_13 REAL_REFERENCE_NUMERICAL_CANDIDATE:",
              json.dumps({"channel":x.channel,"engine":x.mode,
                          "cases":result["package_cases"],
                          "scope":"historical-aggressor candidate only"},
                         allow_nan=False),flush=True)
        return 0
    except (OSError,ValueError,TypeError,RuntimeError,ArithmeticError,
            subprocess.CalledProcessError) as ex:
        print("012_13 REAL_REFERENCE_FAIL_CLOSED:",
              type(ex).__name__,str(ex)[-1000:],file=sys.stderr,flush=True)
        return 2


if __name__=="__main__":
    raise SystemExit(main())
