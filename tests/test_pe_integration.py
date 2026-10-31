"""Boundary contracts that can be checked without running Windows servicing or a GPU."""

from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Integration(unittest.TestCase):
    def test_offline_service_gsp_is_staged(self):
        build = (ROOT / "scripts/Build-Image.ps1").read_text()
        self.assertIn("EnableGpuFirmware", build)
        self.assertIn("nvlddmkm", build)
        self.assertIn("Set-OfflinePeServices", build)

    def test_offline_first_start_hold(self):
        build = (ROOT / "scripts/Build-Image.ps1").read_text()
        self.assertIn("'Start' -Value 4", build)
        self.assertIn("ControlSet", build)

    def test_nv_service_cannot_start_at_boot(self):
        build = (ROOT / "scripts/Build-Image.ps1").read_text()
        policy = build[build.index("function Set-OfflinePeServices") :]
        self.assertIn("-Path $nv -Name 'Start' -Value 4", policy)
        self.assertNotIn("-Path $nv -Name 'Start' -Value 3", policy)

    def test_boot_smoke_has_no_serial_waiver(self):
        script = (ROOT / "tests/Smoke-Boot.ps1").read_text()
        self.assertNotIn("serial_required = $false", script)
        self.assertIn("$serial_boot_marker -and $serial_terminal_fail", script)

    def test_helper_is_built(self):
        build = (ROOT / "scripts/Build-Image.ps1").read_text()
        self.assertIn("/Fe:work/programs/gpu-prepare.exe", build)
        self.assertIn("setupapi.lib cfgmgr32.lib advapi32.lib", build)

    def test_shared_contract_is_linked(self):
        build = (ROOT / "scripts/Build-Image.ps1").read_text()
        self.assertIn("/c programs/pe-contract.c", build)
        self.assertGreaterEqual(build.count("work/programs/pe-contract.obj"), 3)

    def test_bootstrap_holds_before_wpeinit(self):
        path = ROOT / "scripts/pe-bootstrap.cmd"
        self.assertTrue(path.exists(), "native bootstrap command must be staged")
        script = path.read_text()
        self.assertLess(script.index("gpu-prepare.exe --hold"), script.index("\nwpeinit\n"))
        self.assertIn("if not defined WGI_NO_GPU goto failed", script)

    def test_bootstrap_requires_hold_result_before_wpeinit(self):
        path = ROOT / "scripts/pe-bootstrap.cmd"
        self.assertTrue(path.exists(), "bootstrap command required")
        script = path.read_text()
        self.assertLess(
            script.index("if not defined WGI_NO_GPU goto failed"), script.index("\nwpeinit\n")
        )

    def test_startnet_gates_gpu_on_bootstrap_success(self):
        script = (ROOT / "scripts/startnet.cmd").read_text()
        self.assertIn("--bootstrap", script)
        self.assertLess(script.index("if errorlevel 1 goto failed"), script.index("--run 240"))

    def test_bootstrap_precedes_required_serial(self):
        script = (ROOT / "scripts/startnet.cmd").read_text()
        self.assertIn("--bootstrap", script)
        self.assertLess(script.index("--bootstrap"), script.index("--run 240"))
        self.assertIn("if errorlevel 1 goto failed", script)

    def test_bootstrap_receipt_precedes_gpu_probes(self):
        script = (ROOT / "scripts/gpu-tests.cmd").read_text()
        self.assertIn("if not exist X:\\wgi-bootstrap-ok.txt goto failed", script)
        self.assertNotIn("\nwpeinit\n", script)

    def test_bootstrap_is_in_image(self):
        script = (ROOT / "scripts/Build-Image.ps1").read_text()
        self.assertIn("'pe-bootstrap.cmd'", script)

    def test_runtime_failure_stops_probes(self):
        script = (ROOT / "scripts/gpu-tests.cmd").read_text()
        self.assertIn('gpu-prepare.exe "%WGI_DRIVER_INF%"\nif errorlevel 1 goto failed', script)
        self.assertIn("exit /b 1", script)

    def test_actual_com_serial_is_smoke_requirement(self):
        script = (ROOT / "tests/Smoke-Boot.ps1").read_text()
        self.assertIn("serial_required = $true", script)
        self.assertIn("serial_boot_marker", script)
        self.assertIn("serial_terminal_fail", script)

    def test_smoke_rejects_gpu_pass_without_gpu(self):
        script = (ROOT / "tests/Smoke-Boot.ps1").read_text()
        self.assertIn("unexpected_gpu_pass", script)
        self.assertIn("first_terminal_seconds", script)
        self.assertNotIn("COM1 is optional", script)

    def test_software_key_is_device_specific(self):
        source = (ROOT / "programs/gpu-prepare.c").read_text()
        self.assertIn("SetupDiOpenDevRegKey", source)
        self.assertIn("DIREG_DRV, KEY_SET_VALUE | KEY_QUERY_VALUE", source)
        self.assertIn("property(SPDRP_DRIVER", source)

    def test_install_keeps_device_offline(self):
        source = (ROOT / "programs/gpu-prepare.c").read_text()
        self.assertIn("DI_ENUMSINGLEINF | DI_DONOTCALLCONFIGMG", source)
        self.assertIn("DIF_INSTALLDEVICE", source)


if __name__ == "__main__":
    unittest.main()
