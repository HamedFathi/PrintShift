# Security policy

Version 1 is the initial supported release line. Security fixes will be issued in a new patch release; published release assets should not be silently replaced.

Report vulnerabilities through this repository's private GitHub vulnerability reporting / Security Advisories when enabled. If private reporting is unavailable, contact a repository maintainer through an available private channel before disclosing exploit details. Do not attach sensitive typed input or credentials to public issues.

PrintShift uses a global keyboard hook and short-lived in-memory buffering to recognize explicitly configured gestures. It does not write typing logs, send telemetry, use the network, or install a driver. It writes its own settings and an opt-in per-user startup entry. The Windows Print Screen capture preference is changed only when the user explicitly clicks its separate checkbox; it is never changed on launch or exit.

The application normally runs without elevation. Windows integrity boundaries and secure desktops are respected. Output events remain marked as injected; this project does not bypass application or operating-system input protections.

CI actions are pinned, builds use read-only repository access, and only the release publication job receives `contents: write`. Binaries are unsigned; SHA-256 manifests detect accidental modification but are not an Authenticode signature or independent proof of publisher identity.
