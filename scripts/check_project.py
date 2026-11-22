"""Validate version/tag, English-only project text, and pinned workflow actions."""
import argparse
import os
from pathlib import Path
import re
from package import source_files, release_notes
from version import ROOT, read_version, validate_tag

TEXT_EXTENSIONS = {".md", ".txt", ".cpp", ".hpp", ".py", ".json", ".yml", ".yaml", ".in", ".cmake", ".cmd"}
NON_ENGLISH_SCRIPT = re.compile("[\u0600-\u06ff\u0750-\u077f\u08a0-\u08ff\ufb50-\ufdff\ufe70-\ufeff]")
REQUIRED = ["LICENSE", "VERSION", "README.md", "CHANGELOG.md", "SECURITY.md", "CONTRIBUTING.md",
            ".github/workflows/ci.yml", ".github/workflows/release.yml", "docs/RELEASING.md", "docs/KEYBOARD.md"]


def check_project(root: Path = ROOT, tag: str | None = None) -> str:
    version = read_version(root)
    if tag is not None:
        validate_tag(tag, version)
    release_notes(root, version)
    for name in REQUIRED:
        if not (root / name).is_file():
            raise ValueError(f"Missing required project file: {name}")
    for path in source_files(root):
        rel = path.relative_to(root).as_posix()
        if NON_ENGLISH_SCRIPT.search(rel):
            raise ValueError(f"Non-English script in path: {rel}")
        if path.suffix in TEXT_EXTENSIONS or path.name in {"VERSION", "LICENSE", ".gitignore", ".gitattributes", ".editorconfig"}:
            if NON_ENGLISH_SCRIPT.search(path.read_text(encoding="utf-8")):
                raise ValueError(f"Non-English script in project text: {rel}")
    for workflow in (root / ".github/workflows").glob("*.yml"):
        text = workflow.read_text(encoding="utf-8")
        for ref in re.findall(r"^\s*(?:-\s*)?uses:\s*([^\s#]+)", text, re.M):
            if ref.startswith("./"):
                if not (root / ref).is_file():
                    raise ValueError(f"Missing reusable workflow: {ref}")
            elif not re.fullmatch(r"actions/[\w-]+@[0-9a-f]{40}", ref):
                raise ValueError(f"Workflow action is not pinned to an official commit: {ref}")
    return version


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tag")
    parser.add_argument("--github-output", action="store_true")
    args = parser.parse_args()
    version = check_project(tag=args.tag)
    if args.github_output:
        with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as out:
            out.write(f"version={version}\nprerelease={'true' if '-' in version else 'false'}\n")
    print(f"Project checks passed: {version}")
