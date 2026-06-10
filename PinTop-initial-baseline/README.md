# PinTop

PinTop is a planned modern, lightweight Windows window-pinning utility based on the public DeskPins source code by Elias Fotinis.

This repository starts from a **source-only audited baseline**. Binary help files, the old installer, developer experiments, and the deprecated global-hook DLL source are intentionally excluded from the initial import.

## Planned target

- Minimum OS: Windows 10
- Primary OS: Windows 10 / Windows 11
- Architecture: x64 first, ARM64 later
- Build system: CMake
- Compiler: Visual Studio 2022 / MSVC v143
- Language: C++17
- Distribution: portable zip first

## Safety note

Do not run binaries from old DeskPins packages. PinTop should be built from reviewed source through the repository's own build pipeline.

## License

DeskPins was released under the MIT license. The original copyright and license text are preserved in `third_party/deskpins/license.txt`.
