/*
 * Stunt Race FX — Attract mode & per-frame game logic
 *
 * $02:E0A9 — Per-frame dispatch (manages fade timing)
 * $02:D7CD — Attract mode frame body (runs every frame)
 *
 * The attract mode body performs each frame:
 * 1. Save/clear frame counter
 * 2. Read object table from WRAM $7E:204F for current state
 * 3. Set up camera/position data ($02:DAD6)
 * 4. Run object processing ($08:C5A5)
 * 5. Launch GSU rendering program ($7E:E1F5 with A=$01, X=$BBC7)
 * 6. Run palette/display updates ($03:D388, $02:DB59, etc.)
 * 7. Launch second GSU pass if needed ($7E:E1F5 with X=$D307)
 * 8. Update frame timing (60fps → seconds counter)
 * 9. Write frame count to GSU RAM $70:108E for sync
 */

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * $02:E0A9 — Per-frame dispatch
 *
 * Manages the fade-in/out transition. Checks $08FB:
 * - If $08FB == $FF: normal frame, clear flag and return
 * - Otherwise: set up fade object at $059D with timing from $08FB
 */
void srf_02E0A9(void) {
    op_php();
    op_sep(0x20);
    op_rep(0x10);

    uint8_t fade_state = bus_wram_read8(0x08FB);

    if ((uint8_t)(fade_state + 1) == 0) {
        /* $08FB == $FF → normal frame, no fade active */
        op_sep(0x20);

        /* Clear fade flag in $05D5 */
        uint8_t d5 = bus_wram_read8(0x05D5);
        bus_wram_write8(0x05D5, d5 & 0xFD);

        op_plp();
        return;
    }

    /* Fade is active — set up fade object */
    op_sep(0x20);
    op_rep(0x10);

    /* Set flag bit 1 in $05D5 */
    uint8_t flags = bus_wram_read8(0x05D5);
    bus_wram_write8(0x05D5, flags | 0x02);

    /* Configure fade object at DP-relative $059D */
    uint16_t obj_base = 0x059D;
    bus_wram_write8(obj_base + 4, 0x00);      /* clear state */
    bus_wram_write8(obj_base + 5, 0x02);      /* type = 2 (fade) */
    bus_wram_write8(obj_base + 6, 0xA7);      /* handler low */

    uint8_t counter = bus_wram_read8(obj_base + 7);

    if (fade_state == 0) {
        /* Starting fade */
        uint8_t init_flag = bus_read8(0x7E, 0xFF65);
        if (init_flag == 0) {
            /* First-time init */
            bus_write8(0x7E, 0xFF65, 1);
            bus_wram_write8(obj_base + 4, 0x00);
            bus_wram_write8(obj_base + 6, 0xE7);
            bus_wram_write8(obj_base + 0, 0xFF);
        } else {
            if (counter == 0) {
                /* Fade complete */
                bus_wram_write8(0x08FB, fade_state - 1);
                op_plp();
                return;
            }
            counter -= 2;
            bus_wram_write8(obj_base + 7, counter);
        }
    } else {
        /* Ongoing fade */
        bus_wram_write8(obj_base + 4, 0x00);
        bus_wram_write8(obj_base + 6, 0xE7);
        bus_wram_write8(obj_base + 0, 0xFF);
    }

    bus_wram_write8(0x08FB, fade_state - 1);
    op_plp();
}

/*
 * $02:D7CD — Attract mode frame body
 *
 * Called every frame during the title/attract sequence.
 * This is the main rendering loop body.
 */
void srf_02D7CD(void) {
    op_php();
    op_sep(0x20);
    op_rep(0x10);

    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x02;  /* PHK/PLB */

    /* Save and clear frame counter */
    uint8_t fc = bus_wram_read8(0x0306);
    bus_wram_write8(0x06D4, fc);
    bus_wram_write8(0x0306, 0x00);

    /* DP $00 = $02 (processing mode) */
    bus_wram_write8(0x0000, 0x02);

    /* Read object index from state table */
    op_rep(0x30);
    uint16_t state_idx = bus_wram_read16(0x0346);
    uint16_t obj_addr = bus_read16(0x7E, 0x204F + state_idx);
    uint16_t obj_ptr = bus_read16(0x7E, 0x204F + obj_addr);
    g_cpu.X = obj_ptr;

    /* $02:DAD6 — Camera setup from object data */
    srf_02DAD6();

    /* Clear render flag */
    op_sep(0x20);
    bus_wram_write8(0x1F07, 0x00);

    /* Write sentinel values to GSU RAM */
    bus_write8(0x70, 0x246C, 0xFF);
    bus_write8(0x70, 0x246D, 0xFF);

    /* Launch GSU program: main 3D render pass */
    /* $7E:E221 — another GSU-related routine in WRAM */
    /* For now we call the standard launcher */
    op_sep(0x20);
    op_rep(0x10);
    CPU_SET_A8(0x01);
    g_cpu.X = 0xBBC7;
    srf_GSU_launch();

    /* Object/animation processing */
    op_sep(0x20);
    op_rep(0x10);
    srf_03D388();

    /* Palette/color updates */
    srf_02DB59();   /* GSU palette program ($01:AEF8) */
    srf_02D55F();   /* palette fade interpolation */
    srf_02D53D();   /* palette copy from ROM table */

    /* Post-GSU processing */
    op_sep(0x20);
    op_rep(0x10);

    /* Set render-done flag */
    bus_wram_write8(0x1F07, 0x01);

    /* Clear animation counter */
    op_rep(0x20);
    bus_wram_write16(0x1AE8, 0x0000);

    /* Camera angle calculation */
    srf_03D306();

    /* Second GSU pass (if $70:2732 is non-zero) */
    op_rep(0x20);
    uint16_t second_pass = bus_read16(0x70, 0x2732);
    if (second_pass != 0) {
        op_sep(0x20);
        CPU_SET_A8(0x01);
        g_cpu.X = 0xD307;
        srf_GSU_launch();
    }

    /* Frame timing: accumulate frame count into seconds */
    op_sep(0x20);
    uint8_t fc_cur = bus_wram_read8(0x0306);
    bus_wram_write8(0x1F3E, fc_cur);

    uint8_t acc = bus_wram_read8(0x06D5);
    acc += bus_wram_read8(0x06D4);
    bus_wram_write8(0x06D5, acc);

    uint8_t fps_cnt = bus_wram_read8(0x06D6);
    bus_wram_write8(0x06D6, fps_cnt + 1);

    /* Check if we've accumulated 60 frames (1 second) */
    if (acc >= 0x3C) {
        bus_wram_write8(0x06D7, bus_wram_read8(0x06D6));
        bus_wram_write8(0x06D6, 0x00);
        bus_wram_write8(0x06D5, acc - 0x3C);
    }

    /* Write frame count to GSU RAM for sync */
    op_sep(0x20);
    bus_write8(0x70, 0x108E, bus_wram_read8(0x0306));
    bus_write8(0x70, 0x108F, 0x00);

    g_cpu.DB = saved_db;
    op_plp();
}
