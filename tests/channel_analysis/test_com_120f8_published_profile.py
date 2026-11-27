"""012_11: lock the licensed final IEEE 802.3ck-2022 120F.4.1/120F-8 C++ values.

These are independent factual numeric expectations, not executable IEEE algorithms,
not an IEEE-licensed text copy, and not a conformance certificate.
The 316-page original was privately reviewed and is NOT redistributed; its SHA-256 and clause/page audit are recorded in:
docs/COM_8023ck_Normative_Audit_012_11.md.
"""
import ast
import json
import math
from pathlib import Path
import re
import unittest

HEADER = (
    Path(__file__).resolve().parents[2]
    / "include/si/channel_analysis/com_120f8.hpp"
)

# Numeric scalar fields in Com120F8Parameters, all in C++ field units.
# 120F.4.1 adds min COM and Tr. All 68 values are checked directly against
# the user-provided licensed final original: SHA 5f5db5a7e94e49cdc68a18db63b7ec01b349467f680251194fbc0a1295faa1e9,
# printed pp. 236-238 (PDF pp. 237-239). The original must NEVER be committed.
EXPECTED_SCALARS = {
    "symbolRateHz": 53.125e9,
    "maximumStartFrequencyHz": 0.05e9,
    "maximumFrequencyStepHz": 0.01e9,
    "minimumComDb": 3.0,
    "txTransitionTimeS": 7.5e-12,
    "referenceOhmSingleEnded": 50.0,
    "receiverTerminationOhmSingleEnded": 50.0,
    "cdFaradSingleEnded": 1.2e-13,
    "lsHenrySingleEnded": 1.2e-10,
    "cbFaradSingleEnded": 3e-14,
    "cpFaradSingleEnded": 8.7e-14,
    "secondaryPackageLengthMm": 1.8,
    "primaryPackageZcOhm": 87.5,
    "secondaryPackageZcOhm": 92.5,
    "packageA1SqrtNsPerMm": 9.909e-4,
    "packageA2NsPerMm": 2.772e-4,
    "receiverBandwidthHz": 0.75 * 53.125e9,
    "minimumCursor": 0.54,
    "ctleZeroFrequencyHz": 53.125e9 / 2.5,
    "ctlePole1FrequencyHz": 53.125e9 / 2.5,
    "ctlePole2FrequencyHz": 53.125e9,
    "ctleLowFrequencyPoleZeroHz": 53.125e9 / 80,
    "victimPeakV": 0.413,
    "fextPeakV": 0.413,
    "nextPeakV": 0.608,
    "pamLevels": 4,
    "minimumRlm": 0.95,
    "txSnrDb": 33,
    "samplesPerUi": 32,
    "dfeTaps": 6,
    "randomJitterRmsUi": 0.01,
    "dualDiracJitterPeakUi": 0.02,
    "oneSidedNoiseSpectralDensityV2PerGHz": 2e-8,
    "targetDetectorErrorRatio": 1e-5,
}
EXPECTED_RANGES = {
    "txNoncursor": (
        (-0.04, 0, 0.02), (0, 0.10, 0.02),
        (-0.28, 0, 0.02), (-0.10, 0, 0.02),
    ),
    "ctleDcGainDb": ((-20, 0, 1),),
    "ctleDcGain2Db": ((-4, 0, 1),),
}
EXPECTED_ARRAYS = {
    "txPackageLengthMm": (13, 31),  # Test 1 Tx; Test 2 Tx
    "rxPackageLengthMm": (11, 29),  # Test 1 Rx; Test 2 Rx
    "dfeMin": (0.3, 0.05, -0.04, -0.04, -0.04, -0.04),
    "dfeMax": (0.65, 0.15, 0.1, 0.1, 0.1, 0.1),
}

def safe_number(expr: str) -> float:
    """Evaluate ONLY literal arithmetic +,-,*,/; never eval arbitrary C++."""
    ops = {ast.Add: lambda x, y: x + y,
           ast.Sub: lambda x, y: x - y,
           ast.Mult: lambda x, y: x * y,
           ast.Div: lambda x, y: x / y}
    def walk(node):
        if isinstance(node, ast.Expression):
            return walk(node.body)
        if isinstance(node, ast.Constant) and type(node.value) in (float, int):
            return float(node.value)
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return -walk(node.operand)
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.UAdd):
            return walk(node.operand)
        if isinstance(node, ast.BinOp) and type(node.op) in ops:
            return ops[type(node.op)](walk(node.left), walk(node.right))
        raise ValueError("unsupported C++ numeric expression")
    return walk(ast.parse(expr.strip(), mode="eval"))

