"""Hypothesis: recipe helpers reject changed inputs and preserve image bytes.
Falsifier: any tampered input/part passes, or splitting changes the joined digest.
Run: python -m unittest discover -s tests -v
"""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from recipe import missing_inf_sources, sha256_file, split_file, stage_driver, verify_file, verify_manifest


class RecipeTests(unittest.TestCase):
    def test_hash_pin_is_fail_closed(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "input"
            p.write_bytes(b"trusted first observation")
            digest = sha256_file(p)
            verify_file(p, digest)
            p.write_bytes(b"changed upstream")
            with self.assertRaises(ValueError):
                verify_file(p, digest)
            with self.assertRaises(ValueError):
                verify_file(p, "")

    def test_split_reconstruction_and_tamper(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            p = root / "image.iso"
            payload = bytes(range(251)) * 9
            p.write_bytes(payload)
            parts = split_file(p, root / "release", 1024)
            self.assertEqual([x.stat().st_size for x in parts], [1024, 1024, 211])
            self.assertEqual(b"".join(x.read_bytes() for x in parts), payload)
            manifest = {"image_sha256": sha256_file(p), "image_bytes": len(payload),
                        "parts": [{"name": x.name, "bytes": x.stat().st_size,
                                   "sha256": sha256_file(x)} for x in parts]}
            verify_manifest(manifest, root / "release")
            parts[1].write_bytes(b"x" * 1024)
            with self.assertRaises(ValueError):
                verify_manifest(manifest, root / "release")

    def test_manifest_rejects_wrong_image_and_path_traversal(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "image.iso"
            p.write_bytes(b"valid")
            m = {"image_sha256": "0" * 64, "image_bytes": 5,
                 "parts": [{"name": p.name, "bytes": 5, "sha256": sha256_file(p)}]}
            with self.assertRaises(ValueError):
                verify_manifest(m, Path(d))
            m["parts"][0]["name"] = "../image.iso"
            with self.assertRaises(ValueError):
                verify_manifest(m, Path(d))

    def test_display_package_is_preserved_byte_for_byte(self):
        with tempfile.TemporaryDirectory() as d:
            source, staged = Path(d) / "source", Path(d) / "staged"
            source.mkdir()
            files = {"nv_dispi.inf": b'[SourceDisksFiles]\nnvcpl.dll=1\nlibnvidia.so=1\n',
                     "nv_disp.cat": b"signed catalog", "nvcpl.dll": b"control panel",
                     "libnvidia.so": b"referenced Linux payload", "NvCamera/camera.dll": b"Ansel",
                     "nv_other.inf": b"other unmodified INF", "setup.exe": b"included metadata"}
            for name, value in files.items():
                p = source / name; p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(value)
            sizes = Path(d) / "sizes.json"
            stage_driver(source, staged, sizes)
            self.assertEqual({p.relative_to(staged).as_posix(): p.read_bytes()
                              for p in staged.rglob("*") if p.is_file()}, files)
            data = json.loads(sizes.read_text())
            self.assertEqual(data["removed_bytes"], 0)
            self.assertEqual(data["retained_bytes"], sum(map(len, files.values())))
            self.assertTrue(all(x["keep"] and x["sha256"] for x in data["files"]))

    def test_missing_inf_source_diagnostics_include_subdirectories(self):
        with tempfile.TemporaryDirectory() as d:
            source, staged = Path(d) / "source", Path(d) / "staged"
            (source / "NvCamera").mkdir(parents=True)
            staged.mkdir()
            inf = source / "nv_dispi.inf"
            inf.write_text('[SourceDisksFiles]\nNvCamera64.dll = 1,NvCamera\nnvcuda.dll = 1\n')
            (source / "NvCamera/NvCamera64.dll").write_bytes(b"camera")
            (source / "nvcuda.dll").write_bytes(b"cuda")
            (staged / "nvcuda.dll").write_bytes(b"cuda")
            self.assertEqual(missing_inf_sources(inf, source, staged),
                             [{"file": "NvCamera/NvCamera64.dll", "bytes": 6}])


if __name__ == "__main__":
    unittest.main()
