"""Run the actual probe C against synthetic WinAPI/CUDA; no GPU or NVIDIA DLL calls."""

import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
STAGES = (
    "init",
    "count",
    "version",
    "name",
    "device",
    "create",
    "load",
    "function",
    "alloc",
    "upload",
    "launch",
    "sync",
    "download",
    "free",
    "unload",
    "destroy",
    "library_free",
)
EXPORTS = (
    "cuDeviceGet",
    "cuCtxCreate_v2",
    "cuModuleLoadData",
    "cuModuleGetFunction",
    "cuMemAlloc_v2",
    "cuMemcpyHtoD_v2",
    "cuLaunchKernel",
    "cuCtxSynchronize",
    "cuMemcpyDtoH_v2",
    "cuMemFree_v2",
    "cuModuleUnload",
    "cuCtxDestroy_v2",
)


def host_compiler():
    """Find installed tools; Windows recipe tests precede Build-Image's VS activation."""
    env = dict(os.environ)
    compiler = next((shutil.which(n) for n in ("cl", "gcc", "clang") if shutil.which(n)), None)
    if compiler is not None or os.name != "nt":
        return compiler, env
    vswhere = (
        Path(env.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
        / "Microsoft Visual Studio/Installer/vswhere.exe"
    )
    if not vswhere.is_file():
        return None, env
    found = subprocess.run(
        [
            str(vswhere),
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property",
            "installationPath",
        ],
        capture_output=True,
        text=True,
        timeout=30,
        check=True,
    )
    install = found.stdout.strip()
    if not install:
        return None, env
    devcmd = Path(install) / "Common7/Tools/VsDevCmd.bat"
    ready = subprocess.run(
        ["cmd.exe", "/d", "/s", "/c", f'call "{devcmd}" -arch=x64 -host_arch=x64 >nul && set'],
        capture_output=True,
        text=True,
        timeout=60,
        check=True,
    )
    env = {k.upper(): v for k, v in env.items()}
    for line in ready.stdout.splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            env[key.upper()] = value
    path = env.get("PATH", "")
    return shutil.which("cl", path=path), env


class CompiledCudaProbe(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler, compile_env = host_compiler()
        if compiler is None:
            raise unittest.SkipTest(
                "CPU mock needs installed MSVC, GCC or Clang; GPU is never used"
            )
        cls.temp = tempfile.TemporaryDirectory(prefix="wgi-cuda-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.exe = Path(cls.temp.name) / ("cuda-mock.exe" if os.name == "nt" else "cuda-mock")
        mock = ROOT / "tests/cuda_mock"
        sources = [str(ROOT / "programs/cuda-query.c"), str(mock / "nvcuda_mock.c")]
        if Path(compiler).stem.lower() == "cl":
            args = [
                compiler,
                "/nologo",
                "/std:c11",
                "/W4",
                "/WX",
                "/wd4191",
                "/I" + str(mock),
                "/Fe:" + str(cls.exe),
                "/Fo:" + cls.temp.name + os.sep,
                *sources,
            ]
        else:
            args = [
                compiler,
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-Wno-cast-function-type",
                "-I",
                str(mock),
                *sources,
                "-o",
                str(cls.exe),
            ]
        built = subprocess.run(
            args, cwd=cls.temp.name, env=compile_env, capture_output=True, text=True, timeout=60
        )
        if built.returncode:
            raise AssertionError(built.stdout + built.stderr)

    def run_probe(self, failure="", missing=""):
        env = dict(os.environ, CUDA_MOCK_FAIL=failure, CUDA_MOCK_MISSING=missing)
        return subprocess.run([str(self.exe)], env=env, capture_output=True, text=True, timeout=5)

    def assert_refused(self, run):
        self.assertNotEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertIn("CUDA_FAIL", run.stdout)
        self.assertNotIn("CUDA_PASS", run.stdout)

    def test_success_requires_compute_and_verified_byte_count(self):
        run = self.run_probe()
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        self.assertIn("CUDA_PASS devices=1 driver_api=13000", run.stdout)
        self.assertIn("compute=PASS words=32 bytes=128", run.stdout)

    def test_success_checks_actual_launch_sync_and_reverse_cleanup(self):
        run = self.run_probe()
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        events = [line.removeprefix("MOCK ") for line in run.stderr.splitlines()]
        self.assertEqual(events, list(STAGES))

    def test_first_word_corruption_is_not_enumeration_success(self):
        self.assert_refused(self.run_probe("corrupt_first"))

    def test_last_word_corruption_is_not_partial_readback_success(self):
        self.assert_refused(self.run_probe("corrupt_last"))

    def test_all_driver_and_cleanup_errors_refuse_success(self):
        for stage in STAGES:
            with self.subTest(stage=stage):
                self.assert_refused(self.run_probe(stage))

    def test_driver_error_stops_dependent_work_before_cleanup(self):
        for stage, forbidden in (
            ("init", "create"),
            ("device", "create"),
            ("upload", "launch"),
            ("launch", "sync"),
            ("sync", "download"),
        ):
            with self.subTest(stage=stage):
                run = self.run_probe(stage)
                self.assert_refused(run)
                self.assertNotIn("MOCK " + forbidden, run.stderr)

    def test_acquired_resources_are_released_even_after_failures(self):
        for stage in (
            "load",
            "function",
            "alloc",
            "upload",
            "launch",
            "sync",
            "download",
            "free",
            "unload",
            "destroy",
            "library_free",
        ):
            with self.subTest(stage=stage):
                run = self.run_probe(stage)
                events = run.stderr.splitlines()
                self.assertIn("MOCK destroy", events)
                self.assertEqual(events[-1], "MOCK library_free")
                if stage not in ("load",):
                    self.assertIn("MOCK unload", events)
                if stage not in ("load", "function", "alloc"):
                    self.assertIn("MOCK free", events)
                self.assertNotIn("CUDA_PASS", run.stdout)

    def test_cleanup_failure_still_reaches_context_destroy_and_dll_release(self):
        for stage in ("free", "unload"):
            with self.subTest(stage=stage):
                run = self.run_probe(stage)
                self.assertIn("MOCK destroy", run.stderr)
                self.assertIn("MOCK library_free", run.stderr)
                self.assert_refused(run)

    def test_missing_compute_api_refuses_before_init(self):
        for export in EXPORTS:
            with self.subTest(export=export):
                run = self.run_probe(missing=export)
                self.assert_refused(run)
                self.assertNotIn("MOCK init", run.stderr)

    def test_missing_compute_api_releases_dll(self):
        for export in EXPORTS:
            with self.subTest(export=export):
                self.assertIn("MOCK library_free", self.run_probe(missing=export).stderr)

    def test_zero_devices_refused_and_library_released(self):
        run = self.run_probe("zero_devices")
        self.assert_refused(run)
        self.assertIn("MOCK library_free", run.stderr)

    def test_excess_device_count_is_bounded_and_library_released(self):
        run = self.run_probe("excess_devices")
        self.assert_refused(run)
        self.assertNotIn("MOCK name", run.stderr)
        self.assertIn("MOCK library_free", run.stderr)
