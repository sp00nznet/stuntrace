"""Conformance harness: do the hand-ported routines match the ROM, call by call?

The tour is a fixed scripted joypad run (TOUR): boot, title, Game Selection,
Car Selection, the Speed Trax course menu, then a race with B held.

1. Oracle: the genuine ROM (SRF_MODE=realframe) plays the tour with the
   profiler on. Every JSR/JSL target it executed that has a hand-ported body
   registered is a candidate, with its call type.
2. The same tour once per candidate, with that routine under snesrecomp's
   lockstep validation (SRF_VALIDATE): on each call the native body runs on a
   copy of the machine, the genuine routine runs for real, and WRAM/VRAM/
   CGRAM/OAM are compared when it returns. Execution stays genuine. One process
   per routine, in parallel, each with a time limit: a body that waits for
   something the copied machine never does (an IRQ, the GSU finishing) hangs,
   and that fails only that routine.

PASS = called, no memory or M/X-width mismatch on any checked call, no call
skipped (a skipped call was not checked), and no hang. Other register
mismatches are reported, not failed.

  py tools/conformance.py <rom> [--exe path] [--update]

Prints "conformance: P/N pass". Per-function lines go to scratch/conf/results.txt.
tools/conformance_baseline.txt holds the pass count; the run exits 1 if it drops
(--update rewrites it). Without the ROM it prints a SKIP line and exits 0.
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
OUT = os.path.join(ROOT, "scratch", "conf")
BASELINE = os.path.join(HERE, "conformance_baseline.txt")

FRAMES = 2400
TOUR = ("450:START 600:START 720:START 840:START 960:START 1080:START 1200:START "
        "1320:START 1500:START 1650:START 1800:START 1950-2400:B")
# Frame drivers: their bodies wait for the frame IRQ (the wait-for-vblank at
# $0B:E390 spins until the IRQ handler clears $11D8). Not a translation check.
FRAME_DRIVERS = {"0BE390"}
VALIDATE_TIMEOUT = 600  # seconds; a plain tour takes about 30
EXES = ["build/Release/srf_launcher.exe", "build/Debug/srf_launcher.exe",
        "build-farm/srf_launcher.exe", "build/srf_launcher.exe", "build/srf_launcher"]
VALIDATE_RE = re.compile(
    r"^VALIDATE \w{6} calls=(\d+) mem_fail=(\d+) reg_fail=(\d+) mx_fail=(\d+) skipped=(\d+)(.*)$", re.M)
PROF_RE = re.compile(r"^PROF (\w{6}) +\d+ +\w\w (JSR|JSL) .*recompiled", re.M)


def run(exe, rom, env, timeout=None):
    """Play the tour; returns the stderr text, or None on a timeout."""
    e = dict(os.environ, SNESRECOMP_PAD_SCRIPT=TOUR, SRF_MAX_FRAMES=str(FRAMES), SRF_MODE="realframe", **env)
    try:
        r = subprocess.run([exe, rom, "--headless"], env=e, capture_output=True, text=True, errors="replace",
                           timeout=timeout)
    except subprocess.TimeoutExpired:
        return None
    if r.returncode != 0:
        sys.exit(f"FAIL: {exe} exited {r.returncode} ({' '.join(env)})\n{r.stdout[-2000:]}{r.stderr[-2000:]}")
    return r.stderr


def validate(exe, rom, addr, kind):
    err = run(exe, rom, {"SRF_VALIDATE": f"{addr}:{kind}", "SRF_PROFILE": "0", "SNESRECOMP_VALIDATE_MAX": "3000"},
              timeout=VALIDATE_TIMEOUT)
    if err is None:
        return addr, None
    m = VALIDATE_RE.search(err)
    return addr, m.groups() if m else ("0", "0", "0", "0", "0", " (no report)")


def main():
    args = sys.argv[1:]
    update = "--update" in args
    exe = None
    if "--exe" in args:
        exe = args[args.index("--exe") + 1]
        args = [a for a in args if a not in ("--exe", exe)]
    rest = [a for a in args if a != "--update"]
    rom = rest[0] if rest else os.path.join(ROOT, "Stunt Race FX (USA).sfc")
    if not os.path.exists(rom):
        print(f"SKIP conformance: no ROM at {rom} (supply your own dump)")
        return 0
    exe = exe or next((os.path.join(ROOT, p) for p in EXES if os.path.exists(os.path.join(ROOT, p))), None)
    if not exe:
        sys.exit("FAIL: no srf_launcher build found; build first or pass --exe")
    os.makedirs(OUT, exist_ok=True)

    err = run(exe, rom, {"SRF_PROFILE": "5000"})
    prof = [(a, "l" if k == "JSL" else "s") for a, k in PROF_RE.findall(err) if a not in FRAME_DRIVERS]
    if not prof:
        sys.exit("FAIL: the oracle run reached no hand-ported routine")

    workers = max(1, (os.cpu_count() or 2) // 2)
    with ThreadPoolExecutor(workers) as pool:
        rows = sorted(pool.map(lambda p: validate(exe, rom, *p), prof))

    passed, verified = 0, []
    kind = dict(prof)
    with open(os.path.join(OUT, "results.txt"), "w") as f:
        for addr, g in rows:
            if g is None:
                f.write(f"{addr} FAIL hang: no return within {VALIDATE_TIMEOUT}s on the copied machine\n")
                continue
            calls, mem, reg, mx, skip, first = g
            ok = int(calls) > 0 and int(mem) == 0 and int(mx) == 0 and int(skip) == 0
            passed += ok
            if ok:
                verified.append(f"{addr}:{kind[addr]}")
            f.write(f"{addr} {'PASS' if ok else 'FAIL'} calls={calls} mem_fail={mem} reg_fail={reg} "
                    f"mx_fail={mx} skipped={skip}{first}\n")
    # SRF_INTERCEPT format: the candidates for k_intercepts in main.c
    with open(os.path.join(OUT, "verified.txt"), "w") as f:
        f.write(",".join(sorted(verified)) + "\n")
    print(f"conformance: {passed}/{len(rows)} pass (details: scratch/conf/results.txt)")

    base = int(open(BASELINE).read().split()[0]) if os.path.exists(BASELINE) else 0
    if update or not os.path.exists(BASELINE):
        open(BASELINE, "w").write(f"{passed} {len(rows)}\n")
    elif passed < base:
        print(f"REGRESSION: {passed} < baseline {base}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
