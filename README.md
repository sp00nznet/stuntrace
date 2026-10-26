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

## What it is

A **static recompilation** of Stunt Race FX (SNES, 1994) from WDC 65C816 + Super FX 2 (GSU-2) assembly into native C code, built on the [snesrecomp](https://github.com/sp00nznet/snesrecomp) toolkit and SDL2.

Each original subroutine is hand-translated into C that talks to SNES hardware (PPU, APU, DMA, GSU-2) through snesrecomp. The project follows snesrecomp's **incremental interception** model: the genuine ROM runs on a cycle-accurate CPU, and each recompiled function replaces its ROM subroutine once it is verified against genuine execution, until coverage reaches 100% native.

Follows the recomp house style shared with snesrecomp, gbarecomp, lynxrecomp,
xboxrecomp, ps3recomp and pcrecomp: the toolkit lives in its own repo, generated
code is never committed, and every build runs headless.

## Status

**Alpha. Unversioned. Playable from boot into a race.** Checked headless on the
current build (2026-10-26, realframe mode, scripted joypad):

- Boot, title screen, Game Selection, Car Selection (the GSU-rendered 3D car),
  the Speed Trax course menu, and a race all run: countdown, driving, speed,
  rank and timer. This needed a rewritten Super FX core in LakeSnes
  ([docs/super-fx.md](docs/super-fx.md)); before it the game stopped on a
  black screen after the Nintendo logo.
- The genuine ROM does the running. **82 routines** are hand-translated to C
  ([docs/functions.md](docs/functions.md)); `intercept` mode runs the verified
  ones natively in place of the ROM.
- **Conformance: 17/55.** Of the 55 hand-ported routines the title-to-race
  tour reaches, 17 match the ROM on every checked call (`tools/conformance.py`,
  below; 2026-10-26). The other 38 are older hand-ports with known mismatches;
  each one's first mismatch is in `scratch/conf/results.txt`.
- Builds with MSVC 2022 and, on the netlab farm, with mingw-w64.
- Not checked yet: the other modes (Stunt Trax, Battle Trax, Test Run), a full
  race to the finish, audio quality, and timing against real hardware (the
  GSU's clock costs are approximate).

## Screenshots

From headless runs of the current build:

| | |
|---|---|
| ![Title screen](docs/screenshots/title.png) | ![Game selection](docs/screenshots/game-selection.png) |
| ![Car selection, the 3D car is drawn by the Super FX](docs/screenshots/car-selection.png) | ![Race start](docs/screenshots/race-start.png) |
| ![Racing on Speed Trax](docs/screenshots/race.png) | ![Nintendo logo during boot](docs/screenshots/boot-nintendo-logo.png) |

## Getting Started

Two routes that end in the same place: Quick start for a double-click install,
Step by step for the commands it runs.

### Quick start (Windows)

1. Download the source ([Code → Download ZIP](https://github.com/sp00nznet/stuntrace/archive/refs/heads/main.zip)) and unzip it.
2. Put your Stunt Race FX (USA) ROM in that folder: a `.sfc`, `.smc`, or a `.zip`
   with one inside (see "ROM required" below).
3. Double-click **`Setup.cmd`**. It checks for Git, CMake and the Visual Studio
   C++ build tools and asks before installing any that are missing (with winget).
   It finds your ROM by checksum, fetches snesrecomp and SDL2 (through vcpkg,
   in `deps\`), and builds. The first run takes about 5 minutes. A rerun
   skips what's done.
4. Double-click **`Stunt Race FX (recomp).cmd`** to play.

If it stops, it says what to do in one sentence. The details are in
`logs\setup.log`. The same script powers the
[recomp-launcher](https://github.com/sp00nznet/recomp-launcher) install
(`launcher/launcher.json`).

### Step by step

These are the commands `Setup.cmd` runs.

1. **Install the tools:**
   - [Git](https://git-scm.com/download/win)
   - [Visual Studio 2022](https://visualstudio.microsoft.com/vs/community/) (Community or Build Tools) with **Desktop development with C++**. It includes CMake.
2. **Get the source, with its submodules** (snesrecomp, and LakeSnes inside it, which has the GSU-2), and SDL2 through a vcpkg kept in `deps\`:
   ```
   git clone --recursive https://github.com/sp00nznet/stuntrace.git
   cd stuntrace
   git clone https://github.com/microsoft/vcpkg deps\vcpkg
   deps\vcpkg\bootstrap-vcpkg.bat -disableMetrics
   deps\vcpkg\vcpkg install sdl2:x64-windows
   ```
   If you cloned without `--recursive`: `git submodule update --init --recursive`.
   Check it worked: `ext\snesrecomp\ext\LakeSnes\snes\gsu.c` must exist.
3. **Build** (from a *Developer PowerShell for VS 2022*):
   ```
   cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=deps/vcpkg/scripts/buildsystems/vcpkg.cmake
   cmake --build build --config Release --parallel
   ```
4. **Add your ROM.** Put your own dump in the `stuntrace` folder as `Stunt Race FX (USA).sfc`
   (see "ROM required" below).
5. **Run:**
   ```
   build\Release\srf_launcher.exe "Stunt Race FX (USA).sfc"
   ```
   Expected start of the output:
   ```
   Loaded LoROM+SuperFX rom (NTSC)
   "Stunt Race FX        "
   Super FX GSU-2 initialized (64 KB work RAM)
   snesrecomp: ROM loaded successfully
   Super FX GSU-2 coprocessor detected!
   Recompiled functions registered.
   Run mode: realframe
   ```

Trip-ups: `SDL2.dll` missing → vcpkg puts it next to the exe when the toolchain
file is passed; if you configured without it, delete `build` and configure again.
`gsu.c` missing → the submodules weren't fetched (step 2).

### ROM required

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


## Usage

```
srf_launcher.exe "Stunt Race FX (USA).sfc"                         # windowed
srf_launcher.exe "Stunt Race FX (USA).sfc" --headless              # no window
srf_launcher.exe "Stunt Race FX (USA).sfc" --headless --record out.mp4   # needs ffmpeg on PATH
```

`--headless` and `--record` and the capture variables (`SNESRECOMP_SHOT_FRAMES`,
`SNESRECOMP_EXIT_FRAME`) come from snesrecomp ([docs/runtime.md](https://github.com/sp00nznet/snesrecomp/blob/main/docs/runtime.md)).
Example: `SNESRECOMP_SHOT_FRAMES=150 SNESRECOMP_EXIT_FRAME=200` writes the boot logo to `screenshot_0000.png`.

### Run modes

The launcher selects its execution model via the `SRF_MODE` environment variable:

| `SRF_MODE` | What runs | Use |
|------------|-----------|-----|
| `realframe` *(default)* | The genuine ROM on LakeSnes's cycle-accurate CPU (GSU-2 included). The whole game runs. The call-target profiler ranks the hottest JSR/JSL targets on exit (your best recompilation candidates). | Play the full game; find what to recompile next. |
| `intercept` | `realframe` **+ timed-recomp interception**: recompiled functions run *in place of* their ROM subroutines while everything else stays on the timed CPU. Coverage grows function-by-function toward 100% native. | The incremental static-recompilation path. |
| `shells` | Legacy hand-stitched frame model (manual NMI + main-loop dispatch per frame). | Regression only. |

Helper env vars:

- `SRF_MAX_FRAMES=N`: stop after N frames, so the profiler and checksum print on exit.
- `SRF_INTERCEPT="addr:l,addr:s,..."`: override the intercept set for per-function bisection (`l` = JSL/RTL, `s` = JSR/RTS).
- `SRF_VALIDATE="addr:l,..."`: lockstep-validate those routines against the ROM in any mode.
- `SRF_PROFILE=N`: how many profiler rows to print to stderr (default 30).

### Scripted input

snesrecomp's `SNESRECOMP_PAD_SCRIPT` drives the joypad in a headless run. This one goes from boot into a Speed Trax race and holds B to drive, with a screenshot mid-race:

```
set SNESRECOMP_PAD_SCRIPT=450:START 600:START 720:START 840:START 960:START 1080:START 1200:START 1320:START 1500:START 1650:START 1800:START 1950-3000:B
set SNESRECOMP_SHOT_FRAMES=2300
set SRF_MAX_FRAMES=2400
build\Release\srf_launcher.exe "Stunt Race FX (USA).sfc" --headless
```

### Conformance

```
py tools/conformance.py "Stunt Race FX (USA).sfc"
```

It plays that same tour twice. The first run, on the genuine ROM, lists every hand-ported routine the tour calls. The second runs each of them under snesrecomp's lockstep validation. On every call the C body runs on a copy of the machine and its memory is compared with what the ROM did. It prints `conformance: P/N pass`, writes per-routine lines to `scratch/conf/results.txt`, and exits 1 if the pass count falls below `tools/conformance_baseline.txt`. Without the ROM it prints a SKIP line, as it does in CI. `scratch/conf/verified.txt` lists the passing routines in `SRF_INTERCEPT` format; they are the candidates for `k_intercepts`.

### Verifying a recompiled function is faithful

An intercepted function must reproduce genuine execution bit-for-bit. The workflow:

1. **Profile** — `SRF_MODE=realframe SRF_MAX_FRAMES=N` and read the hottest targets.
2. **Determine `is_long`** by scanning the ROM for the call opcode (JSL `22` = cross-bank/RTL, JSR `20` = in-bank/RTS) — never guess.
3. **Bisect** — `SRF_MODE=intercept SRF_INTERCEPT=<addr>:<l|s>` and compare the printed **WRAM checksum** to the pure-`realframe` reference. Match ⇒ faithful ⇒ promote it into `k_intercepts` in `main.c`.

**Verified faithful drop-in so far:** `$0B:B4C3` (entity allocator) — WRAM checksum identical to genuine execution over 300 frames. `intercept` mode is byte-for-byte identical to `realframe` with the current verified set.


## Building from source

As in Getting Started. The netlab farm cross-builds it with mingw-w64 (recipe
`projects/stuntrace.env` in recomp-netlab):

```
cmake -S . -B build -G Ninja -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_PREFIX_PATH=<SDL2 mingw devel folder>
cmake --build build
```

## Documentation

- [docs/architecture.md](docs/architecture.md): how it fits together, the Super FX, rendering pipeline, WRAM map, project layout.
- [docs/super-fx.md](docs/super-fx.md): why the game stopped after the Nintendo logo, and what the rewritten GSU-2 core does.
- [docs/functions.md](docs/functions.md): every recompiled routine, by subsystem.
- [ROADMAP.md](ROADMAP.md), [CHANGELOG.md](CHANGELOG.md), [CONTRIBUTING.md](CONTRIBUTING.md).

## Related Projects

This is part of the [sp00nznet](https://github.com/sp00nznet) recompilation universe:

| Project | Platform | Game |
|---------|----------|------|
| **stuntrace** | SNES + Super FX | Stunt Race FX (1994) |
| [mk](https://github.com/sp00nznet/mk) | SNES | Super Mario Kart (1992) |
| [snesrecomp](https://github.com/sp00nznet/snesrecomp) | SNES | The toolkit (GSU-2 via its LakeSnes fork) |
| [mariopaint](https://github.com/sp00nznet/mariopaint) | SNES | Mario Paint (1992) |
| [LinksAwakening](https://github.com/sp00nznet/LinksAwakening) | Game Boy Color | Link's Awakening DX |
| [diddykongracing](https://github.com/sp00nznet/diddykongracing) | N64 | Diddy Kong Racing |
| [burnout3](https://github.com/sp00nznet/burnout3) | Xbox | Burnout 3: Takedown |

## Contributors

See [CONTRIBUTORS.md](CONTRIBUTORS.md).

## Legal

This project contains no ROM data. You supply your own dump; anything generated
from it stays on your machine and is never distributed.

## License

MIT, see [LICENSE](LICENSE).

---

*"The Super FX chip was the future. It's 2026 and we're still not over it."*
