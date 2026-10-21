# Nyx NES 🎮

This NES emulator is built from scratch in C++17.

Full 6502 CPU, PPU, APU, and seven cartridge mappers, wrapped in a Dear ImGui
frontend with auto-extracted title screen thumbnails, save states, and
gamepad support. Built as a single-session project — every line of the CPU,
PPU, and APU core is hand-written; no code borrowed from existing emulators 😉

## 👇 Features 👇

### Emulation

- 6502 CPU — instruction-accurate, fully documented opcode set
- PPU — backgrounds, sprites, sprite-0 hit, VRAM write buffering, one-screen /
  horizontal/vertical mirroring, NTSC and PAL timing
- APU — 2× pulse, triangle, noise, high-pass + low-pass filter, PAL clock rate
- NES light gun (Zapper) — mouse aim, left click to fire, phosphor decay

### Cartridge support

| Mapper | Name  | Notable games |
|--------|-------|---------------|
| 0      | NROM  | Super Mario Bros., Donkey Kong, Pac-Man, Ice Climber |
| 1      | MMC1  | The Legend of Zelda, Metroid, Mega Man 2, Final Fantasy |
| 2      | UxROM | Contra, Castlevania, Mega Man 1, DuckTales |
| 3      | CNROM | Arkanoid, Gradius |
| 4      | MMC3  | Super Mario Bros. 3, Kirby's Adventure, Mega Man 3–6, Batman |
| 7      | AxROM | Battletoads, Marble Madness |
| 66     | GNROM | Super Mario Bros. + Duck Hunt combo cart |

### Frontend

- Dear ImGui ROM library with auto-extracted title screen thumbnails
- Search, sort by recent / name A–Z / name Z–A
- Grid and list views
- 4 save state slots (`F5` / `F7`)
- Battery-backed save RAM (`.sav` files, persisted between sessions)
- Xbox / PS4 / PS5 / Switch Pro controller support via SDL_GameController
- Integer scaling, aspect-preserving fit, and optional CRT scanline filter
- Fullscreen, volume slider, drag-and-drop ROM loading

## Controls

| Keyboard | Gamepad | NES button |
|----------|---------|------------|
| Arrow keys | D-pad / left stick | D-pad |
| Z | A / Y | A button |
| X | B / X | B button |
| Enter | Start | Start |
| Left mouse click | — | Zapper trigger |

### Hotkeys

- `F5` / `F7` — save / load state slot 1
- `F2` — reset
- `F11` — toggle fullscreen
- `Ctrl+L` — toggle ROM library
- `Ctrl+O` — load ROM from disk
- `Space` — pause
- `Esc` — quit

## Building

### Dependencies

- C++17 compiler (GCC 15+, Clang 14+, MSVC 2019+)
- CMake 3.15+
- SDL2

Dear ImGui is fetched automatically by CMake at configuration time, so no
manual dependency setup is required.

### Windows (MSYS2 UCRT64)

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-SDL2
mkdir build && cd build
cmake ..
cmake --build .
