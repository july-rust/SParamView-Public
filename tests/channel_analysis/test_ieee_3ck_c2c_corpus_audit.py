import importlib.util
import io
import pathlib
import unittest
import zipfile

PATH = pathlib.Path(__file__).resolve().parents[2] / \
       "scripts/ieee_3ck_public_c2c_corpus_audit.py"
spec=importlib.util.spec_from_file_location("ieee_3ck_c2c",PATH)
module=importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def archive(files):
    output=io.BytesIO()
    with zipfile.ZipFile(output,"w") as z:
        for name,value in files:
            z.writestr(name,value)
    return output.getvalue()

class SourceInventoryTests(unittest.TestCase):
    def test_only_literal_snp_members_and_sha_provenance(self):
        data=archive([
            ("c2c/",b""),
            ("c2c/10db/THRU_A.s4p",b"# GHz S RI R 50\n0 0 0\n"),
            ("c2c/16db/THRU_B.S4P",b"# GHz S RI R 50\n0 0 1\n"),
            ("c2c/18db/NEXT_A.s8p",b"# GHz S RI R 50\n0 0 2\n"),
            ("c2c/notes/readme.txt",b"test only"),
        ])
        report=module.inspect(data)
        self.assertEqual(report["touchstone_count"],3)
        self.assertEqual(report["physical_four_port_count"],2)
        self.assertEqual(report["physical_ports_histogram"],{"4":2,"8":1})
        self.assertEqual(len(report["fingerprints"]),3)
        self.assertEqual(len(report["archive_sha256"]),64)
        self.assertFalse(report["normative_8023ck_final_profile_qualified"])
        self.assertFalse(report["third_party_bytes_uploaded"])
        self.assertEqual(report["qualified_independent_same_phy_cases"],0)

    def test_reject_archive_escape(self):
        with self.assertRaisesRegex(ValueError,"Unsafe"):
            module.inspect(archive([("../bad.s4p",b"fake")]))

    def test_reject_duplicate_entry(self):
        with self.assertRaisesRegex(ValueError,"Duplicate"):
            module.inspect(archive([
                ("a.s4p",b"first"),("a.s4p",b"second")
            ]))

    def test_reject_excessive_member(self):
        data=archive([("a.s4p",b"0123456789")])
        previous=module.MAX_MEMBER
        try:
            module.MAX_MEMBER=8
            with self.assertRaisesRegex(ValueError,"Excessive"):
                module.inspect(data)
        finally:
            module.MAX_MEMBER=previous

    def test_nested_archives_are_not_mislabeled_as_channels(self):
        report=module.inspect(archive([("channels.zip",b"placeholder")]))
        self.assertEqual(report["physical_four_port_count"],0)
        self.assertEqual(report["qualified_independent_same_phy_cases"],0)
        self.assertEqual(report["nested_archives_ignored"],["channels.zip"])

if __name__=="__main__":
    unittest.main()
