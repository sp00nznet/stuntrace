# Stunt Race FX — Static Recompilation

## Project scope
Convert the WDC 65C816 + Super FX 2 (GSU-2) assembly of **Stunt Race FX** (SNES, 1994) into native C code, playable on modern hardware via SDL2.

This is the second SNES recompilation in the sp00nznet portfolio, following Super Mario Kart (`mk`).

## Target hardware
| Component | Detail |
|-----------|--------|
| Main CPU | WDC 65C816 @ 3.58 MHz |
| Coprocessor | Super FX 2 (GSU-2) — 21 MHz, 3D polygon renderer |
| ROM | 1 MB (8 Mbit), LoROM + Super FX mapping |
| RAM | 128 KB WRAM, 64 KB GSU backup RAM |
| Output | Native x86-64 C → SDL2 (video + audio + input) |

## Hardware backend
**snesrecomp** (ext/snesrecomp/) provides the full SNES hardware stack via LakeSnes:
- PPU: scanline rendering, all modes, sprites, windows, color math
- APU/SPC700: complete audio DSP, 8 channels, BRR, echo, noise
- DMA: GPDMA + HDMA, all 8 channels
- Memory bus: 24-bit routing with LoROM/HiROM auto-detection
- Cartridge: SRAM, bank mapping

Recompiled functions interface through `bus_read8()` / `bus_write8()`.

## Super FX 2 notes
Stunt Race FX uses the GSU-2 for all 3D rendering. The GSU has:
- Its own register file (R0-R15), separate from the 65816
- Memory-mapped I/O at $3000-$303F
- Access to a 64 KB backup RAM bank
- Plot/color/screen-base registers for framebuffer drawing

The GSU executes its own instruction set — this will need separate recompilation or emulation handling via snesrecomp.

## ROM identification
- **Title:** STUNT RACE FX (US)
- **Region:** USA (NTSC)
- **Format:** LoROM + Super FX
- **Size:** 1,048,576 bytes (1 MB)
- **ROM file:** `Stunt Race FX (USA).sfc`

## Code conventions
- Language: C17 (MSVC)
- Function prefix: `srf_BBAAAA` (bank + address, hex)
- Global CPU state: `SnesCpu g_cpu`
- Build: CMake 3.16+, Visual Studio 2022, SDL2 via vcpkg

## Build
```
vcpkg install sdl2:x64-windows
cmake -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Debug
```

## References
- [Super FX Wiki](https://en.wikibooks.org/wiki/Super_NES_Programming/Super_FX_tutorial)
- [fullsnes — Super FX](https://problemkaputt.de/fullsnes.htm#snescartgsu1telecomsuper)
- [LakeSnes](https://github.com/elzo-d/LakeSnes)
