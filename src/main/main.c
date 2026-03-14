/*
 * Stunt Race FX — Launcher
 *
 * Initialises snesrecomp (LakeSnes + SDL2), loads the ROM,
 * registers recompiled functions, and runs the frame loop.
 */

#include <stdio.h>
#include <stdlib.h>

#include <snesrecomp/snesrecomp.h>
#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>

#include <srf/functions.h>

static const char *find_rom_path(int argc, char **argv) {
    if (argc > 1) return argv[1];
    return "Stunt Race FX (USA).sfc";
}

int main(int argc, char **argv) {
    printf("=== Stunt Race FX — Static Recompilation ===\n");
    printf("    snesrecomp + LakeSnes backend\n\n");

    /* ── platform init (scale = 3 → 768x672 window) ──── */
    if (!snesrecomp_init("Stunt Race FX", 3)) {
        fprintf(stderr, "snesrecomp_init failed\n");
        return 1;
    }

    /* ── load ROM ───────────────────────────────────────── */
    const char *rom = find_rom_path(argc, argv);
    printf("Loading ROM: %s\n", rom);
    if (!snesrecomp_load_rom(rom)) {
        fprintf(stderr, "Failed to load ROM: %s\n", rom);
        return 1;
    }

    /* ── check for GSU ──────────────────────────────────── */
    if (bus_has_gsu()) {
        printf("Super FX GSU-2 coprocessor detected!\n");
    }

    /* ── register recompiled functions ──────────────────── */
    srf_register_all();
    printf("Recompiled functions registered.\n");

    /* ── boot ───────────────────────────────────────────── */
    printf("Executing reset vector...\n");
    srf_008000();

    /* ── frame loop ─────────────────────────────────────── */
    printf("Entering frame loop.\n");
    while (snesrecomp_begin_frame()) {
        /* clear NMI-done flag */
        bus_wram_write8(0x0044, 0x00);

        /* run NMI handler */
        srf_008018();

        /* run one iteration of game logic */
        srf_0080C0();

        /* render + present */
        snesrecomp_end_frame();
    }

    snesrecomp_shutdown();
    printf("Shutdown complete.\n");
    return 0;
}
