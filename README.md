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

### Infrastructure

| Component | Status | Details |
|-----------|--------|---------|
| GSU-2 emulator | Done | ~1,200 lines of C, full instruction set, pixel cache, pushed to snesrecomp |
| GSU bus API | Done | `bus_gsu_read/write/run`, `bus_has_gsu()` |
| Super FX cart detection | Done | Cart type 4 (LoROM+SuperFX), auto-detect from ROM header $FFD6 |
| 65816 disassembler | Done | M/X flag tracking, LoROM mapping, branch targets |
| snesrecomp integration | Done | LakeSnes backend with GSU, SDL2 platform layer |

### Boot & Initialization

| Function | Address | Status |
|----------|---------|--------|
| Reset vector | $00:FE88 | Done |
| Full HW init (PPU/DMA/CPU regs) | $03:89B4 | Done |
| WRAM DMA clear | $03:8CF6 | Done |
| Full init chain | $03:8AA9 | Done |
| NMI handler code DMA to WRAM | $02:8000 → $7E:A2D9 | Done |
| NMI WRAM trampoline | $00:0108 | Done |
| GSU register setup | CFGR/SCBR/CLSR | Done |
| GSU work RAM clear | $70:0000-$27FF | Done |
| SPC700 audio engine upload | $04:D44C | Done |
| IPL transfer protocol | $04:D720 | Done |
| Audio state clear | $04:D6E1 | Done |
| Audio command queue | $04:D649 | Done |
| Audio/music reload | $04:D0DB | Done |
| WRAM jump table patches | $09:ECE0 | Done |
| Game scene init (title setup) | $03:8B96-$8C62 | Done |
| Initial GSU 3D scene launch | $04:8800 | Done |

### NMI / VBlank

| Function | State | ROM | Status |
|----------|-------|-----|--------|
| NMI handler | — | $02:8000 | Done |
| State machine dispatch | — | $7E:A305 | Done |
| Title brightness | $00 | $02:8057 | Done |
| Title force blank + DMA | $02 | $02:8074 | Done |
| Race brightness + OAM | $04 | $02:821C | Done |
| Race force blank + color | $06 | $02:8292 | Done |
| Race 2P brightness | $08 | $02:82DD | Done |
| Race 2P force blank + scroll | $0A | $02:8336 | Done |
| Race brightness + DMA | $0C | $02:83A8 | Done |
| Race force blank + BG DMA | $0E | $02:83D7 | Done |
| Gameplay brightness + scroll | $10 | $02:809C | Done |
| Gameplay force blank + VRAM | $12 | $02:80CF | Done |
| IRQ mid-screen | $14 | $02:8264 | Done |
| No-op | $16 | $02:8056 | Done |
| OAM DMA helper | — | $02:8A3D | Done |
| Joypad auto-read + edge detect | — | $02:8ED5 | Done |
| Audio sync / APU dispatch | — | $02:8E39 | Done |
| Brightness fade ramp | — | $02:8974 | Done |
| GSU RAM VRAM DMA | — | $02:88FE | Done |
| Window/HDMA table config | — | $02:8192 | Done |

### Display & Rendering

| Function | Address | Status |
|----------|---------|--------|
| Brightness control | $02:D65A | Done |
| Scanline wait | $02:D7AB | Done |
| Screen setup / scene transition | $02:CF45 | Done |
| Display mode DMA dispatcher | $03:DD1B | Done (all 4 modes + post-dispatch) |
| GSU tile decompressor launch | $03:EC01 | Done |
| Display config lookup | $03:DCEF | Done |
| Display mode config tables | $03:EF3F | Done |
| VRAM DMA from GSU RAM | $03:DCC0 | Done |
| GSU framebuffer copy | MVN $70→$7F | Done (12 KB per frame) |
| GSU framebuffer → VRAM DMA | (race mode) | Done |
| GSU program launcher | $7E:E1F5 | Done |
| PPU Mode 3 setup | $03:EB0E | Done |
| VRAM DMA engine (table-driven) | $03:EB83 | Done |
| Title screen scene builder | $03:D9B9 | Done |
| Title/attract setup wrapper | $03:D996 | Done |
| Display mode setup (RGB→BGR) | $02:E289 | Done |
| Viewport config init | $08:B893 | Done |

### Game Logic

