"""Hypothesis: an exported PE WIM has index 1 and usable boot metadata.
Falsifier: a missing boot flag/resource or truncated header passes the gate.
DISM integrity checking and the real QEMU boot remain separate checks.
"""
from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from recipe import verify_boot_wim


class WimTests(unittest.TestCase):
    def test_boot_metadata_and_missing_export_boot_flag(self):
        header = bytearray(208)
        header[:8] = b"MSWIM\0\0\0"
        struct.pack_into("<I", header, 44, 1)
        struct.pack_into("<QQ", header, 96, 16, 208)
        struct.pack_into("<I", header, 120, 1)
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "boot.wim"
            p.write_bytes(header + b"m" * 16)
            self.assertEqual(verify_boot_wim(p)["boot_index"], 1)
            struct.pack_into("<I", header, 120, 0)
            p.write_bytes(header + b"m" * 16)
            with self.assertRaises(ValueError): verify_boot_wim(p)
            p.write_bytes(b"truncated")
            with self.assertRaises(ValueError): verify_boot_wim(p)
