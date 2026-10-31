"""Cross-compile CPU doubles without redefining Windows compiler/runtime APIs."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class WindowsMockPortability(unittest.TestCase):
    def build(self, sources, includes=(), compile_only=False):
        compiler = shutil.which("x86_64-w64-mingw32-gcc")
        if compiler is None:
            self.skipTest("optional Linux-to-Windows compiler absent; native Windows tests still run")
        with tempfile.TemporaryDirectory(prefix="wgi-cross-") as scratch:
            command = [
                compiler,
                "-std=c11",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-Wno-cast-function-type",
            ]
            for folder in includes:
                command += ["-I", str(ROOT / folder)]
            command += [str(ROOT / name) for name in sources]
            if compile_only:
                command += ["-c"]
            command += ["-o", str(Path(scratch) / ("mock.obj" if compile_only else "mock.exe"))]
            result = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_cuda_double_links_without_windows_api_collision(self):
        self.build(
            ["programs/cuda-query.c", "tests/cuda_mock/nvcuda_mock.c"],
            ["tests/cuda_mock"],
        )

    def test_device_double_links_without_windows_api_collision(self):
        self.build(["tests/gpu_mock.c", "programs/pe-contract.c"])

    def test_cuda_mock_object_preserves_windows_calling_convention(self):
        self.build(["tests/cuda_mock/nvcuda_mock.c"], ["tests/cuda_mock"], compile_only=True)
