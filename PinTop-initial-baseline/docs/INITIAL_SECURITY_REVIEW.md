# Initial static security review

Input archive: `DeskPins-main.zip`  
SHA-256: `dc70ca0207e2b151bff860ca69415e9689aec3f31433d92632ce32654b0052f7`  
Review type: static inspection only; no code or binary was executed.

## Archive observations

- Entries: 455
- Uncompressed size: 1,272,168 bytes
- No path traversal entries were detected.
- No PE executable files were detected by `MZ` header scan.
- No `.exe` or `.dll` files were present in the uploaded archive.

## Files requiring caution

The original archive contains files that are intentionally excluded from this source-only baseline:

- `help/*.chm`: compiled Microsoft HTML Help files. These are binary help containers and are not needed for the first source migration.
- `installer/DeskPins.nsi`: old NSIS installer script. It writes uninstall keys and an optional startup Run key.
- `dphook/`: deprecated global shell-hook DLL source using `SetWindowsHookEx(WH_SHELL, ...)`. Current main code uses `SetWinEventHook(...)` instead.
- `_misc/`: old developer experiments/utilities, not part of the main build.

## Notable source findings

### Mouse-capture bug candidate

`mainwnd.cpp` and `pinlayerwnd.cpp` synthesize mouse input using `mouse_event(...)` while entering/exiting pin mode. This is a strong candidate for the reported bug where a window snaps to the mouse and follows it.

Files:

- `third_party/deskpins/mainwnd.cpp`
- `third_party/deskpins/pinlayerwnd.cpp`

### 64-bit modernization required

The code uses 32-bit-era Win32 APIs and casts such as:

- `SetWindowLong(...)`
- `GetWindowLong(...)`
- `GWL_HWNDPARENT`
- `LONG(pointer)`

These must be migrated to `SetWindowLongPtr(...)`, `GetWindowLongPtr(...)`, `GWLP_*`, and `LONG_PTR` for a proper x64 build.

### Missing dependency

The original source depends on the author's private `eflib` framework and some small Boost utilities. `eflib` is not included in the uploaded archive, so the preferred modernization path is to replace those wrappers with standard C++17 and direct Win32 API code.

## Initial safety conclusion

The uploaded archive does not show an obvious embedded executable payload in the first static pass. However, the project should not be imported or built wholesale. PinTop should start from a reduced source-only baseline, then replace legacy code incrementally.
