#!/usr/bin/env python3
"""012_13: bounded final-profile reference numerical evidence safety gate.

This intentionally does not modify or emulate a COM optimizer. 012_12 emits
NO_COM=1 and nonphysical NEXT/FEXT package sentinels: real parser success
must NEVER be promoted to a final 802.3ck COM numerical golden.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import sys

import com_120f8_config_contract as cfg

LOSSES = ("10", "16", "18")
REFERENCE_LOADER = "cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75"


def require(ok: bool, reason: str) -> None:
    if not ok:
        raise ValueError("012_13 FAIL_CLOSED: " + reason)


def expected_csv_sha256() -> str:
    rows, source = cfg.config_rows()
    require(source["status"] == "STATIC_CONTRACT_PASS" and
            source["mapped_source_leaves"] == 68 and
            source["reference_config_no_com"] is True and
            source["next_fext_zero_package_sentinels"] is True and
            source["aggressor_package_and_s4p_compliance"] == "NOT_QUALIFIED",
            "final 120F source contract or NO_COM safety barrier changed")
    out = io.StringIO(newline="")
    csv.writer(out, lineterminator="\n").writerows(rows)
    return hashlib.sha256(out.getvalue().encode("utf-8")).hexdigest()


def assess(receipts: list[dict]) -> dict:
    """Verify all three actual 012_12 execution receipts, emit six BLOCKED cases."""
    require(len(receipts) == 3, "exactly three channel receipts required")
    csv_digest = expected_csv_sha256()
    historical = cfg.module_from_file("012_13_pinned_historical", cfg.HISTORICAL_SOURCE)
    cases = []
    seen: set[str] = set()
    for receipt in receipts:
        require(isinstance(receipt, dict), "malformed input receipt")
        inp = receipt.get("source_inputs")
        loader = receipt.get("independent_parser_load")
        require(isinstance(inp, dict) and isinstance(loader, dict),
                "missing actual input SHA or independent loader evidence")
        loss = inp.get("channel_loss_label_db")
        require(loss in LOSSES and loss not in seen,
                "unexpected or duplicated source channel")
        seen.add(loss)
        require(receipt.get("status") == "STATIC_CONTRACT_PASS" and
                receipt.get("mapped_source_leaves") == 68 and
                receipt.get("generated_csv_sha256") == csv_digest and
                receipt.get("reference_commit") == cfg.REFERENCE_COMMIT and
                receipt.get("reference_source_sha256") == cfg.REFERENCE_SHA and
                receipt.get("source_profile") ==
                    "IEEE Std 802.3ck-2022/120F.4.1/120F-8",
                "final CSV content, standard identity, or source mapping drift")
        require(receipt.get("package_case_order") == ["Test 1", "Test 2"] and
                receipt.get("tx_package_mm") == [13.0, 31.0] and
                receipt.get("rx_package_mm") == [11.0, 29.0],
                "incorrect package case order")
        require(receipt.get("reference_config_no_com") is True and
                receipt.get("next_fext_zero_package_sentinels") is True and
                receipt.get("aggressor_package_and_s4p_compliance") ==
                    "NOT_QUALIFIED" and
                receipt.get("normative_com_db") is None and
                receipt.get("normative_pass_fail") is None and
                receipt.get("official_com_4p17_runtime") == "NOT_RUN",
                "unsupported final numerical result or COM promotion")
        require(inp.get("status") == "INPUT_SHA_PASS" and
                inp.get("source_zip_sha256") == historical.CHANNEL_ZIP_SHA and
                inp.get("historical_only") is True and
                inp.get("port_order") == [1, 2, 3, 4] and
                inp.get("independent_distinct_fext_files") == 1 and
                inp.get("fext_instances_in_historical_com") == 4 and
                inp.get("next_instances_in_historical_com") == 0,
                "source identity or historical role provenance unverified")
        members = inp.get("thru_fext_next")
        require(isinstance(members, list) and len(members) == 3,
                "incomplete historical THRU/FEXT/NEXT")
        for member, role, (name, digest) in zip(
            members, ("thru", "fext", "next"), historical.SOURCES[loss]
        ):
            require(isinstance(member, dict) and
                    member.get("role") == role and
                    member.get("member") == name and
                    member.get("sha256") == digest,
                    "original S4P member SHA or role changed")
        require(loader.get("status") == "RUNTIME_LOAD_PASS" and
                loader.get("reference_sha") == REFERENCE_LOADER and
                loader.get("loader") ==
                    "PINNED_INDEPENDENT_SICOPR_4P17P0_PYTHON_PARSER" and
                loader.get("official_ieee_matlab_run") == "NOT_RUN" and
                loader.get("computed_com_db") is None and
                loader.get("normative_pass_fail") is None,
                "missing real pinned parser execution or forged numerical output")
        for number in (1, 2):
            cases.append({
                "historical_source_loss_label_db": loss,
                "package_test": number,
                "final_profile_csv_sha256": csv_digest,
                "reference_com_db": None,
                "reference_fom_db": None,
                "official_ieee_matlab_run": "NOT_RUN",
                "native_cpp_com_comparison": "NOT_RUN",
                "normative_pass_fail": None,
                "status": "NOT_EVALUATED",
                "reason": "NO_COM_1_AND_UNQUALIFIED_AGGRESSOR_PACKAGE",
            })
    require(seen == set(LOSSES), "missing historical source channel")
    cases.sort(key=lambda c: (int(c["historical_source_loss_label_db"]),
                              c["package_test"]))
    return {
        "task": "012_13",
        "status": "FINAL_120F8_NUMERICAL_BLOCKED",
        "config_and_original_s4p_provenance": "PASS",
        "actual_independent_reference_csv_parse": "PASS",
        "final_profile_numeric_computation": "NOT_RUN",
        "final_120f_golden_qualified": False,
        "normative_pass_fail": None,
        "cases": cases,
        "case_count": 6,
        "source_csv_sha256": csv_digest,
        "historical_input_only": True,
        "blocker": "NO_COM=1 and zero NEXT/FEXT packaging are parser QA sentinels; "
                   "final-profile COM/FOM cannot be computed or qualified.",
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        data = assess([json.loads(p.read_text(encoding="utf-8"))
                       for p in args.receipt])
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n",
                               encoding="utf-8")
        print("012_13 boundary PASS: 6 final-profile cases NOT_EVALUATED; "
              "no misleading numerical golden or PHY verdict")
        return 0
    except (ValueError, KeyError, TypeError, OSError, json.JSONDecodeError) as exc:
        print("012_13 NUMERICAL_GATE_FAIL:", str(exc), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
