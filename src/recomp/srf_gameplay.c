/*
 * Stunt Race FX — Gameplay frame body & scene management
 *
 * $0B:FB26 — Gameplay/menu frame body (2-player capable)
 * $03:865E — Scene reset (loads config from ROM tables)
 * $03:8683 — Scene config loader subroutine
 * $03:8C86 — Full game restart (WRAM clear + reinit)
 *
 * The gameplay frame body is similar to the attract body but
 * adds a second camera/render pass for player 2, audio sync,
 * and menu-specific processing.
 */

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * $0B:FB26 — Gameplay/menu frame body
 *
 * Called every frame when $0D62 != 0 (non-attract mode).
 * Processes both player viewports with dual GSU render passes.
 *
 * Flow:
 * 1. Save/clear frame counter
 * 2. Object/animation processing ($03:D388)
 * 3. P1 camera setup from state table ($02:DAD6)
 * 4. Object system update ($08:C5A5)
 * 5. GSU render pass #1 (program $BBC7) for P1 viewport
 * 6. Palette/color updates ($02:DB59, $02:D55F, $02:D53D)
 * 7. Camera angle calculation ($03:D306)
 * 8. P2 camera setup from $034A state table
 * 9. GSU render pass #2 via $7E:E258 for P2 viewport
 * 10. Audio sync ($7F:112F)
 * 11. Frame timing accumulation
 */
void srf_0BFB26(void) {
    op_php();
    op_sep(0x20);
    op_rep(0x10);

    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x0B;

    /* Save and clear frame counter */
    uint8_t fc = bus_wram_read8(0x0306);
    bus_wram_write8(0x06D4, fc);
    bus_wram_write8(0x0306, 0x00);

    /* Processing mode */
    bus_wram_write8(0x0000, 0x02);

    /* Object/animation processing */
    op_sep(0x20);
    op_rep(0x10);
    srf_03D388();

    /* ── Player 1 viewport ──────────────────────────── */
    op_rep(0x30);
    uint16_t p1_state = bus_wram_read16(0x0346);
    uint16_t p1_obj = bus_read16(0x7E, 0x204F + p1_state);
    p1_obj = bus_read16(0x7E, 0x204F + p1_obj);
    g_cpu.X = p1_obj;

    /* Camera setup from P1 object */
    op_rep(0x30);
    bus_wram_write16(0x00C5, bus_wram_read16(g_cpu.X + 8));
    bus_wram_write16(0x00C7, bus_wram_read16(g_cpu.X + 10));
    bus_wram_write16(0x00C9, bus_wram_read16(g_cpu.X + 12));
    bus_wram_write16(0x0664, bus_wram_read16(g_cpu.X + 14));
    bus_wram_write16(0x0666, bus_wram_read16(g_cpu.X + 16));
    bus_wram_write16(0x0668, bus_wram_read16(g_cpu.X + 18));

    /* Object system update */
    srf_08C5A5();

    /* Clear render flag, write sentinels to GSU RAM */
    op_sep(0x20);
    bus_wram_write8(0x1F07, 0x00);
    bus_write8(0x70, 0x246C, 0xFF);
    bus_write8(0x70, 0x246D, 0xFF);

    /* GSU render pass #1 — P1 viewport */
    op_sep(0x20);
    op_rep(0x10);
    CPU_SET_A8(0x01);
    g_cpu.X = 0xBBC7;
    srf_GSU_launch();

    /* Camera angle calculation */
    srf_03D306();

    /* ── Player 2 viewport ──────────────────────────── */
    op_rep(0x30);
    uint16_t p2_state = bus_wram_read16(0x034A);
    uint16_t p2_obj = bus_read16(0x7E, 0x204F + p2_state);
    p2_obj = bus_read16(0x7E, 0x204F + p2_obj);
    g_cpu.X = p2_obj;

    /* P2 camera setup */
    op_rep(0x30);
    bus_wram_write16(0x00C5, bus_wram_read16(g_cpu.X + 8));
    bus_wram_write16(0x00C7, bus_wram_read16(g_cpu.X + 10));
    bus_wram_write16(0x00C9, bus_wram_read16(g_cpu.X + 12));
    bus_wram_write16(0x0664, bus_wram_read16(g_cpu.X + 14));
    bus_wram_write16(0x0666, bus_wram_read16(g_cpu.X + 16));
    bus_wram_write16(0x0668, bus_wram_read16(g_cpu.X + 18));

    /* Set render-done flag for P2 */
    op_sep(0x20);
    bus_wram_write8(0x1F07, 0x01);

    /* Clear animation counter */
    op_rep(0x20);
    bus_wram_write16(0x1AE8, 0x0000);

    /* Audio sync — write to APU port for sound effect timing */
    op_rep(0x30);
    g_cpu.X = 0x2143;
    uint16_t audio_sync = bus_read16(0x70, 0x2476);
    /* $7F:112F handles the actual APU write */

    /* Camera angle for P2 */
    srf_03D306();

    /* Adjust P2 camera X offset for split-screen */
    op_rep(0x20);
    uint16_t cam_x = bus_wram_read16(0x06CF);
    cam_x -= 0x0140;  /* offset P2 viewport */
    bus_wram_write16(0x06CF, cam_x);

    /* Clear state */
    op_sep(0x20);
    bus_wram_write8(0x0357, 0x00);

    /* ── Frame timing ───────────────────────────────── */
    op_sep(0x20);
    uint8_t fc_cur = bus_wram_read8(0x0306);
    bus_wram_write8(0x1F3E, fc_cur);

    uint8_t acc = bus_wram_read8(0x06D5);
    acc += bus_wram_read8(0x06D4);
    bus_wram_write8(0x06D5, acc);

    uint8_t fps_cnt = bus_wram_read8(0x06D6);
    bus_wram_write8(0x06D6, fps_cnt + 1);

    if (acc >= 0x3C) {
        bus_wram_write8(0x06D7, bus_wram_read8(0x06D6));
        bus_wram_write8(0x06D6, 0x00);
        bus_wram_write8(0x06D5, acc - 0x3C);
    }

    /* Write frame count to GSU RAM */
    op_sep(0x20);
    bus_write8(0x70, 0x108E, bus_wram_read8(0x0306));
    bus_write8(0x70, 0x108F, 0x00);

    g_cpu.DB = saved_db;
    op_plp();
}

