#!/usr/bin/env python3
"""Run pinned independent COM 4.17 reference correlation on IEEE public channel.

Downloads IEEE data only at CI runtime. It NEVER commits channel files or
third-party COM source. Correlates an independent Python engine with *published
Octave 4p17p0 reference* outputs, not with normative 802.3ck compliance.
"""
from __future__ import annotations
import hashlib, json, os, pathlib, re, subprocess, sys, tempfile, zipfile
from urllib.request import Request,urlopen

REFERENCE_COMMIT="cb7c4be5c63d41e88b6ee7d04fb17e0046ab0f75"
OFFICIAL_COM_SOURCE_COMMIT="47459695a169fdf1fd10c541a5b19c69a5ae9285"
DATA_URL="https://www.ieee802.org/3/dj/public/tools/CR/akinwale_3dj_02_2311.zip"

def sha256(path: pathlib.Path)->str:
    h=hashlib.sha256()
    with path.open("rb") as stream:
        while block:=stream.read(1<<20):h.update(block)
    return h.hexdigest()

def checkout_check(repo:pathlib.Path)->None:
    got=subprocess.check_output(["git","-C",str(repo),"rev-parse","HEAD"],text=True).strip()
    if got!=REFERENCE_COMMIT:raise RuntimeError(f"wrong independent oracle revision {got}")

def check_channels(repo:pathlib.Path, channels:pathlib.Path)->dict:
    declaration=(repo/"examples/akinwale_CR_22dB_VendorX/CHANNEL.md").read_text(encoding="utf-8")
    entries=re.findall(r"^([0-9a-f]{64})  (\S+)$",declaration,re.MULTILINE)
    if len(entries)!=6:raise RuntimeError("expected exactly 6 IEEE channel fingerprints")
    matched={}
    for digest,name in entries:
        hits=list(channels.rglob(name))
        if len(hits)!=1:raise RuntimeError(f"missing/ambiguous channel: {name}")
        got=sha256(hits[0])
        if got!=digest:raise RuntimeError(f"IEEE channel SHA mismatch {name}: {got}")
        matched[name]=got
    return matched

def main():
    if len(sys.argv)!=2:raise RuntimeError("usage: com_417_numeric_reference_ci.py <pinned SiCoPR repository>")
    repo=pathlib.Path(sys.argv[1]).resolve(strict=True)
    checkout_check(repo)
    with tempfile.TemporaryDirectory(prefix="ieee-com-417-") as tmp:
        root=pathlib.Path(tmp)
        zip_path=root/"ieee-public-channels.zip"
        req=Request(DATA_URL,headers={"User-Agent":"SParamView-COM-012_02-Numerical-Oracle/1.0"})
        with urlopen(req,timeout=80) as res, zip_path.open("wb") as out:
            while block:=res.read(1<<20):
                out.write(block)
                if out.tell()>70_000_000:raise RuntimeError("IEEE channel archive unexpectedly large")
        archive_sha=sha256(zip_path)
        data=root/"channels"
        data.mkdir()
        with zipfile.ZipFile(zip_path) as z:
            for item in z.infolist():
                name=pathlib.PurePosixPath(item.filename.replace("\\","/"))
                if name.is_absolute() or ".." in name.parts:raise RuntimeError("unsafe zip member")
                if item.is_dir():continue
                if item.file_size>15_000_000:raise RuntimeError("oversized channel member")
                target=data.joinpath(*name.parts)
                target.parent.mkdir(parents=True,exist_ok=True)
                with z.open(item) as src, target.open("wb") as dst:
                    import shutil
                    shutil.copyfileobj(src,dst)
        channels=check_channels(repo,data)
        print(json.dumps({"status":"IEEE_CHANNELS_SHA256_VERIFIED",
                "ieee_channel_archive_url":DATA_URL,"ieee_channel_archive_sha256":archive_sha,
                "reference_commit":REFERENCE_COMMIT,
                "official_4p17_code_commit":OFFICIAL_COM_SOURCE_COMMIT,
                "checked_channels_sha256":channels},indent=2),flush=True)
        env=os.environ.copy()
        env.update({"OPENBLAS_NUM_THREADS":"1","OMP_NUM_THREADS":"1","MKL_NUM_THREADS":"1"})
        cmd=[sys.executable,str(repo/"examples/run_example.py"),
             "--channels",str(data),"--engine","sicopr"]
        print("RUNNING_VERIFIED_EXAMPLE_WITHOUT_AND_WITH_CROSSTALK",flush=True)
        process=subprocess.run(cmd,cwd=repo,env=env,text=True,errors="replace",
            stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=1500)
        report=process.stdout
        print("REFERENCE_CORRELATION_LOG_START\n"+report[-25000:]+
              "\nREFERENCE_CORRELATION_LOG_END",flush=True)
        if process.returncode!=0 or "all pinned values reproduced" not in report.lower():
            raise RuntimeError("Actual independent COM computations did not confirm both published oracle results")
        print("COM_4P17_INDEPENDENT_NUMERICAL_EXAMPLE_REPRODUCED_2_CONDITIONS",flush=True)
        return 0

if __name__=="__main__":
    try:sys.exit(main())
    except Exception as exc:
        print(f"COM_4P17_ORACLE_NOT_VERIFIED {type(exc).__name__}: {exc}",file=sys.stderr)
        sys.exit(2)
