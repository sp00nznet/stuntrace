# Changelog

All notable changes to this project. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow SemVer once the first one is tagged (none yet).

## [Unreleased]

### Added
- `tools/conformance.py`: plays a scripted title-to-race tour twice. The first
  run finds the hand-ported routines it reaches; the second checks each one
  against the ROM call by call with lockstep validation. It reports a pass
  count and fails on a drop below `tools/conformance_baseline.txt`. CI runs it
  (it SKIPs without the ROM).
- `SRF_VALIDATE` and `SRF_PROFILE` in `main.c` (validation list, profiler depth).
- Quick start: `Setup.cmd` / `tools/setup.ps1` check the tools, find the ROM by
  MD5 (`.sfc`, `.smc` or inside a `.zip`), fetch snesrecomp and SDL2, build,
  and leave `Stunt Race FX (recomp).cmd`. `launcher/launcher.json` for
  recomp-launcher.
- `docs/super-fx.md`: why the game stopped after the Nintendo logo; screenshots
  of the title, menus and a race.
- `--headless` and `--record out.mp4` (from snesrecomp): run with no window,
  and optionally pipe frames to ffmpeg.
- `CHANGELOG.md`, `ROADMAP.md`, `CONTRIBUTING.md`, `CONTRIBUTORS.md`, `docs/`
  (architecture and the recompiled-function tables moved out of the README),
  a boot screenshot, and a CI build on push.

### Changed
- snesrecomp bumped for sp00nznet/snesrecomp#10 (`SNESRECOMP_PAD_SCRIPT`) and
  #11 (LakeSnes#2 and #3). This removes the last Snes9x-derived code
  (LakeSnes's DSP-1, compiled into every build though this game never used
  it; mitaudit#2).
- README Getting Started has both routes; Step by step keeps vcpkg in `deps\`
  like the script does.
- snesrecomp bumped from 7a10459 to b73fdcd (headless/record, lockstep
  validation, autogen, LakeSnes mouse fixes). WRAM checksum after 1500
  realframe frames is unchanged (55CB4232).
- README rewritten in the house order, with the status as checked today: the
  boot reaches the Nintendo logo, then a black screen.

### Fixed
- The game gets past the Nintendo logo: title, Game Selection, Car Selection
  and a Speed Trax race all run. The Super FX core in LakeSnes mis-decoded
  much of the GSU opcode map, so the first GSU program never stopped
  (sp00nznet/LakeSnes#2, docs/super-fx.md).
- Clean clones build again. The old snesrecomp pin pointed LakeSnes at a commit
  that no longer existed upstream, so `gsu.c`/`gsu.h` were missing (#1, reported
  by @Bluekey007). The current pins all resolve.
- mingw-w64 builds: the project now enables CXX (snesrecomp links Dear ImGui),
  and `main.c` includes `<SDL.h>` so SDL2main finds `SDL_main`.
