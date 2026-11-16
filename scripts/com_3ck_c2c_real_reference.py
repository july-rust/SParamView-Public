#!/usr/bin/env python3
"""Run a historical IEEE 802.3ck C2C workbook on real public S4P with
independently maintained BSD-3 SiCoPR/COM 4p17.0 in ephemeral CI storage.

This is an independent *reference candidate numerical result*, NOT a C++
SParamView result, not official IEEE 802.3ck-2022 final Table120F-8 lock.
Do not copy third-party ZIP/workbook/S4P into repo or uploaded artifacts.
"""
import argparse
import contextlib
import csv
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

# Historical IEEE workbook has no usable active sheet in some openpyxl
# versions; the original bytes remain pinned. Convert only a uniquely
# selected parameters worksheet to the same CSV input supported natively
# by the independently maintained COM reference implementation.
CONFIG_LABELS = {
    "f_b", "f_min", "Delta_f", "C_d", "L_s", "C_b", "C_p",
    "Z_c", "Z_c2", "z_p", "z_p2", "R_0", "R_d", "f_z",
    "g_DC", "SNR_TX", "DER_0", "A_v", "A_fe", "A_ne",
    "sigma_RJ", "eta_0", "A_DD", "c(0)", "c(-1)",
}
CONFIG_NORMAL = {x.casefold():x for x in CONFIG_LABELS}

