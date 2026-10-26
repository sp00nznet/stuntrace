# Roadmap

## Next

- **Play every mode through.** Stunt Trax, Battle Trax (2P) and Test Run are
  untried, and no race has been driven to the finish yet. Extend the scripted
  tour so conformance covers them.
- **Grow the verified intercept set** (`k_intercepts` in `src/main/main.c`).
  `scratch/conf/verified.txt` from `tools/conformance.py` lists the hand-ports
  that pass lockstep validation; promote them, then fix the failing ones
  (`scratch/conf/results.txt` names the first mismatch of each).
- **GSU timing.** The new core's clock costs are approximate (no ROM/RAM wait
  states), so the frame rate may differ from hardware. Measure it against real
  hardware footage before tuning.
- **Lift the GSU programs.** The 3D renderer still runs on the GSU emulation;
  recompiling GSU code is a toolkit feature snesrecomp doesn't have yet.
- **Release:** a first tag, and a build through recomp-launcher.

## Not yet hand-ported to C (these run on the genuine ROM today)

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
