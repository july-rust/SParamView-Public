#!/usr/bin/env python3
"""Historical C2C GNU Octave comparison, deliberately non-normative.

Loads only SHA-pinned public IEEE engineering input and a SHA-pinned external
BSD-3 COM 4p17p0 Octave-compatible reference. Never copies external files
into SParamView source, release, or artifacts. No C++ COM margin is claimed.
"""
from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path
import re
import subprocess
import sys
import tempfile

import numpy as np
import scipy.io

from com_3ck_c2c_real_reference import (
    CHANNEL_ZIP_SHA, CONFIG_SHA, SICOPR_COMMIT,
    canonical_config_csv, prepare, reference_2019_c2c_inputs, sha
)

NUMERIC = re.compile(r"[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\Z")


def csv_to_octave_mat(csv_path: Path, result: Path) -> dict:
    """Convert verified numeric/text COM_Settings grid to MATLAB cell array.

    Equivalent to xlsread(...,'COM_Settings') raw: numeric -> double, blank ->
    NaN, textual MATLAB vectors remain strings. No formulas, no eval.
    """
    with csv_path.open(newline="", encoding="utf-8-sig") as stream:
        rows = list(csv.reader(stream))
    if not rows or len(rows) > 10000:
        raise ValueError("Octave COM grid empty or excessive")
    width = max(len(x) for x in rows)
    if width > 512:
        raise ValueError("Octave COM grid excessive width")
    grid = np.empty((len(rows), width), dtype=object)
    grid[:] = float("nan")
    numeric_count = 0
    for ri,row in enumerate(rows):
        for ci,raw in enumerate(row):
            if not raw.strip():
                continue
            if len(raw) > 65536:
                raise ValueError("Oversized COM cell")
            if NUMERIC.fullmatch(raw.strip()):
                value = float(raw)
                if not math.isfinite(value):
                    raise ValueError("Nonfinite COM numeric source")
                grid[ri,ci] = value
                numeric_count += 1
            elif raw.lstrip().startswith("="):
                raise ValueError("COM source contains unexpanded formula")
            else:
                grid[ri,ci] = raw
    scipy.io.savemat(str(result), {"parameter":grid},
                     format="5", do_compression=False, oned_as="row")
    saved = scipy.io.loadmat(str(result))["parameter"]
    if saved.shape != grid.shape or saved.dtype != object:
        raise ValueError("COM MAT round-trip shape/type failure")
    return {"rows":len(rows),"columns":width,"numeric_cells":numeric_count,
            "source_csv_sha256":sha(csv_path.read_bytes()),
            "mat_sha256":sha(result.read_bytes()),
            "conversion":"EXACT_SCIPY_OBJECT_MATLAB_CELL_FROM_VERIFIED_CSV"}


def octave_case(channel: str, external_repo: Path, output: Path) -> int:
    code = external_repo / "octave" / "com_ieee8023_4p17p0_octave_compat.m"
    if not code.is_file():
        raise ValueError("Pinned Octave-compatible 4p17p0 implementation missing")
    entry = "com_ieee8023_4p17p0_octave_compat"
    with tempfile.TemporaryDirectory(prefix="c2c-independent-octave-") as d:
        root = Path(d)
        workbook,files,provenance = prepare(channel,root)
        csv_path=root/"config_strict_verified.csv"
        config_meta=canonical_config_csv(workbook,csv_path)
        mat=root/"c2c.mat"
        mat_meta=csv_to_octave_mat(csv_path,mat)
        num_fext,num_next,ordered=reference_2019_c2c_inputs(files)
        if (num_fext,num_next)!=(4,0) or len(ordered)!=5:
            raise ValueError("Historical C2C 4xFEXT/0xNEXT contract broken")
        artifact=root/"octave_result.mat"
        # Paths are generated locally in ASCII-named temporary directories.
        # The upstream reference builds the MAT config path with a literal
        # backslash on Unix when passed an absolute filename. Give only
        # basenames in the runner's working directory; no source is altered.
        quote=lambda s: "'" + str(s).replace("'","''") + "'"
        args=",".join([quote(mat.name),str(num_fext),str(num_next)]+
                      [quote(p.name) for p in ordered])
        statement=(
            "more off; addpath("+quote(external_repo/"octave")+"); "
            "r="+entry+"("+args+"); "
            "save('-v7',"+quote(artifact.name)+",'r'); exit(0);"
        )
        command=["octave-cli","--no-gui","--no-window-system",
                 "--quiet","--eval",statement]
        try:
            proc=subprocess.run(command,cwd=root,text=True,errors="replace",
                                capture_output=True,timeout=2400,check=False)
        except subprocess.TimeoutExpired:
            print("HISTORICAL_OCTAVE_REFERENCE_BLOCKED: timed out",
                  file=sys.stderr,flush=True)
            return 2
        if proc.returncode!=0 or not artifact.exists():
            # Publish only bounded diagnostics, never external input/code bytes.
            print("HISTORICAL_OCTAVE_REFERENCE_BLOCKED:",
                  json.dumps({"exit_code":proc.returncode,
                              "stdout_tail":proc.stdout[-1200:],
                              "stderr_tail":proc.stderr[-1200:]},
                             ensure_ascii=False),file=sys.stderr,flush=True)
            return 2
        observed=scipy.io.loadmat(str(artifact), squeeze_me=True,
                                   struct_as_record=False)["r"]
        cases=np.asarray(observed,dtype=object).ravel()
        if len(cases)!=2:
            raise ValueError("Octave source did not return two package cases")
        margins=[]
        for index,item in enumerate(cases,start=1):
            margin=float(np.asarray(item.COM_dB).ravel()[0])
            fom=float(np.asarray(item.FOM).ravel()[0])
            if not (math.isfinite(margin) and math.isfinite(fom)):
                raise ValueError("Nonfinite independent Octave COM or FOM")
            margins.append({"package_case":index,
                            "octave_com_db":margin,
                            "octave_fom_db":fom})
        receipt={
            "status":"HISTORICAL_OCTAVE_4P17P0_INDEPENDENT_CANDIDATE_ONLY",
            "channel_loss_label":channel,
            "engine_commit":SICOPR_COMMIT,
            "official_historical_workbook_sha256":CONFIG_SHA,
            "official_2019_channel_archive_sha256":CHANNEL_ZIP_SHA,
            "original_s4p_provenance":provenance,
            "validated_config":config_meta,
            "validated_mat_converter":mat_meta,
            "aggressor_fext_count":4,
            "aggressor_next_count":0,
            "aggressor_distinct_fext_source_files":1,
            "package_cases":margins,
            "normative_final_8023ck_qualified":False,
            "sparamview_cpp_com_evaluated":False,
            "licensed_thirdparty_files_uploaded":False
        }
        output.write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf-8")
        print("HISTORICAL_OCTAVE_C2C_RESULT:",json.dumps(receipt))
        return 0


if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--channel", choices=["10","16","18","20"],default="10")
    p.add_argument("--external",type=Path,default=Path("_external_qa_sicopr"))
    p.add_argument("--output",type=Path,required=True)
    args=p.parse_args()
    try:
        raise SystemExit(octave_case(args.channel,args.external.resolve(),
                                     args.output.resolve()))
    except (OSError,ValueError,TypeError,RuntimeError) as exc:
        print("HISTORICAL_OCTAVE_REFERENCE_BLOCKED:",type(exc).__name__,
              str(exc)[:700],file=sys.stderr)
        raise SystemExit(2)
