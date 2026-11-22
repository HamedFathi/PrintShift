import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import publish_release

COMMIT = "a" * 40


class PublishingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.assets = Path(self.temp.name)
        self.version = "1.0.0"
        self.names = {"PrintShift-v1.0.0-windows-x64.zip", "PrintShift-v1.0.0-source.zip", "BUILD_INFO.json", "SHA256SUMS.txt"}
        for name in self.names:
            (self.assets / name).write_text("placeholder", encoding="utf-8")
        (self.assets / "RELEASE_NOTES.md").write_text("Release notes", encoding="utf-8")
        self.metadata()
        self.env = patch.dict(os.environ, {"GITHUB_SHA": COMMIT})
        self.env.start()
        self.version_patch = patch.object(publish_release, "read_version", side_effect=lambda: self.version)
        self.version_patch.start()
        # Package integrity is exercised separately against real ZIP/PE fixtures.
        self.verify_patch = patch.object(publish_release, "verify_packages")
        self.verify_patch.start()
        self.print_patch = patch.object(publish_release, "print", create=True)
        self.print_patch.start()

    def tearDown(self):
        self.print_patch.stop()
        self.verify_patch.stop()
        self.version_patch.stop()
        self.env.stop()
        self.temp.cleanup()

    def metadata(self, commit=COMMIT):
        (self.assets / "BUILD_INFO.json").write_text(json.dumps({"version": self.version, "source_commit": commit}))

    def result(self, code=0, value=None):
        return subprocess.CompletedProcess([], code, json.dumps(value or {}), "")

    def test_upload_failure_never_publishes(self):
        with patch.object(publish_release, "gh", side_effect=[self.result(1), self.result(), subprocess.CalledProcessError(1, "gh upload")]) as gh:
            with self.assertRaises(subprocess.CalledProcessError):
                publish_release.publish("v1.0.0", self.assets)
            self.assertFalse(any(call.args[:2] == ("release", "edit") for call in gh.call_args_list))

    def test_new_release_is_draft_until_uploads_verified(self):
        state = {"assets": [{"name": name} for name in self.names]}
        with patch.object(publish_release, "gh", side_effect=[self.result(1), self.result(), self.result(), self.result(value=state), self.result()]) as gh:
            publish_release.publish("v1.0.0", self.assets)
            self.assertIn("--draft", gh.call_args_list[1].args)
            self.assertEqual(gh.call_args_list[2].args[:2], ("release", "upload"))
            self.assertEqual(gh.call_args_list[-1].args[:3], ("release", "edit", "v1.0.0"))
            self.assertIn("--draft=false", gh.call_args_list[-1].args)

    def test_wrong_source_commit_is_rejected_before_gh(self):
        self.metadata("b" * 40)
        with patch.object(publish_release, "gh") as gh:
            with self.assertRaises(ValueError):
                publish_release.publish("v1.0.0", self.assets)
            gh.assert_not_called()

    def test_existing_draft_is_resumed_without_creation(self):
        state = {"assets": [{"name": name} for name in self.names]}
        with patch.object(publish_release, "gh", side_effect=[self.result(value={"isDraft": True}), self.result(), self.result(value=state), self.result()]) as gh:
            publish_release.publish("v1.0.0", self.assets)
            self.assertFalse(any(call.args[:2] == ("release", "create") for call in gh.call_args_list))

    def test_published_release_is_never_overwritten(self):
        def fake_gh(*args, **kwargs):
            if args[:2] == ("release", "view"):
                return self.result(value={"isDraft": False, "assets": [{"name": name} for name in self.names]})
            if args[:2] == ("release", "download"):
                directory = Path(args[args.index("--dir") + 1])
                (directory / "BUILD_INFO.json").write_text(json.dumps({"version": self.version, "source_commit": COMMIT}))
                return self.result()
            self.fail("Published release must not be mutated")
        with patch.object(publish_release, "gh", side_effect=fake_gh):
            publish_release.publish("v1.0.0", self.assets)

    def test_missing_uploaded_asset_keeps_release_draft(self):
        with patch.object(publish_release, "gh", side_effect=[self.result(1), self.result(), self.result(), self.result(value={"assets": []})]) as gh:
            with self.assertRaises(ValueError):
                publish_release.publish("v1.0.0", self.assets)
            self.assertFalse(any(call.args[:2] == ("release", "edit") for call in gh.call_args_list))

    def test_prerelease_is_not_marked_latest(self):
        self.version = "1.0.0-rc.1"
        self.metadata()
        state = {"assets": [{"name": f"PrintShift-v{self.version}-{kind}.zip"} for kind in ["windows-x64", "source"]] + [{"name": "BUILD_INFO.json"}, {"name": "SHA256SUMS.txt"}]}
        with patch.object(publish_release, "gh", side_effect=[self.result(1), self.result(), self.result(), self.result(value=state), self.result()]) as gh:
            publish_release.publish(f"v{self.version}", self.assets)
            self.assertIn("--prerelease", gh.call_args_list[1].args)
            self.assertIn("--latest=false", gh.call_args_list[-1].args)


if __name__ == "__main__":
    unittest.main()
