#!/usr/bin/env python3
"""Read-only IEEE COM GitLab upstream probe: no source is redistributed."""
import hashlib
import json
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

BASE = "https://opensource.ieee.org/api/v4/projects/802-com%2Fcom_code"
TARGETS = {
    "project_metadata": BASE,
    "main_commit": BASE + "/repository/commits/main",
    "license_root": BASE + "/repository/files/LICENSE/raw?ref=main",
    "license_md": BASE + "/repository/files/LICENSE.md/raw?ref=main",
}
output = {"status": "UNVERIFIED", "results": {}}
for name, uri in TARGETS.items():
    try:
        request = Request(uri, headers={"Accept": "application/json", "User-Agent": "SParamView-COM-Qualification/012_02"})
        with urlopen(request, timeout=15) as response:
            raw = response.read(2_000_001)
            if len(raw) > 2_000_000:
                raise ValueError("payload exceeds safety limit")
            item = {"http": response.status, "sha256": hashlib.sha256(raw).hexdigest(), "bytes": len(raw)}
            if name in ("project_metadata", "main_commit"):
                parsed = json.loads(raw.decode("utf-8"))
                item.update({key: parsed.get(key) for key in ("id", "path_with_namespace", "sha", "web_url", "license") if key in parsed})
            output["results"][name] = item
    except (HTTPError, URLError, ValueError, TimeoutError) as exc:
        output["results"][name] = {"error": type(exc).__name__, "detail": str(exc)[:180]}
if all(output["results"].get(k, {}).get("http") == 200 for k in ("main_commit", "license_root")):
    output["status"] = "SOURCE_METADATA_ACCESSED_LICENSE_UNREAD"
print(json.dumps(output, indent=2, ensure_ascii=False))
# A probe never certifies profile/fixture license or COM numerical accuracy.
