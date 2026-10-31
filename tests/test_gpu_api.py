"""Run actual GSP wrapper with original CPU-only SetupAPI/registry doubles."""

from pathlib import Path
import subprocess
import tempfile
import os
from pe_compile import compile_c
import unittest

ROOT = Path(__file__).resolve().parents[1]


class NativeAPI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(dir="/data" if Path("/data").exists() else None)
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.exe = Path(cls.tmp.name) / ("gpu-mock.exe" if os.name == "nt" else "gpu-mock")
        compile_c([ROOT / "tests/gpu_mock.c", ROOT / "programs/pe-contract.c"], cls.exe)

    def run_case(self, case):
        return subprocess.run([str(self.exe), case], capture_output=True, text=True, timeout=5)

    def test_native_success_reads_both(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0)
        self.assertIn("MOCK_ENABLE reads=2 class=1 service=1", r.stdout)

    def test_native_success_install_held(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0)
        self.assertIn("MOCK_INSTALL flags=12 disabled=1", r.stdout)

    def refused(self, scenario):
        r = self.run_case(scenario)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("enabled=0", r.stdout)

    def test_driver_wrong_property_type(self):
        self.refused("driver_type")

    def test_service_wrong_property_type(self):
        self.refused("service_type_prop")

    def test_driver_reported_size_out_of_bounds(self):
        self.refused("driver_size")

    def test_service_reported_size_out_of_bounds(self):
        self.refused("service_size")

    def test_instance_reported_size_out_of_bounds(self):
        self.refused("instance_size")

    def test_compatible_reported_size_out_of_bounds(self):
        self.refused("compatible_size")

    def test_driver_nul_outside_reported_data(self):
        self.refused("driver_nul_size")

    def test_service_nul_outside_reported_data(self):
        self.refused("service_nul_size")

    def test_unterminated_driver(self):
        self.refused("driver_unterminated")

    def test_unterminated_compatible(self):
        self.refused("compatible_unterminated")

    def test_unterminated_provider(self):
        self.refused("provider_unterminated")

    def test_class_wrong_type(self):
        self.refused("class_type")

    def test_service_wrong_type(self):
        self.refused("service_type")

    def test_class_zero_value(self):
        self.refused("class_zero")

    def test_service_zero_value(self):
        self.refused("service_zero")

    def test_unproven_driver_key(self):
        self.refused("badkey")

    def test_non_nvidia_provider(self):
        self.refused("provider")

    def test_disable_failure(self):
        self.refused("disablefail")

    def test_no_adapter_is_distinct_smoke_case(self):
        r = self.run_case("absent")
        self.assertEqual(r.returncode, 3)
        self.assertNotIn("MOCK_ENABLE", r.stdout)


if __name__ == "__main__":
    unittest.main()
