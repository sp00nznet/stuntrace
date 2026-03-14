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

## Super FX 2 / GSU-2
The GSU-2 coprocessor is now **fully emulated** inside snesrecomp (gsu.c/gsu.h in LakeSnes).

Features implemented:
- 16 x 16-bit GP registers (R0-R15, R15 = PC)
- Full instruction set (~90 opcodes with ALT1/ALT2/ALT3 prefix modes)
- 512-byte instruction cache (32 x 16-byte lines)
- 2-entry pixel write-back cache with read-modify-write support
- Memory-mapped I/O at $3000-$303F (R0-R15, SFR, control regs)
- Cache RAM access at $3100-$32FF
- ROM/RAM buffer system for async memory access
- Plot/RPIX pixel engine with transparency, dither, color modes
- Cart type 4 (LoROM+SuperFX) with proper memory mapping
- GSU work RAM at banks $60-$6F (64-128 KB)

The recompiled 65816 code triggers GSU execution by writing R15's high byte,
which sets the GO flag and runs the GSU until STOP.

## ROM identification
- **Title:** STUNT RACE FX (US)
- **Region:** USA (NTSC)
- **Format:** LoROM + Super FX (cart type 4)
- **Size:** 1,048,576 bytes (1 MB)
- **MD5:** 128b316a74caf17fdc216b3ab46d4a9a
- **Map mode:** 0x20 (LoROM)
- **ROM type:** 0x1A (Super FX + RAM + battery)
- **Reset vector:** $FE88
- **NMI vector:** $0108
- **IRQ vector:** $010C
- **ROM file:** `Stunt Race FX (USA).sfc`

The reset vector at $FE88 does SEI/CLC/XCE/CLD then JML $03:8AA9 for full init.

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
