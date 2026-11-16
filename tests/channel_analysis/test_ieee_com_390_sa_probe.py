"""Offline synthetic tests: the IEEE SA 2022 XLSX inspector is fail-closed."""
import importlib.util
import io
from pathlib import Path
import unittest
import zipfile

SCRIPT = Path(__file__).resolve().parents[2] / "scripts/ieee_com_390_sa_probe.py"
spec = importlib.util.spec_from_file_location("ieee_com_390_sa_probe", SCRIPT)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)

SHEET = """<?xml version="1.0" encoding="utf-8"?>
<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">
<sheetData>
<row r="1"><c r="A1" t="s"><v>0</v></c><c r="B1"><v>53.125</v></c></row>
<row r="2"><c r="A2" t="s"><v>1</v></c><c r="B2"><v>0.05</v></c></row>
<row r="3"><c r="A3" t="s"><v>2</v></c><c r="B3"><v>0.01</v></c></row>
<row r="4"><c r="A4" t="s"><v>3</v></c><c r="B4"><v>0.00012</v></c></row>
<row r="5"><c r="A5" t="s"><v>4</v></c><c r="B5"><v>0.12</v></c></row>
<row r="6"><c r="A6" t="s"><v>5</v></c><c r="B6"><v>0.00003</v></c></row>
<row r="7"><c r="A7" t="s"><v>6</v></c><c r="B7"><v>13</v></c></row>
<row r="8"><c r="A8" t="s"><v>7</v></c><c r="B8"><v>0.000087</v></c></row>
<row r="9"><c r="A9" t="s"><v>8</v></c><c r="B9"><v>87.5</v></c></row>
<row r="10"><c r="A10" t="s"><v>9</v></c><c r="B10"><v>50</v></c></row>
<row r="11"><c r="A11" t="s"><v>10</v></c><c r="B11"><v>50</v></c></row>
<row r="12"><c r="A12" t="s"><v>11</v></c><c r="B12"><v>4</v></c></row>
</sheetData></worksheet>"""
KEYS = ["f_b","f_min","Delta_f","C_d","L_s","C_b",
        "z_p","C_p","Z_c","R_0","R_d","L"]
STRING = ("<sst xmlns='http://schemas.openxmlformats.org/spreadsheetml/2006/main'>"
          + "".join(f"<si><t>{key}</t></si>" for key in KEYS) + "</sst>")

def mock_book():
    out=io.BytesIO()
    with zipfile.ZipFile(out,"w") as z:
        z.writestr("xl/sharedStrings.xml",STRING)
        z.writestr("xl/worksheets/sheet1.xml",SHEET)
    return out.getvalue()

class HistoricalArchiveTests(unittest.TestCase):
    def test_extract_synthetic_readonly_named_settings(self):
        sheets,fields=probe.parse_xlsx(mock_book())
        self.assertEqual(len(sheets),1)
        self.assertEqual(set(fields),set(KEYS))
        self.assertEqual(fields["f_b"][0]["value_cells"][0]["value"],53.125)
        self.assertEqual(fields["C_d"][0]["value_cells"][0]["value"],0.00012)

    def test_descriptions_do_not_qualify_numeric_settings(self):
        # The XML may contain all named fields with only adjacent units,
        # descriptions, or Excel formulas but no actual numeric values.
        labels = "".join(
            f'<row r="{i+1}"><c r="A{i+1}" t="s"><v>{i}</v></c>'
            f'<c r="B{i+1}" t="inlineStr"><is><t>units only</t></is></c>'
            f'</row>' for i in range(len(KEYS)))
        xml = ('<worksheet xmlns="http://schemas.openxmlformats.org/'
               'spreadsheetml/2006/main"><sheetData>'+labels+
               '</sheetData></worksheet>')
        out=io.BytesIO()
        with zipfile.ZipFile(out,"w") as z:
            z.writestr("xl/sharedStrings.xml",STRING)
            z.writestr("xl/worksheets/sheet1.xml",xml)
        sheets,fields=probe.parse_xlsx(out.getvalue())
        self.assertEqual(len(sheets),1)
        self.assertEqual(fields,{})

    def test_reject_zip_path_escape(self):
        out=io.BytesIO()
        with zipfile.ZipFile(out,"w") as z:
            z.writestr("../bad.xml","not allowed")
        with zipfile.ZipFile(io.BytesIO(out.getvalue())) as z:
            with self.assertRaisesRegex(ValueError,"unsafe"):
                probe.safe_zip(z,5_000_000)

    def test_reject_missing_sheet(self):
        out=io.BytesIO()
        with zipfile.ZipFile(out,"w") as z:
            z.writestr("xl/sharedStrings.xml",STRING)
        with self.assertRaisesRegex(ValueError,"worksheet"):
            probe.parse_xlsx(out.getvalue())

    def test_column_index(self):
        self.assertEqual(probe._column("A1"),1)
        self.assertEqual(probe._column("Z2"),26)
        self.assertEqual(probe._column("AA12"),27)
        self.assertEqual(probe._column("AZ3"),52)

if __name__ == "__main__":
    unittest.main()
