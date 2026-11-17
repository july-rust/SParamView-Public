"""012_13: controlled independent 3x2 numerical acceptance and negatives."""
from copy import deepcopy
from pathlib import Path
from tempfile import TemporaryDirectory
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"scripts"))
import com_120f8_reference_candidate as candidate
import com_120f8_reference_compare as compare
from com_3ck_c2c_real_reference import CHANNEL_ZIP_SHA,SOURCES


def receipt(channel,engine):
    with TemporaryDirectory() as temp:
        _,settings=candidate.configure(Path(temp))
    return {
        "status":"FINAL_120F8_PARAMETER_SET_HISTORICAL_AGGRESSOR_CANDIDATE",
        "engine":engine,
        "independent_code_commit":candidate.SICOPR_COMMIT,
        "historical_channel_loss_label_db":channel,
        "historical_ieee_channel_zip_sha256":CHANNEL_ZIP_SHA,
        "historical_ieee_source_sha256_members":[
            {"archive_member":name,"sha256":digest,"role":role,"bytes":42}
            for role,(name,digest) in zip(("thru","fext","next"),SOURCES[channel])
        ],
        "verified_original_thru_port_order":[1,2,3,4],
        "settings":settings,
        "package_cases":[
            {"package_case":1,"reference_com_db":4.1,
             "reference_fom_db":17.4},
            {"package_case":2,"reference_com_db":3.7,
             "reference_fom_db":16.8},
        ],
        "final_ieee_normative_phy_qualified":False,
        "native_sparamview_cpp_qualified":False,
        "official_ieee_matlab_executed":False,
        "license_source_files_uploaded":False,
    }


class RealReferenceCorrelation(unittest.TestCase):
    def setUp(self):
        self.a=receipt("10","python")
        self.b=receipt("10","octave")

    def test_correct_six_case_contract_is_bounded_and_explicit(self):
        for channel in ("10","16","18"):
            result=compare.correlate(receipt(channel,"python"),
                                     receipt(channel,"octave"),channel)
            self.assertEqual(len(result["package_cases"]),2)
            self.assertEqual(result["status"],compare.STATUS)
            self.assertFalse(result["final_ieee_phy_compliance_certified"])
            self.assertFalse(result["native_cpp_com_qualified"])
            self.assertIsNone(result["normative_phy_pass_fail"])

    def test_com_and_fom_value_mismatch_rejected(self):
        for field in ("reference_com_db","reference_fom_db"):
            for wrong in (float("nan"), float("inf"), "3.2", True, 5.3):
                with self.subTest(field=field,wrong=str(wrong)):
                    other=deepcopy(self.b)
                    other["package_cases"][1][field]=wrong
                    with self.assertRaises(ValueError):
                        compare.correlate(self.a,other,"10")

    def test_missing_or_reordered_two_tests_rejected(self):
        for changed in ([self.b["package_cases"][0]],
                        list(reversed(self.b["package_cases"]))):
            other=deepcopy(self.b)
            other["package_cases"]=changed
            with self.assertRaises(ValueError):
                compare.correlate(self.a,other,"10")

    def test_source_profile_or_port_drift_fails_closed(self):
        cases=[
            ("independent_code_commit","wrong"),
            ("historical_ieee_channel_zip_sha256","wrong"),
            ("verified_original_thru_port_order",[1,3,2,4]),
            ("official_ieee_matlab_executed",True),
            ("native_sparamview_cpp_qualified",True),
            ("final_ieee_normative_phy_qualified",True),
            ("license_source_files_uploaded",True),
        ]
        for field,bad in cases:
            with self.subTest(field=field):
                other=deepcopy(self.b)
                other[field]=bad
                with self.assertRaises(ValueError):
                    compare.correlate(self.a,other,"10")
        other=deepcopy(self.b)
        other["settings"]["effective_csv_sha256"]="0"*64
        with self.assertRaises(ValueError):
            compare.correlate(self.a,other,"10")
        other=deepcopy(self.b)
        other["settings"]["historical_aggressor_model_source_status"]="FINAL"
        with self.assertRaises(ValueError):
            compare.correlate(self.a,other,"10")
        other=deepcopy(self.b)
        other["historical_ieee_source_sha256_members"][1]["sha256"]="0"*64
        with self.assertRaises(ValueError):
            compare.correlate(self.a,other,"10")


if __name__=="__main__":
    unittest.main()
