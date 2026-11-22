<p align="center"><img src="assets/printshift-icon.png" width="112" alt="PrintShift icon"></p>

# PrintShift

**Bring Print Screen back to a compact keyboard.**

PrintShift is a small native Windows utility that turns a key, a chord, or a sequence of chords into Print Screen. Record a shortcut, choose its output, and keep the app in the notification area. No scripting runtime, driver, or administrator access is required for ordinary desktop use.

**Windows 10 1703+ / Windows 11 · x64 · C++17 / Win32 · MIT**

## Features

- **Precise recording:** preserve left/right Ctrl, Alt, Shift, and Windows keys; distinguish main/keypad Enter and keypad/navigation keys.
- **Optional physical key positions:** enable scan-code bindings plus the extended-key flag for OEM/international keys that share a virtual-key code. A live inspector displays the key name, VK code, and scan code received from Windows.
- **Flexible gestures:** a single key, simultaneous non-modifier keys, repeated keys, or a multi-step sequence with shared modifiers held.
- **Five native outputs:** Print Screen, Alt+Print Screen, Win+Print Screen, Shift+Print Screen, and Ctrl+Print Screen, sent through Windows `SendInput`.
- **Quick control:** global enable switch, per-shortcut checkboxes, tray settings/enable/exit menu, and optional sign-in startup.
- **Windows capture preference:** a strictly On/Off **Use the Print screen key to open screen capture** checkbox initialized from Windows, with state refresh and a Windows keyboard settings shortcut.
- **Desktop integration:** DPI-aware native controls, single-instance handling, Explorer restart recovery, and lock/resume state reset.
- **Private by design:** no network code, telemetry, typing log, or cloud account.

## Download and run

1. Open this repository's **Releases** page and download `PrintShift-v1.0.0-windows-x64.zip` (or the current version).
2. Extract it to a permanent folder and run `PrintShift.exe`.
3. Press **Ctrl+Shift+P**, then release the keys. The initial binding is `Ctrl+Shift+P` using the Windows virtual key P; either side of Ctrl/Shift is accepted.
4. Open **Add shortcut** or **Edit** to record your own gesture and select an output.

The executable is standalone. No .NET, Python, AutoHotkey, or installation is needed to run it. Releases are currently **unsigned**. Verify the downloaded archive against the release's `SHA256SUMS.txt` if required.

```powershell
Get-FileHash .\PrintShift-v1.0.0-windows-x64.zip -Algorithm SHA256
```

Closing or minimizing the settings window keeps PrintShift in the tray. Use **Exit PrintShift** from the tray menu to stop it completely. Windows may place the icon in the hidden notification-icons area.

## Create a shortcut

Choose **Add shortcut → Record keys**, press the gesture, then choose **Stop recording → Save shortcut**. Release a key to finish each chord. You may keep shared modifiers held: hold Right Alt, press/release K, then press/release P to record two steps. Esc stops recording; type `Esc` manually to bind it.

Two recording options control precision:

| Option | Enabled | Disabled |
| --- | --- | --- |
| **Distinguish left/right modifiers** | One physical side is selected | Ctrl/Alt/Shift/Win accept either side |
| **Bind physical key positions** | Store scan-code positions, independent of typed characters | Store named Windows virtual keys |

**Distinguish left/right modifiers** is enabled by default. **Bind physical key positions** is unchecked by default, so new recordings use named Windows keys unless you opt into physical positions. Existing saved recording preferences are restored, and existing shortcuts are never silently rewritten. The inspector shows what Windows actually reports, which is particularly useful for OEM keys and AltGr layouts.

You can also type expressions:

| Expression | Meaning |
| --- | --- |
| `F10` | A single key |
| `A+B` | A and B held together |
| `RAlt+P` | Right Alt and the Windows virtual key P |
| `LCtrl+RCtrl+P` | Both Ctrl keys and P |
| `Ctrl+K, Ctrl+P` | Two chords; Ctrl may remain held |
| `P, P` | Two separate presses |
| `MainEnter` / `NumEnter` | One specific Enter key |
| `Enter` | Either Enter key |
| `Num7` / `Home` | Physical keypad 7 or dedicated Home |
| `SC01C` / `SCE01C` | Main/keypad Enter by scan code |
| `SCE038+SC019` | Right Alt and the physical P position |
| `VKAF` | A Windows virtual-key code, in hexadecimal |