| Function | Address | Status |
|----------|---------|--------|
| Main loop mode dispatch | $03:8C63 | Done |
| Per-frame dispatch (fade) | $02:E0A9 | Done |
| Attract mode frame body | $02:D7CD | Done |
| Gameplay frame body (2P) | $0B:FB26 | Done |
| Title screen state machine | $0B:AE0A | Done |
| Input check / Start detection | $0B:AE8F | Done |
| Camera setup from object | $02:DAD6 | Done |
| Rotation matrix via GSU | $03:B011 | Done (launches $01:8325) |
| Camera angle calc (3D→screen) | $03:D306 | Done |
| PRNG | $02:DF79 | Done (32-bit LFSR) |
| Object/animation processing | $03:D388 | Done (with indirect callback dispatch) |
| Animation update handler | $03:CAB8 | Done (indirect via func_table) |
| Animation init handler | $03:CAEB | Done (indirect via func_table) |
| Object slot allocator | $03:CB5C | Done |
| Object slot initializer | $03:CB25 | Done |
| Object deallocator | $03:B8A1 | Done |
| Render list insert | $08:8364 | Done |
| Render list remove | $08:8392 | Done |
| Vehicle race animation | $08:94A1 | Done |
| Vehicle collision animation | $08:951B | Done |
| Render list rehash | $08:83CC | Done |
| Object validity + GSU flags | $08:D070 | Done |
| Collision response (chain walk) | $08:CF41 | Done |
| Collision check (standalone) | $08:CF92 | Done |
| Collision state sync (GSU→WRAM) | $08:CE02 | Done |
| GSU flag setup (bit 4/8) | $08:CCA3 | Done |
| GSU animation frame | $08:CCBE | Done |
| GSU position write | $08:CCD2 | Done |
| GSU flag OR | $08:CCF1 | Done |
| Object→GSU full sync | $08:D86F | Done |
| GSU camera sync (3 vals) | $08:D8C2 | Done |
| Vehicle object creation | $08:CC7C | Done |
| Vehicle model setup | $08:88C7 | Done |
| Object callback chain | $08:CD25 | Done |
| Viewport render order | $08:B863 | Done |
| Object state update (per-player) | $08:C60F | Done |
| Object render/callback setup | $08:B4C6 | Done |
| Object system main (P1/P2) | $08:C5A5 | Done (calls $08:C60F) |
| P2 GSU render pipeline | $7E:E258 | Done |
| Gameplay audio sync | $7F:112F | Done |
| Scene state initialization | $03:B3DA | Done |
| Entity system init | $0B:B479 | Done |
| Entity allocator | $0B:B4C3 | Done |
| Entity callback dispatcher | $0B:B450 | Done |
| Sprite compositor | $0B:B64A | Done (single + multi-sprite) |
| VBlank wait | $0B:E390 | Done |
| Display-mode init | $03:D8B3 | Done |
| Palette/GSU checksum | $03:F02B | Done |
| Gameplay scene setup | $0B:FA24 | Done |
| Object table setup for scenes | $03:B48C | Done |
| Palette copy from ROM | $02:D53D | Done |
| Palette fade interpolation | $02:D55F | Done |
| GSU palette program launch | $02:DB59 | Done |
| Scene change A/B/C | $03:863D/$8648/$8653 | Done |
| Scene config loader | $03:8683 | Done |
| Scene reset (3 configs) | $03:865E | Done |
| Full game restart | $03:8C86 | Done |
| Frame timeout logic | — | Done |
| Attract mode auto-cycling | (recomp) | Done (1024 frame timer, 3 demo scenes) |
| Start button → gameplay | (recomp) | Done (input edge detect → scene transition) |

### Not Yet Started

| Component | Notes |
|-----------|-------|
| Race mode physics | Vehicle dynamics, collision detection |
| Track data loading | Course geometry, checkpoints |
| Vehicle control input | Steering, acceleration, braking |
| Menu system | Character select, track select, options |
| GSU 3D program recompilation | The polygon renderer programs themselves |
| Particle effects | Exhaust, sparks, dust |
| HUD / UI rendering | Speed, lap counter, timer |
| Save data management | SRAM read/write for records |

---

**82 exported + 30 static helper functions** across **10 source files** (~5,980 lines) — clean build on MSVC 2022.

### Recompiled Source Files

| File | Functions | Purpose |
|------|-----------|---------|
| `srf_boot.c` | 2 | Reset vector ($FE88), NMI handler ($02:8000) |
| `srf_init.c` | 6 | Full HW init, WRAM clear, GSU setup, scene init, main loop |
| `srf_nmi.c` | 1+12 | NMI state machine — all 12 VBlank states + 6 helpers |
| `srf_audio.c` | 6 | SPC700 upload, IPL transfer, audio clear/queue/reload |
| `srf_display.c` | 11+5 | Camera, PRNG, palette, brightness, display pipeline, GSU launcher |
| `srf_title.c` | 4 | PPU Mode 3 config, VRAM DMA engine, title scene builder |
| `srf_attract.c` | 2 | Per-frame fade dispatch, attract mode frame body |
| `srf_input.c` | 4 | Title state machine, input detection, camera math, object animation |
| `srf_objects.c` | 31 | Scene init, rotation matrix, vehicle creation/model/callbacks, collision chain, GSU sync/accessors, object lifecycle |
| `srf_gameplay.c` | 11 | Entity system, sprite compositor, scene setup, frame body, restart |

### Rendering Pipeline

```
ROM Data ──► GSU Decompress ──► Bank $70 Work RAM ──► DMA ──► VRAM ──► PPU ──► Screen
                  │                     │
                  │              ┌──────┴──────┐
                  │              │  65816 NMI   │
                  │              │  VBlank DMA  │
                  │              └──────────────┘
                  │
           ┌──────┴──────┐
           │  GSU-2 RISC │
           │  21 MHz     │
           │  Programs:  │
           │  $BBC7 (P1) │
           │  $D307 (P2) │
           └─────────────┘
```

