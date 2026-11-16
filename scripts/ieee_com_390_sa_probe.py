#!/usr/bin/env python3
"""Read-only inventory of official IEEE-hosted COM 3.90 sponsor-ballot-era C2C.

This is NOT a qualified IEEE 802.3ck-2022 Table 120F-8 profile and is NOT
an independent COM numerical result. The external ZIP/XLSX and its licensed
third-party source are NEVER checked into SParamView or uploaded as artifacts.
Only source URLs, SHA256, numerical settings and named-field evidence are emitted.
"""
from __future__ import annotations
import hashlib
import io
import json
import re
import sys
import xml.etree.ElementTree as ET
import zipfile
from urllib.request import Request, urlopen

URL = "https://www.ieee802.org/3/df/public/tools/COM/mellitz_3df_02_2211.zip"
EXPECTED = "config_com_ieee8023_93a=3ck_SA_120F_C2C_08_17_2022.xlsx"
MAX_ARCHIVE = 55_000_000
MAX_XLSX = 8_000_000
NS = {"m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}
FIELD_NAMES = (
    "f_b", "f_min", "Delta_f", "C_d", "L_s", "C_b", "z_p",
    "z_p2", "C_p", "Z_c", "Z_c2", "R_0", "R_d",
    "A_v", "A_fe", "A_ne", "L", "M", "DER_0",
    "c(0)", "c(-3)", "c(-2)", "c(-1)", "c(1)",
    "g_DC", "g_DC2", "g_DC_HP", "f_z", "f_p1", "f_p2",
    "f_HP_PZ", "N_b", "b_max(1)", "b_min(1)",
    "R_LM", "SNR_TX", "sigma_RJ", "A_DD", "eta_0",
)
NORM_FIELDS = {f.lower(): f for f in FIELD_NAMES}

def safe_zip(z: zipfile.ZipFile, limit: int) -> None:
    files = z.infolist()
    if len(files) > 2000:
        raise ValueError("archive has excessive file count")
    for f in files:
        normalized = f.filename.replace("\\", "/")
        if f.file_size > limit or normalized.startswith("/") or \
           any(part == ".." for part in normalized.split("/")):
            raise ValueError("unsafe or excessive archive member")

def _text(cell: ET.Element, shared: list[str]) -> str:
    t = cell.attrib.get("t")
    if t == "inlineStr":
        return "".join(x.text or "" for x in cell.findall(".//m:t", NS))
    value = cell.find("m:v", NS)
    if value is None:
        return ""
    if t == "s":
        index = int(value.text or "")
        if index < 0 or index >= len(shared):
            raise ValueError("out-of-range shared string")
        return shared[index]
    return value.text or ""

def _column(address: str) -> int:
    letters = re.match(r"^[A-Z]+", address)
    if not letters:
        raise ValueError("bad workbook cell address")
    number = 0
    for letter in letters.group():
        number = number * 26 + ord(letter) - ord("A") + 1
    return number

def parse_xlsx(data: bytes) -> tuple[list[str], dict]:
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        safe_zip(z, 15_000_000)
        shared: list[str] = []
        if "xl/sharedStrings.xml" in z.namelist():
            root = ET.fromstring(z.read("xl/sharedStrings.xml"))
            shared = ["".join(n.text or "" for n in si.findall(".//m:t",NS))
                      for si in root.findall("m:si", NS)]
        files = sorted(n for n in z.namelist()
                       if re.fullmatch(r"xl/worksheets/sheet\d+\.xml", n))
        if not files:
            raise ValueError("no worksheet XML")
        evidence: dict[str, list[dict]] = {}
        for sheet in files:
            root = ET.fromstring(z.read(sheet))
            for row in root.findall(".//m:sheetData/m:row", NS):
                cells: dict[int,str] = {}
                for cell in row.findall("m:c", NS):
                    val = _text(cell, shared).strip()
                    if val:
                        cells[_column(cell.attrib["r"])] = val
                for col, val in cells.items():
                    symbol = NORM_FIELDS.get(val.strip().lower())
                    if not symbol:
                        continue
                    near = [
                        {"offset": d, "value": cells[col + d][:100]}
                        for d in (1, 2, 3, 4) if cells.get(col + d)
                    ]
                    if not near:
                        continue
                    evidence.setdefault(symbol,[]).append({
                        "sheet_xml": sheet,
                        "row": row.attrib.get("r"),
                        "value_cells": near,
                    })
        return files, evidence

def run() -> int:
    with urlopen(Request(URL, headers={
        "User-Agent":"SParamView-012_09-readonly-standards-audit/1.0"
    }),timeout=60) as response:
        archive = response.read(MAX_ARCHIVE+1)
    if len(archive)>MAX_ARCHIVE:
        raise ValueError("archive exceeds 55 MB limit")
    archive_sha=hashlib.sha256(archive).hexdigest()
    with zipfile.ZipFile(io.BytesIO(archive)) as z:
        safe_zip(z, MAX_XLSX)
        matches = [n for n in z.namelist()
                   if n.replace("\\","/").split("/")[-1].lower() ==
                   EXPECTED.lower()]
        if len(matches) != 1:
            raise ValueError(f"expected one 3ck SA 120F C2C sheet, found {len(matches)}")
        member=matches[0]
        workbook=z.read(member)
        if len(workbook)>MAX_XLSX:
            raise ValueError("C2C workbook exceeds read-only limit")
    sheets,evidence=parse_xlsx(workbook)
    if len(evidence)<12:
        raise ValueError(f"insufficient named C2C fields extracted ({len(evidence)})")
    report={
        "source_url":URL,
        "source_date":"2022-11 (historical IEEE working-group tool)",
        "zip_bytes":len(archive),
        "zip_sha256":archive_sha,
        "profile_workbook_archive_path":member,
        "profile_workbook_sha256":hashlib.sha256(workbook).hexdigest(),
        "source_profile_name":"3ck_SA_120F_C2C_08_17_2022",
        "worksheet_count":len(sheets),
        "named_parameter_count":len(evidence),
        "parameter_evidence":evidence,
        "qualification_status":"HISTORICAL_INDEPENDENT_SOURCE_NOT_FINAL_NORMATIVE_LOCK",
        "normative_table_120F8_qualified":False,
        "ieee_com_cpp_numerical_goldens":0,
        "third_party_bytes_uploaded":False,
        "limitations":(
            "The August 2022 sponsor-ballot-era config is a crosscheck source. "
            "It does not establish byte-verified final published 802.3ck-2022 "
            "Table 120F-8 or Test1/Test2 C++ COM numerical agreement.")
    }
    print(json.dumps(report,indent=2,ensure_ascii=False))
    return 0

if __name__=="__main__":
    try:
        raise SystemExit(run())
    except (OSError,ValueError,zipfile.BadZipFile,ET.ParseError) as exc:
        print(f"IEEE_3CK_SA_2022_PROVENANCE_BLOCKED: {type(exc).__name__}: {exc}",
              file=sys.stderr)
        raise SystemExit(2)
