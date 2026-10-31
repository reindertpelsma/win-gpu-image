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
        self.assertEqual(
            [line for line in r.stdout.splitlines() if line.startswith("MOCK_CALL")],
            ["MOCK_CALL step=0", "MOCK_CALL step=1", "MOCK_CALL step=2"],
        )

    def test_normal_pipeline_checked_after_each(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0)
        self.assertIn("calls=3 selected=1 checks=4", r.stdout)

    def test_existing_com_skips_install(self):
        r = self.run_case("com_present")
        self.assertEqual(r.returncode, 0)
        self.assertIn("calls=0 selected=0", r.stdout)

    def test_existing_com_never_calls_dif(self):
        r = self.run_case("com_present")
        self.assertEqual(r.returncode, 0)
        self.assertNotIn("MOCK_CALL", r.stdout)

    def refused(self, name):
        r = self.run_case(name)
        self.assertNotEqual(r.returncode, 0, r.stdout)
        return r

    def test_guard_rejects_each_bad_snapshot(self):
        for case in (
            "duplicate",
            "parallel",
            "wrong_class",
            "wrong_service",
            "not_started",
            "problem",
            "wrong_inf",
            "wrong_provider",
            "missing_section",
            "wrong_matching",
            "installer_missing",
            "name_present",
            "name_unknown",
            "com_unknown",
        ):
            with self.subTest(case=case):
                self.assertIn("calls=0 selected=0", self.refused(case).stdout)

    def test_guard_never_selects_bad_snapshot(self):
        for case in (
            "duplicate",
            "parallel",
            "wrong_class",
            "wrong_service",
            "not_started",
            "problem",
            "wrong_inf",
            "wrong_provider",
            "missing_section",
            "wrong_matching",
            "installer_missing",
            "name_present",
            "name_unknown",
            "com_unknown",
        ):
            with self.subTest(case=case):
                self.assertNotIn("MOCK_CALL", self.refused(case).stdout)

    def test_bad_snapshot_never_enters_selection(self):
        for case in ("not_started", "problem"):
            with self.subTest(case=case):
                r = self.run_case(case)
                self.assertIn("selected=0 ", r.stdout)

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

    def test_multi_sz_type_and_capacity_are_independent_guards(self):
        for case in ("multi_capacity", "multi_type"):
            with self.subTest(case=case):
                self.assertEqual(self.run_case(case).returncode, 0)

    def test_multi_sz_type_and_capacity_report_invalid(self):
        for case in ("multi_capacity", "multi_type"):
            with self.subTest(case=case):
                self.assertIn("MOCK_MULTI member=-1", self.run_case(case).stdout)

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
        self.assertIn("serial-prepare.exe --prepare", text)
        self.assertLess(text.index("gpu-prepare.exe --hold"), text.index("\nwpeinit"))
        self.assertLess(text.index("\nwpeinit"), text.index("serial-prepare.exe --prepare"))

    def test_bootstrap_install_before_success_receipt(self):
        text = (ROOT / "scripts/pe-bootstrap.cmd").read_text()
        self.assertIn("serial-prepare.exe --prepare\nif errorlevel 1 goto failed", text)
        self.assertLess(text.index("serial-prepare.exe --prepare"), text.index(">X:"))

    def test_build_stages_new_native_helper(self):
        text = (ROOT / "scripts/Build-Image.ps1").read_text()
        self.assertIn("/Fe:work/programs/serial-prepare.exe", text)
        self.assertIn(
            "programs/serial-prepare.c /link setupapi.lib cfgmgr32.lib advapi32.lib", text
        )

    def test_final_postretry_diagnostics_are_retained(self):
        text = (ROOT / "programs/serial-log.c").read_text()
        self.assertIn('serial_diagnostics("after-reenumerate-com-retry-failed")', text)
        self.assertIn("ports_install_diagnostics();", text)

    def test_final_diagnostics_include_install_identity(self):
        text = (ROOT / "programs/serial-log.c").read_text()
        for field in ("InfSection", "InfSectionExt", "MatchingDeviceId", "ProviderName"):
            self.assertIn(f'registry_string(software, "{field}"', text)
        self.assertIn("WGI_UART_INTERFACE id=", text)

    def test_bootstrap_stdout_receipt_is_single_writer(self):
        text = (ROOT / "programs/serial-log.c").read_text()
        self.assertIn('!gpu_required && !strncmp(line, "WGI_SERIAL_PREP ", 16)', text)
        self.assertIn("receipt_record(line, used);", text)


