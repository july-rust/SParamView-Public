#!/usr/bin/env python3
"""Compare independently computed *historical candidate* C2C COM numerics.

This validates two externally maintained implementations on exactly the same
SHA-pinned 2019 C2C channels and 2022 sponsor-era workbook. It does NOT
qualify final 802.3ck-2022 Table 120F-8 or SParamView's C++ COM solver.
"""
import argparse
import json
import math
from pathlib import Path

from com_3ck_c2c_real_reference import (
    CONFIG_SHA, CHANNEL_ZIP_SHA, SOURCES, SICOPR_COMMIT
)

COM_TOL_DB = 1e-4
FOM_TOL_DB = 1e-4


def validate_correlation(py: dict, octave: dict, channel: str) -> dict:
    if channel not in SOURCES:
        raise ValueError("Unpinned C2C channel label")
    if py.get("status") != "HISTORICAL_8023CK_C2C_INDEPENDENT_PYTHON_REFERENCE_CANDIDATE":
        raise ValueError("Python reference not a successful candidate")
    if octave.get("status") != "HISTORICAL_OCTAVE_4P17P0_INDEPENDENT_CANDIDATE_ONLY":
        raise ValueError("Octave reference not a successful candidate")
    if (py.get("channel_condition_db_label") != channel or
        octave.get("channel_loss_label") != channel):
        raise ValueError("Channel mismatch")
    if (py.get("reference_engine_commit") != SICOPR_COMMIT or
        octave.get("engine_commit") != SICOPR_COMMIT):
        raise ValueError("Different independent reference revisions")
    if (py.get("official_2022_workbook_sha256") != CONFIG_SHA or
        octave.get("official_historical_workbook_sha256") != CONFIG_SHA or
        py.get("official_2019_c2c_archive_sha256") != CHANNEL_ZIP_SHA or
        octave.get("official_2019_channel_archive_sha256") != CHANNEL_ZIP_SHA):
        raise ValueError("Source SHA mismatch")
    pmeta=py.get("historical_workbook_settings_conversion",{})
    ometa=octave.get("validated_config",{})
    if (pmeta.get("csv_sha256") != ometa.get("csv_sha256") or
        pmeta.get("source_workbook_sha256") != CONFIG_SHA or
        ometa.get("source_workbook_sha256") != CONFIG_SHA):
        raise ValueError("Unmatched source configuration conversion")
    pport=py.get("temporary_verified_physical_port_order_override",{})
    oport=octave.get("temporary_source_verified_port_order",{})
    if (pport.get("ephemeral_corrected_config_csv_sha256") !=
        oport.get("ephemeral_corrected_config_csv_sha256") or
        pport.get("effective_port_order") != "[1 2 3 4]" or
        oport.get("effective_port_order") != "[1 2 3 4]"):
        raise ValueError("Unmatched evidence-gated physical port mapping")
    if (py.get("input_fext_count")!=4 or py.get("input_next_count")!=0 or
        octave.get("aggressor_fext_count")!=4 or
        octave.get("aggressor_next_count")!=0 or
        octave.get("aggressor_distinct_fext_source_files")!=1):
        raise ValueError("FEXT/NEXT roles differ")
    if (py.get("qualified_final_8023ck_table") is not False or
        py.get("same_phy_cpp_com_comparison") is not False or
        octave.get("normative_final_8023ck_qualified") is not False or
        octave.get("sparamview_cpp_com_evaluated") is not False or
        octave.get("nonreal_fom_or_com_detected") is not False):
        raise ValueError("Unsupported normative/C++ claim or complex Octave result")
    psource=py.get("input_member_provenance",[])
    osource=octave.get("original_s4p_provenance",[])
    expected=[{"archive_member":name,"sha256":digest}
              for name,digest in SOURCES[channel]]
    for source in (psource,osource):
        if len(source)!=len(expected):
            raise ValueError("Missing source member")
        if [{"archive_member":v.get("archive_member"),
             "sha256":v.get("sha256")} for v in source] != expected:
            raise ValueError("Distinct S4P source hash mismatch")
    p_cases=py.get("package_cases",[])
    o_cases=octave.get("package_cases",[])
    if len(p_cases)!=2 or len(o_cases)!=2:
        raise ValueError("Both package cases required")
    cases=[]
    for index,(p,o) in enumerate(zip(p_cases,o_cases),start=1):
        if (p.get("case")!=index or o.get("package_case")!=index or
            o.get("valid_real_com_and_fom") is not True):
            raise ValueError("Non-real, missing or misordered Test1/Test2")
        if (o.get("observed_com_imag_component")!=0 or
            o.get("observed_fom_imag_component")!=0):
            raise ValueError("Octave complex COM or FOM")
        v1=float(p["reference_com_db"])
        v2=float(o["observed_com_real_component_db"])
        f1=float(p["reference_fom"])
        f2=float(o["observed_fom_real_component_db"])
        if not all(math.isfinite(v) for v in (v1,v2,f1,f2)):
            raise ValueError("Nonfinite COM/FOM")
        if abs(v1-v2)>COM_TOL_DB or abs(f1-f2)>FOM_TOL_DB:
            raise ValueError("Independent COM/FOM numerical mismatch")
        cases.append({"package_case":index,
                      "python_com_db":v1,
                      "octave_com_db":v2,
                      "delta_com_db":v1-v2,
                      "python_fom_db":f1,
                      "octave_fom_db":f2,
                      "delta_fom_db":f1-f2})
    return {
        "status":"HISTORICAL_SAME_INPUT_PYTHON_OCTAVE_CANDIDATE_CORRELATION",
        "loss_label_db":channel,
        "package_cases":cases,
        "com_tolerance_db":COM_TOL_DB,
        "fom_tolerance_db":FOM_TOL_DB,
        "historical_workbook_sha256":CONFIG_SHA,
        "source_archive_sha256":CHANNEL_ZIP_SHA,
        "corrected_port_map_sha256":
            pport["ephemeral_corrected_config_csv_sha256"],
        "three_channel_correlation_complete":False,
        "final_ieee_8023ck_2022_qualified":False,
        "native_cpp_com_qualified":False,
        "source_files_redistributed":False
    }


if __name__=="__main__":
    cli=argparse.ArgumentParser()
    cli.add_argument("--channel",choices=sorted(SOURCES),required=True)
    cli.add_argument("--python-reference",type=Path,required=True)
    cli.add_argument("--octave-reference",type=Path,required=True)
    cli.add_argument("--output",type=Path,required=True)
    a=cli.parse_args()
    try:
        p=json.loads(a.python_reference.read_text(encoding="utf-8"))
        o=json.loads(a.octave_reference.read_text(encoding="utf-8"))
        result=validate_correlation(p,o,a.channel)
        a.output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
        print("HISTORICAL_C2C_SAME_INPUT_INDEPENDENT_CORRELATION:",
              json.dumps(result),flush=True)
    except (ValueError,OSError,KeyError,TypeError) as exc:
        import sys
        print("HISTORICAL_C2C_INDEPENDENT_NUMERICAL_MISMATCH:",
              type(exc).__name__,str(exc),file=sys.stderr)
        raise SystemExit(2)
