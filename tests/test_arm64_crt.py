"""Synthetic PE fixtures exercise CRT selection and import parsing."""
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from deploy_arm64_crt import deploy, inspect


def write_pe(path, machine=0xAA64, dependency=None, delay=False):
    data = bytearray(0x600)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 0x84, machine, 1, 0, 0, 0, 240, 0x2022)
    opt = 0x98
    struct.pack_into("<H", data, opt, 0x20B)
    struct.pack_into("<Q", data, opt + 24, 0x140000000)
    struct.pack_into("<II", data, opt + 32, 0x1000, 0x200)
    struct.pack_into("<II", data, opt + 56, 0x2000, 0x200)
    struct.pack_into("<I", data, opt + 108, 16)
    sec = opt + 240
    struct.pack_into("<8sIIIIIIHHI", data, sec, b".rdata\0\0", 0x400,
                     0x1000, 0x400, 0x200, 0, 0, 0, 0, 0x40000040)
    if dependency:
        directory = 13 if delay else 1
        struct.pack_into("<II", data, opt + 112 + directory * 8, 0x1000, 64 if delay else 40)
        if delay:
            struct.pack_into("<8I", data, 0x200, 1, 0x1080, 0, 0x1100, 0x1100, 0, 0, 0)
        else:
            struct.pack_into("<5I", data, 0x200, 0x1100, 0, 0, 0x1080, 0x1100)
        name = dependency.encode("ascii") + b"\0"
        data[0x280:0x280 + len(name)] = name
        struct.pack_into("<Q", data, 0x300, 0x1200)
        data[0x400:0x409] = b"\0\0symbol\0"
    path.write_bytes(data)


class CrtDeploymentTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.crt = self.root / "crt"
        self.bundle = self.root / "bundle"
        self.crt.mkdir()
        self.bundle.mkdir()
        for name in ("msvcp140.dll", "vcruntime140.dll"):
            write_pe(self.crt / name)
        write_pe(self.crt / "vcruntime140_1.dll", machine=0x8664)
        write_pe(self.bundle / "SParamView.exe")

    def test_copy_only_arm64_crt(self):
        names = deploy(self.crt, self.bundle)
        self.assertEqual(names, ["msvcp140.dll", "vcruntime140.dll"])
        self.assertFalse((self.bundle / "vcruntime140_1.dll").exists())
        self.assertEqual(inspect(self.bundle / "msvcp140.dll")[0], 0xAA64)

    def test_preflight_does_not_copy(self):
        deploy(self.crt)
        self.assertEqual(len(list(self.bundle.iterdir())), 1)

    def test_import_of_omitted_runtime_is_rejected_before_copy(self):
        write_pe(self.bundle / "SParamView.exe", dependency="VCRUNTIME140_1.dll")
        with self.assertRaisesRegex(ValueError, "unavailable ARM64 CRT"):
            deploy(self.crt, self.bundle)
        self.assertFalse((self.bundle / "msvcp140.dll").exists())

    def test_delay_import_of_omitted_runtime_is_rejected(self):
        write_pe(self.bundle / "SParamView.exe", dependency="vcruntime140_1.dll", delay=True)
        with self.assertRaisesRegex(ValueError, "unavailable ARM64 CRT"):
            deploy(self.crt, self.bundle)

    def test_selected_runtime_dependency_is_checked(self):
        write_pe(self.crt / "msvcp140.dll", dependency="vcruntime140_1.dll")
        with self.assertRaisesRegex(ValueError, "unavailable ARM64 CRT"):
            deploy(self.crt)

    def test_unexpected_x64_runtime_is_rejected(self):
        write_pe(self.crt / "msvcp140.dll", machine=0x8664)
        with self.assertRaisesRegex(ValueError, "Unexpected CRT architecture"):
            deploy(self.crt)

    def test_missing_required_runtime_is_rejected(self):
        (self.crt / "vcruntime140.dll").unlink()
        with self.assertRaisesRegex(ValueError, "Missing ARM64 CRT"):
            deploy(self.crt)

    def test_mixed_bundle_is_rejected(self):
        write_pe(self.bundle / "Qt6Core.dll", machine=0x8664)
        with self.assertRaisesRegex(ValueError, "Non-ARM64 bundle"):
            deploy(self.crt, self.bundle)

    def test_arm64_variant_is_retained_when_available(self):
        write_pe(self.crt / "vcruntime140_1.dll")
        write_pe(self.bundle / "SParamView.exe", dependency="vcruntime140_1.dll")
        self.assertIn("vcruntime140_1.dll", deploy(self.crt, self.bundle))

    def test_malformed_dll_is_rejected(self):
        (self.crt / "msvcp140.dll").write_bytes(b"not a PE")
        with self.assertRaises(Exception):
            deploy(self.crt)


if __name__ == "__main__":
    unittest.main()
