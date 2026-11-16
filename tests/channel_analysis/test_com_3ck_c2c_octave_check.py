"""Offline, source-free tests for the Octave interop/COM configuration grid."""
import csv
import importlib.util
import pathlib
import sys
import tempfile
import unittest

SCRIPT=pathlib.Path(__file__).resolve().parents[2]/"scripts/com_3ck_c2c_octave_check.py"
sys.path.insert(0,str(SCRIPT.parent))
spec=importlib.util.spec_from_file_location("c2c_octave_check",SCRIPT)
mod=importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


class VerifiedHistoricalOctaveGrid(unittest.TestCase):
    def test_typed_matlab_cell_grid_preserves_vectors(self):
        import scipy.io
        with tempfile.TemporaryDirectory() as d:
            base=pathlib.Path(d)
            source=base/"config.csv"
            with source.open("w",newline="",encoding="utf-8") as stream:
                csv.writer(stream).writerows([
                    ["f_b","53.1250","GBd"],
                    ["Port Order","[1 3 2 4]",""],
                    ["c(-1)","[-0.28:0.02:0]",""],
                    ["optional","", ""]])
            target=base/"data.mat"
            proof=mod.csv_to_octave_mat(source,target)
            self.assertEqual((proof["rows"],proof["columns"]),(4,3))
            self.assertEqual(proof["numeric_cells"],1)
            arr=scipy.io.loadmat(target)["parameter"]
            self.assertEqual(arr.shape,(4,3))
            self.assertAlmostEqual(float(arr[0,1][0,0]),53.125)
            self.assertEqual(str(arr[1,1][0]),"[1 3 2 4]")
            self.assertEqual(str(arr[2,1][0]),"[-0.28:0.02:0]")

    def test_reject_unexpanded_formulas(self):
        with tempfile.TemporaryDirectory() as d:
            base=pathlib.Path(d)
            source=base/"bad.csv"
            source.write_text("f_b,=26.5625*2\n",encoding="utf-8")
            with self.assertRaisesRegex(ValueError,"unexpanded formula"):
                mod.csv_to_octave_mat(source,base/"bad.mat")

    def test_octave_complex_scalar_must_retain_imaginary_part(self):
        import numpy as np
        real,imag = mod.finite_scalar_parts(np.array([complex(-12.86,27.287527)]),
                                           "FOM")
        self.assertAlmostEqual(real,-12.86)
        self.assertAlmostEqual(imag,27.287527)
        self.assertGreater(abs(imag),1e-12)
        self.assertEqual(mod.finite_scalar_parts(np.array([3.5]),"COM_dB"),
                         (3.5,0.0))
        with self.assertRaisesRegex(ValueError,"Nonfinite"):
            mod.finite_scalar_parts(np.array([float("nan")]),"COM_dB")
        with self.assertRaisesRegex(ValueError,"type/shape"):
            mod.finite_scalar_parts(np.array([2.0,3.0]),"FOM")

    def test_reject_nonfinite_numeric(self):
        with tempfile.TemporaryDirectory() as d:
            base=pathlib.Path(d)
            source=base/"bad.csv"
            source.write_text("f_b,1e999\n",encoding="utf-8")
            with self.assertRaisesRegex(ValueError,"Nonfinite"):
                mod.csv_to_octave_mat(source,base/"bad.mat")


if __name__=="__main__":
    unittest.main()
