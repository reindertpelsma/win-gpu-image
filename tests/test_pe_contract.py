"""CPU-only native orchestration policies; no Windows device or GPU required."""
import ctypes as c
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
STEP = c.CFUNCTYPE(c.c_int, c.c_int)
WRITE = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint, c.POINTER(c.c_uint))


class Contract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(dir="/data")
        so = Path(cls.tmp.name) / "policy.so"
        subprocess.run(["cc", "-shared", "-fPIC", "-DWGI_TEST", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "programs/pe-contract.c"), "-o", str(so)], check=True)
        cls.lib = c.CDLL(str(so))
        cls.lib.wgi_gsp.argtypes = [STEP]
        cls.lib.wgi_write_all.argtypes = [WRITE, c.c_char_p, c.c_uint]
        cls.lib.wgi_terminal.argtypes = [c.c_char_p, c.c_int, c.c_int]

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def sequence(self, fail=-1):
        calls = []
        def step(n):
            calls.append(n)
            return n != fail
        cb = STEP(step)
        result = self.lib.wgi_gsp(cb)
        return result, calls

    def test_complete_readbacks_before_enable(self):
        rc, calls = self.sequence()
        self.assertEqual(rc, 0)
        self.assertEqual(calls, list(range(9)))

    def test_install_after_disable(self):
        _, calls = self.sequence()
        self.assertLess(calls.index(0), calls.index(1))

    def test_class_failure_never_enables(self):
        rc, calls = self.sequence(5)
        self.assertNotEqual(rc, 0)
        self.assertNotIn(8, calls)

    def test_service_failure_never_enables(self):
        rc, calls = self.sequence(6)
        self.assertNotEqual(rc, 0)
        self.assertNotIn(8, calls)

    def test_disable_failure_prevents_install(self):
        rc, calls = self.sequence(0)
        self.assertNotEqual(rc, 0)
        self.assertEqual(calls, [0])

    def test_install_failure_prevents_keys(self):
        rc, calls = self.sequence(1)
        self.assertNotEqual(rc, 0)
        self.assertNotIn(2, calls)

    def test_class_write_failure_stops(self):
        rc, calls = self.sequence(4)
        self.assertNotEqual(rc, 0)
        self.assertNotIn(8, calls)

    def test_service_write_failure_stops(self):
        rc, calls = self.sequence(3)
        self.assertNotEqual(rc, 0)
        self.assertNotIn(8, calls)

    def writer(self, sizes, ok=True):
        seen = []
        def write(p, n, written):
            seen.append(c.string_at(p, n))
            written[0] = sizes.pop(0) if sizes else n
            return ok
        cb = WRITE(write)
        return self.lib.wgi_write_all(cb, b"abcdef", 6), seen

    def test_partial_write_retries_remaining(self):
        rc, seen = self.writer([2, 4])
        self.assertEqual(rc, 0)
        self.assertEqual(seen, [b"abcdef", b"cdef"])

    def test_three_partial_writes_preserve_bytes(self):
        rc, seen = self.writer([1, 2, 3])
        self.assertEqual(rc, 0)
        self.assertEqual(seen, [b"abcdef", b"bcdef", b"def"])

    def test_zero_write_fails(self):
        rc, _ = self.writer([0])
        self.assertNotEqual(rc, 0)

    def test_write_error_fails(self):
        rc, _ = self.writer([0], False)
        self.assertNotEqual(rc, 0)

    def test_timeout_cannot_pass(self):
        self.assertNotEqual(self.lib.wgi_terminal(b"WGI_GPU_PASS\r\n", 0, 1), 0)

    def test_child_failure_cannot_pass(self):
        self.assertNotEqual(self.lib.wgi_terminal(b"WGI_GPU_PASS\r\n", 2, 0), 0)

    def test_missing_marker_cannot_pass(self):
        self.assertNotEqual(self.lib.wgi_terminal(b"", 0, 0), 0)

    def test_fail_marker_cannot_pass(self):
        self.assertNotEqual(self.lib.wgi_terminal(b"WGI_GPU_FAIL\r\n", 0, 0), 0)

    def test_explicit_pass(self):
        self.assertEqual(self.lib.wgi_terminal(b"WGI_GPU_PASS\r\n", 0, 0), 0)

    def test_boot_smoke_separate(self):
        self.assertNotEqual(self.lib.wgi_terminal(b"WGI_PE_BOOT_OK_V1\r\n", 0, 0), 0)


if __name__ == "__main__":
    unittest.main()