`+` separates simultaneous keys; `,` separates steps. Use `Plus` and `Comma` for the literal separator keys. Named letters follow Windows virtual-key mappings, not the text produced by the current input language. Physical bindings distinguish positions **only when Windows reports distinct scan/extended identities**.

See [Keyboard reference](docs/KEYBOARD.md) for key coverage, timing, replay behavior, and known limitations.

### Choose the output

The **Send to Windows** dropdown supports all five combinations:

| Output | Keys sent |
| --- | --- |
| Print Screen | Print Screen alone |
| Alt + Print Screen | Alt held while Print Screen is pressed and released |
| Win + Print Screen | Windows held while Print Screen is pressed and released |
| Shift + Print Screen | Shift held while Print Screen is pressed and released |
| Ctrl + Print Screen | Ctrl held while Print Screen is pressed and released |

Output modifiers are released afterward. Windows and the receiving screenshot application decide each combination's behavior; Shift/Ctrl combinations do not imply a universal capture action. The original three output IDs are preserved, so saved mappings keep their selected output.

## Settings and startup

- **Enable keyboard shortcuts** controls every mapping. A row's checkbox controls that mapping alone.
- **Run when I sign in to Windows** adds a per-user startup entry; startup launches directly into the tray.
- **Open in the notification area** also hides the window on manual launch.
- **Time between steps** defaults to 1200 ms and applies while building each chord and between chords.
- **Use the Print screen key to open screen capture** changes the current user's Windows preference. It affects Print Screen outside PrintShift as well.
- Settings are saved automatically to `%LOCALAPPDATA%\PrintShift\settings.ini`.

Keep the executable in a stable location after enabling startup. If it moves, toggle startup off/on from its new location. To uninstall, disable startup, exit through the tray menu, and delete the extracted folder. Settings may be deleted separately.

### Windows Print Screen capture setting

The checkbox uses the Windows title exactly: **Use the Print screen key to open screen capture**. It has only two states: **On** (checked) and **Off** (unchecked). PrintShift reads the Windows preference before showing the control. If no explicit preference is stored, it resolves the documented default for the detected Windows version instead of introducing a third checkbox state. The first click switches the current On/Off value to its opposite.

Only clicking the checkbox changes this Windows preference. Starting, disabling, or exiting PrintShift does not change or restore it. The app reads the state again when activated, on Windows settings notifications, and once per second while the window is visible. If an initial read/default cannot be resolved reliably, the control is not shown with an invented value; an error and **Windows keyboard settings** remain available. If a later read fails, its last known On/Off state stays visible but disabled. There is no intermediate checkbox state.

After a successful write, PrintShift verifies the saved value and sends a Windows settings-change notification in the background. Some Windows versions cache the preference; sign out and back in, or restart Windows, if the behavior does not update. The saved checkbox state is not proof that Windows has applied it immediately. See [Windows capture setting details](docs/WINDOWS_CAPTURE.md).

The millisecond selector is a gesture/sequence timeout, not an added screenshot delay. Completed shortcuts send their output as soon as all gesture keys are released.

## Tips and tricks

### Use Print Screen with your preferred screenshot application

If PrintShift's shortcut opens Windows' built-in capture interface instead of your preferred screenshot application, check for a competing Print Screen assignment:

1. In PrintShift, turn **Use the Print screen key to open screen capture** off. Alternatively, use **Windows keyboard settings** or search Windows Settings for **Print screen**.
2. If Windows has not applied the saved preference, sign out and back in. Some Windows versions label the option **Use the Print Screen button to open screen snipping**.
3. Exit and restart your screenshot application, then reapply its **Print Screen** hotkey in that application's shortcut settings. The location and wording vary by application.
4. If your screenshot application expects Print Screen alone, select plain **Print Screen** as the output rather than one of the modifier combinations.

