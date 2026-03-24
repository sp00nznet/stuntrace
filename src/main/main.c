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
    printf("Executing reset vector ($00:FE88)...\n");
    srf_00FE88();

    /* ── frame loop ─────────────────────────────────────── */
    printf("Entering frame loop.\n");
    int frame_count = 0;
    while (snesrecomp_begin_frame()) {
        /* trigger VBlank so NMI flags are set correctly */
        snesrecomp_trigger_vblank();

        /* run NMI handler (the real one from $02:8000) */
        srf_028000();

        /* run one iteration of game logic */
        srf_038C63();

        /* diagnostic logging for first few frames */
        if (frame_count == 0) {
            /* Dump key GSU RAM areas on first frame */
            printf("GSU RAM dump (control):\n");
            for (int blk = 0; blk < 0x100; blk += 16) {
                /* Only print non-zero lines */
                int has_data = 0;
                for (int b = 0; b < 16; b += 2)
                    if (bus_read16(0x70, blk + b) != 0) has_data = 1;
                if (has_data) {
                    printf("  $70:%04X:", blk);
                    for (int b = 0; b < 16; b += 2)
                        printf(" %04X", bus_read16(0x70, blk + b));
                    printf("\n");
                }
            }
            /* Key control addresses */
            printf("GSU ctrl: $01C4=%04X $01D0=%04X $108C=%02X $238E=%04X $2396=%04X\n",
                   bus_read16(0x70, 0x01C4), bus_read16(0x70, 0x01D0),
                   bus_read8(0x70, 0x108C), bus_read16(0x70, 0x238E),
                   bus_read16(0x70, 0x2396));
            /* Count non-zero GSU RAM regions */
            int total_nz = 0;
            printf("GSU RAM regions with data:\n");
            for (int region = 0; region < 0x10000; region += 0x100) {
                int nz = 0;
                for (int a = 0; a < 0x100; a += 2)
                    if (bus_read16(0x70, region + a) != 0) nz++;
                if (nz > 0) {
                    printf("  $70:%04X: %d words\n", region, nz);
                    total_nz += nz;
                }
            }
            printf("Total non-zero: %d words\n", total_nz);
        }
        if (frame_count < 10 || (frame_count % 300 == 0)) {
            uint8_t nmi_state = bus_wram_read8(0x0D3F);
            uint8_t brightness = bus_wram_read8(0x0D61);
            uint8_t display_mode = bus_wram_read8(0x0D62);
            uint8_t screen_reg = bus_read8(0x00, 0x2100);
            uint8_t bg_mode = bus_read8(0x00, 0x2105);
            uint8_t main_scr = bus_read8(0x00, 0x212C);
            uint8_t hdma = bus_read8(0x00, 0x420C);
            uint16_t fade = bus_wram_read16(0x0D60);
            /* Check multiple GSU RAM locations for data */
            uint16_t gsu_fb0 = bus_read16(0x70, 0x3000);
            uint16_t gsu_fb1 = bus_read16(0x70, 0x3080);
            uint16_t wram_fb = bus_read16(0x7F, 0x63D7);
            /* Check GSU object/control areas */
            uint16_t gsu_ctrl = bus_read16(0x70, 0x0040);
            uint16_t gsu_e4 = bus_read16(0x70, 0x00E4);  /* rotation matrix output */
            uint16_t gsu_68 = bus_read16(0x70, 0x0068);  /* tile decomp param */
            printf("Frame %d: NMI=$%02X bright=$%02X $2100=$%02X "
                   "$2105=$%02X $212C=$%02X "
                   "FB[$3000]=$%04X [$3080]=$%04X WRAM=$%04X "
                   "ctrl=$%04X E4=$%04X p68=$%04X\n",
                   frame_count, nmi_state, brightness, screen_reg,
                   bg_mode, main_scr,
                   gsu_fb0, gsu_fb1, wram_fb,
                   gsu_ctrl, gsu_e4, gsu_68);
        }
        frame_count++;

        /* render + present */
        snesrecomp_end_frame();
    }

    snesrecomp_shutdown();
    printf("Shutdown complete.\n");
    return 0;
}
