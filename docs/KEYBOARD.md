# Keyboard reference

## Input identity

Each physical keyboard event retains its normalized Windows virtual key, hardware scan code, and extended-key flag. The event's press/release identity is retained while held, preventing a layout/Num Lock change from producing a mismatched release. Separate physical positions with the same virtual-key value can therefore remain distinct.

**Physical recording** stores scan-code predicates. `SC01C` means make code 0x1C without the extended flag; `SCE01C` means the same make code with that flag. `SC` accepts 2-4 hexadecimal digits; `SCE0` accepts 2-4 digits after the prefix. The source event must actually contain a scan code. Keys with no scan code use their Windows virtual-key identity. Pause and Print Screen use their named identities because their make sequences are special.

**Named recording** stores normalized virtual-key predicates. Main/keypad Enter and keypad/navigation keys remain distinct. Generic Ctrl/Alt/Shift/Win deliberately accept either side; L/R variants accept one side. Keypad digits and decimal use their physical keypad positions regardless of Num Lock. Dedicated Home, End, arrows, Insert, and Delete retain their own identities.

Named recording is the default: **Bind physical key positions (scan codes)** starts unchecked for new settings and for settings files without that preference. Left/right modifier distinction starts enabled. An explicitly saved recording preference is still restored. Existing bindings, including scan-code bindings from older configurations, remain unchanged.

Bindings use physical positions, not the produced text. OEM punctuation and letter virtual-key meanings can change with the selected layout. Use the inspector while recording to see the received values.

## Key families

| Family | Support |
| --- | --- |
| Letters, top-row digits, punctuation, ISO/OEM keys | Named keys, physical scan codes, or VK fallback |
| F1-F24 | Named and physical bindings |
| Left/right Ctrl, Alt, Shift, Windows | Separate identities; generic aliases available |
| Main/keypad Enter; keypad/navigation/digits | Separate identities |
| Locks, Pause, Print Screen, Menu, editing keys | Named and/or physical bindings when Windows reports keyboard events |
| Browser, launch, volume, media, IME and extended virtual keys | Named families and `VKxx` fallback when delivered through the keyboard hook |
| Unlisted OEM/international key | Physical scan-code binding, or `VKxx` if no scan code is supplied |
| Firmware-only Fn, lighting/macro/profile controls | Not detectable unless firmware emits keyboard events |
| Consumer HID controls that emit only HID usages / WM_APPCOMMAND | Not handled by this keyboard-event path |

`Plus`, `Comma`, `Minus`, `Period`, `Slash`, `Backslash`, `Backtick`, `Semicolon`, `Quote`, `LBracket`, `RBracket`, and `OEM102` name common punctuation keys. Other virtual keys are written `VK08` through `VKFE`; named definitions also include special documented values such as Cancel.

## Recording and matching

A chord is finalized when you release a key. Shared modifiers may stay held between steps, and auto-repeat does not create additional recorded steps. The matching engine requires all keys in a chord, rejects unrelated extra held keys, and does not allow one physical event to satisfy two different required key tokens.

Limits: 64 mappings, 16 chords per mapping, 12 key tokens per chord. Ordinary keyboard rollover/ghosting still limits what the hardware can report.

Potential gesture input is temporarily buffered. A mismatch or timeout replays it in order. A foreground-window change discards pending gesture input rather than typing it into another window. The step timeout defaults to 1200 ms. When a gesture finishes, release all its held keys within five seconds to send the selected Print Screen output. Holding unrelated keys before starting a gesture prevents recognition until those keys are released.

Competing active prefixes such as A vs A+B, or Ctrl+K vs Ctrl+K,Ctrl+P, are rejected. Shared prefixes with different later chords are supported. Physical/virtual expressions that could overlap under a different keyboard layout are conservatively rejected; keep competing shortcuts in a consistent recording mode. AltGr can generate additional Ctrl/Alt events depending on the layout; record the actual reported sequence instead of assuming it equals a plain Right Alt.

## Coverage and limits

No user-mode Windows utility can truthfully promise every physical button on every keyboard:

- Firmware can consume Fn, lighting, profile, macro, and vendor controls without exposing a keyboard event.
- The operating system reserves security combinations such as Ctrl+Alt+Delete and Win+L. Named forms are rejected; physical forms cannot bypass Windows security handling.
- Power/sleep and consumer HID controls may travel through other input paths.
- Secure desktops, the sign-in screen, elevated receivers, other keyboard hooks, accessibility settings, and remote sessions affect behavior.
- Injected input from other software is deliberately ignored. Identical positions on separate keyboards are not distinguished.
- If two controls report the same Windows virtual/scan/extended identity, this input path cannot infer a distinction the device did not provide.
- Some special keys/drivers do not produce a normal down/up pair. Verify them on the target hardware before assigning a global shortcut.

## Output combinations

| Saved output ID | Combination |
| --- | --- |
| 0 | Print Screen |
| 1 | Alt + Print Screen |
| 2 | Win + Print Screen |
| 3 | Shift + Print Screen |
| 4 | Ctrl + Print Screen |

Each modified output sends modifier down, Print Screen down, Print Screen up, then modifier up. Alt, Win, Shift, and Ctrl outputs use their left-hand modifier key. Existing IDs 0-2 retain their meaning; the two added options are appended.

Output uses `SendInput` after the gesture is released. This is recognized as software-injected input, not a physical HID keyboard report. Snipping Tool, screenshot utilities, and Windows settings determine the result. Shift/Ctrl outputs send those combinations without assigning a universal screenshot action to them. A receiver may reject injected events; normal-integrity processes cannot inject into a higher-integrity receiver.

References: [Windows keyboard input](https://learn.microsoft.com/en-us/windows/win32/inputdev/about-keyboard-input), [low-level event structure](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-kbdllhookstruct), [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput).
