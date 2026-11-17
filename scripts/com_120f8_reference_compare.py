#!/usr/bin/env python3
"""012_13: strict identical-input SiCoPR Python vs COM4p17 Octave numerical QA.

This is a six-case reference numerical correlation for the precise
source-verified FINAL Table 120F-8 values + separately sourced 2020 draft
aggressor topology. A correlation is not normative PHY certification.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path
import sys
from com_120f8_reference_candidate import (
    FEXT_PACKAGE, NEXT_PACKAGE, ROLE_SOURCE, SICOPR_COMMIT, SOURCE_CHANNELS,
)
from com_120f8_config_contract import (
    REFERENCE_COMMIT, REFERENCE_SHA
)
from com_3ck_c2c_real_reference import CHANNEL_ZIP_SHA, SOURCES

COM_TOL_DB = 1e-4
FOM_TOL_DB = 1e-4
STATUS = "FINAL_120F8_WITH_HISTORICAL_AGGRESSOR_NUMERIC_CORRELATED"


def require(ok: bool, message: str) -> None:
    if not ok:
        raise ValueError("012_13 INDEPENDENT_NUMERIC_FAIL_CLOSED: " + message)


def validate_source_receipt(d: dict, channel: str, engine: str) -> tuple[str,list]:
    require(isinstance(d,dict), "missing reference receipt")
    require(d.get("status") ==
            "FINAL_120F8_PARAMETER_SET_HISTORICAL_AGGRESSOR_CANDIDATE",
            "unsupported Python/Octave reference receipt")
    require(d.get("engine") == engine and
            d.get("independent_code_commit") == SICOPR_COMMIT and
            d.get("historical_channel_loss_label_db") == channel and
            d.get("historical_ieee_channel_zip_sha256") == CHANNEL_ZIP_SHA,
            "reference engine/source identity mismatch")
    require(d.get("verified_original_thru_port_order") == [1,2,3,4],
            "unverified physical source port mapping")
    require(d.get("final_ieee_normative_phy_qualified") is False and
            d.get("native_sparamview_cpp_qualified") is False and
            d.get("official_ieee_matlab_executed") is False and
            d.get("license_source_files_uploaded") is False,
            "false normative, official-code, C++ or source-redistribution claim")
    actual = d.get("historical_ieee_source_sha256_members")
    require(isinstance(actual,list) and len(actual)==3,
            "missing original THRU/FEXT/NEXT provenance")
    for found,(name,digest),role in zip(
        actual, SOURCES[channel], ("thru","fext","next")
    ):
        require(isinstance(found,dict) and
                found.get("archive_member") == name and
                found.get("sha256") == digest and
                found.get("role") == role,
                "original S4P member SHA/name/role mismatch")
    s=d.get("settings")
    require(isinstance(s,dict), "missing effective final CSV settings receipt")
    require(s.get("source_120f8_68_leaves_verified") is True and
            s.get("source_profile") ==
                "IEEE Std 802.3ck-2022/120F.4.1/120F-8" and
            s.get("external_engine_matlab_4p17_source_commit") ==
                REFERENCE_COMMIT and
            s.get("historical_aggressor_model_source") == ROLE_SOURCE and
            s.get("historical_aggressor_model_source_status") ==
                "DRAFT_2020_NOT_NORMATIVE_FINAL" and
            s.get("effective_next_package") == NEXT_PACKAGE and
            s.get("effective_fext_package") == FEXT_PACKAGE and
            s.get("package_tau_adapter_ns_per_mm") == 0.006141 and
            s.get("historical_fixture_fext_count") == 4 and
            s.get("historical_fixture_fext_distinct_originals") == 1 and
            s.get("historical_fixture_next_count") == 0 and
            s.get("run_com_not_safety_disabled") is True and
            s.get("normative_phy_pass_fail") is None and
            s.get("source_s4p_files_redistributed") is False,
            "incorrect final parameter / aggressor model contract")
    from com_120f8_reference_candidate import configure
    from tempfile import TemporaryDirectory
    with TemporaryDirectory() as folder:
        _,expected=configure(Path(folder))
    require(s.get("immutable_base_csv_sha256")==
                expected["immutable_base_csv_sha256"] and
            s.get("effective_csv_sha256")==expected["effective_csv_sha256"],
            "effective numerical input not exactly source-qualified")
    cases=d.get("package_cases")
    require(isinstance(cases,list) and len(cases)==2,
            "Test1/Test2 both required")
    return expected["effective_csv_sha256"], cases


def correlate(py: dict,octave: dict, channel: str) -> dict:
    require(channel in SOURCE_CHANNELS,"unsupported historical source group")
    csv_py,cases_py=validate_source_receipt(py,channel,"python")
    csv_oc,cases_oc=validate_source_receipt(octave,channel,"octave")
    require(csv_py==csv_oc,"Python/Octave effective CSV SHA mismatch")
    results=[]
    for index,(p,o) in enumerate(zip(cases_py,cases_oc),start=1):
        require(isinstance(p,dict) and isinstance(o,dict) and
                p.get("package_case")==o.get("package_case")==index,
                "incorrect package Test1/Test2 alignment")
        values=[]
        for record in (p,o):
            for field in ("reference_com_db","reference_fom_db"):
                value=record.get(field)
                require(type(value) in (int,float) and math.isfinite(value),
                        "missing, non-real, or nonfinite COM/FOM")
                values.append(float(value))
        py_com,py_fom,oc_com,oc_fom=values
        require(abs(py_com-oc_com)<=COM_TOL_DB and
                abs(py_fom-oc_fom)<=FOM_TOL_DB,
                "independent reference numerical mismatch")
        results.append({
            "package_case":index,
            "python_com_db":py_com,
            "octave_com_db":oc_com,
            "com_difference_db":py_com-oc_com,
            "python_fom_db":py_fom,
            "octave_fom_db":oc_fom,
            "fom_difference_db":py_fom-oc_fom,
            "normative_phy_pass_fail":None,
        })
    return {
        "status":STATUS, "channel":channel,
        "package_cases":results,
        "effective_final_parameter_historical_model_csv_sha256":csv_py,
        "final_ieee_8023ck_2022_table_120f8_parameter_values":True,
        "source_2020_draft_aggressor_model_not_final_normative":True,
        "historical_2019_ieee_channels_only":True,
        "python_independent_reference_commit":SICOPR_COMMIT,
        "official_matlab_reference_source_commit_for_compatibility":
            REFERENCE_COMMIT,
        "com_tolerance_db":COM_TOL_DB, "fom_tolerance_db":FOM_TOL_DB,
        "official_matlab_executed":False,
        "native_cpp_com_qualified":False,
        "final_ieee_phy_compliance_certified":False,
        "normative_phy_pass_fail":None,
        "external_source_files_uploaded":False
    }


def main() -> int:
    cli=argparse.ArgumentParser()
    cli.add_argument("--channel",required=True,choices=SOURCE_CHANNELS)
    cli.add_argument("--python-result",type=Path,required=True)
    cli.add_argument("--octave-result",type=Path,required=True)
    cli.add_argument("--output",type=Path,required=True)
    args=cli.parse_args()
    try:
        py=json.loads(args.python_result.read_text(encoding="utf-8"))
        oc=json.loads(args.octave_result.read_text(encoding="utf-8"))
        result=correlate(py,oc,args.channel)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_text(json.dumps(result,sort_keys=True,indent=2,
                                          allow_nan=False)+"\n",
                               encoding="utf-8")
        print("012_13 CANDIDATE_REFERENCE_CORRELATION:",
              json.dumps({"channel":args.channel,
                          "cases":result["package_cases"],
                          "normative":False},allow_nan=False),flush=True)
        return 0
    except (OSError,ValueError,KeyError,TypeError,
            json.JSONDecodeError) as exc:
        print("012_13 INDEPENDENT_NUMERIC_FAIL_CLOSED:",
              str(exc),file=sys.stderr,flush=True)
        return 2

if __name__=="__main__":
    raise SystemExit(main())
