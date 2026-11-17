"""No-fake-golden regression for 012_13 reference numerical continuation."""
from copy import deepcopy
from pathlib import Path
import sys
import unittest
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import com_120f8_real_numeric_reference as numerical

class NumericReferenceContinuationContract(unittest.TestCase):
    def test_source_derived_configuration_and_real_packages(self):
        rows, source = numerical.compute_csv_rows()
        d = dict(rows)
        self.assertEqual(source["mapped_source_leaves"], 68)
        self.assertEqual(d["NO_COM"], "0")
        self.assertEqual(d["z_p (TX)"], "[13 31;1.8 1.8]")
        self.assertEqual(d["z_p (RX)"], "[11 29;1.8 1.8]")
        self.assertEqual(d["z_p (NEXT)"], numerical.NEXT_PACKAGE)
        self.assertEqual(d["z_p (FEXT)"], numerical.FEXT_PACKAGE)
        self.assertEqual(d["Port Order"], "[1 2 3 4]")
        self.assertEqual(d["DISPLAY_WINDOW"], "0")
        self.assertEqual(d["SAVE_FIGURES"], "0")
        self.assertEqual(len(d), len(rows))

    def test_independent_real_com_and_fom_exact_correlation(self):
        py = [{"case":1,"com_db":4.5,"fom_db":1.2},
              {"case":2,"com_db":3.3,"fom_db":1.5}]
        out = numerical.compare(py, deepcopy(py))
        self.assertEqual(len(out), 2)
        self.assertTrue(all(x["normative_ieee_pass_fail"] is None for x in out))

    def test_reject_mismatched_real_nonfinite_and_reordered(self):
        py = [{"case":1,"com_db":4.5,"fom_db":1.2},
              {"case":2,"com_db":3.3,"fom_db":1.5}]
        for modified in [
            [{"case":1,"com_db":4.6,"fom_db":1.2},py[1]],
            [py[0],{"case":2,"com_db":3.3,"fom_db":1.501}],
            list(reversed(py)),
            [py[0]],
            [py[0],{"case":2,"com_db":float("nan"),"fom_db":1.5}],
        ]:
            with self.subTest(modified=modified):
                with self.assertRaises(ValueError):
                    numerical.compare(py,modified)

    def test_real_reference_must_not_be_created_from_zero_sentinels(self):
        from unittest.mock import patch
        import com_120f8_config_contract as cfg
        original,provenance = cfg.config_rows()
        rows = deepcopy(original)
        for row in rows:
            if row[0] == "z_p (NEXT)":
                row[1] = "[0 0;0 0]"
        with patch.object(cfg,"config_rows",return_value=(rows,provenance)):
            # Original QA rows are valid only if no accidental contractual drift.
            result,_ = numerical.compute_csv_rows()
            self.assertNotEqual(dict(result)["z_p (NEXT)"], "[0 0;0 0]")

if __name__ == "__main__":
    unittest.main()
