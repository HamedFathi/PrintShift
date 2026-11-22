import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from package import build_packages, source_files, validate_pe, verify_packages
from version import validate_tag, validate_version


def fake_pe() -> bytes:
    data = bytearray(1024)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 128)
    data[128:132] = b"PE\0\0"
    struct.pack_into("<HH", data, 132, 0x8664, 1)
    struct.pack_into("<H", data, 148, 240)
    struct.pack_into("<H", data, 152, 0x20B)
    struct.pack_into("<H", data, 220, 2)
    data[392:400] = b".rsrc\0\0\0"
    return bytes(data)


class ReleaseToolingTests(unittest.TestCase):
    def test_tag_must_match_version(self):
        validate_tag("v1.0.0", "1.0.0")
        for tag in ["1.0.0", "v1.0.1", "v01.0.0", "v1.0.0;echo unsafe"]:
            with self.assertRaises(ValueError):
                validate_tag(tag, "1.0.0")

    def test_semver_and_prereleases(self):
        for version in ["1.0.0", "1.2.3-rc.1", "0.1.0-beta"]:
            self.assertEqual(validate_version(version), version)
        for version in ["01.0.0", "1.0", "1.0.0-01", "1.0.0+local", "65536.0.0"]:
            with self.assertRaises(ValueError):
                validate_version(version)

    def test_wrong_architecture_is_rejected(self):
        binary = bytearray(fake_pe())
        struct.pack_into("<H", binary, 132, 0x14C)
        with self.assertRaises(ValueError):
            validate_pe(binary)
        with self.assertRaises(ValueError):
            validate_pe(b"not an executable")

    def test_complete_packages_and_tamper_detection(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "project"
            root.mkdir()
            for name, content in {"VERSION": "1.0.0\n", "README.md": "# Example\n", "LICENSE": "MIT\n",
                                  "CHANGELOG.md": "# Changelog\n\n## [1.0.0]\n\n- Initial release.\n"}.items():
                (root / name).write_text(content, encoding="utf-8")
            (root / ".github/workflows").mkdir(parents=True)
            (root / ".github/workflows/ci.yml").write_text("name: CI\n")
            (root / "build").mkdir()
            (root / "build/private.txt").write_text("must not ship")
            (root / "scripts/__pycache__").mkdir(parents=True)
            (root / "scripts/__pycache__/cached.pyc").write_bytes(b"cache")
            (root / "docs").mkdir()
            (root / "docs/KEYBOARD.md").write_text("# Keyboard\n")
            binary = Path(temp) / "PrintShift.exe"
            binary.write_bytes(fake_pe())
            output = Path(temp) / "release"
            assets = build_packages(binary, output, root)
            self.assertEqual(len(assets), 4)
            verify_packages(output)
            with zipfile.ZipFile(output / "PrintShift-v1.0.0-source.zip") as archive:
                self.assertIn("PrintShift-v1.0.0/.github/workflows/ci.yml", archive.namelist())
                self.assertFalse(any("/build/" in p or "__pycache__" in p for p in archive.namelist()))
            with zipfile.ZipFile(output / "PrintShift-v1.0.0-windows-x64.zip") as archive:
                self.assertEqual(archive.read("PrintShift.exe"), fake_pe())
            binary_asset = output / "PrintShift-v1.0.0-windows-x64.zip"
            binary_asset.write_bytes(binary_asset.read_bytes() + b"tampered")
            with self.assertRaises(ValueError):
                verify_packages(output)

    def test_missing_changelog_version_fails_before_publication(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "VERSION").write_text("1.0.0")
            (root / "CHANGELOG.md").write_text("# Changelog\n")
            binary = root / "binary.exe"
            binary.write_bytes(fake_pe())
            with self.assertRaises(ValueError):
                build_packages(binary, root / "output", root)


if __name__ == "__main__":
    unittest.main()