PrintShift sends a keyboard event; it does not select or launch a particular screenshot application. Windows and screenshot utilities decide which program handles that event. See [Microsoft's Print Screen shortcut documentation](https://support.microsoft.com/en-us/accessibility/windows/keyboard-shortcuts-in-windows).

If your screenshot application supports custom shortcuts and still does not respond, configure your available key combination **directly in that application's hotkey settings** and disable the matching PrintShift mapping. For example, assign `Ctrl+Shift+P` to your preferred screenshot application and turn off the PrintShift row that consumes the same physical gesture. This avoids the synthetic Print Screen step and prevents the two apps from competing for that gesture.

Software-generated input can be treated differently from a physical key. A failure to activate a screenshot application does not, by itself, prove that it filters injected events. Compatibility depends on the application, its shortcut registration, and Windows settings.

## How closely does it match a real Print Screen key?

PrintShift sends Print Screen key-down/key-up input to Windows; it does not implement its own screenshot capture. Windows, Snipping Tool, and installed screenshot utilities decide the result according to their current settings.

**It is not a hardware keyboard emulator.** Output is sent after all gesture keys are released. Windows marks injected events, applications can reject them, and privilege boundaries still apply. Fn/firmware-only controls, secure attention sequences, power buttons, and consumer HID controls that do not produce keyboard events cannot be universally intercepted by this app. Identical keys on separate keyboards are not distinguished. See [exact coverage and limits](docs/KEYBOARD.md#coverage-and-limits).

## Build from source

Install Visual Studio 2022 or newer with **Desktop development with C++**, a Windows SDK, and **CMake 3.25+**. Python 3.10+ is optional for native compilation and required for packaging/project-policy tests.

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The executable is `build\Release\PrintShift.exe`. `build.cmd` performs the same steps. The application uses the static C++ runtime. Version strings, executable resources, and the manifest are generated from the root `VERSION` file.

Create the same assets used by CI:

```powershell
python scripts/package.py --binary build/Release/PrintShift.exe --output dist
python scripts/package.py --verify-only --output dist
```

Portable engine tests also run on Linux/macOS:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DPRINTSHIFT_SANITIZERS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## CI and automated releases

The included GitHub Actions workflows need no personal access token or third-party release service.

| Trigger | Automation |
| --- | --- |
| Branch push / pull request | Project policy and package tests; GCC/Clang sanitizer tests; Windows x64 MSVC Debug/Release builds and tests; native resource smoke and isolated registry tests; downloadable release-ready artifact |
| Tag `v1.0.0` | Require `VERSION=1.0.0`; run the full CI suite on the tagged commit; verify package checksums; create a draft release; upload assets; publish only after the uploads succeed |
| Tag `v1.1.0-rc.1` | Same checks, with a matching prerelease version; publish as a prerelease |
| Failed release rerun | Resume an existing draft; leave an already published release unchanged if its commit/version and expected assets match |

For the initial release, commit this project into a GitHub repository with Actions enabled, then:

```sh
git tag -a v1.0.0 -m "PrintShift 1.0.0"
git push origin v1.0.0
```

The release contains:

- `PrintShift-v1.0.0-windows-x64.zip` — executable and user documentation.
- `PrintShift-v1.0.0-source.zip` — source, including hidden `.github` workflow files.
- `SHA256SUMS.txt` — archive and metadata checksums.
- `BUILD_INFO.json` — version, source commit, build toolchain label, and binary digest.

Repository/organization policy must allow Actions and the release job's `contents: write` permission. Future releases require updating `VERSION` and adding a matching changelog entry before tagging. See [release operations](docs/RELEASING.md) for setup, retries, and repository rules.

## Testing status

Automated tests cover key predicates and distinct physical events, left/right modifiers, keypad normalization, scan-code parsing/round trips, record/replay state, shared-prefix sequences, timeouts, Windows-default resolution, all five output plans with balanced modifier releases, packaging integrity, and version/tag guards. Windows CI also checks native resource loading, creation of a hidden window, and capture-preference reads/writes under an isolated temporary registry key, including explicit-value precedence and initial On/Off defaults. It does **not** prove physical-key interception, immediate application of Windows' real preference, screenshot behavior, or tray/startup UX on every desktop.

The development handoff includes a locally cross-compiled x64 binary. GitHub-hosted MSVC runs and the [manual desktop acceptance checklist](docs/TESTING.md) must be completed in the target repository/environment. A successful unit test is not a claim of universal keyboard compatibility.

## Contributing and security

Read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting changes. For vulnerabilities, follow [SECURITY.md](SECURITY.md); do not post sensitive typed input in reports.

## License

[MIT](LICENSE). The included application icon is distributed with the project under the same license.
