"""Single-source semantic version and tag validation (standard library only)."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
SEMVER = re.compile(r"(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?\Z")


def validate_version(value: str) -> str:
    match = SEMVER.fullmatch(value)
    if not match:
        raise ValueError(f"Invalid semantic version: {value!r}")
    if any(int(part) > 65535 for part in match.groups()[:3]):
        raise ValueError("Version components exceed Windows resource limits")
    for identifier in (match.group(4) or "").split("."):
        if identifier.isdigit() and len(identifier) > 1 and identifier.startswith("0"):
            raise ValueError("Numeric prerelease identifiers must not have leading zeros")
    return value


def read_version(root: Path = ROOT) -> str:
    return validate_version((root / "VERSION").read_text(encoding="utf-8").strip())


def validate_tag(tag: str, version: str) -> None:
    validate_version(version)
    if tag != f"v{version}":
        raise ValueError(f"Tag {tag!r} must match VERSION exactly: v{version}")
