"""Regression checks for release tooling, using binary probe fixtures, not Touchstone substitutes."""
import csv
import importlib.util
import json
import math
import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch, MagicMock

spec = importlib.util.spec_from_file_location('arm64_validation',
    Path(__file__).resolve().parents[1] / 'scripts/validate_arm64_native.py')
v = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)
TOL = json.loads(v.DEFAULT_BASELINE.read_text())['tolerances']


class NumericalTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.folder = Path(self.temp.name)
        self.addCleanup(self.temp.cleanup)

    def matrix(self, real=0.5, imag=0.0, hz=1.0):
        path = self.folder / 'matrix.bin'
        path.write_bytes(struct.pack('<QQddd', 1, 1, hz, real, imag))
        base = {'ports': 1, 'points': 1, 'frequency_samples': [{'k': 0, 'hz': 1.0}],
                'matrix_samples': [{'i': 0, 'j': 0, 'k': 0, 'real': 0.5, 'imag': 0.0}]}
        return v.compare_matrix(path, base, TOL)

    def tdr(self, **changes):
        values = dict(hz=1.0, real=0.0, imag=0.0, sec=0.0, z=50.0, rho=0.0)
        values.update(changes)
        row = dict(index=0, channel='CH1', quality='OK', termination=0, nf=1, nt=1, reference=50)
        with (self.folder / 'index.csv').open('w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=list(row))
            writer.writeheader()
            writer.writerow(row)
        (self.folder / '0.bin').write_bytes(struct.pack('<QdddQddd', 1,
            values['hz'], values['real'], values['imag'], 1, values['sec'], values['z'], values['rho']))
        base = dict(row, reference_ohm=50.0,
            frequency_samples=[dict(k=0, hz=1.0, real=0.0, imag=0.0)],
            time_samples=[dict(k=0, seconds=0.0, ohm=50.0, rho=0.0, stable_impedance=True)])
        return v.compare_tdr(self.folder, [base], TOL)

    def test_matrix_exact_match(self):
        self.assertEqual(self.matrix()[0], [])

    def test_matrix_within_original_tolerance(self):
        self.assertEqual(self.matrix(real=0.5 + TOL['complex_abs'] / 2)[0], [])

    def test_matrix_outside_original_tolerance(self):
        self.assertTrue(self.matrix(real=0.5 + TOL['complex_abs'] * 2)[0])

    def test_matrix_nonfinite_components_fail(self):
        for component in ('real', 'imag', 'hz'):
            for invalid in (math.nan, math.inf, -math.inf):
                with self.subTest(component=component, invalid=invalid):
                    errors, _, _ = self.matrix(**{component: invalid})
                    self.assertTrue(errors)

    def test_nan_matrix_error_is_not_reported_as_zero(self):
        self.assertEqual(self.matrix(real=math.nan)[1], math.inf)

    def test_tdr_exact_match(self):
        self.assertEqual(self.tdr()[0], [])

    def test_tdr_nonfinite_samples_fail(self):
        for component in ('hz', 'real', 'imag', 'sec', 'z', 'rho'):
            for invalid in (math.nan, math.inf, -math.inf):
                with self.subTest(component=component, invalid=invalid):
                    self.assertTrue(self.tdr(**{component: invalid})[0])

    def test_tdr_original_impedance_tolerance_preserved(self):
        self.assertEqual(self.tdr(z=50 + TOL['impedance_ohm_abs'] / 2)[0], [])
        self.assertTrue(self.tdr(z=50 + TOL['impedance_ohm_abs'] * 2)[0])

    def test_nan_gui_fraction_fails(self):
        expected = dict(results=0, metric_counts={}, tdr_start_fraction=0.2)
        data = dict(application='1.0.4', results=0, analysis=[], tdr_start_fraction=math.nan,
                    termination_settings_roundtrip=True)
        (self.folder / 'Validation.json').write_text(json.dumps(data))
        with zipfile.ZipFile(self.folder / 'Analysis.xlsx', 'w') as z:
            z.writestr('xl/media/test.png', b'fixture')
        self.assertIn('tdr_start_fraction mismatch', v.verify_gui_output(self.folder, expected, '1.0.4')[0])


class ReleaseGateTests(unittest.TestCase):
    def decision(self, **changes):
        args = dict(records=[{'passed': True}], required_count=1, ctest={'seconds': 0},
                    skip_gui=False, bundle_audit={'pe_files': 7, 'wrong_architecture': []})
        args.update(changes)
        return v.validation_passed(**args)

    def test_complete_automated_gates_pass(self):
        self.assertTrue(self.decision())

    def test_skipped_ctest_cannot_pass(self):
        self.assertFalse(self.decision(ctest=None))

    def test_skipped_gui_cannot_pass(self):
        self.assertFalse(self.decision(skip_gui=True))

    def test_absent_or_empty_bundle_cannot_pass(self):
        self.assertFalse(self.decision(bundle_audit=None))
        self.assertFalse(self.decision(bundle_audit={'pe_files': 0, 'wrong_architecture': []}))

    def test_wrong_architecture_cannot_pass(self):
        self.assertFalse(self.decision(bundle_audit={'pe_files': 7, 'wrong_architecture': ['Qt6Core.dll']}))

    def test_missing_failed_or_zero_cases_cannot_pass(self):
        self.assertFalse(self.decision(records=[]))
        self.assertFalse(self.decision(records=[{'passed': False}]))
        self.assertFalse(self.decision(records=[], required_count=0))

    def test_non_windows_host_is_rejected(self):
        with patch.object(v.sys, 'platform', 'linux'):
            with self.assertRaisesRegex(RuntimeError, 'not Windows'):
                v.native_windows_arm64()

    def test_windows_native_machine_detection(self):
        import ctypes
        for native in (0xAA64, 0x8664):
            with self.subTest(native=native):
                kernel = MagicMock()
                def get_machines(handle, process, system):
                    process._obj.value = 0x8664  # Emulated Python is allowed; tested EXE must be ARM64.
                    system._obj.value = native
                    return 1
                kernel.IsWow64Process2.side_effect = get_machines
                with patch.object(v.sys, 'platform', 'win32'), patch.object(ctypes, 'WinDLL', return_value=kernel, create=True):
                    if native == 0xAA64:
                        self.assertEqual(v.native_windows_arm64()['os_machine'], '0xAA64')
                    else:
                        with self.assertRaises(RuntimeError):
                            v.native_windows_arm64()


if __name__ == '__main__':
    unittest.main()
