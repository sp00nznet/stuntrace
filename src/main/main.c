/*
 * Stunt Race FX — Launcher
 *
 * Three run modes (select with the SRF_MODE env var):
 *
 *   realframe  (default) — run the genuine ROM on LakeSnes's cycle-accurate
 *                          CPU (full hardware fidelity, GSU-2 3D included).
 *                          The whole game runs: title, attract, menus, race.
 *                          The call-target profiler is enabled and dumps the
 *                          hottest JSR/JSL targets on exit — these are the
 *                          best next candidates for static recompilation.
 *
 *   intercept            — realframe + timed-recomp interception: every
 *                          recompiled function registered in the func_table
 *                          runs *in place of* its original ROM subroutine
 *                          while everything else keeps running on the timed
 *                          CPU. This is the incremental path from a real-frame
 *                          game to a fully static-recompiled one: coverage
 *                          grows function-by-function toward 100% native.
 *
 *   shells               — legacy hand-stitched frame model: manually call the
 *                          NMI handler + main-loop dispatch each frame. Kept
 *                          for regression against the pre-migration behaviour.
 *
 * The interception model is snesrecomp's answer to N64Recomp-style static
 * recompilation: instead of requiring 100% AOT coverage before anything runs,
 * the real hardware fills every gap so the game is playable from day one and
 * each recompiled subroutine is verifiable against genuine execution.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <snesrecomp/snesrecomp.h>
#include <SDL.h>   /* SDL2main needs main renamed to SDL_main */
#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <snesrecomp/func_table.h>

#include <srf/functions.h>

static const char *find_rom_path(int argc, char **argv) {
    if (argc > 1) return argv[1];
    return "Stunt Race FX (USA).sfc";
}

/* Optional headless frame cap: SRF_MAX_FRAMES=N stops after N frames so a
 * non-interactive run terminates cleanly (and dumps the profiler). 0 = run
 * until the window is closed. */
static unsigned long frame_cap(void) {
    const char *e = getenv("SRF_MAX_FRAMES");
    return e ? strtoul(e, NULL, 10) : 0;
}

/*
 * Recompiled entry points to expose to the timed-recomp interception hook.
 *
 * is_long = true  → the original subroutine returns via RTL (entered by JSL,
 *                   typically a cross-bank call). The hook pops 3 bytes + PBR.
 * is_long = false → returns via RTS (entered by JSR, in-bank). Pops 2 bytes.
 *
 * Getting is_long wrong corrupts the simulated return, so this list is grown
 * deliberately: run `SRF_MODE=realframe` first, read the profiler's top JSR/JSL
 * targets (it reports the call type), and add only entries confirmed both hot
 * and faithfully recompiled. Boot/interrupt entries (reset $00FE88, NMI
 * $028000) and the JMP-based main loop ($038C63) are intentionally NOT
 * intercepted — they are not clean JSR/JSL subroutines.
 */
typedef struct { uint32_t addr; bool is_long; const char *name; } Intercept;

static const Intercept k_intercepts[] = {
    /* VERIFIED-FAITHFUL set: each reproduces genuine execution bit-for-bit
     * (WRAM checksum identical to the pure real-frame reference over 300+
     * frames). is_long confirmed by scanning ROM call sites (JSL 0x22 vs
     * JSR 0x20). Grow this list by: (1) profile (SRF_MODE=realframe) to find a
     * hot target, (2) ROM-scan its call op for is_long, (3) SRF_INTERCEPT it
     * alone and compare the WRAM checksum to the reference — add here only when
     * they match. Known-diverging (need per-function fixing before promotion):
     *   $0BB64A sprite compositor (JSL) — path runs, WRAM differs
     *   $0BB450 entity dispatcher  (JSL) — breaks the entity loop */
    { 0x0BB4C3, false, "entity allocator (JSR near)" },
    { 0x02DF79, true,  "PRNG 32-bit LFSR (JSL)" },
};
enum { k_intercept_count = sizeof(k_intercepts) / sizeof(k_intercepts[0]) };

/* ── legacy hand-stitched frame model ─────────────────────────── */
static int run_shells(void) {
    printf("Executing reset vector ($00:FE88)...\n");
    srf_00FE88();
    printf("Entering shell frame loop.\n");
    int frame = 0;
    while (snesrecomp_begin_frame()) {
        snesrecomp_trigger_vblank();
        srf_028000();   /* real NMI handler ($02:8000) */
        srf_038C63();   /* one iteration of main-loop dispatch */
        frame++;
        snesrecomp_end_frame();
    }
    printf("Shell loop exited after %d frames.\n", frame);
    return 0;
}

