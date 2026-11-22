# Testing

## Automated checks

- Linux GCC and Clang: C++17 build, assertions enabled in every configuration, AddressSanitizer and UndefinedBehaviorSanitizer.
- Windows x64: MSVC Debug/Release builds with warnings as errors; engine, packaging, project-policy, and native smoke tests.
- Native smoke test: embedded icon/manifest/version loading, common-control initialization, hidden-window creation, and DPI API availability. It does not inject keys, capture screenshots, change startup, or modify settings.
- Windows capture setting tests: initial On/Off default resolution, explicit preference precedence, absent values without writes, invalid value/type, and unrelated-value preservation using a private temporary HKCU test key. The real Windows capture setting is never touched by this test.
- Release tooling: semantic-version/tag guards, x64 PE validation, source inclusion/exclusion, ZIP/payload checksums, and tamper rejection.
- Output plans: all five dropdown combinations, preserved IDs 0-2, exact key order, modifier release balance, invalid-output rejection, and engine dispatch for IDs 0-4.
- Project policy: English-only text, required project files, current changelog section, and immutable references for official actions.

Run `ctest --test-dir build -C Release --output-on-failure` after building on Windows. Python tests can also run with `python -m unittest discover -s tests -p 'test_*.py' -v`.

## Manual desktop acceptance

Run on a real interactive Windows desktop with the target keyboard/layout:

1. Verify left/right Ctrl, Alt, Shift, and Windows keys in the recorder and inspector.
2. Compare main Enter, keypad Enter, keypad digits/decimal with Num Lock on/off, and dedicated navigation keys.
3. Check ISO/OEM/international and available browser/media keys. Compare their scan/extended codes. Document controls that emit no keyboard event.
4. Record single keys, A+B, repeated keys, and shared-modifier sequences. Test wrong keys, timeouts, auto-repeat, and conflicting mappings.
5. Confirm ordinary input is restored after mismatches. Switch the foreground window during a pending gesture and ensure text is not replayed into the new window.
6. Test all five Print Screen outputs with the user's Windows/Snipping Tool/screenshot utility settings: plain, Alt, Win, Shift, and Ctrl. Confirm modifier keys are released afterward and that IDs 3/4 survive saving/restarting. Existing IDs 0-2 must retain their original output.
7. Toggle global/per-rule enable. Edit a single-key mapping without triggering it in the editor.
8. Close/minimize to tray, open settings, toggle enabled, exit, and restart Explorer.
9. Enable startup, sign out/in, verify hidden launch, then disable startup. Check lock/unlock and sleep/resume.
10. Check mixed DPI monitors, AltGr, elevated foreground apps, remote sessions, accessibility features, and any other remapper used on the system.
11. Inspect **Use the Print screen key to open screen capture** with explicit On/Off preferences and no explicit value. Confirm it loads the current preference or known version default before display, shows only On/Off, and switches to the opposite value on the first click. Verify Windows Settings after any required sign-out and check external-change refresh. Confirm that enabling/disabling mappings, restarting, and exiting do not write or restore that preference. Verify read failures are reported without displaying an invented initial value. Restore the desired preference after testing.
12. With fresh settings or no saved `PhysicalPositions` preference, confirm physical key-position binding is unchecked. Confirm the initial shortcut is named `Ctrl+Shift+P`. Explicitly enable/disable physical recording and check that the saved choice and existing shortcuts are restored without being rewritten.

A CI pass is not a guarantee of keyboard firmware behavior, universal screenshot compatibility, secure-desktop support, or equivalence to a hardware HID key.
