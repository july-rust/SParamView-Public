import importlib.util
import io
import pathlib
import tempfile
import unittest
from unittest.mock import patch
import zipfile

SCRIPT=(pathlib.Path(__file__).resolve().parents[2]/
        "scripts/com_3ck_c2c_real_reference.py")
spec=importlib.util.spec_from_file_location("c2c_numerical_reference",SCRIPT)
ref=importlib.util.module_from_spec(spec)
spec.loader.exec_module(ref)

def zipped(*entries):
    out=io.BytesIO()
    with zipfile.ZipFile(out,"w") as archive:
        for name,data in entries:
            archive.writestr(name,data)
    return out.getvalue()

class HistoricalC2CReferencePreflight(unittest.TestCase):
    def test_all_four_pinned_channels_have_thru_fext_next(self):
        self.assertEqual(sorted(ref.SOURCES),["10","16","18","20"])
        signatures=set()
        for case,files in ref.SOURCES.items():
            self.assertEqual(len(files),3)
            for (name,digest),role in zip(files,("thru","fext","next")):
                self.assertEqual(len(digest),64)
                self.assertTrue(name.lower().endswith(".s4p"))
                self.assertIn(case+"dB",name)
                self.assertIn(role,name.lower())
                self.assertNotIn(digest,signatures)
                signatures.add(digest)
        self.assertEqual(len(signatures),12)
        self.assertEqual(len(ref.CONFIG_SHA),64)
        self.assertEqual(len(ref.CHANNEL_ZIP_SHA),64)

    def test_archive_members_must_match_exact_name_and_sha(self):
        data=zipped(("some/thru.s4p",b"# GHz S RI R 50\n"))
        raw=b"# GHz S RI R 50\n"
        self.assertEqual(ref.verified_member(
            data,"some/thru.s4p",ref.sha(raw)),raw)
        with self.assertRaisesRegex(ValueError,"SHA"):
            ref.verified_member(data,"some/thru.s4p","0"*64)
        with self.assertRaisesRegex(ValueError,"not found"):
            ref.verified_member(data,"some/missing.s4p",ref.sha(raw))

    def test_download_rejects_non_ieee_host(self):
        with self.assertRaisesRegex(ValueError,"Untrusted"):
            ref.download("https://not-ieee.example.org/x.zip","0"*64)

    def test_synthetic_preparation_requires_all_pinned_real_roles(self):
        files=[
            ("c/thru.s4p",b"thru"),
            ("c/fext.s4p",b"fext"),
            ("c/next.s4p",b"next")
        ]
        config=zipped(("reference.xlsx",b"test workbook"))
        channels=zipped(*files)
        with patch.object(ref,"CONFIG_MEMBER","reference.xlsx"), \
             patch.object(ref,"CONFIG_SHA",ref.sha(b"test workbook")), \
             patch.object(ref,"SOURCES",{"10":[(name,ref.sha(data))
                                        for name,data in files]}), \
             patch.object(ref,"download",side_effect=lambda url,digest:
                    config if url==ref.CONFIG_URL else channels):
            with tempfile.TemporaryDirectory() as tmp:
                workbook,sources,metadata=ref.prepare("10",pathlib.Path(tmp))
                self.assertEqual(workbook.read_bytes(),b"test workbook")
                self.assertEqual([p.read_bytes() for p in sources],
                                 [b"thru",b"fext",b"next"])
                self.assertEqual([m["role"] for m in metadata],
                                 ["thru","fext","next"])
                self.assertTrue(all(len(m["sha256"])==64 for m in metadata))

    def test_bad_channel_label_must_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError,"Unknown C2C"):
                ref.prepare("not_a_case",pathlib.Path(tmp))

if __name__=="__main__":
    unittest.main()
