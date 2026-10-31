"""CPU-only native orchestration policies; no Windows device or GPU required."""

import ctypes as c
from pathlib import Path
import subprocess
import tempfile
import os
from pe_compile import compile_c
import unittest

ROOT = Path(__file__).resolve().parents[1]
STEP = c.CFUNCTYPE(c.c_int, c.c_int)
WRITE = c.CFUNCTYPE(c.c_int, c.c_void_p, c.c_uint, c.POINTER(c.c_uint))


class Contract(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory(dir="/data" if Path("/data").exists() else None)
        so = Path(cls.tmp.name) / ("policy.dll" if os.name == "nt" else "policy.so")
        compile_c([ROOT / "programs/pe-contract.c"], so, shared=True, defines=("WGI_TEST",))
        cls.lib = c.CDLL(str(so))
        cls.lib.wgi_gsp.argtypes = [STEP]
        cls.lib.wgi_write_all.argtypes = [WRITE, c.c_char_p, c.c_uint]
        cls.lib.wgi_terminal.argtypes = [c.c_char_p, c.c_int, c.c_int]

    @classmethod
    def tearDownClass(cls):
        if os.name == "nt":
            import _ctypes

            _ctypes.FreeLibrary(cls.lib._handle)
        del cls.lib
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

    def test_bootstrap_child_success_is_not_gpu_acceptance(self):
        self.lib.wgi_supervised.argtypes = [c.c_char_p, c.c_int, c.c_int, c.c_int]
        self.assertEqual(self.lib.wgi_supervised(b"", 0, 0, 0), 0)
        self.assertNotEqual(self.lib.wgi_supervised(b"", 0, 0, 1), 0)

    def test_bootstrap_begin_is_not_gpu_verdict(self):
        self.lib.wgi_supervised.argtypes = [c.c_char_p, c.c_int, c.c_int, c.c_int]
        self.assertEqual(self.lib.wgi_supervised(b"WGI_BOOTSTRAP_OK\n", 0, 0, 0), 0)
        self.assertNotEqual(self.lib.wgi_supervised(b"WGI_BOOTSTRAP_OK\n", 0, 0, 1), 0)

    def test_bootstrap_child_error_stops(self):
        self.lib.wgi_supervised.argtypes = [c.c_char_p, c.c_int, c.c_int, c.c_int]
        self.assertNotEqual(self.lib.wgi_supervised(b"", 2, 0, 0), 0)

    def test_bootstrap_timeout_stops(self):
        self.lib.wgi_supervised.argtypes = [c.c_char_p, c.c_int, c.c_int, c.c_int]
        self.assertNotEqual(self.lib.wgi_supervised(b"", 0, 1, 0), 0)

    def tee(self, com_ok, required):
        calls = []

        def diagnostic(p, n, written):
            calls.append(("diag", c.string_at(p, n)))
            written[0] = n
            return 1

        def com(p, n, written):
            calls.append(("com", c.string_at(p, n)))
            written[0] = n if com_ok else 0
            return com_ok

        d, s = WRITE(diagnostic), WRITE(com)
        self.lib.wgi_tee.argtypes = [WRITE, WRITE, c.c_char_p, c.c_uint, c.c_int]
        return self.lib.wgi_tee(d, s, b"diagnostic", 10, required), calls

    def test_diagnostics_survive_com_failure(self):
        rc, calls = self.tee(False, 1)
        self.assertNotEqual(rc, 0)
        self.assertEqual(calls[0], ("diag", b"diagnostic"))

    def test_missing_com_does_not_remove_diagnostics(self):
        rc, calls = self.tee(False, 0)
        self.assertEqual(rc, 0)
        self.assertEqual(calls, [("diag", b"diagnostic")])

    def test_required_serial_has_no_disk_waiver(self):
        self.assertNotEqual(self.tee(False, 1)[0], 0)

    def test_valid_diag_cannot_make_com_failure_success(self):
        rc, calls = self.tee(False, 1)
        self.assertEqual(len(calls), 2)
        self.assertNotEqual(rc, 0)

    def capped(self, chunks, limit=128, fail=False):
        observed = []

        def write(p, n, written):
            if fail:
                written[0] = 0
                return 0
            observed.append(c.string_at(p, n))
            written[0] = n
            return 1

        callback = WRITE(write)
        used, truncated = c.c_uint(0), c.c_int(0)
        self.lib.wgi_bounded_write.argtypes = [
            WRITE,
            c.POINTER(c.c_uint),
            c.POINTER(c.c_int),
            c.c_uint,
            c.c_char_p,
            c.c_uint,
        ]
        statuses = []
        for chunk in chunks:
            statuses.append(
                self.lib.wgi_bounded_write(
                    callback, c.byref(used), c.byref(truncated), limit, chunk, len(chunk)
                )
            )
        return statuses, b"".join(observed), used.value, truncated.value

    def test_receipt_total_byte_cap(self):
        statuses, data, used, truncated = self.capped([b"x" * 1024])
        self.assertEqual(statuses, [0])
        self.assertLessEqual(len(data), 128)
        self.assertEqual(used, len(data))
        self.assertEqual(truncated, 1)

    def test_multiwrite_receipt_cap(self):
        _, data, used, truncated = self.capped([b"a" * 80, b"b" * 80])
        self.assertLessEqual(len(data), 128)
        self.assertEqual(used, len(data))
        self.assertEqual(truncated, 1)

    def test_receipt_censoring_is_explicit(self):
        _, data, _, _ = self.capped([b"x" * 1024])
        self.assertIn(b"WGI_DIAGNOSTIC_TRUNCATED", data)

    def test_repeated_truncation_has_one_marker(self):
        _, data, _, _ = self.capped([b"x" * 1024, b"later"])
        self.assertEqual(data.count(b"WGI_DIAGNOSTIC_TRUNCATED"), 1)
        self.assertNotIn(b"later", data)

    def test_receipt_write_error_is_visible(self):
        statuses, _, _, _ = self.capped([b"entry"], fail=True)
        self.assertNotEqual(statuses[0], 0)

    def test_runtime_write_error_is_visible(self):
        statuses, _, _, _ = self.capped([b"runtime"], limit=512, fail=True)
        self.assertNotEqual(statuses[0], 0)

    def test_small_receipt_preserved(self):
        _, data, used, truncated = self.capped([b"WGI_UART problem=28\r\n"])
        self.assertEqual(data, b"WGI_UART problem=28\r\n")
        self.assertEqual(used, len(data))
        self.assertEqual(truncated, 0)

    def test_explicit_pass(self):
        self.assertEqual(self.lib.wgi_terminal(b"WGI_GPU_PASS\r\n", 0, 0), 0)

    def test_bare_cr_is_not_terminal(self):
        self.assertNotEqual(self.lib.wgi_terminal(b"WGI_GPU_PASS\r", 0, 0), 0)

    def test_second_cr_suffix_is_not_terminal(self):
        self.assertNotEqual(self.lib.wgi_terminal(b"WGI_GPU_PASS\rmore\n", 0, 0), 0)

    def test_cr_suffix_after_pass_is_not_terminal(self):
        self.assertNotEqual(self.lib.wgi_terminal(b"WGI_GPU_PASS\rgarbage\n", 0, 0), 0)

    def test_cr_suffix_fail_after_pass_is_fault(self):
        text = b"WGI_GPU_PASS\nWGI_GPU_FAIL\rgarbage\n"
        self.assertNotEqual(self.lib.wgi_terminal(text, 0, 0), 0)

    def test_prefixed_fail_after_pass_is_fault(self):
        text = b"WGI_GPU_PASS\nPREFIX_WGI_GPU_FAIL\n"
        self.assertNotEqual(self.lib.wgi_terminal(text, 0, 0), 0)

    def test_suffixed_fail_after_pass_is_fault(self):
        text = b"WGI_GPU_PASS\nWGI_GPU_FAIL_SUFFIX\n"
        self.assertNotEqual(self.lib.wgi_terminal(text, 0, 0), 0)

    def test_prefixed_pass_is_not_terminal(self):
        self.assertNotEqual(self.lib.wgi_terminal(b"PREFIX_WGI_GPU_PASS\n", 0, 0), 0)

    def test_suffixed_pass_is_not_terminal(self):
        self.assertNotEqual(self.lib.wgi_terminal(b"WGI_GPU_PASS_SUFFIX\n", 0, 0), 0)

    def test_boot_smoke_separate(self):
        self.assertNotEqual(self.lib.wgi_terminal(b"WGI_PE_BOOT_OK_V1\r\n", 0, 0), 0)


if __name__ == "__main__":
    unittest.main()
