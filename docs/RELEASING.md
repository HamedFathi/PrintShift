# Release operations

## Initial repository setup

Commit the extracted source directory as the repository root. Include its hidden `.github` directory; do not upload only visible files through a file picker. Exclude build outputs using the supplied `.gitignore`.

Enable GitHub Actions. The workflow uses official actions pinned to immutable commits and the repository's automatic `GITHUB_TOKEN`; no PAT or custom secret is needed. Organization rules must permit these actions and `contents: write` for the publish job. Branch protection and tag rules are recommended: restrict release-tag creation to maintainers and prevent retagging published versions.

The release runs directly from a pushed tag. No publishing environment with a mandatory approval is configured. Add one only if your repository requires that gate. No code-signing identity is bundled: release binaries are unsigned.

## Publish version 1.0.0

The project already contains `VERSION` = `1.0.0` and a matching changelog section. After committing and pushing the source:

```sh
git tag -a v1.0.0 -m "PrintShift 1.0.0"
git push origin v1.0.0
```

`release.yml` validates the tag, calls the full CI workflow, downloads the Windows Release build's packages, verifies checksums, then publishes a GitHub release. Assets are uploaded while the release is a draft; failure leaves it as a draft. Publishing happens only after the expected assets are present.

The workflow does not trust an old branch-build artifact: it builds the tagged commit in the same run. Normal branch and PR runs only upload a temporary Actions artifact and never publish a release.

## Future versions

1. Change the single root `VERSION` value, for example `1.1.0`.
2. Add `## [1.1.0] - YYYY-MM-DD` to `CHANGELOG.md` with the release notes.
3. Commit and push those changes.
4. Tag that commit `v1.1.0` and push the tag.

CMake generates the C++ version header, executable version resource, and manifest from `VERSION`. The tag must match exactly. Prerelease versions such as `1.1.0-rc.1` produce prerelease GitHub releases and are not marked latest. Version build metadata (`+...`) is intentionally not supported by this pipeline.

## Release contents

- `PrintShift-vVERSION-windows-x64.zip`: standalone executable, license, version, changelog, and user documentation.
- `PrintShift-vVERSION-source.zip`: clean source including workflows and build tooling.
- `BUILD_INFO.json`: version, source commit, toolchain label, binary digest, runner metadata, and signing status.
- `SHA256SUMS.txt`: checksums for both ZIPs and build metadata. The portable ZIP also contains payload checksums.

Checksums are integrity checks, not signatures. GitHub CI does not add an Authenticode signature. If code signing is introduced later, sign the executable before packaging and update the metadata/signature checks accordingly.

## Failures and retries

Use GitHub's **Re-run failed jobs** after correcting transient runner/network/policy problems. An existing draft is resumed and its matching asset names can be replaced. An already published release is left unchanged only when the expected assets exist and its recorded source commit/version matches the rerun. A different commit/version fails instead of overwriting a published release.

Do not move a published tag to a different commit. Publish a new patch version. If a workflow or source fix is needed before the first successful publication, use a new version/tag rather than treating a failed immutable tag as editable history.

Typical failures are a tag/VERSION mismatch, a missing changelog entry, failing tests, disabled Actions, restrictive organization permissions, or a missing release artifact. All stop publication.

## Local packaging

```powershell
python scripts/check_project.py --tag v1.0.0
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
python scripts/package.py --binary build/Release/PrintShift.exe --output dist
python scripts/package.py --verify-only --output dist
```

`publish_release.py` is a workflow helper and requires GitHub CLI, the workflow token/repository environment, and matching build provenance. Do not run it as a substitute for testing.
