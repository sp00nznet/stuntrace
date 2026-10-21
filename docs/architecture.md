# Architecture

How the recompiled C, snesrecomp, LakeSnes and the GSU-2 fit together.

## How it works

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

## The Super FX factor

Most SNES recompilations only need to worry about the 65816. Stunt Race FX brought a friend: the **Super FX 2 (GSU-2)**, a 21 MHz RISC chip that handled all the 3D rendering. It has its own instruction set, its own register file (R0-R15), and its own 64 KB of backup RAM.

This means we're not just recompiling one CPU — we're recompiling *two*. Buckle up.

We built a **complete GSU-2 emulator from scratch**. It lives in the [LakeSnes fork](https://github.com/sp00nznet/LakeSnes) that [snesrecomp](https://github.com/sp00nznet/snesrecomp) builds on (`ext/snesrecomp/ext/LakeSnes/snes/gsu.c`), so any Super FX recompilation gets it. Full instruction set, pixel cache, instruction cache, the works. Written in pure C, MIT licensed, zero external dependencies.

## Rendering pipeline

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

## Key WRAM addresses

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

## Project structure

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
└── README.md              # You are here
```
## Infrastructure

| Component | Status | Details |
|-----------|--------|---------|
| GSU-2 emulator | Done | ~1,200 lines of C, full instruction set, pixel cache; lives in the LakeSnes fork (`snes/gsu.c`) |
| GSU bus API | Done | `bus_gsu_read/write/run`, `bus_has_gsu()` |
| Super FX cart detection | Done | Cart type **6** (LoROM+SuperFX), auto-detect from ROM header $FFD6 (4/5 now reserved for DSP-1) |
| 65816 disassembler | Done | M/X flag tracking, LoROM mapping, branch targets |
| snesrecomp integration | Done | LakeSnes backend with GSU, SDL2 platform layer |
| **Timed-recomp framework** | Done | On snesrecomp `main`: interpreter fallback, opcode-fetch interception hook, call-target profiler, RECOMP_PATCH auto-registration |
| **GSU ↔ recomp-hook merge** | Done | Merged GSU-2 into the SMK-lineage LakeSnes (`c251118`) — the new framework had dropped Super FX; restored it + rewired the GSU bus impl |
| **Real-frame run** | Partial | Genuine ROM on the cycle-accurate CPU, deterministic frame to frame; today it reaches the Nintendo logo and then shows a black screen (ROADMAP) |
| **Interception + verification** | Done | Recompiled functions replace ROM subroutines live; WRAM-checksum harness proves bit-exact fidelity |

