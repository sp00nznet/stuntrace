# Contributing

Contributions are welcome, especially translations of 65816 routines and fixes to
existing ones. The bar for a recompiled function: run under `SRF_MODE=intercept`
it leaves the same WRAM checksum as the genuine ROM (`SRF_MODE=realframe`); the
README's "Verifying a recompiled function is faithful" has the steps.

## What never goes in a PR

- ROMs, dumps, or anything extracted from one (graphics, audio, tables).
- Recompiler output, disassembly listings, or symbol maps generated from the ROM.
  The tools ship; their output stays on your machine (`gen/` and `scratch/` are
  gitignored).
- Code from proprietary SDKs or leaked material.

## Where your code comes from

This project is MIT. Contributions must be your own work or under an
MIT-compatible licence. The usual way that goes wrong is porting a fix you saw in
a GPL project — bsnes, higan, snes9x (non-commercial licence), Mesen — which
relicenses it by accident and is very hard to untangle later. Reading those
projects to understand hardware is fine; copying their code is not. If a change
came from somewhere, say where in the PR.

## AI-assisted contributions

Welcome, provided a human understood and verified the change. "The model wrote
it" is not a reason it is right; the checksum match is.

## Toolkit changes

Anything hardware-level, harness-level or useful to other titles belongs in
[snesrecomp](https://github.com/sp00nznet/snesrecomp), not here. Open the PR
there and reference it from the game-side change (`needs snesrecomp#N`).

## Before you open a PR

1. Build (see the README).
2. Show the WRAM checksum for `realframe` and `intercept` with your change, at the same `SRF_MAX_FRAMES`.
3. If you touched a screen, attach a before/after from a headless run (`SNESRECOMP_SHOT_FRAMES`).
