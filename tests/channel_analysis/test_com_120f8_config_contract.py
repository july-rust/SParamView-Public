"""012_12 independent QA contract tests: static mapping, source SHA and fail-closed."""
import importlib.util
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

SCRIPT=(Path(__file__).resolve().parents[2]/
        "scripts/com_120f8_config_contract.py")
spec=importlib.util.spec_from_file_location("reference_contract_012_12", SCRIPT)
contract=importlib.util.module_from_spec(spec)
spec.loader.exec_module(contract)


class Com120F8ReferenceConfigurationContract(unittest.TestCase):
    def test_all_68_final_table_leaves_are_mapped_exactly_once(self):
        rows,receipt=contract.config_rows()
        self.assertEqual(receipt["status"],"STATIC_CONTRACT_PASS")
        self.assertEqual(receipt["mapped_source_leaves"],68)
        keys=[row[0] for row in rows]
        self.assertEqual(len(keys),len(set(keys)))
        self.assertEqual(receipt["package_case_order"],["Test 1","Test 2"])
        self.assertEqual(receipt["tx_package_mm"],[13.0,31.0])
        self.assertEqual(receipt["rx_package_mm"],[11.0,29.0])
        self.assertFalse(receipt["normative_pass_fail"])
        self.assertIsNone(receipt["normative_com_db"])
        self.assertEqual(receipt["official_com_4p17_runtime"],"NOT_RUN")
        self.assertEqual(receipt["aggressor_package_and_s4p_compliance"],"NOT_QUALIFIED")
        self.assertTrue(receipt["reference_config_no_com"])
        self.assertTrue(receipt["next_fext_zero_package_sentinels"])

    def test_reference_csv_units_and_package_matrix_orientation(self):
        d=dict(contract.config_rows()[0])
        self.assertEqual(float(d["f_b"]),53.125)
        self.assertEqual(float(d["f_min"]),0.05)
        self.assertEqual(float(d["Delta_f"]),0.01)
        self.assertEqual(float(d["T_r"]),0.0075)
        self.assertEqual(float(d["COM Pass threshold"]),3.0)
        self.assertEqual(d["z_p (TX)"],"[13 31;1.8 1.8]")
        self.assertEqual(d["z_p (RX)"],"[11 29;1.8 1.8]")
        self.assertEqual(d["package_Z_c"],"[87.5 87.5;92.5 92.5]")
        self.assertEqual(d["Port Order"],"[1 2 3 4]")
        self.assertEqual(d["z_p select"],"[1 2]")
        self.assertEqual(d["z_p (NEXT)"],"[0 0;0 0]")
        self.assertEqual(d["z_p (FEXT)"],"[0 0;0 0]")
        self.assertEqual(d["NO_COM"],"1")
        self.assertEqual(float(d["C_d"].strip("[]").split()[0]),0.00012)
        self.assertEqual(float(d["L_s"].strip("[]").split()[0]),0.12)
        self.assertEqual(float(d["C_p"].strip("[]").split()[0]),0.000087)
        self.assertEqual(float(d["C_b"].strip("[]").split()[0]),0.00003)
        self.assertEqual(float(d["f_r"]),0.75)
        self.assertEqual(float(d["f_z"]),21.25)
        self.assertEqual(float(d["f_p1"]),21.25)
        self.assertEqual(float(d["f_p2"]),53.125)
        self.assertEqual(float(d["f_HP_PZ"]),0.6640625)
        self.assertEqual(float(d["eta_0"]),2e-8)

    def test_discrete_ffe_ctle_and_dfe_limits_not_rounded(self):
        d=dict(contract.config_rows()[0])
        self.assertEqual(d["c(-3)"],"[-0.04:0.02:0]")
        self.assertEqual(d["c(-2)"],"[0:0.02:0.1]")
        self.assertEqual(d["c(-1)"],"[-0.28:0.02:0]")
        self.assertEqual(d["c(1)"],"[-0.1:0.02:0]")
        self.assertEqual(d["g_DC"],"[-20:1:0]")
        self.assertEqual(d["g_DC_HP"],"[-4:1:0]")
        self.assertEqual(float(d["c(0)"]),0.54)
        self.assertEqual(int(d["N_b"]),6)
        self.assertEqual(d["b_min(2..N_b)"],"[0.05 -0.04 -0.04 -0.04 -0.04]")
        self.assertEqual(d["b_max(2..N_b)"],"[0.15 0.1 0.1 0.1 0.1]")
        self.assertEqual(int(d["M"]),32)
        self.assertEqual(int(d["L"]),4)
        self.assertEqual(float(d["DER_0"]),1e-5)

    def test_original_cxx_profile_drift_fails_closed(self):
        original=contract.HEADER.read_text(encoding="utf-8")
        self.assertIn("53.125e9;",original)
        with TemporaryDirectory() as temp:
            path=Path(temp)/"different.hpp"
            path.write_text(original.replace("53.125e9;", "54.125e9;", 1),
                            encoding="utf-8")
            with patch.object(contract,"HEADER",path):
                with self.assertRaisesRegex(contract.ContractError,"C\\+\\+ scalar drift"):
                    contract.config_rows()

    def test_reference_identity_drift_fails_closed(self):
        with TemporaryDirectory() as temp:
            manifest=json.loads(contract.SOURCE_AUDIT.read_text(encoding="utf-8"))
            manifest["reference"]["source_commit"]="0"*40
            path=Path(temp)/"manifest.json"
            path.write_text(json.dumps(manifest),encoding="utf-8")
            with patch.object(contract,"SOURCE_AUDIT",path):
                with self.assertRaisesRegex(contract.ContractError,"Reference version"):
                    contract.config_rows()

    def test_offline_never_claims_real_download_or_com_result(self):
        with TemporaryDirectory() as temp:
            destination=Path(temp)/"candidate.csv"
            receipt=contract.execute("10",destination)
            self.assertTrue(destination.is_file())
            self.assertEqual(receipt["source_inputs"]["status"],"NOT_RUN")
            self.assertEqual(receipt["independent_parser_load"]["status"],"NOT_RUN")
            self.assertIsNone(receipt["normative_pass_fail"])
            self.assertIsNone(receipt["normative_com_db"])
            self.assertEqual(receipt["csv_key_count"],
                             len(contract.config_rows()[0]))
            with self.assertRaisesRegex(contract.ContractError,"Only three"):
                contract.execute("20",destination)

    def test_unverified_s4p_and_unavailable_runtime_are_rejected(self):
        with TemporaryDirectory() as temp:
            root=Path(temp)
            fake=root/"impostor.zip"
            fake.write_bytes(b"fake IEEE channel file")
            with self.assertRaisesRegex(contract.ContractError,"SHA-256 mismatch"):
                contract.qualify_ieee_source_zip(fake,"10")
            with self.assertRaisesRegex(contract.ContractError,"not found"):
                contract.actual_reference_parse(fake,root/"missing-checkout")
            with self.assertRaisesRegex(contract.ContractError,"Only three"):
                contract.qualify_ieee_source_zip(fake,"20")

    def test_never_treat_unchanged_preflight_as_normative(self):
        m=json.loads(contract.SOURCE_AUDIT.read_text(encoding="utf-8"))
        self.assertEqual(m["status"],"REFERENCE_READY")
        self.assertIs(m["standard"]["parameter_values_qualified"],False)
        self.assertEqual(m["cases"],[])
        self.assertEqual(m["reference"]["release"],"4.17.0")
        self.assertEqual(m["standard"]["parameter_source_audit"]["numeric_mismatches"],0)


if __name__=="__main__":
    unittest.main()
