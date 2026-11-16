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

    def test_workbook_to_native_csv_requires_one_matching_settings_sheet(self):
        import openpyxl
        from csv import reader
        book=openpyxl.Workbook()
        first=book.active
        first.title="About"
        first.append(["This is not a COM settings worksheet"])
        table=book.create_sheet("Historical_C2C_parameter_sheet")
        for idx,label in enumerate(sorted(ref.CONFIG_LABELS),start=1):
            table.append([label,float(idx)])
        with tempfile.TemporaryDirectory() as tmp:
            directory=pathlib.Path(tmp)
            original=directory/"historical_2022.xlsx"
            output=directory/"settings.csv"
            book.save(original)
            evidence=ref.canonical_config_csv(original,output)
            self.assertEqual(evidence["sheet_name"],table.title)
            self.assertEqual(evidence["source_workbook_sha256"],
                             ref.sha(original.read_bytes()))
            self.assertEqual(evidence["csv_sha256"],
                             ref.sha(output.read_bytes()))
            with output.open(newline="",encoding="utf-8") as stream:
                rows=list(reader(stream))
            self.assertEqual(len(rows),len(ref.CONFIG_LABELS))
            # openpyxl may deserialize a whole-valued 19.0 as integer 19.
            # CSV preserves the numeric value, not float string formatting.
            f_b_rows = [row for row in rows if len(row) == 2 and row[0] == "f_b"]
            self.assertEqual(len(f_b_rows), 1)
            self.assertEqual(float(f_b_rows[0][1]),
                             float(sorted(ref.CONFIG_LABELS).index("f_b") + 1))
            # Two plausible settings sheets cannot be silently conflated.
            clone=book.create_sheet("Ambiguous")
            for idx,label in enumerate(sorted(ref.CONFIG_LABELS),start=1):
                clone.append([label,float(idx)])
            book.save(original)
            with self.assertRaisesRegex(ValueError,"Cannot uniquely"):
                ref.canonical_config_csv(original,output)

    def _strict_com_workbook(self, formula=False, duplicate_sheet=False,
                             hostile_target=False):
        # Minimal Strict OOXML fixture with explicit workbook relations.
        strict = "http://purl.oclc.org/ooxml/spreadsheetml/main"
        document_r = "http://purl.oclc.org/ooxml/officeDocument/relationships"
        labels = sorted(ref.CONFIG_LABELS)
        rows = []
        for i, label in enumerate(labels, start=1):
            a = '<c r="A{0}" t="inlineStr"><is><t>{1}</t></is></c>'.format(i,label)
            if formula and i==1:
                expression = "SUM(9,1)" if formula=="unsupported" else "9+1"
                cached = "11" if formula=="stale" else "10"
                b = '<c r="B1"><f>{0}</f><v>{1}</v></c>'.format(expression,cached)
            else:
                b = '<c r="B{0}"><v>{0}</v></c>'.format(i)
            rows.append('<row r="{0}">{1}{2}</row>'.format(i,a,b))
        sheet = ('<worksheet xmlns="{0}"><sheetData>{1}</sheetData>'
                 '</worksheet>').format(strict,''.join(rows))
        extra = ('<sheet name="COM_Settings" sheetId="2" r:id="rId2"/>'
                 if duplicate_sheet else '')
        book = ('<workbook xmlns="{0}" xmlns:r="{1}"><sheets>'
                '<sheet name="COM_Settings" sheetId="1" r:id="rId1"/>'
                '{2}</sheets></workbook>').format(strict,document_r,extra)
        target = '../untrusted.xml' if hostile_target else 'worksheets/sheet1.xml'
        rels = ('<Relationships xmlns="http://schemas.openxmlformats.org/'
                'package/2006/relationships"><Relationship Id="rId1"'
                ' Type="worksheet" Target="{0}"/></Relationships>').format(target)
        return zipped(("xl/workbook.xml",book),
                      ("xl/_rels/workbook.xml.rels",rels),
                      ("xl/worksheets/sheet1.xml",sheet))

    def test_strict_ooxml_exact_com_settings_numeric_csv(self):
        from csv import reader
        raw = self._strict_com_workbook()
        with patch.object(ref,"CONFIG_SHA",ref.sha(raw)):
            rows, evidence = ref.strict_ooxml_com_settings(raw)
            self.assertEqual(evidence["sheet_name"],"COM_Settings")
            self.assertEqual(evidence["recognized_numeric_parameter_labels"],
                             len(ref.CONFIG_LABELS))
            self.assertEqual(len(rows),len(ref.CONFIG_LABELS))
            with tempfile.TemporaryDirectory() as tmp:
                original = pathlib.Path(tmp)/"source.xlsx"
                csv_path = pathlib.Path(tmp)/"output.csv"
                original.write_bytes(raw)
                converted = ref.canonical_config_csv(original,csv_path)
                self.assertEqual(converted["csv_sha256"],
                                 ref.sha(csv_path.read_bytes()))
                with csv_path.open(newline="",encoding="utf-8") as stream:
                    parsed = list(reader(stream))
                self.assertEqual(parsed[0],[sorted(ref.CONFIG_LABELS)[0],"1"])

    def test_strict_ooxml_recalculates_exact_formula(self):
        raw = self._strict_com_workbook(formula="good")
        with patch.object(ref,"CONFIG_SHA",ref.sha(raw)):
            rows, evidence = ref.strict_ooxml_com_settings(raw)
            self.assertEqual(rows[0][1],"10")
            self.assertEqual(len(evidence["formula_recomputation_receipts"]),1)
            self.assertEqual(evidence["formula_recomputation_receipts"][0]["recomputed"],
                             "10")

    def test_strict_ooxml_rejects_formula_and_duplicate_sheet(self):
        for name, flags, error in [
            ("unsupported",{"formula":"unsupported"},"Unsupported"),
            ("stale",{"formula":"stale"},"stale/overflow"),
            ("duplicate",{"duplicate_sheet":True},"one visible"),
            ("path",{"hostile_target":True},"worksheet path invalid"),
        ]:
            with self.subTest(name=name):
                raw = self._strict_com_workbook(**flags)
                with patch.object(ref,"CONFIG_SHA",ref.sha(raw)):
                    with self.assertRaisesRegex(ValueError,error):
                        ref.strict_ooxml_com_settings(raw)
        raw = self._strict_com_workbook()
        with self.assertRaisesRegex(ValueError,"byte-pinned"):
            ref.strict_ooxml_com_settings(raw)

    def test_historical_ieee_four_fext_zero_next_reference_roles(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp)
            files = [root/"thru.s4p",root/"fext.s4p",root/"next.s4p"]
            n_fext,n_next,ordered = ref.reference_2019_c2c_inputs(files)
            self.assertEqual((n_fext,n_next),(4,0))
            self.assertEqual(ordered,[files[0]]+[files[1]]*4)
            self.assertNotIn(files[2],ordered)
            with self.assertRaisesRegex(ValueError,"Expected SHA-pinned"):
                ref.reference_2019_c2c_inputs(files[:2])

    def test_workbook_with_no_com_settings_is_not_accepted(self):
        import openpyxl
        book=openpyxl.Workbook()
        book.active.append(["This is not a reference parameter"])
        with tempfile.TemporaryDirectory() as tmp:
            directory=pathlib.Path(tmp)
            source=directory/"empty.xlsx"
            book.save(source)
            with self.assertRaisesRegex(ValueError,"Cannot uniquely"):
                ref.canonical_config_csv(source,directory/"empty.csv")

    def test_hidden_settings_sheet_must_not_be_silently_selected(self):
        import openpyxl
        book = openpyxl.Workbook()
        book.active.title = "Visible_About"
        book.active.append(["README, not an approved COM configuration"])
        hidden = book.create_sheet("Hidden_Parameters")
        for idx, label in enumerate(sorted(ref.CONFIG_LABELS), start=1):
            hidden.append([label, float(idx)])
        hidden.sheet_state = "hidden"
        with tempfile.TemporaryDirectory() as tmp:
            source = pathlib.Path(tmp) / "historical.xlsx"
            result = pathlib.Path(tmp) / "candidate.csv"
            book.save(source)
            with self.assertRaisesRegex(ValueError, "Cannot uniquely"):
                ref.canonical_config_csv(source, result)
            self.assertFalse(result.exists())

    def test_bad_channel_label_must_fail_closed(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(ValueError,"Unknown C2C"):
                ref.prepare("not_a_case",pathlib.Path(tmp))

if __name__=="__main__":
    unittest.main()
