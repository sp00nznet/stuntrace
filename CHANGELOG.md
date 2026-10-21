# Changelog

All notable changes to this project. Format: [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versions follow SemVer once the first one is tagged (none yet).

## [Unreleased]

### Added
- `--headless` and `--record out.mp4` (from snesrecomp): run with no window,
  and optionally pipe frames to ffmpeg.
- `CHANGELOG.md`, `ROADMAP.md`, `CONTRIBUTING.md`, `CONTRIBUTORS.md`, `docs/`
  (architecture and the recompiled-function tables moved out of the README),
  a boot screenshot, and a CI build on push.

### Changed
- snesrecomp bumped from 7a10459 to b73fdcd (headless/record, lockstep
  validation, autogen, LakeSnes mouse fixes). WRAM checksum after 1500
  realframe frames is unchanged (55CB4232).
- README rewritten in the house order, with the status as checked today: the
  boot reaches the Nintendo logo, then a black screen.

### Fixed
- Clean clones build again. The old snesrecomp pin pointed LakeSnes at a commit
  that no longer existed upstream, so `gsu.c`/`gsu.h` were missing (#1, reported
  by @Bluekey007). The current pins all resolve.
- mingw-w64 builds: the project now enables CXX (snesrecomp links Dear ImGui),
  and `main.c` includes `<SDL.h>` so SDL2main finds `SDL_main`.