### Key WRAM Addresses

| Address | Purpose |
|---------|---------|
| $0D3F | NMI state machine index |
| $0D2B | Display mode (0=title, 3=race) |
| $0D62 | Game mode (0=attract, non-zero=gameplay) |
| $0374 | SCMR base value (OR'd with $18 during GSU exec) |
| $0306 | Frame counter (per-frame) |
| $05E9 | Frame counter (master) |
| $0346 | P1 object state index |
| $034A | P2 object state index |
| $0309 | P1 current button state |
| $030D | P2 current button state |
| $0D61 | Screen brightness |
| $0D60 | Brightness fade accumulator (16-bit) |
| $0D45 | HDMA channel enable mask |
| $0D47 | Color math register value |
| $0D7F | Sprite VRAM DMA pending flag |
| $0311 | P1 new button presses (edge) |
| $0313 | P2 new button presses (edge) |

---

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
| MD5 | `128b316a74caf17fdc216b3ab46d4a9a` |
| Reset Vector | $FE88 |
| NMI Vector | $0108 → $7E:A2D9 |
| File | `Stunt Race FX (USA).sfc` |

This project does not include any copyrighted ROM data.

## Project Structure

```
stuntrace/
├── include/srf/           # Headers
│   ├── cpu_ops.h          #   65816 instruction helpers (REP/SEP/XCE/stack/etc)
│   └── functions.h        #   All 82 recompiled function declarations
├── src/
│   ├── recomp/            # Recompiled game code (10 files)
│   │   ├── srf_boot.c     #   Reset vector, NMI handler
│   │   ├── srf_init.c     #   HW init, WRAM clear, GSU setup, main loop
│   │   ├── srf_nmi.c      #   NMI VBlank state machine (12 states + 6 helpers)
│   │   ├── srf_audio.c    #   SPC700 audio upload via IPL protocol
│   │   ├── srf_display.c  #   Screen setup, brightness, GSU launcher
│   │   ├── srf_title.c    #   Title screen PPU/VRAM/tile setup
│   │   ├── srf_attract.c  #   Attract mode per-frame rendering
│   │   ├── srf_input.c    #   Input handling, camera math, objects
│   │   ├── srf_objects.c  #   Object system, display mode, WRAM patches
│   │   └── srf_gameplay.c #   Gameplay frame body, scene mgmt, restart
│   └── main/
│       └── main.c         #   Launcher / frame loop
├── ext/snesrecomp/        # SNES hardware library (submodule)
│   └── ext/LakeSnes/snes/
│       ├── gsu.h          #   GSU-2 coprocessor header
│       └── gsu.c          #   GSU-2 coprocessor emulation (~1,200 lines)
├── tools/
│   └── disasm/
│       └── disasm.py      #   65816 disassembler (LoROM, M/X tracking)
├── CMakeLists.txt
├── CLAUDE.md              # Developer notes & ROM details
└── README.md              # You are here
```

## How It Works

```
┌─────────────────────┐     ┌──────────────────────┐
│   Recompiled C      │     │     snesrecomp        │
│   (10 srf_*.c files)│────▶│   (LakeSnes backend)  │
│                     │     │                      │
│  "We are the CPU"   │     │  PPU · APU · DMA     │
│  bus_read8/write8   │     │  GSU-2 · Cart · WRAM │
└─────────────────────┘     └──────────┬───────────┘
                                       │
                                       ▼
                              ┌────────────────┐
                              │     SDL2       │
                              │  Window · Audio│
                              │  Input · VSync │
                              └────────────────┘
```

Each recompiled function follows the naming convention `srf_BBAAAA` where `BB` is the SNES bank and `AAAA` is the address. The GSU launch protocol writes PBR, sets SCMR with RON+RAN, writes R15 to trigger execution, waits for STOP, then restores bus ownership.

## Related Projects

This is part of the [sp00nznet](https://github.com/sp00nznet) recompilation universe:

| Project | Platform | Game |
|---------|----------|------|
| **stuntrace** | SNES + Super FX | Stunt Race FX (1994) |
| [mk](https://github.com/sp00nznet/mk) | SNES | Super Mario Kart (1992) |
| [snesrecomp](https://github.com/sp00nznet/snesrecomp) | SNES | Hardware library (now with GSU-2!) |
| [LinksAwakening](https://github.com/sp00nznet/LinksAwakening) | Game Boy Color | Link's Awakening DX |
| [diddykongracing](https://github.com/sp00nznet/diddykongracing) | N64 | Diddy Kong Racing |
| [burnout3](https://github.com/sp00nznet/burnout3) | Xbox | Burnout 3: Takedown |

## License

MIT — see [LICENSE](LICENSE).

---

*"The Super FX chip was the future. It's 2026 and we're still not over it."*
