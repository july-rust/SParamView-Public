#!/usr/bin/env python3
"""Fail-closed COM reference manifest checks (NOT a COM numerical solver).

Schema checks are safe for a BLOCKED scaffold. Qualification requires a pinned
reference, license evidence and three physically present independently sourced
reference cases. A PASS here verifies evidence integrity and numeric comparison,
not IEEE compliance or the correctness of the original external reference.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
import re
from pathlib import Path

SHA256 = re.compile(r"^[0-9a-f]{64}$")
COMMIT = re.compile(r"^[0-9a-f]{40}$")
VALID_STATES = {"BLOCKED", "REFERENCE_READY", "LOCKED"}

class QualificationError(ValueError):
    pass

def _require(test: bool, reason: str) -> None:
    if not test:
        raise QualificationError(reason)

def _dict(value: object, label: str) -> dict:
    _require(isinstance(value, dict), f"{label}: expected object")
    return value

def schema_check(manifest: dict) -> None:
    _require(manifest.get("contract_version") == 1, "contract_version must be 1")
    _require(manifest.get("status") in VALID_STATES, "invalid status")
    _require(isinstance(manifest.get("profile_id"), str) and bool(manifest["profile_id"]), "profile_id missing")
    for name in ("standard", "reference", "license"):
        _dict(manifest.get(name), name)
    _require(isinstance(manifest.get("cases"), list), "cases must be an array")

def _evidence_file(item: object, root: Path, label: str) -> Path:
    spec = _dict(item, label)
    relative = spec.get("path")
    digest = spec.get("sha256")
    _require(isinstance(relative, str) and bool(relative), f"{label}: path missing")
    _require(isinstance(digest, str) and SHA256.fullmatch(digest) is not None, f"{label}: SHA256 absent/invalid")
    path = Path(relative)
    _require(not path.is_absolute(), f"{label}: absolute paths prohibited")
    root = root.resolve(strict=True)
    resolved = (root / path).resolve(strict=True)
    _require(resolved.is_relative_to(root) and resolved.is_file(), f"{label}: path escapes fixture root or is not file")
    actual = hashlib.sha256(resolved.read_bytes()).hexdigest()
    _require(actual == digest, f"{label}: SHA256 mismatch")
    return resolved

def _finite(value: object, label: str) -> float:
    _require(type(value) in (int, float) and math.isfinite(value), f"{label}: expected finite number")
    return float(value)

def reference_ready(manifest: dict) -> dict:
    """012_02 stage gate: source/reproducible 2-case oracle, NOT PHY compliance.

    The CI's numerical job must independently execute and prove the evidence
    linked here. Manifest fields alone are never a substitute for actual runs.
    """
    schema_check(manifest)
    _require(manifest["status"] == "REFERENCE_READY", "reference stage is not REFERENCE_READY")
    std = manifest["standard"]
    for key in ("edition", "annex", "applicable_clause", "parameter_table"):
        _require(isinstance(std.get(key), str) and bool(std[key].strip()), f"standard.{key} missing")
    _require(std.get("approval_status") == "PUBLISHED", "draft IEEE PHY standard not approved")
    _require(str(std.get("normative_source_url", "")).startswith("https://standards.ieee.org/ieee/802.3ck/"),
             "normative source must be IEEE 802.3ck published catalog")
    _require(std.get("parameter_values_qualified") is False,
             "reference readiness does not qualify the final PHY numeric parameter table")
    ref = manifest["reference"]
    _require(isinstance(ref.get("source_commit"), str) and COMMIT.fullmatch(ref["source_commit"]) is not None,
             "official COM reference source not pinned")
    _require(str(ref.get("release")) == "4.17.0", "unexpected reference version")
    _require(isinstance(ref.get("source_content_sha256"), str) and SHA256.fullmatch(ref["source_content_sha256"]) is not None,
             "source bytes SHA-256 not pinned")
    lic = manifest["license"]
    _require(lic.get("verified_spdx") == "BSD-3-Clause",
             "BSD3 COM source repository license required")
    _require(isinstance(lic.get("confirmed_root_license_sha256"), str) and
             SHA256.fullmatch(lic["confirmed_root_license_sha256"]) is not None,
             "BSD3 license SHA-256 must be pinned")
    _require(lic.get("distribution_strategy") == "QA_ONLY_REFERENCE_EXTERNAL" and
             lic.get("bundled_third_party_sources") is False and
             lic.get("bundled_third_party_channel_files") is False,
             "do not redistribute third-party code/channel data under an unreviewed license")
    oracle = _dict(manifest.get("independent_oracle"), "independent_oracle")
    _require(isinstance(oracle.get("source_commit"), str) and COMMIT.fullmatch(oracle["source_commit"]) is not None,
             "independent code commit not pinned")
    _require(oracle.get("source_license_spdx") == "BSD-3-Clause", "independent source license not pinned")
    _require(isinstance(oracle.get("channels_zip_sha256"), str) and SHA256.fullmatch(oracle["channels_zip_sha256"]) is not None,
             "external channel data ZIP SHA256 not pinned")
    for key in ("expected_results_url", "execution_run_url", "evidence_artifact_url", "channels_url"):
        _require(str(oracle.get(key, "")).startswith("https://"),
                 f"independent_oracle.{key} missing")
    _require(oracle.get("execution_result") == "SUCCESS", "no successful actual numerical run")
    _require("NOT IEEE 802.3ck normative conformance" in oracle.get("scope", ""),
             "non-normative correlation must be explicitly labeled")
    cases = manifest.get("reference_oracles")
    _require(isinstance(cases, list) and len(cases) >= 2, "two actual reference comparisons required")
    profiles = set()
    for item in cases:
        case = _dict(item, "reference_oracle")
        _require(case.get("kind") == "ALGORITHM_CORRELATION_NON_NORMATIVE",
                 "normative and algorithm oracles must not be conflated")
        _require(isinstance(case.get("id"), str) and bool(case["id"]), "case ID missing")
        counts = _dict(case.get("aggressor_count"), "aggressor_count")
        nex, fex = counts.get("next"), counts.get("fext")
        _require(type(nex) is int and type(fex) is int and nex >= 0 and fex >= 0,
                 "aggressor counts invalid")
        profile = "WITH_CROSSTALK" if nex + fex > 0 else "NO_CROSSTALK"
        _require(profile not in profiles, "duplicate independent case condition")
        profiles.add(profile)
        for metric in ("com_db", "fom"):
            expected = _finite(case.get("published_" + metric), "published_" + metric)
            measured = _finite(case.get("ci_reproduced_" + metric), "ci_reproduced_" + metric)
            _require(abs(expected - measured) <= 1e-9, "independent numerical correlation mismatch")
        tolerance = _finite(case.get("tolerance_db"), "tolerance_db")
        _require(0 < tolerance <= 1e-9, "published reference tolerance not locked to strict scalar gate")
    _require(profiles == {"NO_CROSSTALK", "WITH_CROSSTALK"}, "both crosstalk conditions required")
    _require(manifest.get("cases") == [], "normative IEEE conformance cases must remain separate")
    return {"status": "REFERENCE_READY_NOT_IEEE_CONFORMANCE",
            "independent_numeric_cases": len(cases),
            "remaining": "012_09/11 require final approved PHY full table/Test1+Test2 and >=3 same-profile goldens"}


def qualify(manifest: dict, root: Path) -> dict:
    """Check provenance and reference arithmetic; never claim full COM compliance."""
    schema_check(manifest)
    _require(manifest["status"] == "LOCKED", "COM profile is BLOCKED: no qualification")
    std = manifest["standard"]
    for key in ("edition", "annex", "applicable_clause", "parameter_table"):
        _require(isinstance(std.get(key), str) and bool(std[key].strip()), f"standard.{key} missing")
    _require(std.get("approval_status") == "PUBLISHED",
             "unapproved IEEE drafts cannot be used for conformance profiles")
    official_url = std.get("normative_source_url")
    _require(isinstance(official_url, str) and official_url.startswith("https://standards.ieee.org/"),
             "standard.normative_source_url must reference a published IEEE standard")
    ref = manifest["reference"]
    _require(isinstance(ref.get("source_commit"), str) and COMMIT.fullmatch(ref["source_commit"]) is not None,
             "reference.source_commit requires a pinned 40-character SHA")
    _require(isinstance(ref.get("release"), str) and bool(ref["release"]), "reference.release missing")
    _require(str(ref.get("source_url", "")).startswith("https://"), "reference.source_url must be HTTPS")
    lic = manifest["license"]
    _require(lic.get("verified_spdx") in ("BSD-3-Clause", "Apache-2.0", "MIT"),
             "license not independently verified and approved")
    _evidence_file(lic.get("evidence"), root, "license.evidence")
    cases = manifest["cases"]
    _require(len(cases) >= 3, "at least three independently reproducible cases required")
    seen = set()
    details = []
    observed_kinds = set()
    for case in cases:
        c = _dict(case, "case")
        case_id = c.get("id")
        _require(isinstance(case_id, str) and case_id and case_id not in seen, "duplicate/missing case id")
        seen.add(case_id)
        _require(str(c.get("published_source_url", "")).startswith("https://"),
                 f"{case_id}: independent published source URL required")
        assets = c.get("input_files")
        _require(isinstance(assets, list) and len(assets) >= 2, f"{case_id}: THRU/config input required")
        kinds = {str(_dict(a, "input").get("kind")) for a in assets}
        _require({"thru", "config"} <= kinds, f"{case_id}: missing THRU/config")
        observed_kinds |= kinds
        for index, asset in enumerate(assets):
            _evidence_file(asset, root, f"{case_id}.input_files[{index}]")
        run_file = _evidence_file(c.get("reproduced_result"), root, f"{case_id}.reproduced_result")
        run = _dict(json.loads(run_file.read_text(encoding="utf-8")), f"{case_id}.result")
        result = _finite(run.get("com_db"), f"{case_id}.result.com_db")
        _require(run.get("reference_commit") == ref["source_commit"], f"{case_id}: reference commit mismatch")
        _require(run.get("profile_id") == manifest["profile_id"], f"{case_id}: profile mismatch")
        expected = _finite(c.get("published_expected_com_db"), f"{case_id}.published_expected_com_db")
        tolerance = _finite(c.get("tolerance_db"), f"{case_id}.tolerance_db")
        _require(tolerance >= 0, f"{case_id}: tolerance cannot be negative")
        _require(str(c.get("tolerance_basis_url", "")).startswith("https://"),
                 f"{case_id}: independent tolerance basis URL required")
        delta = abs(result - expected)
        _require(delta <= tolerance, f"{case_id}: numerical deviation {delta} dB exceeds {tolerance} dB")
        details.append({"id": case_id, "delta_db": delta, "tolerance_db": tolerance})
    _require({"next", "fext"} <= observed_kinds,
             "qualified set must include both NEXT and FEXT inputs")
    return {"status": "MANIFEST_AND_REFERENCE_COMPARISON_PASS",
            "limitations": "Not a certification of IEEE conformance or source independence",
            "cases": details}

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--root", required=True, type=Path)
    parser.add_argument("--mode", choices=("schema", "reference-ready", "qualify"), default="qualify")
    args = parser.parse_args()
    try:
        manifest = _dict(json.loads(args.manifest.read_text(encoding="utf-8")), "manifest")
        if args.mode == "schema":
            schema_check(manifest)
            print("SCHEMA PASS (COM numerical qualification NOT claimed)")
        elif args.mode == "reference-ready":
            print(json.dumps(reference_ready(manifest), indent=2))
        else:
            print(json.dumps(qualify(manifest, args.root), indent=2))
        return 0
    except (QualificationError, OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"BLOCKED/FAIL: {exc}")
        return 2

if __name__ == "__main__":
    raise SystemExit(main())
