## Nyx NES v1.0.0✅

First public release of Nyx NES, a cycle-accurate NES emulator written from scratch in C++17.

### Download

Download **nyx-nes-windows-x64.zip**, extract it anywhere, drop your `.nes` files into the `roms` folder, and run `nyx-nes.exe`.

No install, no dependencies - SDL2 and required runtime DLLs are bundled u{1F97A}

- Cycle-accurate 6502 CPU
- PPU with sprite rendering, sprite-0 hit, PAL/NTSC
- APU with 5 channels, high-pass/low-pass filtering
- 7 mappers: NROM, MMC1, UxROM, CNROM, MMC3, AxROM, GNROM (~85% of the NES library)
- Save states (F5/F7) and battery-backed saves
- ROM library with auto-generated thumbnails, search, sort, delete
- Gamepad (Xbox/PS/Switch) + keyboard, both merged
- Fast forward (TAB or Emulation menu)
- CRT scanline filter, 5 video scale modes
- Drag-and-drop ROM loading

- SMB3 has minor cosmetic cloud/edge glitches (PPU/MMC3 timing)
- Audio is ~95% accurate (some high notes drop)
- Battletoads and Punch-Out may not boot (require deeper cycle accuracy)

| Keyboard | Gamepad | NES |
|----------|---------|-----|
| Arrows | D-pad / left stick | D-pad |
| Z | A / Y | A |
| X | B / X | B |
| Enter | Start | Start |
| F5 / F7 | - | Save / Load state |
| TAB | - | Fast forward |
| F11 | - | Fullscreen |
| Ctrl+L | - | Toggle ROM library |
| Esc | - | Quit |

### Source

Build from source with CMake + SDL2. See README for details.

No ROMs are included. You must supply your own. ⚖️