/*
 * $03:8683 — Scene configuration loader
 *
 * Reads scene parameters from ROM table at $03:8004+X:
 *   +$04: display config value → $18DB
 *   +$06: display mode index → $0E3D
 *
 * Then resets GSU, sets display mode $0D2B = 0,
 * SCMR base = $01, CLSR = high-speed, CFGR = $20, SCBR = $0B.
 * Calls $08:B893 to init viewport config and $03:D996 for title setup.
 */
void srf_038683(void) {
    op_rep(0x30);

    uint16_t table_idx = bus_wram_read16(0x1A57);

    /* Read display config from ROM table */
    uint16_t display_cfg = bus_read16(0x03, 0x8004 + table_idx);
    bus_wram_write16(0x18DB, display_cfg);

    uint16_t mode_idx = bus_read16(0x03, 0x8006 + table_idx);
    bus_wram_write16(0x0E3D, mode_idx);

    /* Set display params */
    bus_wram_write16(0x1A5B, 0x0020);
    bus_wram_write16(0x0D2B, 0x0000);

    /* Reset GSU configuration */
    op_sep(0x20);
    bus_wram_write8(0x0374, 0x01);  /* SCMR base */
    bus_write8(0x00, 0x3039, 0x01); /* CLSR = high speed */
    bus_write8(0x00, 0x3037, 0x20); /* CFGR */
    bus_write8(0x00, 0x3038, 0x0B); /* SCBR */

    /* Init viewport config */
    srf_08B893();

    /* Disable HDMA, force blank, disable interrupts */
    op_sep(0x20);
    bus_write8(0x00, 0x420C, 0x00);
    bus_write8(0x00, 0x2100, 0x80);
    OP_SEI();

    /* Title/attract setup */
    srf_03D996();
    g_cpu.DB = 0x03;
}

