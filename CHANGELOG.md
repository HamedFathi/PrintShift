# Changelog

Versions follow Semantic Versioning. Release tags use a `v` prefix; `VERSION` does not.

## [1.0.0] - 2026-11-22

### Added

- Native Windows Print Screen remapping with single keys, chords, and multi-step sequences.
- Five output combinations: plain Print Screen, Alt, Win, Shift, and Ctrl, with the original saved output IDs preserved.
- Physical scan/extended-code bindings and a key inspector for OEM/international keys.
- Distinct left/right modifiers, main/keypad Enter, and keypad/navigation identities.
- Shared-modifier recording, independent mappings, tray controls, and sign-in startup.
- Named-key recording by default, with physical-position recording available as an opt-in and explicit saved preferences preserved.
- Dedicated multi-resolution application icon and English-only project documentation.
- Two-state On/Off control of the current user's Windows Print Screen capture preference, initialized from the explicit preference or known Windows default, with read-back verification and external-change refresh.
- CMake builds and single-source versioned resources.
- Windows MSVC Debug/Release CI and Linux GCC/Clang sanitizer tests.
- Isolated Windows registry integration tests for the capture setting.
- Verified source/portable packages, SHA-256 manifests, and tag-triggered GitHub releases.

### Known limitations

- Hardware/firmware-only and non-keyboard HID controls are outside the keyboard-hook path.
- Injected output differs from hardware input and follows Windows privilege restrictions.
- Target-desktop physical keyboard, screenshot, tray, and startup acceptance remains required.
