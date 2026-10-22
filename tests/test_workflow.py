"""Hypothesis: only trusted entry points reach the single credentialed step.
Falsifier: an extra trigger, permission, unpinned action, or early store secret.
This is a source contract check; GitHub's executor remains a trust dependency.
"""
from pathlib import Path
import re
import unittest


class WorkflowTests(unittest.TestCase):
    def test_workflow_contract(self):
        s = (Path(__file__).resolve().parents[1] / ".github/workflows/build.yml").read_text()
        self.assertIn("permissions: {}", s)
        self.assertIn("branches: [main]", s)
        for forbidden in ["pull_request", "workflow_run", "issue_comment", "upload-artifact"]:
            self.assertNotIn(forbidden, s)
        uses = re.findall(r"uses:\s*(\S+)", s)
        self.assertEqual(len(uses), 2)
        self.assertTrue(all(re.fullmatch(r"[\w/-]+@[0-9a-f]{40}", x) for x in uses))
        self.assertEqual(s.count("secrets.STORE_GITHUB_PUSH_TOKEN"), 1)
        tail = s.split("- name: Publish to private store", 1)[1]
        self.assertIn("secrets.STORE_GITHUB_PUSH_TOKEN", tail)
        self.assertNotIn("- name:", tail)
        self.assertIn("environment: push_to_image_store", s)
        self.assertIn("persist-credentials: false", s)
        self.assertIn("subject-path: out/image.iso", s)
        self.assertEqual(re.findall(r"^      ([\w-]+): (read|write)$", s, re.M),
                         [("contents", "read"), ("id-token", "write"), ("attestations", "write")])
