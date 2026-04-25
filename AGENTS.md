# AGENTS.md

## Build

```powershell
rm -rf build; cmake -B build; cmake --build build
```

- Default generator: Visual Studio 17 2022
- If switching generators, always delete `build/` first
- Output: `build\Debug\MusicPlayer.exe` or `build\Release\MusicPlayer.exe`

## Project Type

- C++ Win32 GUI application (not console)
- Entry point: `WinMain` in `main.cpp`
- Target: `WIN32` executable

## Dependencies

- Windows native APIs only (user32, gdi32, winmm, comdlg32, comctl32, ole32, shell32)
- No external libraries

## Key Files

- `CMakeLists.txt` - build configuration
- `main.cpp` - application source with MCI audio playback