def strict_ooxml_com_settings(raw: bytes) -> tuple[list[list[str]],dict]:
    """Convert only pinned Strict OOXML COM_Settings cells, no guessed sheet.

    The public 2022 source has the Strict (purl.oclc.org) namespace, which
    openpyxl 3.1 does not recognize. Require an explicit relationship-mapped
    COM_Settings sheet; preserve cell positions, package block rows, and
    literal values. For a named parameter, reject formula-derived data.
    """
    import posixpath
    import re
    import xml.etree.ElementTree as ET

    def local(tag):
        return tag.rsplit("}", 1)[-1]

    def children(node, tag):
        return [x for x in node if local(x.tag) == tag]

    def text(cell, strings):
        kind = cell.get("t", "n")
        if kind == "inlineStr":
            return "".join(x.text or "" for x in cell.iter() if local(x.tag) == "t")
        val = children(cell, "v")
        if not val:
            return ""
        raw_value = val[0].text or ""
        if kind == "s":
            if not re.fullmatch(r"\d+", raw_value):
                raise ValueError("Invalid Strict OOXML shared string reference")
            index = int(raw_value)
            if index >= len(strings):
                raise ValueError("Strict OOXML shared string index out of range")
            return strings[index]
        if kind in ("n", "str", "inlineStr", "b", "e"):
            return raw_value
        raise ValueError("Unrecognized Strict OOXML cell type")

    if sha(raw) != CONFIG_SHA:
        raise ValueError("Strict OOXML parser requires byte-pinned IEEE workbook")
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        entries = archive.infolist()
        if len(entries) > 2000 or len({i.filename for i in entries}) != len(entries):
            raise ValueError("Unsafe Strict OOXML ZIP member inventory")
        for info in entries:
            parts = info.filename.replace("\\", "/").split("/")
            if (info.filename.startswith("/") or ".." in parts or
                info.file_size > 16_000_000 or
                ((info.external_attr >> 16) & 0o170000) == 0o120000):
                raise ValueError("Unsafe Strict OOXML ZIP entry")
        book_xml = ET.fromstring(archive.read("xl/workbook.xml"))
        if not book_xml.tag.startswith(
                "{http://purl.oclc.org/ooxml/spreadsheetml/main}"):
            raise ValueError("Historical workbook is not Strict OOXML")
        declared = [v for v in book_xml.iter() if local(v.tag) == "sheet"]
        matches = [v for v in declared if v.get("name") == "COM_Settings"
                   and v.get("state", "visible") == "visible"]
        if len(matches) != 1 or sum(v.get("name") == "COM_Settings"
                                    for v in declared) != 1:
            raise ValueError("Strict OOXML must identify one visible COM_Settings")
        relationship_id = next(
            (v for k, v in matches[0].attrib.items() if k.endswith("}id")), None)
        if not relationship_id:
            raise ValueError("Strict OOXML COM_Settings relationship missing")
        relationships = ET.fromstring(archive.read("xl/_rels/workbook.xml.rels"))
        candidates = [v for v in relationships.iter()
                      if local(v.tag) == "Relationship" and
                      v.get("Id") == relationship_id]
        if len(candidates) != 1 or candidates[0].get("TargetMode") == "External":
            raise ValueError("Strict OOXML COM_Settings relationship invalid")
        target = candidates[0].get("Target", "")
        if not target or ":" in target or ".." in target.split("/"):
            raise ValueError("Strict OOXML COM_Settings worksheet path invalid")
        if target.startswith("/"):
            path = posixpath.normpath(target.lstrip("/"))
        else:
            path = posixpath.normpath(posixpath.join("xl", target))
        if (not re.fullmatch(r"xl/worksheets/sheet\d+\.xml", path) or
            path not in archive.namelist()):
            raise ValueError("Strict OOXML worksheet reference not confirmed")
        strings = []
        if "xl/sharedStrings.xml" in archive.namelist():
            shared_root = ET.fromstring(archive.read("xl/sharedStrings.xml"))
            for si in shared_root.iter():
                if local(si.tag) == "si":
                    strings.append("".join(e.text or "" for e in si.iter()
                                           if local(e.tag) == "t"))
            if len(strings) > 50000:
                raise ValueError("Too many Strict OOXML shared strings")
        sheet = ET.fromstring(archive.read(path))
        sheet_data = [v for v in sheet if local(v.tag) == "sheetData"]
        if len(sheet_data) != 1:
            raise ValueError("Strict OOXML COM_Settings sheetData missing")
        rows = []
        formulas = {}
        seen_cells = set()
        for row in sheet_data[0]:
            if local(row.tag) != "row":
                continue
            row_number = int(row.get("r", "0"))
            if not 1 <= row_number <= 10000:
                raise ValueError("Strict OOXML parameter row out of range")
            while len(rows) < row_number:
                rows.append([])
            for cell in children(row, "c"):
                addr = cell.get("r", "")
                match = re.fullmatch(r"([A-Z]{1,3})([1-9]\d*)", addr)
                if not match or int(match[2]) != row_number:
                    raise ValueError("Invalid Strict OOXML cell address")
                column = 0
                for letter in match[1]:
                    column = column * 26 + ord(letter) - ord("A") + 1
                if column > 512 or addr in seen_cells:
                    raise ValueError("Excessive/duplicate Strict OOXML cell")
                seen_cells.add(addr)
                while len(rows[row_number-1]) < column:
                    rows[row_number-1].append("")
                if children(cell, "f"):
                    formula = children(cell, "f")[0].text or ""
                    cached = children(cell, "v")
                    formulas[(row_number, column)] = {
                        "expression": formula[:240],
                        "cached": (cached[0].text or "")[:80] if cached else None,
                    }
                    rows[row_number-1][column-1] = ""
                else:
                    value = text(cell, strings)
                    if len(value) > 65536:
                        raise ValueError("Oversized Strict OOXML cell")
                    rows[row_number-1][column-1] = value
        found = {}
        for r, cells in enumerate(rows, start=1):
            for c, value in enumerate(cells, start=1):
                if not isinstance(value, str):
                    continue
                key = CONFIG_NORMAL.get(value.strip().casefold())
                if not key:
                    continue
                if key in found:
                    raise ValueError("Ambiguous repeated COM parameter " + key)
                if (r, c+1) in formulas:
                    print("STRICT_COM_FORMULA_BLOCK:",
                          json.dumps({"parameter":key, "row":r, "label_column":c,
                                      "row_cells":cells[:8],
                                      "formula_columns":sorted(col for rr,col in formulas
                                                               if rr==r),
                                      "adjacent_formula":formulas.get((r,c+1))},
                                     ensure_ascii=False),flush=True)
                    raise ValueError("Formula-derived COM parameter is not qualified: " + key)
                if c >= len(cells) or not cells[c].strip():
                    raise ValueError("Blank Strict OOXML COM parameter: " + key)
                found[key] = (r,c+1)
        required = {"f_b","f_min","Delta_f","DER_0","R_0","c(0)"}
        if len(found) < 12 or not required.issubset(found):
            raise ValueError("Strict OOXML COM_Settings lacks verified literals")
        return rows, {"sheet_name": "COM_Settings", "sheet_xml": path,
                      "recognized_numeric_parameter_labels": len(found),
                      "row_count": len(rows), "formula_cells_suppressed": len(formulas),
                      "conversion": "BYTE_PINNED_STRICT_OOXML_EXACT_COM_SETTINGS"}

