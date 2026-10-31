"""CPU-only execution of the actual conditional standard-install policy."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

from pe_compile import compile_c

ROOT = Path(__file__).resolve().parents[1]


class SerialPrepare(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(dir="/data" if Path("/data").exists() else None)
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.exe = Path(cls.tmp.name) / ("serial-mock.exe" if os.name == "nt" else "serial-mock")
        compile_c([ROOT / "tests/serial_prepare_mock.c"], cls.exe)

    def run_case(self, name):
        return subprocess.run([str(self.exe), name], text=True, capture_output=True, timeout=5)

    def test_normal_pipeline_order(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0)
        self.assertIn("step=0\nMOCK_CALL step=1\nMOCK_CALL step=2", r.stdout)

    def test_normal_pipeline_checked_after_each(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0)
        self.assertIn("calls=3 selected=1 checks=4", r.stdout)

    def test_existing_com_skips_install(self):
        r = self.run_case("com_present")
        self.assertEqual(r.returncode, 0)
        self.assertIn("calls=0 selected=0", r.stdout)

    def refused(self, name):
        r = self.run_case(name)
        self.assertNotEqual(r.returncode, 0, r.stdout)
        return r

    def test_guard_rejects_each_bad_snapshot(self):
        for case in (
            "duplicate", "parallel", "wrong_class", "wrong_service", "not_started", "problem",
            "wrong_inf", "wrong_provider", "missing_section", "wrong_matching",
            "installer_missing", "name_present", "name_unknown", "com_unknown",
        ):
            with self.subTest(case=case):
                self.assertIn("calls=0 selected=0", self.refused(case).stdout)

    def test_guard_never_selects_bad_snapshot(self):
        for case in (
            "duplicate", "parallel", "wrong_class", "wrong_service", "not_started", "problem",
            "wrong_inf", "wrong_provider", "missing_section", "wrong_matching",
            "installer_missing", "name_present", "name_unknown", "com_unknown",
        ):
            with self.subTest(case=case):
                self.assertNotIn("MOCK_CALL", self.refused(case).stdout)

    def test_driver_selection_failure_stops(self):
        for case in ("select_failed", "wrong_selected"):
            with self.subTest(case=case):
                self.assertIn("calls=0 selected=1", self.refused(case).stdout)

    def test_driver_selection_failure_has_no_dif(self):
        for case in ("select_failed", "wrong_selected"):
            with self.subTest(case=case):
                self.assertNotIn("MOCK_CALL", self.refused(case).stdout)

    def test_each_reboot_flag_stops_at_observation(self):
        for case, count in (("reboot_first", 1), ("restart_second", 2), ("preexisting_reboot", 0)):
            with self.subTest(case=case):
                self.assertIn(f"calls={count} ", self.refused(case).stdout)

    def test_each_reboot_flag_never_runs_later_phase(self):
        for case in ("reboot_first", "restart_second", "preexisting_reboot"):
            with self.subTest(case=case):
                self.assertNotIn("step=2", self.refused(case).stdout)

    def test_current_identity_failure_stops(self):
        for case in ("identity_changed", "check_failed"):
            with self.subTest(case=case):
                self.assertIn("calls=0 ", self.refused(case).stdout)

    def test_current_identity_failure_no_install(self):
        for case in ("identity_changed", "check_failed"):
            with self.subTest(case=case):
                self.assertNotIn("MOCK_CALL", self.refused(case).stdout)

    def test_dif_failure_stops(self):
        r = self.refused("call_fail")
        self.assertIn("calls=2 ", r.stdout)

    def test_dif_failure_no_final_install(self):
        self.assertNotIn("step=2", self.refused("call_fail").stdout)

    def test_typed_strings_reject_malformed(self):
        for case in ("typed_bad", "typed_bounds", "typed_nul", "typed_embedded"):
            with self.subTest(case=case):
                self.assertEqual(self.run_case(case).returncode, 0)

    def test_typed_strings_report_invalid(self):
        for case in ("typed_bad", "typed_bounds", "typed_nul", "typed_embedded"):
            with self.subTest(case=case):
                self.assertIn("valid=0", self.run_case(case).stdout)


class SerialWiring(unittest.TestCase):
    def test_bootstrap_install_after_hold_and_wpeinit(self):
        text = (ROOT / "scripts/pe-bootstrap.cmd").read_text()
        self.assertLess(text.index("gpu-prepare.exe --hold"), text.index("\nwpeinit"))
        self.assertLess(text.index("\nwpeinit"), text.index("serial-prepare.exe --prepare"))

    def test_bootstrap_install_before_success_receipt(self):
        text = (ROOT / "scripts/pe-bootstrap.cmd").read_text()
        self.assertIn("serial-prepare.exe --prepare\nif errorlevel 1 goto failed", text)
        self.assertLess(text.index("serial-prepare.exe --prepare"), text.index(">X:"))

    def test_build_stages_new_native_helper(self):
        text = (ROOT / "scripts/Build-Image.ps1").read_text()
        self.assertIn("/Fe:work/programs/serial-prepare.exe", text)
        self.assertIn("programs/serial-prepare.c /link setupapi.lib cfgmgr32.lib advapi32.lib", text)

    def test_bootstrap_stdout_receipt_is_single_writer(self):
        text = (ROOT / "programs/serial-log.c").read_text()
        self.assertIn('!gpu_required && !strncmp(line, "WGI_SERIAL_PREP ", 16)', text)
        self.assertIn("receipt_record(line, used);", text)


if __name__ == "__main__":
    unittest.main()
