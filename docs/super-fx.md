# The Super FX (GSU-2) and the black screen

Until October 2026, the game drew the Nintendo logo and then showed a black
screen forever. This page records why, how it was found, and what the GSU core
does now. The code is in LakeSnes, `snes/gsu.c` (sp00nznet/LakeSnes#2).

## What was wrong

The first GSU program the game starts (bank `$01`, launched around frame 200)
never executed `STOP`. The old core ran each program to completion inside the
65816's write to R15, with a 200-million-cycle cap. The 65816 polled for
completion forever:

```
f210 cpu 7E:E20E gsu go=1 r15=89E6 pbr=01 cyc=200000000
f240 cpu 7E:E20E gsu go=1 r15=89E6 pbr=01 cyc=200000000
...
```

The cause was the opcode decode, which disagreed with fullsnes for a large
part of the map:

| Opcodes | Old core | Hardware |
|---|---|---|
| `20-2F` | FROM | WITH |
| `B0-BF` | OR | FROM |
| `C1-CF` | INC/DEC | OR/XOR |
| `D0-DE` | GETC/RAMB/ROMB | INC |
| `E0-EE` | JMP | DEC |
| `DF` / `EF` | GETB / JMP | GETC·RAMB·ROMB / GETB·GETBH·GETBL·GETBS |

It also plotted with a fixed 32-column layout, where hardware is column-major
(`(X/8)*height/8 + Y/8`). It truncated the RAM bank to 16 bits, let a ROM
refill overwrite code the 65816 had uploaded to the cache, and never raised
the STOP IRQ.

## What the core does now

Written from fullsnes ("SNES Cart GSU-n" chapters) only.

- **Pipeline.** A step fetches the byte after the opcode before executing it.
  Every jump (Bxx, JMP, LJMP, LOOP, or any opcode with R15 as destination)
  therefore runs the next byte first, and R15 reads as "address of the next
  opcode" (`LINK #n` and `MOVE R13,R15` depend on that).
- **Prefixes.** ALT1/2/3, TO, WITH and FROM survive into the next opcode. Bxx
  keeps them; everything else resets them. A missing ALT variant falls back
  ALT3 → ALT1 → plain.
- **Timing.** The GSU is ticked from `snes_runCycle` with an approximate cost
  of 1 clock per cached byte and 3 or 5 clocks per uncached ROM/RAM byte. The
  65816 really runs while the GSU draws:
  - CPU reads of ROM return the fixed vector bytes while `SCMR.RON` is set, so
    the game's IRQ/NMI handlers live in WRAM (`$0108`/`$010C`).
  - `STOP` raises the CPU IRQ (unless `CFGR.7` masks it), ORed with the PPU's
    H/V IRQ.
- **Not modeled:** ROM/RAM wait states, and stalls while RON/RAN are cleared.
  Nothing in this game has needed them so far.

## How it was found

Headless runs with a per-frame dump of the CPU PC and the GSU state
(`go`, `r15`, `pbr`) showed the GSU stuck with `go=1`.

A second trap came after the fix. The display looked blanked at every frame
boundary (`INIDISP=$80`), but that is normal for this game: its IRQ handler
forces blank at line 224 for its DMA and turns the screen back on around line
238. The screenshots were the reliable check.

## Frame interrupt

The game drives its frame from the H/V timer IRQ, not NMI:

- `$0108` (NMI) is `RTI`.
- `$010C` (IRQ) is `JSL $7E:A2D9`.
- The main loop waits at `$0B:E390` for the handler to clear `$11D8`.