/*
 * $08:B893 — Viewport configuration init
 *
 * Sets up viewport indices at $7E:FFA6-$7E:FFAC.
 * These control which viewports are active (1-4 players).
 */
void srf_08B893(void) {
    op_php();
    op_rep(0x20);

    bus_write16(0x7E, 0xFFA6, 0x0001);
    bus_write16(0x7E, 0xFFA8, 0x0002);
    bus_write16(0x7E, 0xFFAA, 0x0003);
    bus_write16(0x7E, 0xFFAC, 0x0004);

    op_plp();
}

/*
 * $03:865E — Scene reset (3 scenes)
 *
 * Loads three scene configurations from ROM tables
 * at offsets $0159, $01FA, $0287 within the $03:8000 table.
 * Each call to $03:8683 resets GSU and rebuilds the scene.
 */
void srf_03865E(void) {
    op_rep(0x30);

    /* Scene 1: offset $0159 */
    bus_wram_write16(0x1A57, 0x0159);
    srf_038683();

    /* Scene 2: offset $01FA */
    op_rep(0x30);
    bus_wram_write16(0x1A57, 0x01FA);
    srf_038683();

    /* Scene 3: offset $0287 */
    op_rep(0x30);
    bus_wram_write16(0x1A57, 0x0287);
    srf_038683();
}

/*
 * $03:8C86 — Full game restart
 *
 * Called when the game needs a complete restart (e.g., after
 * game over or returning to title). Performs:
 * 1. Set restart flags ($10DC = $FFFF, $10DE = 0)
 * 2. Call $03:865E for 3 scene resets
 * 3. Send audio stop command ($2141 = $06)
 * 4. Force blank, disable NMI/DMA/HDMA
 * 5. Clear PPU register area via DMA
 * 6. Clear WRAM banks $7E/$7F
 * 7. Clear GSU work RAM
 * 8. Reload audio engine ($07:AB88 calls $04:D720)
 * 9. Jump back to main init at $03:8B34
 */
void srf_038C86(void) {
    op_rep(0x20);

    /* Set restart flags */
    bus_wram_write16(0x10DC, 0xFFFF);
    bus_wram_write16(0x10DE, 0x0000);

    /* Scene resets */
    srf_03865E();

    /* Audio stop command */
    op_sep(0x20);
    bus_write8(0x00, 0x2141, 0x06);

    /* Force blank, disable everything */
    bus_write8(0x00, 0x2100, 0x80);
    bus_write8(0x00, 0x4200, 0x00);
    bus_write8(0x00, 0x420B, 0x00);
    bus_write8(0x00, 0x420C, 0x00);

    /* Clear PPU register area */
    op_sep(0x20);
    op_rep(0x10);
    CPU_SET_A8(0x00);
    g_cpu.X = 0x0300;
    g_cpu.Y = 0x2100;
    srf_038CF6();

    /* Clear WRAM banks */
    CPU_SET_A8(0x7E);
    g_cpu.X = 0x2000;
    g_cpu.Y = 0xE000;
    srf_038CF6();

    CPU_SET_A8(0x7F);
    g_cpu.X = 0x0000;
    g_cpu.Y = 0xFFFF;
    srf_038CF6();

    /* Clear GSU work RAM */
    op_rep(0x30);
    bus_write8(0x70, 0x0000, 0x00);
    for (uint32_t i = 1; i <= 0x27FF; i++) {
        bus_write8(0x70, (uint16_t)i, 0x00);
    }

    /* Reload audio engine */
    /* $07:AB88 does busy-wait + sends stop + re-uploads SPC700 */
    bus_write8(0x00, 0x2141, 0x06);
    srf_04D44C();  /* re-upload audio engine */

    /* The original JMPs back to $03:8B34 which continues init.
     * For recomp, the main loop will re-enter on next frame. */
}
