# Roadmap

## Next

- **Get past the Nintendo logo.** A headless run of the current build draws the
  logo (frames ~74-192) and then a black screen through frame 6000. It is the
  same before and after the snesrecomp bump (identical WRAM checksum at frame
  1500), so it predates it. Find out whether the GSU is being stepped in the
  real-frame loop and whether the screen is really blank or just not presented.
- **Grow the verified intercept set** (`k_intercepts` in `src/main/main.c`):
  profile, bisect against the WRAM checksum, promote. Only `$0B:B4C3` is verified so far.
- **Conformance harness** that reports a pass count (per-function WRAM checksum
  match, the README's verification steps), with a baseline that fails on regression, like
  Mario Paint's `tools/conformance.py`.
- **Quick-start `Setup.cmd`** per the house rules (prerequisite checks, ROM
  prompt, build, launcher shortcut). Not written yet; Getting Started is the
  step-by-step route only.
- **Installer** through [recomp-launcher](https://github.com/sp00nznet/recomp-launcher)
  (`launcher/launcher.json`), as other titles are moving to.

## Not yet started

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

## Out of scope

- Distributing the ROM, anything extracted from it, or generated source.