def extract_scalar(text, field):
    matches = re.findall(r"\b" + re.escape(field) + r"\s*=\s*([^;]+);", text)
    if len(matches) != 1:
        raise AssertionError(f"{field}: expected 1 initializer, got {len(matches)}")
    return safe_number(matches[0])

def extract_array(text, field):
    match = re.search(r"\b" + re.escape(field) + r"\s*\{([^{}]+)\}", text)
    if match is None:
        raise AssertionError(f"{field}: array missing")
    return tuple(safe_number(s) for s in match.group(1).split(","))

def extract_ranges(text, field):
    if field == "txNoncursor":
        m = re.search(r"\btxNoncursor\s*\{(.*?)\};", text, flags=re.S)
        if m is None:
            raise AssertionError("txNoncursor missing")
        entries = re.findall(r"Com120F8DiscreteRange\s*\{([^{}]+)\}", m.group(1))
    else:
        m = re.search(r"\b" + re.escape(field) + r"\s*\{([^{}]+)\}", text)
        entries = [m.group(1)] if m else []
    return tuple(tuple(safe_number(s) for s in e.split(",")) for e in entries)

class Test120F8PublishedValues(unittest.TestCase):
    def test_all_68_numeric_entries(self):
        source = HEADER.read_text(encoding="utf-8")
        checked = 0
        for name, expected in EXPECTED_SCALARS.items():
            actual = extract_scalar(source, name)
            self.assertTrue(math.isclose(actual, expected, rel_tol=1e-13,
                                          abs_tol=1e-20), (name, actual, expected))
            checked += 1
        for name, expected_groups in EXPECTED_RANGES.items():
            actual_groups = extract_ranges(source, name)
            self.assertEqual(len(actual_groups), len(expected_groups), name)
            for actual, expected in zip(actual_groups, expected_groups):
                self.assertEqual(len(actual), len(expected), name)
                for a, b in zip(actual, expected):
                    self.assertTrue(math.isclose(a, b, rel_tol=1e-13,
                                                  abs_tol=1e-20), (name, a, b))
                    checked += 1
        for name, expected in EXPECTED_ARRAYS.items():
            actual = extract_array(source, name)
            self.assertEqual(len(actual), len(expected), name)
            for a, b in zip(actual, expected):
                self.assertTrue(math.isclose(a, b, rel_tol=1e-13,
                                              abs_tol=1e-20), (name, a, b))
                checked += 1
        self.assertEqual(checked, 68)

    def test_direct_original_source_audit_receipt_and_production_lock(self):
        # Offline CI validates the review receipt and executable source defaults,
        # not the original licensed PDF (which is intentionally not redistributed).
        manifest_path = (Path(__file__).resolve().parents[2]
                         / "tests/fixtures/channel/com_reference/manifest.json")
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        audit = manifest["standard"]["parameter_source_audit"]
        self.assertEqual(audit["status"], "SOURCE_VERIFIED_012_11")
        self.assertEqual(audit["edition"], "IEEE Std 802.3ck-2022")
        self.assertEqual(audit["clause"], "120F.4.1")
        self.assertEqual(audit["table"], "120F-8")
        self.assertEqual(audit["pdf_pages"], 316)
        self.assertEqual(audit["published_print_pages"], [236, 237, 238])
        self.assertEqual(audit["source_size_bytes"], 4606528)
        self.assertEqual(audit["numeric_leaves_checked"], 68)
        self.assertEqual(audit["numeric_mismatches"], 0)
        self.assertEqual(
            audit["pdf_sha256"],
            "5f5db5a7e94e49cdc68a18db63b7ec01b349467f680251194fbc0a1295faa1e9")
        self.assertEqual(manifest["status"], "REFERENCE_READY")
        self.assertIs(manifest["standard"]["parameter_values_qualified"], False)
        self.assertEqual(manifest["cases"], [])

    def test_reject_executable_initializers(self):
        for expr in ("__import__('os').system('echo PWN')", "foo(2)", "1**3",
                     "1 / 0 if False else 0", "pow(4,2)"):
            with self.assertRaises((ValueError, SyntaxError, ZeroDivisionError)):
                safe_number(expr)

    def test_test1_test2_order_and_strict_nonborrow(self):
        source = HEADER.read_text(encoding="utf-8")
        self.assertEqual(extract_array(source, "txPackageLengthMm"), (13, 31))
        self.assertEqual(extract_array(source, "rxPackageLengthMm"), (11, 29))
        self.assertNotEqual(
            extract_array(source, "txPackageLengthMm"),
            extract_array(source, "rxPackageLengthMm"))
        # A published parameter table is NOT a proof of computed C++ COM.
        self.assertIn("NOT IEEE COM dB or PASS/FAIL", source)

if __name__ == "__main__":
    unittest.main()