if __name__ == "__main__":
    unittest.main()


class SerialWinAPI(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(dir="/data" if Path("/data").exists() else None)
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.exe = Path(cls.tmp.name) / ("serial-win.exe" if os.name == "nt" else "serial-win")
        compile_c([ROOT / "tests/serial_winmock.c"], cls.exe)

    def run_case(self, name):
        return subprocess.run([str(self.exe), name], text=True, capture_output=True, timeout=5)

    def test_observed_star_hardware_match_runs_normal_pipeline(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn("calls=3 ", r.stdout)

    def test_observed_star_hardware_match_checks_each_phase(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn("name=MatchingDeviceId value=*PNP0501", r.stdout)
        self.assertEqual(r.stdout.count("selected same_installed=1"), 5)

    def test_hardware_match_is_case_insensitive_exact(self):
        r = self.run_case("hardware_case")
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn("calls=3 ", r.stdout)

    def test_hardware_case_preserves_install_identity(self):
        r = self.run_case("hardware_case")
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertEqual(r.stdout.count("selected same_installed=1"), 5)

    def test_hardware_malformed_and_nonmember_stop(self):
        for case in (
            "hardware_wrong_type",
            "hardware_read_error",
            "hardware_size",
            "hardware_no_double_nul",
            "hardware_unterminated",
            "hardware_trailing",
            "hardware_nonmember",
            "hardware_star_not_glob",
            "hardware_early_match_bad_tail",
            "hardware_early_empty",
        ):
            with self.subTest(case=case):
                r = self.run_case(case)
                self.assertNotEqual(r.returncode, 0, r.stdout)
                self.assertIn("calls=0 builds=0", r.stdout)

    def test_hardware_malformed_and_nonmember_never_install(self):
        for case in (
            "hardware_wrong_type",
            "hardware_read_error",
            "hardware_size",
            "hardware_no_double_nul",
            "hardware_unterminated",
            "hardware_trailing",
            "hardware_nonmember",
            "hardware_star_not_glob",
            "hardware_early_match_bad_tail",
            "hardware_early_empty",
        ):
            with self.subTest(case=case):
                self.assertNotIn("WINMOCK_DIF", self.run_case(case).stdout)

    def test_hardware_membership_rechecked_after_each_dif(self):
        for case in ("hardware_after_dif", "hardware_member_lost", "matching_after_dif"):
            with self.subTest(case=case):
                r = self.run_case(case)
                self.assertNotEqual(r.returncode, 0, r.stdout)
                self.assertIn("calls=1 ", r.stdout)

    def test_hardware_membership_change_blocks_next_dif(self):
        for case in ("hardware_after_dif", "hardware_member_lost", "matching_after_dif"):
            with self.subTest(case=case):
                r = self.run_case(case)
                self.assertNotIn("WINMOCK_DIF function=32", r.stdout)
                self.assertNotIn("WINMOCK_DIF function=2\n", r.stdout)

    def test_refused_candidate_provider_is_visible(self):
        r = self.run_case("selected_provider")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("selected_driver provider_valid=1 provider=Other", r.stdout)

    def test_malformed_candidate_provider_never_printed(self):
        r = self.run_case("selected_provider_nul")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("selected_driver provider_valid=0 provider=UNPROVEN", r.stdout)
        self.assertNotIn("xxxxxxxx", r.stdout)

    def test_malformed_candidate_provider_does_not_leak_bytes(self):
        r = self.run_case("selected_provider_nul")
        self.assertNotIn("xxxxxxxx", r.stdout)
        self.assertIn("provider=UNPROVEN", r.stdout)

    def test_refused_candidate_inf_is_visible(self):
        r = self.run_case("selected_inf")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("selected_detail inf_valid=1 inf=X:\\Other\\msports.inf", r.stdout)

    def test_normal_candidate_inf_is_visible(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0)
        self.assertIn("selected_detail inf_valid=1 inf=X:\\Windows\\INF\\msports.inf", r.stdout)

    def test_refused_resolved_section_is_visible(self):
        r = self.run_case("selected_section")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("selected_actual valid=1 section=Other.NT matches_current=0", r.stdout)

    def test_normal_resolved_section_is_visible(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0)
        self.assertIn("selected_actual valid=1 section=ComPort.NT matches_current=1", r.stdout)

    def test_cleared_flags_receipt_before_refusal(self):
        r = self.run_case("cleared_flagsex")
        self.assertNotEqual(r.returncode, 0)
        self.assertIn("install_params flags=8388608 flagsex=0 restart_flags=0", r.stdout)
        self.assertNotIn("WINMOCK_DIF", r.stdout)

    def test_normal_flags_receipt_before_call(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0)
        self.assertIn("install_params flags=8388608 flagsex=67110912 restart_flags=0", r.stdout)

    def test_real_adapter_normal_pipeline(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertEqual(
            [line for line in r.stdout.splitlines() if line.startswith("WINMOCK_DIF")],
            ["WINMOCK_DIF function=34", "WINMOCK_DIF function=32", "WINMOCK_DIF function=2"],
        )

    def test_real_adapter_installed_only_quiet_flags(self):
        r = self.run_case("ok")
        self.assertEqual(r.returncode, 0, r.stdout)
        self.assertIn("WINMOCK_LIST flags=8388608 flagsex=67110912", r.stdout)

    def test_real_adapter_guard_errors_stop(self):
        for case in (
            "duplicate",
            "instance_size",
            "property_type",
            "registry_type",
            "registry_bounds",
            "registry_nul",
            "hardware_key_error",
            "namespace_error",
            "installer_missing",
            "installer_untrusted",
            "dll_missing",
            "multiple_drivers",
            "selected_provider",
            "selected_inf",
            "detail_size",
            "detail_nul",
            "selected_section",
        ):
            with self.subTest(case=case):
                r = self.run_case(case)
                self.assertNotEqual(r.returncode, 0, r.stdout)
                self.assertIn("calls=0 ", r.stdout)

    def test_real_adapter_guard_errors_never_install(self):
        for case in (
            "duplicate",
            "instance_size",
            "property_type",
            "registry_type",
            "registry_bounds",
            "registry_nul",
            "hardware_key_error",
            "namespace_error",
            "installer_missing",
            "installer_untrusted",
            "dll_missing",
            "multiple_drivers",
            "selected_provider",
            "selected_inf",
            "detail_size",
            "detail_nul",
            "selected_section",
        ):
            with self.subTest(case=case):
                self.assertNotIn("WINMOCK_DIF", self.run_case(case).stdout)

    def test_real_adapter_mid_pipeline_flags_stop(self):
        for case, calls in (("reboot_first", 1), ("restart_second", 2), ("selected_changed", 1)):
            with self.subTest(case=case):
                r = self.run_case(case)
                self.assertNotEqual(r.returncode, 0, r.stdout)
                self.assertIn(f"calls={calls} ", r.stdout)

    def test_real_adapter_mid_pipeline_no_final_install(self):
        for case in ("reboot_first", "restart_second", "selected_changed"):
            with self.subTest(case=case):
                self.assertNotIn("WINMOCK_DIF function=2\n", self.run_case(case).stdout)
