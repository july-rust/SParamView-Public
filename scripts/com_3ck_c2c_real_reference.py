#!/usr/bin/env python3
"""Run a historical IEEE 802.3ck C2C workbook on real public S4P with
independently maintained BSD-3 SiCoPR/COM 4p17.0 in ephemeral CI storage.

This is an independent *reference candidate numerical result*, NOT a C++
SParamView result, not official IEEE 802.3ck-2022 final Table120F-8 lock.
Do not copy third-party ZIP/workbook/S4P into repo or uploaded artifacts.
"""
import argparse
import contextlib
import hashlib
import io
import json
import math
import os
from pathlib import Path
import sys
import tempfile
from urllib.request import Request, urlopen
import zipfile

CONFIG_URL = "https://www.ieee802.org/3/df/public/tools/COM/mellitz_3df_02_2211.zip"
CONFIG_ZIP_SHA = "895857fbfb602c8ec10b221ab40a23d4b39244b4b57101dd27abc4b2a1a2fddc"
CONFIG_MEMBER = "config_sheets_100G/config_com_ieee8023_93a=3ck_SA_120F_C2C_08_17_2022.xlsx"
CONFIG_SHA = "e676b3fb3cb3048f80c98deaa8faca1d03c13daa216c6259de26885e715ca925"
CHANNEL_URL = "https://www.ieee802.org/3/ck/public/tools/c2c/rabinovich_3ck_informal_08162019.zip"
CHANNEL_ZIP_SHA = "670fe10711b5c530320f770d22a717387b161f2721a2203b27e84ebf7ac597e2"
SICOPR_COMMIT = "cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75"
MAX_DOWNLOAD = 50_000_000
# IEEE public archive filenames and per-member SHA-256: independently
# discovered in 012_09 GitHub Actions read-only audit #43866966952.
SOURCES = {
 "10": [
  ("Impaired_C2C_10dB_P1_to_P2/Impaired_C2C_10dB_P1_to_P2_THRU_ExtPEC.s4p",
   "c6ca6b40f66ba887ca8c7962d188a744e2ec9213c8e7589a3ae51218a6e93c51"),
  ("Impaired_C2C_10dB_P1_to_P2/Impaired_C2C_10dB_P1_to_P4_FEXT_ExtPEC.s4p",
   "2a67eb48a6c4b8718cc9ad1ae987682a0f37b600a64cfeeeada971747a26b2ee"),
  ("Impaired_C2C_10dB_P1_to_P2/Impaired_C2C_10dB_P3_to_P1_NEXT_ExtPEC.s4p",
   "f7d1ae40342dc8379d488e91f5231314c3e469159822aec44819cf78974dbb4c"),
 ],
 "16": [
  ("Impaired_C2C_16dB_P1_to_P2/Impaired_C2C_16dB_P1_to_P2_THRU_ExtPEC.s4p",
   "7e24b53b88f2bf30ed49c924f330c0bb181a453d39b825cda9190c52ed2c76e6"),
  ("Impaired_C2C_16dB_P1_to_P2/Impaired_C2C_16dB_P1_to_P4_FEXT_ExtPEC.s4p",
   "abfe71ac22fa959d47e40b1d7f21192dbb1e71af83090bc4c888a71c6eee3f03"),
  ("Impaired_C2C_16dB_P1_to_P2/Impaired_C2C_16dB_P3_to_P1_NEXT_ExtPEC.s4p",
   "fa3f17a819f30a1cc108b74a1d50787b86b619ac852f09e753704b6d15a59226"),
 ],
 "18": [
  ("Impaired_C2C_18dB_P1_to_P2/Impaired_C2C_18dB_P1_to_P2_THRU_ExtPEC.s4p",
   "73903dfe863b24b395c5712a4ce42e7e4fb76f8054c12fcc9f493f1447c7d2bf"),
  ("Impaired_C2C_18dB_P1_to_P2/Impaired_C2C_18dB_P1_to_P4_FEXT_ExtPEC.s4p",
   "5b4076ca7a3c7dc2548f9c3dbbe18aa79d23f4f2ae3192d07e4634c9a746faa0"),
  ("Impaired_C2C_18dB_P1_to_P2/Impaired_C2C_18dB_P3_to_P1_NEXT_ExtPEC.s4p",
   "a9bf87153d281432c0670148ad0b779f00722d18336cdf2214bb313aea586896"),
 ],
 "20": [
  ("Impaired_C2C_20dB_P1_to_P2/Impaired_C2C_20dB_P1_to_P2_thru_ExtPEC.s4p",
   "870555d2361f3f744ed60a4409f5c4024155887d111ba6b344eb4f1e57ac6fb6"),
  ("Impaired_C2C_20dB_P1_to_P2/Impaired_C2C_20dB_P1_to_P4_FEXT_ExtPEC.s4p",
   "fe5c8c33d122aaf132d88ea1dc11f96ff5c8f8ed1a87ff42c2f6b2af9b97b8e7"),
  ("Impaired_C2C_20dB_P1_to_P2/Impaired_C2C_20dB_P3_to_P1_NEXT_ExtPEC.s4p",
   "432a81964a8d7d08449657010a6ca58bf48cf05bfee94caf47074727fbcf6f42"),
 ],
}

