"""Hypothesis: MSI table mapping selects amd64 members without running setup.
Falsifier: a member maps to the wrong cabinet, architecture, size, or target name.
"""
from pathlib import Path
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from extract_msi import file_plan


class MsiTests(unittest.TestCase):
    def test_directory_long_names_and_cabinet_sequence(self):
        tables = {
            "Directory": [{"Directory": "TARGETDIR", "Directory_Parent": "", "DefaultDir": "SourceDir"},
                          {"Directory": "kit", "Directory_Parent": "TARGETDIR", "DefaultDir": "short|Windows Kits"},
                          {"Directory": "x64", "Directory_Parent": "kit", "DefaultDir": "amd64"},
                          {"Directory": "arm", "Directory_Parent": "kit", "DefaultDir": "arm64"}],
            "Component": [{"Component": "c1", "Directory_": "x64"}, {"Component": "c2", "Directory_": "arm"}],
            "Media": [{"LastSequence": "0", "Cabinet": ""}, {"LastSequence": "2", "Cabinet": "first.cab"},
                      {"LastSequence": "4", "Cabinet": "second.cab"}],
            "File": [{"File": "id1", "Component_": "c1", "FileName": "short|winpe.wim", "FileSize": "123", "Sequence": "3"},
                     {"File": "id2", "Component_": "c2", "FileName": "winpe.wim", "FileSize": "456", "Sequence": "1"}]}
        self.assertEqual(file_plan(tables), [{"cabinet": "second.cab", "member": "id1",
                                             "target": "Windows Kits/amd64/winpe.wim", "bytes": 123}])
