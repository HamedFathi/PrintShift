"""Build verified portable/source ZIP assets; requires Python 3.10+ only."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import struct
import subprocess
import zipfile
from datetime import datetime, timezone
from version import ROOT, read_version

ROOT_FILES = {"README.md", "LICENSE", "VERSION", "CHANGELOG.md", "CONTRIBUTING.md", "SECURITY.md",
              "CMakeLists.txt", "CMakePresets.json", "build.cmd", "app.ico", ".gitignore", ".gitattributes", ".editorconfig"}
SOURCE_DIRS = {"src", "tests", "scripts", "cmake", "docs", "assets", ".github"}


def source_files(root: Path) -> list[Path]:
    result = []
    for path in root.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        rel = path.relative_to(root)
        if "__pycache__" in rel.parts or path.suffix in {".pyc", ".exe", ".zip", ".obj", ".log"}:
            continue
        if (len(rel.parts) == 1 and path.name in ROOT_FILES) or rel.parts[0] in SOURCE_DIRS:
            result.append(path)
    return sorted(result, key=lambda p: p.relative_to(root).as_posix())


def validate_pe(data: bytes) -> None:
    if len(data) < 256 or data[:2] != b"MZ":
        raise ValueError("Binary is not a Windows PE executable")
    offset = struct.unpack_from("<I", data, 0x3C)[0]
    if offset + 96 > len(data) or data[offset:offset + 4] != b"PE\0\0":
        raise ValueError("Invalid PE header")
    machine, count = struct.unpack_from("<HH", data, offset + 4)
    optional_size = struct.unpack_from("<H", data, offset + 20)[0]
    if machine != 0x8664 or struct.unpack_from("<H", data, offset + 24)[0] != 0x20B:
        raise ValueError("Release binary must be Windows x64 (PE32+)")
    if struct.unpack_from("<H", data, offset + 24 + 68)[0] != 2:
        raise ValueError("Release binary must use the Windows GUI subsystem")
    sections = offset + 24 + optional_size
    if sections + count * 40 > len(data):
        raise ValueError("Truncated section table")
    if not any(data[sections + i * 40:sections + i * 40 + 8].rstrip(b"\0") == b".rsrc" for i in range(count)):
        raise ValueError("Application icon/version/manifest resources are missing")


def release_notes(root: Path, version: str) -> str:
    text = (root / "CHANGELOG.md").read_text(encoding="utf-8")
    match = re.search(rf"^## \[{re.escape(version)}\][^\n]*\n(.*?)(?=^## |\Z)", text, re.M | re.S)
    if not match:
        raise ValueError(f"CHANGELOG.md has no entry for {version}")
    return (f"# PrintShift {version}\n\n" + match.group(1).strip() +
            "\n\nDownload the windows-x64 ZIP to run the app; download the source ZIP to build it.\n"
            "SHA256SUMS.txt covers all release ZIPs and BUILD_INFO.json. Binaries are unsigned.\n")


def write_zip(path: Path, entries: dict[str, bytes]) -> None:
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(entries.items()):
            info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, data)
    with zipfile.ZipFile(path) as archive:
        if archive.testzip() is not None:
            raise ValueError("ZIP integrity verification failed")


def build_packages(binary: Path, output: Path, root: Path = ROOT) -> list[Path]:
    version = read_version(root)
    data = binary.read_bytes()
    validate_pe(data)
    notes = release_notes(root, version)
    output.mkdir(parents=True, exist_ok=True)
    prefix = f"PrintShift-v{version}"
    portable = output / f"{prefix}-windows-x64.zip"
    source = output / f"{prefix}-source.zip"
    entries = {"PrintShift.exe": data}
    for name in ["README.md", "LICENSE", "CHANGELOG.md", "VERSION"]:
        entries[name] = (root / name).read_bytes()
    for name in ["CONTRIBUTING.md", "SECURITY.md", "assets/printshift-icon.png", "assets/ICON.md"]:
        if (root / name).is_file():
            entries[name] = (root / name).read_bytes()
    for path in (root / "docs").rglob("*.md"):
        entries[path.relative_to(root).as_posix()] = path.read_bytes()
    entries["SHA256SUMS.txt"] = "".join(f"{hashlib.sha256(v).hexdigest()}  {k}\n" for k, v in sorted(entries.items())).encode()
    write_zip(portable, entries)
    write_zip(source, {f"{prefix}/{p.relative_to(root).as_posix()}": p.read_bytes() for p in source_files(root)})
    try:
        commit = subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], stderr=subprocess.DEVNULL, text=True).strip()
    except (subprocess.SubprocessError, FileNotFoundError):
        commit = None
    metadata = {"version": version, "architecture": "x64", "source_commit": commit,
                "build_host": platform.platform(), "toolchain": os.environ.get("PRINTSHIFT_TOOLCHAIN", "unspecified"),
                "github_repository": os.environ.get("GITHUB_REPOSITORY"), "github_run_id": os.environ.get("GITHUB_RUN_ID"),
                "created_utc": datetime.now(timezone.utc).isoformat(), "binary_sha256": hashlib.sha256(data).hexdigest(),
                "authenticode_signed": False}
    info = output / "BUILD_INFO.json"
    info.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    assets = [portable, source, info]
    sums = output / "SHA256SUMS.txt"
    sums.write_text("".join(f"{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n" for p in assets), encoding="utf-8")
    (output / "RELEASE_NOTES.md").write_text(notes, encoding="utf-8")
    verify_packages(output)
    return assets + [sums]


def verify_packages(output: Path) -> None:
    sums = (output / "SHA256SUMS.txt").read_text(encoding="utf-8").splitlines()
    if len(sums) != 3:
        raise ValueError("Expected exactly two ZIPs and build metadata in release checksums")
    for line in sums:
        digest, name = line.split("  ", 1)
        if Path(name).name != name or "/" in name or "\\" in name or not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise ValueError("Unsafe checksum entry")
        path = output / name
        if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise ValueError(f"Checksum mismatch: {name}")
        if path.suffix == ".zip":
            with zipfile.ZipFile(path) as archive:
                if archive.testzip() is not None:
                    raise ValueError(f"Corrupt archive: {name}")
                if name.endswith("-windows-x64.zip"):
                    data = archive.read("PrintShift.exe")
                    validate_pe(data)
                    inner = archive.read("SHA256SUMS.txt").decode().splitlines()
                    for row in inner:
                        sha, member = row.split("  ", 1)
                        if hashlib.sha256(archive.read(member)).hexdigest() != sha:
                            raise ValueError(f"Portable payload mismatch: {member}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--output", type=Path, default=ROOT / "dist")
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    if args.verify_only:
        verify_packages(args.output)
    elif args.binary:
        for asset in build_packages(args.binary, args.output):
            print(asset)
    else:
        parser.error("--binary is required unless --verify-only is used")
