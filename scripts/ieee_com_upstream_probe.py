#!/usr/bin/env python3
"""Read-only IEEE GitLab metadata / license / tree / tag probe.

No IEEE source is committed or redistributed. Access is evidence, not COM PASS.
"""
import hashlib
import json
import re
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

BASE = "https://opensource.ieee.org/api/v4/projects/802-com%2Fcom_code"
TARGETS = {
    "project_metadata": BASE,
    "main_commit": BASE + "/repository/commits/main",
    "license_root": BASE + "/repository/files/LICENSE/raw?ref=main",
    "release_tree": BASE + "/repository/tree?path=release&ref=main&per_page=100",
    "root_tree": BASE + "/repository/tree?ref=main&per_page=100",
    "tags": BASE + "/repository/tags?per_page=30",
    "release_4_17_tag": BASE + "/repository/tags/4.17.0",
    "release_4_17_commit": BASE + "/repository/commits/4.17.0",
    "release_4_17_license": BASE + "/repository/files/LICENSE/raw?ref=4.17.0",
    "release_4_17_code_meta": BASE + "/repository/files/release%2Fcom_ieee8023_4p17p0.m?ref=4.17.0",
    "config_templates": BASE + "/repository/tree?path=config_templates&ref=4.17.0&per_page=100",
    "template_C2C": BASE + "/repository/tree?path=config_templates%2FC2C&ref=4.17.0&per_page=100",
    "template_C2M": BASE + "/repository/tree?path=config_templates%2FC2M&ref=4.17.0&per_page=100",
    "template_CR": BASE + "/repository/tree?path=config_templates%2FCR&ref=4.17.0&per_page=100",
    "template_C2C_200G": BASE + "/repository/tree?path=config_templates%2FC2C%2F200G&ref=4.17.0&per_page=100",
    "template_C2M_200G": BASE + "/repository/tree?path=config_templates%2FC2M%2F200G&ref=4.17.0&per_page=100",
    "template_CR_200G": BASE + "/repository/tree?path=config_templates%2FCR%2F200G&ref=4.17.0&per_page=100",
    "template_KR": BASE + "/repository/tree?path=config_templates%2FKR_and_CPC&ref=4.17.0&per_page=100",
    "template_readme": BASE + "/repository/files/config_templates%2FREADME.md/raw?ref=4.17.0",
    "release_code_prefix": BASE + "/repository/files/release%2Fcom_ieee8023_4p17p0.m/raw?ref=4.17.0",
    "readme": BASE + "/repository/files/README.md/raw?ref=main",
}
output = {"status": "UNVERIFIED", "results": {}}
for name, uri in TARGETS.items():
    try:
        request = Request(uri, headers={"Accept": "application/json",
                                        "User-Agent": "SParamView-COM-Qualification/012_02"})
        with urlopen(request, timeout=20) as response:
            raw = response.read(2_000_001)
            if len(raw) > 2_000_000:
                raise ValueError("payload exceeds limit")
            item = {"http": response.status, "sha256": hashlib.sha256(raw).hexdigest(), "bytes": len(raw)}
            if name in ("project_metadata", "main_commit", "release_4_17_commit", "release_4_17_code_meta"):
                data = json.loads(raw)
                item.update({k: data.get(k) for k in ("id", "path_with_namespace", "sha",
                             "web_url", "license", "created_at", "committed_date", "blob_id", "size", "file_path") if k in data})
            elif name in ("root_tree", "release_tree", "config_templates", "template_C2C", "template_C2M", "template_CR", "template_C2C_200G", "template_C2M_200G", "template_CR_200G", "template_KR"):
                data = json.loads(raw)
                item["paths"] = [x.get("path") for x in data][:100]
            elif name == "release_4_17_tag":
                data = json.loads(raw)
                item["tag_name"] = data.get("name")
                item["tag_commit_id"] = (data.get("commit") or {}).get("id")
                item["release"] = (data.get("release") or {}).get("description", "")[:300]
            elif name == "tags":
                data = json.loads(raw)
                item["tag_names"] = [x.get("name") for x in data][:30]
            elif name in ("license_root", "release_4_17_license"):
                text = raw.decode("utf-8", errors="replace")
                item["license_first_lines"] = text.splitlines()[:7]
                item["has_bsd_three_conditions"] = all(phrase in text for phrase in (
                    "Redistributions of source code", "Redistributions in binary form",
                    "Neither the name"))
                item["has_bsd_redistribution"] = "Redistribution and use in source and binary forms" in text
            elif name == "readme":
                item["intro"] = raw.decode("utf-8", errors="replace")[:1600]
            elif name == "template_readme":
                item["text"] = raw.decode("utf-8", errors="replace")[:1600]
            elif name == "release_code_prefix":
                txt = raw.decode("utf-8", errors="replace")
                item["first_30_lines"] = txt.splitlines()[:30]
                item["functions_head"] = [line[:120] for line in txt.splitlines() if line.strip().startswith("function")][:15]
            output["results"][name] = item
    except (HTTPError, URLError, ValueError, TimeoutError) as exc:
        output["results"][name] = {"error": type(exc).__name__, "detail": str(exc)[:180]}
if all(output["results"].get(k, {}).get("http") == 200
       for k in ("main_commit", "license_root", "root_tree")):
    output["status"] = "UPSTREAM_META_RETRIEVED_NOT_NUMERICALLY_VERIFIED"
print(json.dumps(output, indent=2, ensure_ascii=False))
