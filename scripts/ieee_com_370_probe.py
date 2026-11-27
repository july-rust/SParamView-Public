#!/usr/bin/env python3
"""Inspect the official IEEE 802.3ck COM 3.70 package (read-only, no redistribution).

This is evidence inventory only. It does NOT claim numeric COM validation.
"""
from __future__ import annotations
import hashlib, io, json, re, sys, zipfile
from urllib.request import Request, urlopen

URL = "https://www.ieee802.org/3/ck/public/tools/tools/mellitz_3ck_adhoc_01_032322_COM3p70.zip"
def run():
    with urlopen(Request(URL,headers={"User-Agent":"SParamView-012_02-reference-audit/1.0"}),timeout=35) as r:
        data=r.read(60_000_001)
        if len(data)>60_000_000:
            raise RuntimeError("upstream package exceeds safety limit")
    result={"source_url":URL,"package_bytes":len(data),
            "package_sha256":hashlib.sha256(data).hexdigest(),"files":[]}
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        entries=z.infolist()
        if len(entries)>2500: raise RuntimeError("too many entries")
        for item in entries:
            if item.is_dir(): continue
            if item.file_size>25_000_000: raise RuntimeError("unexpected large embedded file")
            name=item.filename.replace("\\","/")
            if name.startswith("/") or any(p==".." for p in name.split("/")):
                raise RuntimeError("unsafe archive entry")
            desc={"path":name,"bytes":item.file_size}
            if name.lower().endswith((".m",".csv",".xlsx",".s2p",".s4p",".s8p",".zip",".txt",".md")):
                blob=z.read(item)
                desc["sha256"]=hashlib.sha256(blob).hexdigest()
                if name.lower().endswith(".m"):
                    t=blob.decode("utf-8","replace")
                    desc["function_signatures"]=[ln.strip()[:160] for ln in t.splitlines()
                       if ln.strip().startswith("function")][:10]
                    desc["preamble"]=t[:1300]
                if name.lower().endswith(".csv") and item.file_size<15000:
                    desc["first_lines"]=blob.decode("utf-8","replace").splitlines()[:15]
                if name.lower().endswith(".zip") and item.file_size<15_000_000:
                    try:
                        with zipfile.ZipFile(io.BytesIO(blob)) as nested:
                            desc["nested_paths"]=nested.namelist()[:30]
                    except zipfile.BadZipFile: desc["nested_error"]="Not a ZIP"
            result["files"].append(desc)
    print(json.dumps(result,indent=2,ensure_ascii=False))
    return 0
if __name__=="__main__":
    try: sys.exit(run())
    except Exception as e:
        print(f"IEEE_COM_370_ARCHIVE_UNVERIFIED: {type(e).__name__}: {e}",file=sys.stderr)
        sys.exit(2)