def canonical_config_csv(original: Path, result_csv: Path) -> dict:
    import openpyxl
    import xml.etree.ElementTree as ET
    with zipfile.ZipFile(original) as archive:
        root = ET.fromstring(archive.read("xl/workbook.xml"))
    if root.tag.startswith("{http://purl.oclc.org/ooxml/spreadsheetml/main}"):
        rows, evidence = strict_ooxml_com_settings(original.read_bytes())
        with result_csv.open("w", newline="", encoding="utf-8") as stream:
            csv.writer(stream).writerows(rows)
        evidence["source_workbook_sha256"] = sha(original.read_bytes())
        evidence["csv_sha256"] = sha(result_csv.read_bytes())
        print("HISTORICAL_STRICT_OOXML_COM_SETTINGS:",
              json.dumps(evidence, ensure_ascii=False), flush=True)
        return evidence
    workbook=openpyxl.load_workbook(original, read_only=True, data_only=True)
    try:
        # Preserve evidence when a pinned historical OOXML workbook cannot be
        # represented as visible worksheets by modern openpyxl. Never fall back
        # to workbook.active or silently choose a hidden/ambiguous worksheet.
        sheet_states = [
            {"title": sheet.title, "state": sheet.sheet_state,
             "rows": sheet.max_row, "columns": sheet.max_column}
            for sheet in workbook.worksheets
        ]
        print("HISTORICAL_2022_C2C_WORKSHEET_STATES:",
              json.dumps(sheet_states, ensure_ascii=False), flush=True)
        observations=[]
        for sheet in workbook.worksheets:
            if sheet.sheet_state != "visible":
                continue
            if sheet.max_row and sheet.max_row > 10000:
                raise ValueError("Unbounded historical COM workbook row count")
            if sheet.max_column and sheet.max_column > 512:
                raise ValueError("Unbounded historical COM workbook column count")
            rows=[]
            observed=set()
            for row in sheet.iter_rows(values_only=True):
                cells=list(row)
                rows.append(cells)
                for cell in cells:
                    if isinstance(cell,str):
                        label=cell.strip().casefold()
                        if label in CONFIG_NORMAL:
                            observed.add(CONFIG_NORMAL[label])
            observations.append((sheet.title,len(observed),rows,
                                 sorted(observed)))
        summary=[{"sheet":name,"parameter_count":count,
                  "rows":len(rows),"recognized_labels":labels}
                 for name,count,rows,labels in observations]
        print("HISTORICAL_2022_C2C_WORKSHEET_INVENTORY:",
              json.dumps(summary,ensure_ascii=False),flush=True)
        qualified=[x for x in observations if x[1]>=12]
        if len(qualified)!=1 and not observations:
            # Historical IEEE workbook may use Strict OOXML namespaces not
            # understood by openpyxl. Record only layout metadata: no
            # arbitrary sheet selection or untrusted XML-to-CSV conversion.
            import xml.etree.ElementTree as ET
            with zipfile.ZipFile(original) as archive:
                workbook_xml = ET.fromstring(archive.read("xl/workbook.xml"))
                sheets = [{
                    "name": node.attrib.get("name"),
                    "state": node.attrib.get("state","visible"),
                    "id": node.attrib.get("sheetId"),
                    "relationship": next((v for k,v in node.attrib.items()
                                          if k.endswith("}id")),None),
                } for node in workbook_xml.iter()
                  if node.tag.split("}")[-1]=="sheet"]
                xml_paths = sorted(name for name in archive.namelist()
                                   if name.startswith("xl/worksheets/") and
                                   name.endswith(".xml"))
                print("HISTORICAL_OOXML_WORKBOOK_LAYOUT:",
                      json.dumps({
                          "namespace":workbook_xml.tag.split("}")[0]+"}",
                          "declared_sheets":sheets,
                          "raw_worksheet_xmls":xml_paths},
                          ensure_ascii=False),flush=True)
        if len(qualified)!=1:
            # We cannot interpret a multi-sheet reference as a single COM
            # profile without an independently validated sheet selector.
            raise ValueError(
                "Cannot uniquely identify 2022 120F C2C settings worksheet: "
                +json.dumps(summary,ensure_ascii=False))
        name,count,rows,labels=qualified[0]
        with result_csv.open("w",newline="",encoding="utf-8") as stream:
            writer=csv.writer(stream)
            for row in rows:
                writer.writerow(["" if value is None else value
                                 for value in row])
        csv_bytes=result_csv.read_bytes()
        # Same engine parses both .xlsx and .csv parameter rows; unlike
        # workbook.active this selection is explicit and SHA-audited.
        return {
            "source_workbook_sha256":sha(original.read_bytes()),
            "sheet_name":name,
            "recognized_numeric_parameter_labels":count,
            "row_count":len(rows),
            "csv_sha256":sha(csv_bytes),
            "conversion":"EXACT_OPENPYXL_DATA_ONLY_CELLS_TO_CSV",
        }
    finally:
        workbook.close()

def run(channel: str, output: Path) -> int:
    import sicopr
    if getattr(sicopr,"COM_MATLAB_VERSION",None) is None:
        raise ValueError("Expected pinned SiCoPR 4p17p0 version selector absent")
    sicopr.COM_MATLAB_VERSION="4p17p0"
    with tempfile.TemporaryDirectory(prefix="ck-golden-") as d:
        base=Path(d)
        config,files,provenance=prepare(channel,base)
        csv_config=base/"working_c2c_reference_settings.csv"
        config_transform=canonical_config_csv(config,csv_config)
        errors=io.StringIO()
        original=Path.cwd()
        try:
            os.chdir(base)
            with contextlib.redirect_stdout(errors):
                result=sicopr._run_com(str(csv_config),1,1,
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
      "historical_workbook_settings_conversion":config_transform,
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
