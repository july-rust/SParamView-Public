"""Offline fail-closed tests for 3ck same-source Python↔Octave C2C comparison."""
import copy
import importlib.util
from pathlib import Path
import sys
import unittest

SCRIPTS=Path(__file__).resolve().parents[2]/"scripts"
sys.path.insert(0,str(SCRIPTS))
name="com_3ck_c2c_compare_numerics"
spec=importlib.util.spec_from_file_location(name,SCRIPTS/(name+".py"))
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def good_receipts():
    sources=[{"archive_member":n,"sha256":d}
             for n,d in mod.SOURCES["10"]]
    key="f"*64
    py={
        "status":"HISTORICAL_8023CK_C2C_INDEPENDENT_PYTHON_REFERENCE_CANDIDATE",
        "channel_condition_db_label":"10",
        "reference_engine_commit":mod.SICOPR_COMMIT,
        "official_2022_workbook_sha256":mod.CONFIG_SHA,
        "official_2019_c2c_archive_sha256":mod.CHANNEL_ZIP_SHA,
        "historical_workbook_settings_conversion":{
            "csv_sha256":"c"*64,
            "source_workbook_sha256":mod.CONFIG_SHA},
        "temporary_verified_physical_port_order_override":{
            "ephemeral_corrected_config_csv_sha256":key,
            "effective_port_order":"[1 2 3 4]"},
        "input_fext_count":4,"input_next_count":0,
        "qualified_final_8023ck_table":False,
        "same_phy_cpp_com_comparison":False,
        "input_member_provenance":sources,
        "package_cases":[{"case":1,"reference_com_db":4.8,"reference_fom":17.2},
                         {"case":2,"reference_com_db":4.7,"reference_fom":17.1}],
    }
    octave={
        "status":"HISTORICAL_OCTAVE_4P17P0_INDEPENDENT_CANDIDATE_ONLY",
        "channel_loss_label":"10",
        "engine_commit":mod.SICOPR_COMMIT,
        "official_historical_workbook_sha256":mod.CONFIG_SHA,
        "official_2019_channel_archive_sha256":mod.CHANNEL_ZIP_SHA,
        "validated_config":{
            "csv_sha256":"c"*64,"source_workbook_sha256":mod.CONFIG_SHA},
        "temporary_source_verified_port_order":{
            "ephemeral_corrected_config_csv_sha256":key,
            "effective_port_order":"[1 2 3 4]"},
        "aggressor_fext_count":4,
        "aggressor_next_count":0,
        "aggressor_distinct_fext_source_files":1,
        "normative_final_8023ck_qualified":False,
        "sparamview_cpp_com_evaluated":False,
        "nonreal_fom_or_com_detected":False,
        "original_s4p_provenance":sources,
        "package_cases":[
          {"package_case":1,"valid_real_com_and_fom":True,
           "observed_com_real_component_db":4.800000000001,
           "observed_com_imag_component":0,
           "observed_fom_real_component_db":17.2,
           "observed_fom_imag_component":0},
          {"package_case":2,"valid_real_com_and_fom":True,
           "observed_com_real_component_db":4.7,
           "observed_com_imag_component":0,
           "observed_fom_real_component_db":17.1,
           "observed_fom_imag_component":0}]
    }
    return py,octave


class HistoricalCorrelationTests(unittest.TestCase):
    def test_independent_same_input_both_packages(self):
        p,o=good_receipts()
        r=mod.validate_correlation(p,o,"10")
        self.assertEqual(len(r["package_cases"]),2)
        self.assertLess(abs(r["package_cases"][0]["delta_com_db"]),1e-8)
        self.assertFalse(r["final_ieee_8023ck_2022_qualified"])
        self.assertFalse(r["native_cpp_com_qualified"])

    def test_rejects_complex_and_missing_data(self):
        p,o=good_receipts()
        for change in (
            lambda p,o: o["package_cases"][0].update(observed_fom_imag_component=27.287),
            lambda p,o: o.update(nonreal_fom_or_com_detected=True),
            lambda p,o: o["package_cases"].pop(),
            lambda p,o: p["package_cases"][0].update(reference_com_db=3.5),
            lambda p,o: o["temporary_source_verified_port_order"].update(
                ephemeral_corrected_config_csv_sha256="d"*64),
            lambda p,o: p["input_member_provenance"][0].update(sha256="0"*64),
            lambda p,o: p.update(qualified_final_8023ck_table=True),
            lambda p,o: o.update(aggressor_next_count=1),
        ):
            left,right=copy.deepcopy(p),copy.deepcopy(o)
            change(left,right)
            with self.assertRaises(ValueError):
                mod.validate_correlation(left,right,"10")


if __name__=="__main__":
    unittest.main()
