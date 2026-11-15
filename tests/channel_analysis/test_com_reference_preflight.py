"""Synthetic-only tests for the 012_02 provenance checker; NOT COM numeric goldens."""
import hashlib
import importlib.util
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = REPO_ROOT / "scripts" / "com_reference_preflight.py"
spec = importlib.util.spec_from_file_location("com_reference_preflight", SCRIPT)
preflight = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preflight)

def attach(root, name, content, kind=None):
    target = root / name
    target.parent.mkdir(parents=True, exist_ok=True)
    data = content.encode("utf-8")
    target.write_bytes(data)
    value = {"path": name, "sha256": hashlib.sha256(data).hexdigest()}
    if kind:
        value["kind"] = kind
    return value

def synthetic_manifest(root):
    ref_commit = "a" * 40
    lic = attach(root, "license.txt", "Synthetic fixture license evidence (not real IEEE license)")
    cases = []
    for i in range(3):
        inputs = [
            attach(root, f"case{i}/thru.s4p", "synthetic THRU", "thru"),
            attach(root, f"case{i}/config.txt", "synthetic profile", "config")
        ]
        run = attach(root, f"case{i}/run.json", json.dumps({
            "com_db": 3.0 + i, "reference_commit": ref_commit, "profile_id": "TEST-ONLY"
        }))
        cases.append({
            "id": f"SYN-{i}", "published_source_url": "https://example.org/synthetic-only",
            "input_files": inputs, "reproduced_result": run,
            "published_expected_com_db": 3.0 + i, "tolerance_db": 0.01
        })
    return {
        "contract_version": 1, "status": "LOCKED", "profile_id": "TEST-ONLY",
        "standard": {
            "edition": "Synthetic", "annex": "Synthetic",
            "applicable_clause": "Synthetic", "parameter_table": "Synthetic"
        },
        "reference": {
            "release": "synthetic", "source_url": "https://example.org/synthetic-only",
            "source_commit": ref_commit
        },
        "license": {"verified_spdx": "BSD-3-Clause", "evidence": lic},
        "cases": cases
    }

class ReferenceManifestTests(unittest.TestCase):
    def test_real_scaffold_schema_is_valid_but_blocked(self):
        root = REPO_ROOT / "tests" / "fixtures" / "channel" / "com_reference"
        manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
        preflight.schema_check(manifest)
        with self.assertRaisesRegex(preflight.QualificationError, "BLOCKED"):
            preflight.qualify(manifest, root)

    def test_synthetic_three_case_integrity(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = synthetic_manifest(root)
            result = preflight.qualify(data, root)
            self.assertEqual(result["status"], "MANIFEST_AND_REFERENCE_COMPARISON_PASS")
            self.assertEqual(len(result["cases"]), 3)

    def test_missing_commit_is_rejected(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = synthetic_manifest(root)
            data["reference"]["source_commit"] = None
            with self.assertRaises(preflight.QualificationError):
                preflight.qualify(data, root)

    def test_missing_license_evidence_is_rejected(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = synthetic_manifest(root)
            data["license"]["evidence"]["sha256"] = "0" * 64
            with self.assertRaisesRegex(preflight.QualificationError, "SHA256 mismatch"):
                preflight.qualify(data, root)

    def test_numerical_difference_is_rejected(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = synthetic_manifest(root)
            data["cases"][0]["published_expected_com_db"] = 4.0
            with self.assertRaisesRegex(preflight.QualificationError, "numerical deviation"):
                preflight.qualify(data, root)

    def test_path_escape_is_rejected(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = synthetic_manifest(root)
            data["license"]["evidence"]["path"] = "../leak"
            with self.assertRaises((preflight.QualificationError, FileNotFoundError)):
                preflight.qualify(data, root)

    def test_nonfinite_reference_is_rejected(self):
        with TemporaryDirectory() as tmp:
            root = Path(tmp)
            data = synthetic_manifest(root)
            data["cases"][0]["published_expected_com_db"] = float("nan")
            with self.assertRaises(preflight.QualificationError):
                preflight.qualify(data, root)

if __name__ == "__main__":
    unittest.main()
