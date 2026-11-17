"""012_13: negative and source-identity tests for six-case COM numeric boundary."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import com_120f8_config_contract as cfg
import com_120f8_numerical_gate as gate


def qualified_parser_receipt(loss: str) -> dict:
    historical = cfg.module_from_file("012_13_test_historical", cfg.HISTORICAL_SOURCE)
    _, base = cfg.config_rows()
    receipt = dict(base)
    receipt.update({
        "generated_csv_sha256": gate.expected_csv_sha256(),
        "source_inputs": {
            "status": "INPUT_SHA_PASS",
            "channel_loss_label_db": loss,
            "port_order": [1, 2, 3, 4],
            "source_zip_sha256": historical.CHANNEL_ZIP_SHA,
            "historical_only": True,
            "independent_distinct_fext_files": 1,
            "fext_instances_in_historical_com": 4,
            "next_instances_in_historical_com": 0,
            "thru_fext_next": [
                {"role": role, "member": path, "sha256": digest}
                for role, (path, digest) in zip(
                    ("thru", "fext", "next"), historical.SOURCES[loss]
                )
            ],
        },
        "independent_parser_load": {
            "status": "RUNTIME_LOAD_PASS",
            "reference_sha": gate.REFERENCE_LOADER,
            "loader": "PINNED_INDEPENDENT_SICOPR_4P17P0_PYTHON_PARSER",
            "official_ieee_matlab_run": "NOT_RUN",
            "computed_com_db": None,
            "normative_pass_fail": None,
        },
    })
    return receipt


class Final120FReferenceNumericGateTests(unittest.TestCase):
    def setUp(self):
        self.receipts = [qualified_parser_receipt(x) for x in gate.LOSSES]

    def test_correct_six_cases_are_explicitly_not_evaluated(self):
        result = gate.assess(self.receipts)
        self.assertEqual(result["status"], "FINAL_120F8_NUMERICAL_BLOCKED")
        self.assertEqual(result["case_count"], 6)
        self.assertEqual([(x["historical_source_loss_label_db"],
                           x["package_test"]) for x in result["cases"]],
                         [("10", 1), ("10", 2), ("16", 1), ("16", 2),
                          ("18", 1), ("18", 2)])
        self.assertTrue(all(x["status"] == "NOT_EVALUATED" and
                            x["reference_com_db"] is None and
                            x["reference_fom_db"] is None and
                            x["normative_pass_fail"] is None
                            for x in result["cases"]))
        self.assertFalse(result["final_120f_golden_qualified"])

    def test_non_matching_csv_or_reference_source_never_passes(self):
        for field, value in [
            ("generated_csv_sha256", "0" * 64),
            ("mapped_source_leaves", 67),
            ("reference_commit", "wrong"),
            ("reference_source_sha256", "wrong"),
            ("source_profile", "historical"),
            ("reference_config_no_com", False),
            ("next_fext_zero_package_sentinels", False),
            ("aggressor_package_and_s4p_compliance", "QUALIFIED"),
            ("normative_com_db", 4.2),
            ("normative_pass_fail", True),
        ]:
            with self.subTest(field=field):
                receipts = deepcopy(self.receipts)
                receipts[0][field] = value
                with self.assertRaises(ValueError):
                    gate.assess(receipts)

    def test_parser_or_member_identity_change_is_rejected(self):
        for path, value in [
            (("independent_parser_load", "status"), "NOT_RUN"),
            (("independent_parser_load", "reference_sha"), "bad-sha"),
            (("independent_parser_load", "official_ieee_matlab_run"), "PASS"),
            (("independent_parser_load", "computed_com_db"), 4.2),
            (("source_inputs", "status"), "NOT_RUN"),
            (("source_inputs", "source_zip_sha256"), "bad-sha"),
            (("source_inputs", "historical_only"), False),
            (("source_inputs", "fext_instances_in_historical_com"), 1),
        ]:
            with self.subTest(path=path):
                receipts = deepcopy(self.receipts)
                receipts[0][path[0]][path[1]] = value
                with self.assertRaises(ValueError):
                    gate.assess(receipts)
        receipts = deepcopy(self.receipts)
        receipts[1]["source_inputs"]["thru_fext_next"][0]["sha256"] = "0" * 64
        with self.assertRaises(ValueError):
            gate.assess(receipts)

    def test_missing_duplicate_extra_cases_and_wrong_package_order_blocked(self):
        for receipts in (self.receipts[:2],
                         self.receipts + [self.receipts[0]],
                         [self.receipts[0], self.receipts[0], self.receipts[2]]):
            with self.assertRaises(ValueError):
                gate.assess(receipts)
        receipts = deepcopy(self.receipts)
        receipts[1]["package_case_order"] = ["Test 2", "Test 1"]
        with self.assertRaises(ValueError):
            gate.assess(receipts)


if __name__ == "__main__":
    unittest.main()
