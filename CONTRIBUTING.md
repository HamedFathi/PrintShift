# Contributing

Keep source, UI text, comments, documentation, filenames, and issue content in English. Use C++17 and native Win32 APIs for the app. Build/packaging helpers use Python's standard library only.

Before opening a pull request:

1. Build and run CTest. Add regression coverage for changes to key identity, sequence matching, buffering, or release tooling.
2. Run `python scripts/check_project.py` and the Python tests.
3. Describe observable behavior, test evidence, and remaining Windows/hardware limitations.
4. Update user documentation for changed behavior. Add a changelog entry when preparing a versioned release.

Do not add typing logs, telemetry, automatic elevation, keyboard drivers, or silent registry remapping. Startup is an explicit per-user option. Do not claim support for hardware events Windows never exposes.

The Windows Print Screen capture preference may be written only in response to an explicit user action. Test registry access through the helper's injectable temporary root/path; automated tests must not change the actual Windows preference.

Use `VERSION` as the only release-version source. Do not hand-edit generated headers/resources or commit build outputs. GitHub Actions and release packaging are documented in `docs/RELEASING.md`.