/* ── real-frame model (optionally with timed interception) ────── */
static int run_realframe(bool intercept) {
    if (intercept) {
        /* SRF_INTERCEPT overrides the built-in list for per-function bisection:
         * "addr:l,addr:s,..." (l=JSL/RTL, s=JSR/RTS). e.g.
         *   SRF_INTERCEPT=0BB4C3:s   intercepts only the entity allocator. */
        const char *ov = getenv("SRF_INTERCEPT");
        if (ov && *ov) {
            char buf[512]; strncpy(buf, ov, sizeof(buf) - 1); buf[sizeof(buf) - 1] = 0;
            int n = 0;
            for (char *tok = strtok(buf, ","); tok; tok = strtok(NULL, ",")) {
                unsigned addr = 0; char kind = 'l';
                sscanf(tok, "%x:%c", &addr, &kind);
                bool is_long = (kind != 's' && kind != 'S');
                recomp_timed_add_intercept(addr, is_long);
                printf("  intercept: $%06X  %-4s  (SRF_INTERCEPT)\n",
                       addr, is_long ? "RTL" : "RTS");
                n++;
            }
            printf("Timed-recomp interception enabled (%d entries, override).\n", n);
        } else {
            for (int i = 0; i < k_intercept_count; i++) {
                recomp_timed_add_intercept(k_intercepts[i].addr, k_intercepts[i].is_long);
                printf("  intercept: $%06X  %-4s  %s\n",
                       k_intercepts[i].addr,
                       k_intercepts[i].is_long ? "RTL" : "RTS",
                       k_intercepts[i].name);
            }
            printf("Timed-recomp interception enabled (%d entries).\n", k_intercept_count);
        }
    }

    /* Profiler ranks the hottest JSR/JSL targets so we know what to recompile
     * next. Both profiling and interception run inside the CPU opcode-fetch
     * hook, so the hook must be installed for either to do anything — even with
     * an empty intercept set it simply profiles and intercepts nothing. */
    recomp_timed_profile_enable();
    recomp_timed_recomp_enable();

    unsigned long cap = frame_cap();
    printf("Entering real-frame loop (genuine ROM via LakeSnes)%s%s.\n",
           intercept ? " + interception" : "",
           cap ? " [frame-capped]" : "");
    unsigned long frame = 0;
    while (snesrecomp_realframe_begin()) {
        snesrecomp_realframe_end();
        frame++;
        if (cap && frame >= cap) break;
    }

    printf("\nReal-frame loop exited after %lu frames.\n", frame);
    printf("WRAM checksum: %08X\n", snesrecomp_wram_checksum());
    if (intercept)
        printf("Interception hits: %lu\n", recomp_timed_intercept_hits());

    /* Divergence check: SRF_SNAPSHOT=path dumps WRAM+VRAM at loop exit. Run
     * realframe (reference) and intercept to two paths and diff them
     * (tools/diff_snapshots.py) — identical => the intercepted functions
     * faithfully reproduce genuine execution. */
    const char *snap = getenv("SRF_SNAPSHOT");
    if (snap && *snap) {
        snesrecomp_dump_snapshot(snap);
        printf("Snapshot written: %s\n", snap);
    }
    printf("\n=== Hottest recompilation targets (profiler) ===\n");
    recomp_timed_profile_dump(30);
    return 0;
}

int main(int argc, char **argv) {
    argc = snesrecomp_parse_args(argc, argv);   /* --headless, --record out.mp4 */
    printf("=== Stunt Race FX — Static Recompilation ===\n");
    printf("    snesrecomp + LakeSnes backend (GSU-2)\n\n");

    if (!snesrecomp_init("Stunt Race FX", 3)) {
        fprintf(stderr, "snesrecomp_init failed\n");
        return 1;
    }

    const char *rom = find_rom_path(argc, argv);
    printf("Loading ROM: %s\n", rom);
    if (!snesrecomp_load_rom(rom)) {
        fprintf(stderr, "Failed to load ROM: %s\n", rom);
        return 1;
    }
    if (bus_has_gsu())
        printf("Super FX GSU-2 coprocessor detected!\n");

    /* Register every recompiled function at its SNES address. Used by the
     * interception hook and by any recompiled routine that dispatches through
     * func_table_call. Safe in all modes. */
    srf_register_all();
    printf("Recompiled functions registered.\n");

    const char *mode = getenv("SRF_MODE");
    if (!mode || !*mode) mode = "realframe";
    printf("Run mode: %s\n\n", mode);

    int rc;
    if (!strcmp(mode, "shells"))
        rc = run_shells();
    else if (!strcmp(mode, "intercept"))
        rc = run_realframe(true);
    else
        rc = run_realframe(false);   /* realframe (default) */

    snesrecomp_shutdown();
    printf("Shutdown complete.\n");
    return rc;
}
