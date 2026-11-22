# Windows Print Screen capture preference

## Control

**Use the Print screen key to open screen capture** is a separate Windows preference for the current account. The checkbox title matches Windows' English wording. It is independent of PrintShift's global and per-shortcut enable switches.

| Display | Preference |
| --- | --- |
| Checked | On: explicit `REG_DWORD` 1, or a resolved Windows On default |
| Unchecked | Off: explicit `REG_DWORD` 0, or a resolved Windows Off default |

This is a standard two-state checkbox. PrintShift fetches the Windows preference before displaying it, and the first click switches that value to its opposite. There is no third selectable state. Launching, hiding, disabling, or exiting PrintShift never writes or restores this Windows preference. Changes apply to the current account outside PrintShift, including physical Print Screen presses and other screenshot applications.

## Storage and refresh

The preference is `PrintScreenKeyForSnippingEnabled` under `HKEY_CURRENT_USER\Control Panel\Keyboard`. An explicit On/Off value always wins. Only a checkbox action writes a DWORD; existing unrelated registry values are left intact. Unexpected formats are not overwritten through the UI.

If the value is absent, the app reads the Windows client version and, where needed, the installed update revision. It resolves the known Windows 10/early Windows 11 Off default or the modern Windows 11 On default. Windows 11 build 22621 requires its update revision: KB5027303 (22621.1928) documented the default changing to On. The earlier 22H2 rollout interval and unsupported/unknown versions are not guessed. This reads the documented preference default; it does not inject a test screenshot or interrogate a cached screenshot handler.

If the first state cannot be determined, the checkbox is hidden rather than displaying a false On/Off value, and an error plus the Windows settings shortcut is shown. If refresh fails after a successful read, the last known On/Off state remains visible but disabled. Neither case introduces an intermediate checkbox position.

The UI verifies a successful write by reading it back. It then queues a standard `WM_SETTINGCHANGE` broadcast from a Windows worker thread, so a slow recipient cannot block the keyboard-hook thread. It refreshes on activation/settings notifications and once per second while the main window is visible. This is saved-state feedback, not a guarantee of immediate operating-system behavior.

Registry access can be restricted or the preference can be overridden by management policy. Failed writes are reported and the checkbox is refreshed from storage. No automatic elevation or registry-wide remapping is performed.

## When a change is not immediately effective

Windows versions can cache this preference. Sign out and back in, or restart Windows, if the saved choice does not affect behavior. Restart your preferred screenshot application and reapply its hotkey if needed. **Windows keyboard settings** opens the native keyboard settings page; the exact location of Print Screen options varies by Windows version.

Turning this preference Off does not directly launch or select a third-party screenshot application. That application must be running and configured to handle Print Screen. If it supports a suitable direct hotkey, it can also use that key combination without synthetic Print Screen input.

## Validation

Portable tests cover Windows default-version boundaries and unresolved rollout/version cases. `windows_settings_tests` exercises initial On/Off defaults, explicit preference precedence, absent-key/value reads without writes, explicit On/Off writes, type/value validation, and preservation of unrelated data under its own temporary `HKCU\Software\PrintShift\Tests` subkey. It never changes the real Windows capture preference or broadcasts settings changes.

Actual Windows settings propagation and the checkbox/layout require desktop acceptance on the target Windows build. The local Linux cross-build does not execute this Windows integration test; the Windows CI jobs do.

## References

- [Microsoft support discussion of the preference](https://learn.microsoft.com/en-us/answers/questions/5775212/missing-use-print-screen-key-to-open-snipping)
- [Microsoft KB5027303: default changed in build 22621.1928](https://support.microsoft.com/en-us/servicing/os/windows-11/2023/06/june-27-2023-kb5027303-os-build-22621-1928-preview)
- [Registry value writes](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regsetvalueexw)
- [Settings-change notification](https://learn.microsoft.com/en-us/windows/win32/winmsg/wm-settingchange)
- [Windows Settings URI reference](https://learn.microsoft.com/en-us/windows/apps/develop/launch/launch-settings)