def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def download(url: str, expected: str) -> bytes:
    if not url.startswith("https://www.ieee802.org/"):
        raise ValueError("Untrusted source URL")
    with urlopen(Request(url,headers={
        "User-Agent":"SParamView-012_09-independent-reference/1.0"
    }),timeout=90) as response:
        data=response.read(MAX_DOWNLOAD+1)
    if len(data)>MAX_DOWNLOAD or sha(data)!=expected:
        raise ValueError("Official IEEE ZIP source size/SHA mismatch")
    return data

def verified_member(archive: bytes, member: str, expected: str) -> bytes:
    with zipfile.ZipFile(io.BytesIO(archive)) as z:
        matches=[x for x in z.namelist() if x==member]
        if len(matches)!=1:
            raise ValueError("Required exact public ZIP member not found")
        entry=z.getinfo(member)
        if entry.file_size>10_000_000 or entry.is_dir():
            raise ValueError("Bad or oversized source member")
        data=z.read(member)
    if sha(data)!=expected:
        raise ValueError("Public source member content SHA mismatch")
    return data

def prepare(channel: str, base: Path) -> tuple[Path,list[Path],list[dict]]:
    if channel not in SOURCES:
        raise ValueError("Unknown C2C channel; only pinned cases allowed")
    config=download(CONFIG_URL,CONFIG_ZIP_SHA)
    source=download(CHANNEL_URL,CHANNEL_ZIP_SHA)
    workbook=base/"historical_3ck_SA_120F_C2C_2022.xlsx"
    workbook.write_bytes(verified_member(config,CONFIG_MEMBER,CONFIG_SHA))
    files=[]
    provenance=[]
    for index,(entry,expected) in enumerate(SOURCES[channel]):
        dest=base/("thru.s4p" if index==0 else
                   ("fext.s4p" if index==1 else "next.s4p"))
        raw=verified_member(source,entry,expected)
        dest.write_bytes(raw)
        files.append(dest)
        provenance.append({
            "archive_member":entry,
            "sha256":sha(raw),
            "bytes":len(raw),
            "role":("thru","fext","next")[index],
        })
    return workbook,files,provenance

def run(channel: str, output: Path) -> int:
    import sicopr
    if getattr(sicopr,"COM_MATLAB_VERSION",None) is None:
        raise ValueError("Expected pinned SiCoPR 4p17p0 version selector absent")
    sicopr.COM_MATLAB_VERSION="4p17p0"
    with tempfile.TemporaryDirectory(prefix="ck-golden-") as d:
        base=Path(d)
        config,files,provenance=prepare(channel,base)
        errors=io.StringIO()
        original=Path.cwd()
        try:
            os.chdir(base)
            with contextlib.redirect_stdout(errors):
                result=sicopr._run_com(str(config),1,1,
                    [str(p) for p in files],export_mat=False)
        finally:
            os.chdir(original)
        records=result if isinstance(result,list) else [result]
        if len(records)!=2:
            raise ValueError(
                f"Expected separate 802.3ck C2C Test1/Test2, got {len(records)}")
        evidence=[]
        for number,r in enumerate(records,start=1):
            if r is None:
                raise ValueError("COM reference did not evaluate package case")
            margin=float(r.COM_dB)
            fom=float(r.FOM)
            if not(math.isfinite(margin) and math.isfinite(fom)):
                raise ValueError("Reference engine produced invalid COM/FOM")
            evidence.append({
                "case":number,
                "reference_com_db":margin,
                "reference_fom":fom,
                "sampling_phase_itick":int(r.itick),
                "reference_com_pass_3db":margin>=3.0,
            })
    data={
      "status":"HISTORICAL_8023CK_C2C_INDEPENDENT_PYTHON_REFERENCE_CANDIDATE",
      "channel_condition_db_label":channel,
      "reference_engine":"SiCoPR 1.2.1 compatible IEEE COM 4p17p0",
      "reference_engine_commit":SICOPR_COMMIT,
      "official_2022_workbook_sha256":CONFIG_SHA,
      "official_2022_workbook_zip_sha256":CONFIG_ZIP_SHA,
      "official_2019_c2c_archive_sha256":CHANNEL_ZIP_SHA,
      "input_member_provenance":provenance,
      "package_cases":evidence,
      "input_fext_count":1,
      "input_next_count":1,
      "qualified_final_8023ck_table":False,
      "independent_official_octave_com_execution":False,
      "same_phy_cpp_com_comparison":False,
      "normative_ieee_com_pass_fail":None,
      "third_party_bytes_uploaded":False,
      "limitation":"Historical C2C profile, independently maintained Python port, "
                   "not final licensed standard or C++ COM agreement.",
    }
    output.write_text(json.dumps(data,indent=2)+"\n",encoding="utf-8")
    print("C2C SiCoPR independently computed physical channel",
          channel,"both package cases:",
          [(x["case"],x["reference_com_db"]) for x in evidence])
    print("Output metadata JSON:",output)
    return 0

if __name__ == "__main__":
    cli=argparse.ArgumentParser()
    cli.add_argument("--channel",choices=sorted(SOURCES),default="10")
    cli.add_argument("--output",type=Path,required=True)
    args=cli.parse_args()
    try:
        raise SystemExit(run(args.channel,args.output))
    except (OSError, ValueError, zipfile.BadZipFile, RuntimeError) as exc:
        print("C2C_REFERENCE_NUMERICAL_BLOCKED: "
              f"{type(exc).__name__}: {exc}",file=sys.stderr)
        raise SystemExit(2)
