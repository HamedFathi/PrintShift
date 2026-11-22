"""Publish already-tested assets with GitHub CLI; called only by release.yml."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
from package import verify_packages
from version import read_version, validate_tag


def gh(*args: str, check: bool = True) -> subprocess.CompletedProcess:
    return subprocess.run(["gh", *args], check=check, text=True, capture_output=True)


def publish(tag: str, assets: Path) -> None:
    version = read_version()
    validate_tag(tag, version)
    verify_packages(assets)
    metadata = json.loads((assets / "BUILD_INFO.json").read_text(encoding="utf-8"))
    commit = os.environ.get("GITHUB_SHA")
    if not commit or metadata.get("source_commit") != commit or metadata.get("version") != version:
        raise ValueError("Build metadata does not match the release workflow commit/version")
    files = [assets / f"PrintShift-v{version}-windows-x64.zip", assets / f"PrintShift-v{version}-source.zip",
             assets / "BUILD_INFO.json", assets / "SHA256SUMS.txt"]
    required = {p.name for p in files}
    existing = gh("release", "view", tag, "--json", "isDraft,assets", check=False)
    if existing.returncode == 0:
        state = json.loads(existing.stdout)
        if not state["isDraft"]:
            if not required.issubset({a["name"] for a in state["assets"]}):
                raise ValueError("Published release is incomplete; published assets will not be overwritten")
            with tempfile.TemporaryDirectory() as directory:
                gh("release", "download", tag, "--pattern", "BUILD_INFO.json", "--dir", directory)
                previous = json.loads((Path(directory) / "BUILD_INFO.json").read_text(encoding="utf-8"))
            if previous.get("source_commit") != commit or previous.get("version") != version:
                raise ValueError("A published release already exists for a different commit/version")
            print(f"{tag} is already published; existing assets left unchanged")
            return
    else:
        args = ["release", "create", tag, "--verify-tag", "--draft", "--title", f"PrintShift {version}",
                "--generate-notes", "--notes-file", str(assets / "RELEASE_NOTES.md")]
        if "-" in version:
            args.append("--prerelease")
        gh(*args)
    gh("release", "upload", tag, *(str(p) for p in files), "--clobber")
    state = json.loads(gh("release", "view", tag, "--json", "assets").stdout)
    if not required.issubset({a["name"] for a in state["assets"]}):
        raise ValueError("Asset verification failed; release remains a draft")
    gh("release", "edit", tag, "--draft=false", f"--latest={'false' if '-' in version else 'true'}")
    print(f"Published {tag} with {len(files)} verified assets")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()
    try:
        publish(args.tag, args.assets)
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.stderr or error.stdout or str(error)) from error
