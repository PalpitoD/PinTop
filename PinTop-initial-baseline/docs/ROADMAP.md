# PinTop roadmap

## v0.1: Clean source baseline

- Keep original DeskPins source as reference under `third_party/deskpins/`.
- Exclude compiled help, old installer, deprecated hook DLL, and misc utilities.
- Document safety findings and migration plan.

## v0.2: Buildable modern skeleton

- Add CMake project.
- Target Windows 10+ and x64.
- Use Visual Studio 2022 / MSVC v143 / C++17.
- Replace missing `eflib` wrappers.
- Remove Boost dependency where practical.

## v0.3: Core pinning functionality

- Implement tray icon.
- Implement safe window selection without synthetic mouse events.
- Implement topmost/un-topmost handling.
- Add DPI awareness and Windows 11 testing notes.

## v0.4: Compatibility improvements

- Add better failure messages for elevated windows, fullscreen games, protected windows, and UWP/modern app edge cases.
- Add hotkeys.
- Add optional auto-start.

## v1.0: Stable release

- Publish x64 portable zip.
- Add optional ARM64 build.
- Add signed release later if a certificate becomes available.
