"""CPU branch checks cannot enter the credentialed image-publishing path."""

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


class CpuWorkflowTests(unittest.TestCase):
    def source(self):
        path = ROOT / ".github/workflows/source.yml"
        self.assertTrue(path.is_file(), "bounded credential-free CPU workflow is missing")
        return path.read_text()

    def test_source_checks_have_no_publish_authority(self):
        text = self.source()
        self.assertIn("permissions: {}", text)
        self.assertNotIn("secrets", text)
        self.assertNotIn("environment:", text)
        self.assertNotIn("write", text)
        self.assertNotIn("Publish-Image", text)
        self.assertNotIn("Build-Image", text)
        self.assertNotIn("Fetch-Inputs", text)

    def test_source_checks_are_confined_to_read_only_cpu_jobs(self):
        text = self.source()
        forbidden = ["secrets", "environment:", "id-token", "attestations", "write"]
        self.assertTrue(all(word not in text for word in forbidden))
        self.assertEqual(re.findall(r"^      contents: (\w+)$", text, re.M), ["read", "read"])
        self.assertNotIn("upload-artifact", text)
        self.assertIn("persist-credentials: false", text)

    def test_source_events_do_not_run_unreviewed_pull_requests(self):
        text = self.source()
        self.assertRegex(text, r"branches:\s*\[main,\s*['\"]codex/\*\*['\"]\]")
        self.assertIn("workflow_dispatch:", text)
        for event in ["pull_request", "workflow_run", "issue_comment"]:
            self.assertNotIn(event, text)
        self.assertEqual(
            re.findall(r"^  ([\w]+):$", text.split("permissions:")[0], re.M),
            ["push", "workflow_dispatch"],
        )

    def test_every_action_is_pinned_and_both_platforms_are_bounded(self):
        text = self.source()
        uses = re.findall(r"uses:\s*(\S+)", text)
        self.assertEqual(len(uses), 2)
        self.assertTrue(all(re.fullmatch(r"actions/checkout@[0-9a-f]{40}", x) for x in uses))
        self.assertEqual(text.count("timeout-minutes: 20"), 2)
        self.assertIn("runs-on: ubuntu-24.04", text)
        self.assertIn("runs-on: windows-2022", text)
        self.assertIn("python3 -m unittest discover -s tests -v", text)
        self.assertIn("./tests/Test-Source.ps1", text)
