# Stunt Race FX — Static Recompilation

```
    ____  _____  _   _  _   _  _____   ____      _     ____  _____
   / ___||_   _|| | | || \ | ||_   _| |  _ \    / \   / ___|| ____|
   \___ \  | |  | | | ||  \| |  | |   | |_) |  / _ \ | |    |  _|
    ___) | | |  | |_| || |\  |  | |   |  _ <  / ___ \| |___ | |___
   |____/  |_|   \___/ |_| \_|  |_|   |_| \_\/_/   \_\\____||_____|
                          _____  __  __
                         |  ___| \ \/ /
                         | |_     \  /
                         |  _|    /  \
                         |_|     /_/\_\
```

> **Revving the Super FX chip back to life — one recompiled function at a time.**

Remember 1994? Argonaut Software and Nintendo strapped a custom RISC coprocessor onto a cartridge and said "yeah, we're doing 3D on the Super Nintendo." The result was **Stunt Race FX** — polygonal racing on a 16-bit console that had no business doing polygons. It was glorious. It was janky. It ran at approximately 4 frames per second on a good day. *We loved it.*

This project rips the original 65816 + Super FX 2 machine code out of the ROM and rebuilds it as **native C**, linked against [snesrecomp](https://github.com/sp00nznet/snesrecomp) for authentic PPU/APU/DMA hardware simulation. The CPU is us. The hardware is real. The vibes are immaculate.

---

## What Is This?

A **static recompilation** of Stunt Race FX (SNES, 1994) from WDC 65C816 + Super FX 2 (GSU-2) assembly into native C code, playable on modern hardware via SDL2.

Instead of emulating the CPU, we **are** the CPU. Each original subroutine has been hand-translated into C that calls into real SNES hardware emulation (PPU, APU, DMA) through the snesrecomp library. No interpreter. No JIT. Just raw, recompiled C.

## The Super FX Factor

Most SNES recompilations only need to worry about the 65816. Stunt Race FX brought a friend: the **Super FX 2 (GSU-2)**, a 21 MHz RISC chip that handled all the 3D rendering. It has its own instruction set, its own register file (R0-R15), and its own 64 KB of backup RAM.

This means we're not just recompiling one CPU — we're recompiling *two*. Buckle up.

We built a **complete GSU-2 emulator from scratch** and integrated it directly into [snesrecomp](https://github.com/sp00nznet/snesrecomp)'s LakeSnes backend. It's now available for any Super FX game recompilation, not just this one. Full instruction set, pixel cache, instruction cache, the works. Written in pure C, MIT licensed, zero external dependencies.

## Status

| Milestone | Status |
|-----------|--------|
| Project scaffolding | Done |
| snesrecomp integration | Done |
| GSU-2 emulator (in snesrecomp) | Done |
| GSU bus API (read/write/run) | Done |
| Super FX cart type detection | Done |
| 65816 disassembler tool | Done |
| Reset vector ($00:FE88) | Done |
| Full HW init ($03:89B4) | Done |
| WRAM clear via DMA ($03:8CF6) | Done |
| Full init chain ($03:8AA9) | Done |
| NMI handler ($02:8000) | Done |
| NMI WRAM trampoline setup | Done |
| NMI DMA copy to WRAM ($7E:A2D9) | Done |
| GSU register setup (CFGR/SCBR/CLSR) | Done |
| Game data DMA to WRAM | Done |
| GSU work RAM clear ($70:0000) | Done |
| Main loop entry ($03:8C63) | Stubbed |
| NMI state machine dispatch | Done |
| NMI state 0 (title VBlank) | Done |
| NMI state 1 (force blank / DMA) | Done |
| NMI state 4 (gameplay VBlank) | Done |
| NMI state 8 (gameplay force-blank) | Done |
| SPC700 audio engine upload | Done |
| IPL transfer protocol | Done |
| Brightness control ($02:D65A) | Done |
| Scanline wait ($02:D7AB) | Done |
| Screen setup / scene transition ($02:CF45) | Done |
| Display mode DMA dispatcher ($03:DD1B) | Partial |
| GSU framebuffer → VRAM DMA (race mode) | Done |
| GSU program launcher ($7E:E1F5) | Done |
| PPU Mode 3 setup ($03:EB0E) | Done |
| VRAM DMA engine ($03:EB83) | Done |
| Title screen scene builder ($03:D9B9) | Done |
| Title/attract wrapper ($03:D996) | Done |
| Title screen tile/tilemap DMA | Done |
| Title screen GSU program exec | Done |
| Per-frame dispatch ($02:E0A9) | Done |
| Attract mode frame body ($02:D7CD) | Done |
| Camera/object setup ($02:DAD6) | Done |
| Frame timing (60fps → seconds) | Done |
| GSU 3D render pass (main + second) | Done |
| Fade in/out management | Done |
| Race gameplay | Not started |

**Recompiled functions: 25** across 7 source files

**Current state:** The attract mode frame loop is running. Every frame: the per-frame dispatch manages fade timing ($02:E0A9), then the attract body ($02:D7CD) reads the object/camera state table from WRAM $7E:204F, sets up 3D camera position, launches the main GSU 3D render pass (program at $BBC7), processes palettes and display updates, optionally runs a second GSU pass ($D307), and accumulates frame timing for the 60fps→seconds counter. The GSU renders to bank $70 work RAM, and the NMI VBlank handler DMAs the result to VRAM. The full pipeline from boot to per-frame rendering is now traced and recompiled.

## Building

### Prerequisites
- CMake 3.16+
- Visual Studio 2022 (MSVC)
- SDL2 via vcpkg: `vcpkg install sdl2:x64-windows`
- Python 3.10+ (tooling)

### Build
```bash
git clone --recursive https://github.com/sp00nznet/stuntrace.git
cd stuntrace

cmake -B build -G "Visual Studio 17 2022" -A x64 \
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build --config Debug
```

### Run
```bash
# Place your legally obtained ROM as "Stunt Race FX (USA).sfc"
build/Debug/srf_launcher.exe "Stunt Race FX (USA).sfc"
```

## ROM Required

You must supply your own legally obtained copy of the US release:

| Field | Value |
|-------|-------|
| Title | STUNT RACE FX |
| Region | USA (NTSC) |
| Format | LoROM + Super FX |
| Size | 1 MB (8 Mbit) |
| File | `Stunt Race FX (USA).sfc` |

This project does not include any copyrighted ROM data.

## Project Structure

```
stuntrace/
├── include/srf/           # Headers
│   ├── cpu_ops.h          #   65816 instruction helpers
│   └── functions.h        #   Recompiled function declarations
├── src/
│   ├── recomp/            # Recompiled game code
│   │   ├── srf_boot.c     #   Reset vector, NMI handler
│   │   └── srf_init.c     #   Hardware init, main loop, func registration
│   └── main/
│       └── main.c         #   Launcher / frame loop
├── ext/snesrecomp/        # SNES hardware library (submodule)
│   └── ext/LakeSnes/snes/
│       ├── gsu.h          #   ★ GSU-2 coprocessor header (new!)
│       └── gsu.c          #   ★ GSU-2 coprocessor emulation (new!)
├── tools/                 # Disassembly & trace tooling
├── CMakeLists.txt
├── CLAUDE.md              # Developer notes
└── README.md              # You are here
```

## How It Works

```
┌─────────────────────┐     ┌──────────────────────┐
│   Recompiled C      │     │     snesrecomp        │
│   (srf_*.c files)   │────▶│   (LakeSnes backend)  │
│                     │     │                      │
│  "I am the CPU"     │     │  PPU · APU · DMA     │
│  bus_read8/write8   │     │  Memory bus · Cart    │
└─────────────────────┘     └──────────┬───────────┘
                                       │
                                       ▼
                              ┌────────────────┐
                              │     SDL2       │
                              │  Window · Audio│
                              │  Input · VSync │
                              └────────────────┘
```

Each recompiled function follows the naming convention `srf_BBAAAA` where `BB` is the SNES bank and `AAAA` is the address. So `srf_008000` = bank $00, address $8000 = the reset vector.

## Related Projects

This is part of the [sp00nznet](https://github.com/sp00nznet) recompilation universe:

| Project | Platform | Game |
|---------|----------|------|
| **stuntrace** | SNES + Super FX | Stunt Race FX (1994) |
| [mk](https://github.com/sp00nznet/mk) | SNES | Super Mario Kart (1992) |
| [snesrecomp](https://github.com/sp00nznet/snesrecomp) | SNES | Hardware library |
| [LinksAwakening](https://github.com/sp00nznet/LinksAwakening) | Game Boy Color | Link's Awakening DX |
| [diddykongracing](https://github.com/sp00nznet/diddykongracing) | N64 | Diddy Kong Racing |
| [burnout3](https://github.com/sp00nznet/burnout3) | Xbox | Burnout 3: Takedown |

## License

MIT — see [LICENSE](LICENSE).

---

*"The Super FX chip was the future. It's 2026 and we're still not over it."*
