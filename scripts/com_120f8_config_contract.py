#!/usr/bin/env python3
"""012_12: final 120F-8 values -> COM 4.17 CSV reference-input *contract*.

This is QA configuration, NOT the official COM 4.17 source or a COM solver.
Licensed IEEE PDF, external S4P, and third-party program remain outside repo.
A successful parse is not a normative 802.3ck COM calculation.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import io
import json
import math
from pathlib import Path
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE_AUDIT = ROOT / "tests/fixtures/channel/com_reference/manifest.json"
HEADER = ROOT / "include/si/channel_analysis/com_120f8.hpp"
PROFILE_TEST = ROOT / "tests/channel_analysis/test_com_120f8_published_profile.py"
HISTORICAL_SOURCE = ROOT / "scripts/com_3ck_c2c_real_reference.py"
REFERENCE_COMMIT = "47459695a169fdf1fd10c541a5b19c69a5ae9285"
REFERENCE_SHA = "cdcd739ac13c9cc3ca59733fd938a015f040429655b5dbf5543c897789cfed9b"
PORT_ORDER = (1, 2, 3, 4)
CHANNELS = ("10", "16", "18")


class ContractError(ValueError):
    pass


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ContractError(message)


def module_from_file(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None,
            f"Cannot resolve source module: {path.name}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def checked_parameters() -> tuple[dict, dict, dict, set]:
    """Extract from C++ typed source; independently compare all 68 leaves."""
    m = module_from_file("com120f8_published_contract", PROFILE_TEST)
    header = HEADER.read_text(encoding="utf-8")
    scalar = {key: m.extract_scalar(header, key) for key in m.EXPECTED_SCALARS}
    arrays = {key: m.extract_array(header, key) for key in m.EXPECTED_ARRAYS}
    ranges = {key: m.extract_ranges(header, key) for key in m.EXPECTED_RANGES}
    for key, value in scalar.items():
        require(math.isclose(value, m.EXPECTED_SCALARS[key],
                             rel_tol=1e-13, abs_tol=1e-15),
                f"C++ scalar drift: {key}")
    for family, expected in ((arrays, m.EXPECTED_ARRAYS),
                             (ranges, m.EXPECTED_RANGES)):
        for key, value in family.items():
            require(len(value) == len(expected[key]), f"Wrong dimension: {key}")
            flat = lambda a: (x for row in a for x in row) if (
                a and isinstance(a[0], tuple)) else iter(a)
            require(all(math.isclose(x, y, rel_tol=1e-13, abs_tol=1e-15)
                        for x, y in zip(flat(value), flat(expected[key]))),
                    f"C++ vector drift: {key}")
    count = (len(scalar) +
             sum(len(row) for vector in ranges.values() for row in vector) +
             sum(len(vector) for vector in arrays.values()))
    require(count == 68, f"Expected 68 source-verified leaves, got {count}")
    manifest = json.loads(SOURCE_AUDIT.read_text(encoding="utf-8"))
    audit = manifest["standard"]["parameter_source_audit"]
    ref = manifest["reference"]
    require(manifest["status"] == "REFERENCE_READY" and
            manifest["standard"]["parameter_values_qualified"] is False and
            manifest["cases"] == [], "Production qualification was silently changed")
    require(audit["status"] == "SOURCE_VERIFIED_012_11" and
            audit["numeric_leaves_checked"] == 68 and
            audit["numeric_mismatches"] == 0, "Final licensed source audit missing")
    require(ref["release"] == "4.17.0" and
            ref["source_commit"] == REFERENCE_COMMIT and
            ref["source_content_sha256"] == REFERENCE_SHA,
            "Reference version/source SHA does not match 4.17.0")
    return scalar, arrays, ranges, set()


def config_rows() -> tuple[list[list[str]], dict]:
    """All 68 C++ leaves consumed; no hidden profile defaults called normative."""
    scalar, arrays, ranges, used = checked_parameters()
    def s(key: str) -> float:
        require(key not in used, f"Scalar mapped twice: {key}")
        used.add(key)
        return scalar[key]
    def a(key: str) -> tuple:
        require(key not in used, f"Array mapped twice: {key}")
        used.add(key)
        return arrays[key]
    def r(key: str) -> tuple:
        require(key not in used, f"Range mapped twice: {key}")
        used.add(key)
        return ranges[key]
    def fmt(v: float) -> str:
        require(math.isfinite(v), "Nonfinite COM configuration value")
        return format(v, ".15g")
    def matrix(rows: list[list[float]]) -> str:
        require(all(len(row) == 2 for row in rows), "Two cases required")
        return "[" + ";".join(" ".join(map(fmt, row)) for row in rows) + "]"
    def span(vals: tuple) -> str:
        low, high, step = vals
        require(step > 0 and high >= low and
                abs(round((high-low)/step) - (high-low)/step) < 1e-8,
                "Invalid FFE/CTLE discrete grid")
        return "[" + ":".join(map(fmt, (low, step, high))) + "]"
    rows: list[list[str]] = []
    def put(key: str, value: object) -> None:
        require(not any(row[0] == key for row in rows),
                f"Duplicate COM 4.17 keyword: {key}")
        rows.append([key, fmt(value) if isinstance(value, (int,float)) else str(value)])

    fb = s("symbolRateHz")
    put("f_b", fb/1e9)  # GBd
    put("f_min", s("maximumStartFrequencyHz")/1e9)  # GHz
    put("Delta_f", s("maximumFrequencyStepHz")/1e9)  # GHz
    put("COM Pass threshold", s("minimumComDb"))  # informational, no verdict
    put("T_r", s("txTransitionTimeS")/1e-9)  # ns
    put("FORCE_TR", 1)
    put("R_0", s("referenceOhmSingleEnded"))
    rd = s("receiverTerminationOhmSingleEnded")
    put("R_d", "[" + " ".join(map(fmt, (rd, rd))) + "]")
    for field, key, factor in (
        ("cdFaradSingleEnded", "C_d", 1e-9),
        ("lsHenrySingleEnded", "L_s", 1e-9),
        ("cbFaradSingleEnded", "C_b", 1e-9),
        ("cpFaradSingleEnded", "C_p", 1e-9)):
        v = s(field)/factor
        put(key, "["+" ".join(map(fmt, (v,v)))+"]")  # TX/RX equal
    tx = a("txPackageLengthMm")
    rx = a("rxPackageLengthMm")
    secondary = s("secondaryPackageLengthMm")
    put("z_p (TX)", matrix([list(tx), [secondary, secondary]]))
    put("z_p (RX)", matrix([list(rx), [secondary, secondary]]))
    # The real COM 4.17 CSV parser requires NEXT/FEXT matrices of the
    # same 2-case x 2-segment shape even when only validating the THRU setup.
    # These ZERO matrices are deliberately NON-PHYSICAL shape sentinels.
    # Coupled with NO_COM=1, they must never be used to calculate a margin.
    put("z_p (FEXT)", matrix([[0., 0.], [0., 0.]]))
    put("z_p (NEXT)", matrix([[0., 0.], [0., 0.]]))
    put("NO_COM", 1)
    # NEXT/FEXT aggressor packaging is intentionally NOT qualified.
    zc1, zc2 = s("primaryPackageZcOhm"), s("secondaryPackageZcOhm")
    put("package_Z_c", matrix([[zc1,zc1], [zc2,zc2]]))
    a1 = s("packageA1SqrtNsPerMm")
    a2 = s("packageA2NsPerMm")
    put("package_tl_gamma0_a1_a2",
        "[" + " ".join(map(fmt, (0., a1, a2))) + "]")
    # gamma0=0 is an explicit COM 4.17 compatibility setting, not one of
    # the 68 independently source-verified final-table numerical leaves.
    put("f_r", s("receiverBandwidthHz")/fb)
    noncursor = r("txNoncursor")
    require(len(noncursor) == 4, "Expected four noncursor tap grids")
    for key, values in zip(("c(-3)","c(-2)","c(-1)","c(1)"), noncursor):
        put(key, span(values))
    put("c(0)", s("minimumCursor"))
    put("g_DC", span(r("ctleDcGainDb")[0]))
    put("g_DC_HP", span(r("ctleDcGain2Db")[0]))
    for field, label in (
        ("ctleZeroFrequencyHz", "f_z"),
        ("ctlePole1FrequencyHz", "f_p1"),
        ("ctlePole2FrequencyHz", "f_p2"),
        ("ctleLowFrequencyPoleZeroHz", "f_HP_PZ")):
        put(label, s(field)/1e9)  # GHz
    for field,label in (("victimPeakV","A_v"),("fextPeakV","A_fe"),
                        ("nextPeakV","A_ne")):
        put(label, s(field))
    put("L", int(s("pamLevels")))
    put("R_LM", s("minimumRlm"))
    put("SNR_TX", s("txSnrDb"))
    put("M", int(s("samplesPerUi")))
    nb = int(s("dfeTaps"))
    put("N_b", nb)
    low,high = a("dfeMin"),a("dfeMax")
    require(nb == len(low) == len(high) == 6, "DFE tap size mismatch")
    for label, values in (("b_min",low),("b_max",high)):
        put(label+"(1)", values[0])
        put(label+"(2..N_b)", "["+" ".join(map(fmt,values[1:]))+"]")
    put("sigma_RJ", s("randomJitterRmsUi"))
    put("A_DD", s("dualDiracJitterPeakUi"))
    put("eta_0", s("oneSidedNoiseSpectralDensityV2PerGHz"))
    put("DER_0", s("targetDetectorErrorRatio"))
    # QA-specific I/O values, not claimed as 120F-8 parameter leaves.
    put("Include PCB", 0)
    put("Port Order", "["+" ".join(map(str, PORT_ORDER))+"]")
    put("z_p select", "[1 2]")
    put("RESULT_DIR", "results")
    require(used == set(scalar)|set(arrays)|set(ranges),
            "Unmapped C++ 120F-8 fields: " +
            str(sorted((set(scalar)|set(arrays)|set(ranges))-used)))
    return rows, {
        "status": "STATIC_CONTRACT_PASS",
        "mapped_source_leaves": 68,
        "package_case_order": ["Test 1", "Test 2"],
        "tx_package_mm": list(tx), "rx_package_mm": list(rx),
        "reference_commit": REFERENCE_COMMIT,
        "reference_source_sha256": REFERENCE_SHA,
        "source_profile": "IEEE Std 802.3ck-2022/120F.4.1/120F-8",
        "default_gamma0_not_source_verified": True,
        "aggressor_package_and_s4p_compliance": "NOT_QUALIFIED",
        "reference_config_no_com": True,
        "next_fext_zero_package_sentinels": True,
        "official_com_4p17_runtime": "NOT_RUN",
        "normative_com_db": None,
        "normative_pass_fail": None,
    }


def qualify_ieee_source_zip(archive: Path, channel: str) -> dict:
    """Validate external original ZIP/member hashes without extracting files."""
    require(channel in CHANNELS, "Only three explicitly audited channels")
    m = module_from_file("historical_c2c_source_contract", HISTORICAL_SOURCE)
    raw = archive.read_bytes()
    require(len(raw) <= m.MAX_DOWNLOAD and
            hashlib.sha256(raw).hexdigest() == m.CHANNEL_ZIP_SHA,
            "2019 IEEE C2C ZIP SHA-256 mismatch")
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        names = z.namelist()
        require(len(names) == len(set(names)), "Duplicate source entries")
        items = []
        for role, (path, sha) in zip(("thru","fext","next"),m.SOURCES[channel]):
            info = z.getinfo(path)
            require(info.file_size <= 10_000_000 and not info.is_dir(),
                    "Bad S4P ZIP member")
            member = z.read(path)
            require(hashlib.sha256(member).hexdigest() == sha,
                    f"{role} S4P member SHA mismatch")
            items.append({"role":role,"sha256":sha,"member":path})
    return {"status":"INPUT_SHA_PASS","channel_loss_label_db":channel,
            "port_order": list(PORT_ORDER),
            "source_zip_sha256":m.CHANNEL_ZIP_SHA,
            "thru_fext_next":items,
            "historical_only":True,
            "independent_distinct_fext_files":1,
            "fext_instances_in_historical_com":4,
            "next_instances_in_historical_com":0}


def actual_reference_parse(config: Path, module_dir: Path) -> dict:
    """Only real execution of a pinned independent COM 4.17 loader earns PASS."""
    require(module_dir.is_dir(), "Independent COM port checkout not found")
    sys.path.insert(0,str(module_dir))
    try:
        import sicopr
        from types import SimpleNamespace
        require(getattr(sicopr,"COM_MATLAB_VERSION",None) is not None,
                "4p17p0-compatible parser not found")
        sicopr.COM_MATLAB_VERSION = "4p17p0"
        op = SimpleNamespace(GET_FD=False, TDMODE=0,
                             CONFIG2MAT_ONLY=False, SAVE_CONFIG2MAT=False)
        p,parsed_op = sicopr.read_ParamConfigFile(str(config), op)
        import numpy as np
        require(getattr(parsed_op, "DO_NOT_COMPUTE_COM", None) == 1,
                "Unqualified reference config must prohibit COM computation")
        next_zp = np.asarray(p.z_p_next_cases,dtype=float)
        fext_zp = np.asarray(p.z_p_fext_cases,dtype=float)
        require(next_zp.shape == (2,4) and fext_zp.shape == (2,4) and
                np.all(next_zp == 0.) and np.all(fext_zp == 0.),
                "Unqualified aggressor package sentinels were modified")
        tx = np.asarray(p.z_p_tx_cases,dtype=float)
        rx = np.asarray(p.z_p_rx_cases,dtype=float)
        require(tx.ndim==2 and rx.ndim==2 and tx.shape[0]==2 and
                rx.shape[0]==2 and
                np.allclose(tx[:,:2], [[13.,1.8],[31.,1.8]]) and
                np.allclose(rx[:,:2], [[11.,1.8],[29.,1.8]]) and
                math.isclose(float(p.fb),53.125e9,rel_tol=1e-13) and
                int(p.ndfe)==6 and int(p.samples_per_ui)==32,
                "Independent COM 4.17 parser changed numeric values/case order")
    finally:
        sys.path.remove(str(module_dir))
    return {"status":"RUNTIME_LOAD_PASS",
            "loader":"PINNED_INDEPENDENT_SICOPR_4P17P0_PYTHON_PARSER",
            "reference_sha":"cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75",
            "official_ieee_matlab_run":"NOT_RUN",
            "computed_com_db":None,"normative_pass_fail":None}


def execute(channel: str, output_csv: Path,
            source_zip: Path|None=None,
            loader_checkout: Path|None=None) -> dict:
    require(channel in CHANNELS, "Only three source-pinned historical channels")
    rows, receipt = config_rows()
    output_csv.parent.mkdir(parents=True,exist_ok=True)
    with output_csv.open("w",encoding="utf-8",newline="") as stream:
        csv.writer(stream,lineterminator="\n").writerows(rows)
    data = output_csv.read_bytes()
    receipt["generated_csv_sha256"] = hashlib.sha256(data).hexdigest()
    receipt["csv_key_count"] = len(rows)
    receipt["source_inputs"] = (
        qualify_ieee_source_zip(source_zip, channel)
        if source_zip is not None else {"status":"NOT_RUN"})
    receipt["independent_parser_load"] = (
        actual_reference_parse(output_csv, loader_checkout)
        if loader_checkout is not None else {"status":"NOT_RUN"})
    require(receipt["normative_com_db"] is None and
            receipt["normative_pass_fail"] is None, "COM verdict is forbidden")
    return receipt


def main() -> int:
    cli=argparse.ArgumentParser()
    cli.add_argument("--channel",required=True,choices=CHANNELS)
    cli.add_argument("--output-csv",type=Path,required=True)
    cli.add_argument("--source-zip",type=Path)
    cli.add_argument("--loader-checkout",type=Path)
    args=cli.parse_args()
    try:
        result=execute(args.channel,args.output_csv,
                       args.source_zip,args.loader_checkout)
        print(json.dumps(result,ensure_ascii=False,sort_keys=True,indent=2))
        return 0
    except (ContractError,ValueError,OSError,KeyError,IndexError,zipfile.BadZipFile) as error:
        print("012_12 FAIL_CLOSED:",str(error),file=sys.stderr)
        return 2


if __name__=="__main__":
    raise SystemExit(main())
