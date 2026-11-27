"""012_13: bounded final 120F-8 numerical candidate input contract."""
from pathlib import Path
from tempfile import TemporaryDirectory
import csv
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"scripts"))
import com_120f8_reference_candidate as candidate


class FinalCandidateInputTests(unittest.TestCase):
    def test_source_verified_68_values_and_explicit_historical_aggressors(self):
        with TemporaryDirectory() as temp:
            cfg, receipt = candidate.configure(Path(temp))
            with cfg.open(newline="",encoding="utf-8") as stream:
                values = dict(csv.reader(stream))
            self.assertTrue(receipt["source_120f8_68_leaves_verified"])
            self.assertEqual(values["NO_COM"],"0")
            self.assertEqual(values["z_p (TX)"],"[13 31;1.8 1.8]")
            self.assertEqual(values["z_p (RX)"],"[11 29;1.8 1.8]")
            self.assertEqual(values["z_p (NEXT)"],candidate.NEXT_PACKAGE)
            self.assertEqual(values["z_p (FEXT)"],candidate.FEXT_PACKAGE)
            self.assertEqual(values["T_r"],"0.0075")
            self.assertEqual(values["f_b"],"53.125")
            self.assertEqual(receipt["historical_aggressor_model_source_status"],
                             "DRAFT_2020_NOT_NORMATIVE_FINAL")
            self.assertEqual(receipt["normative_phy_pass_fail"],None)
            self.assertEqual(receipt["historical_fixture_fext_count"],4)
            self.assertEqual(receipt["historical_fixture_next_count"],0)
            self.assertNotEqual(receipt["immutable_base_csv_sha256"],
                                receipt["effective_csv_sha256"])

    def test_wrong_reference_source_fails_before_calculation(self):
        with patch.object(candidate.final,"config_rows",
                          return_value=([["NO_COM","0"]],{
                              "mapped_source_leaves":67,
                              "status":"STATIC_CONTRACT_PASS",
                              "reference_config_no_com":False,
                              "next_fext_zero_package_sentinels":True,
                              "aggressor_package_and_s4p_compliance":"NOT_QUALIFIED"
                          })):
            with TemporaryDirectory() as temp:
                with self.assertRaises(ValueError):
                    candidate.configure(Path(temp))

    def test_reject_nonreal_nan_or_missing_fom(self):
        class Bad:
            itick=1
            COM_dB=3.0
            FOM=complex(9, 0.01)
        with self.assertRaises(ValueError):
            candidate.result_pair([Bad(),Bad()])
        Bad.FOM=float("nan")
        with self.assertRaises(ValueError):
            candidate.result_pair([Bad(),Bad()])
        del Bad.FOM
        with self.assertRaises(AttributeError):
            candidate.result_pair([Bad(),Bad()])

    def test_two_package_cases_and_finite_fields_only(self):
        class Good:
            itick=1
            COM_dB=3.45
            FOM=9.32
        expected=candidate.result_pair([Good(),Good()])
        self.assertEqual([v["package_case"] for v in expected],[1,2])
        self.assertEqual(expected[0]["reference_com_db"],3.45)
        with self.assertRaises(ValueError):
            candidate.result_pair([Good()])


if __name__=="__main__":
    unittest.main()
