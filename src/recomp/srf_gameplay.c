/*
 * Stunt Race FX — Gameplay frame body & scene management
 *
 * $0B:FA24 — Gameplay scene setup (OAM, sprites, GSU, NMI config)
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
 * $0B:FA24 — Gameplay scene setup
 *
 * Called from the display dispatcher's gameplay path ($E6FA).
 * Sets up the race mode display:
 * 1. Queue music commands from config tables
 * 2. Clear OAM buffer (all sprites offscreen at $E0E0)
 * 3. Copy sprite position data from ROM tables at $0B:FCA2
 * 4. OAM DMA to PPU
 * 5. GSU tile decompress (program $01:CF42 with A=$D970)
 * 6. Set sprite name base, NMI state = $08 (race 2P cycle)
 * 7. Configure viewport dimensions and display parameters
 */
void srf_0BFA24(void) {
    op_php();
    op_rep(0x30);

    /* $0B:FAF9 — Queue music commands based on display config */
    {
        op_sep(0x30);
        /* Music table at $0B:FB18: { $7C, $7E, $7D, $7F, $9F } */
        uint8_t idx_a = bus_wram_read8(0x0D8A);
        uint8_t cmd_a = bus_read8(0x0B, 0xFB18 + idx_a) | 0x80;
        CPU_SET_A8(cmd_a);
        srf_04D649();

        op_sep(0x30);
        uint8_t idx_b = bus_wram_read8(0x0D89);
        uint8_t cmd_b = bus_read8(0x0B, 0xFB18 + idx_b);
        CPU_SET_A8(cmd_b);
        srf_04D649();
    }

    /* Clear OAM buffer: fill $0375-$0574 with $E0E0 (offscreen) */
    op_rep(0x30);
    for (uint16_t x = 0; x < 0x0200; x += 2) {
        bus_wram_write16(0x0375 + x, 0xE0E0);
    }

    /* Fill OAM high table: $0575-$0594 with $5555 */
    for (uint16_t x = 0; x < 0x0020; x += 2) {
        bus_wram_write16(0x0575 + x, 0x5555);
    }

    /* Copy sprite data from ROM tables at $0B:FCA2 */
    op_sep(0x20);
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x0B;
    op_rep(0x30);

    /* 4 sprite blocks from ROM table */
    struct { uint16_t dst_x; uint16_t src_y; } blocks[] = {
        { 0x0000, 0x0000 },
        { 0x0200, 0x0154 },
        { 0x0100, 0x00A8 },
        { 0x0210, 0x0162 },
    };
    for (int b = 0; b < 4; b++) {
        uint16_t dx = blocks[b].dst_x;
        uint16_t sy = blocks[b].src_y;
        while (1) {
            uint16_t val = bus_read16(0x0B, 0xFCA2 + sy);
            if (val == 0xFFFF) break;
            bus_wram_write16(0x0375 + dx, val);
            dx += 2;
            sy += 2;
        }
    }
    g_cpu.DB = saved_db;

    /* OAM DMA */
    op_sep(0x20);
    op_rep(0x10);
    bus_write8(0x00, 0x2102, 0x00);
    bus_write8(0x00, 0x2103, 0x00);
    bus_write16(0x00, 0x4300, 0x0400);
    bus_write16(0x00, 0x4302, 0x0375);
    bus_write8(0x00, 0x4304, 0x00);
    bus_write16(0x00, 0x4305, 0x0220);
    bus_write8(0x00, 0x420B, 0x01);

    /* GSU tile decompress: A=$D970, X=$001D */
    op_rep(0x30);
    bus_write16(0x70, 0x0068, 0xD970);
    bus_write16(0x70, 0x002C, 0x3000);
    bus_write16(0x70, 0x00A2, 0x0000);
    bus_write16(0x70, 0x006A, 0x001D);
    op_sep(0x20);
    CPU_SET_A8(0x01);
    g_cpu.X = 0xCF42;
    srf_GSU_launch();

    /* Sprite name base register */
    op_sep(0x20);
    bus_write8(0x00, 0x2101, 0x03);

    /* NMI state = $08 (race 2P VBlank cycle) */
    bus_wram_write8(0x0D3F, 0x08);

    /* Display configuration */
    bus_wram_write8(0x0D7D, 0x0C);
    bus_wram_write8(0x0E0E, 0xB0);
    bus_wram_write8(0x0E10, 0x1C);
    bus_wram_write8(0x0E12, 0x18);
    bus_wram_write8(0x0E14, 0x5A);
    bus_wram_write8(0x0E09, 0xB7);
    bus_wram_write8(0x0E0C, 0x3C);
    bus_wram_write8(0x0E0F, 0x10);
    bus_wram_write8(0x0E11, 0x84);
    bus_wram_write8(0x0E13, 0x68);
    bus_wram_write8(0x0E15, 0xC2);
    bus_wram_write8(0x0E0A, 0x45);
    bus_wram_write8(0x0E0D, 0xA4);

    op_rep(0x20);
    op_plp();
}

/*
 * $0B:B479 — Entity system initialization
 *
 * Sets up the entity pool: 80 entity slots at $1265-$162D,
 * each $2C bytes. Initializes function pointers to $D8A8
 * (default handler), builds pointer table at $120D.
 */
void srf_0BB479(void) {
    op_php();
    op_rep(0x30);

    uint16_t y = 0x0000;
    uint16_t x = 0x1265;

    while (x != 0x162D) {
        bus_wram_write16(0x120D + y, x);        /* pointer table */
        bus_wram_write16(0x0008 + x, 0xD8A8);   /* function ptr (default) */
        bus_wram_write16(0x0010 + x, 0xFFFF);   /* sentinel */
        y += 2;
        x += 0x002C;
    }

    bus_wram_write16(0x11F5, 0x0000);  /* free entity pointer */
    bus_wram_write16(0x11F7, 0x0000);  /* active entity count */
    bus_wram_write16(0x1201, 0x0200);  /* OAM buffer size */
    bus_wram_write16(0x1203, 0x0020);  /* high table size */
    bus_wram_write16(0x120B, 0x2881);  /* config */

    op_sep(0x20);
    bus_write8(0x00, 0x2101, 0x02);    /* sprite name base */

    op_rep(0x20);
    op_plp();
}

/*
 * $0B:B4C3 — Entity allocator
 *
 * Takes the next free entity from the pool ($120D) and adds
 * it to the active list ($1239). Returns entity base in X,
 * or $FFFF if no free entities.
 */
void srf_0BB4C3(void) {
    uint16_t free_idx = bus_wram_read16(0x11F5);
    if (free_idx >= 0x002C) {
        /* No free entities */
        g_cpu.X = 0xFFFF;
        return;
    }

    uint16_t entity_base = bus_wram_read16(0x120D + free_idx);
    free_idx += 2;
    bus_wram_write16(0x11F5, free_idx);

    uint16_t active_count = bus_wram_read16(0x11F7);
    bus_wram_write16(0x1239 + active_count, entity_base);
    active_count += 2;
    bus_wram_write16(0x11F7, active_count);

    g_cpu.X = entity_base;
}

/*
 * $0B:B450 — Entity callback dispatcher
 *
 * Walks the active entity list and calls each entity's function
 * pointer at offset +8. The function pointer is a 16-bit address
 * in the WRAM mirror, called in bank $0B context.
 */
void srf_0BB450(void) {
    op_php();
    uint8_t saved_db = g_cpu.DB;
    op_sep(0x20);
    op_rep(0x10);
    g_cpu.DB = 0x00;

    op_rep(0x20);
    int16_t y = (int16_t)bus_wram_read16(0x11F7) - 2;

    while (y >= 0) {
        bus_wram_write16(0x11FB, (uint16_t)y);
        uint16_t entity_base = bus_wram_read16(0x1239 + (uint16_t)y);
        bus_wram_write16(0x11F9, entity_base);

        /* Read function pointer from entity+8 */
        uint16_t func_ptr = bus_wram_read16(0x0008 + entity_base);
        if (func_ptr != 0 && func_ptr != 0xD8A8) {
            /* Dispatch via func_table (bank $0B assumed) */
            g_cpu.X = entity_base;
            uint32_t addr = 0x0B0000 | func_ptr;
            func_table_call(addr);
        }

        y = (int16_t)bus_wram_read16(0x11FB) - 2;
    }

    g_cpu.DB = saved_db;
    op_plp();
}

/*
 * $0B:E390 — Wait for NMI/VBlank processing
 *
 * Sets bit 0 of $11D8 and waits for the NMI handler to clear it.
 * In the recomp, NMI processing is synchronous so we just set
 * and immediately clear the flag.
 */
void srf_0BE390(void) {
    op_php();
    op_rep(0x20);
    uint16_t flags = bus_wram_read16(0x11D8);
    bus_wram_write16(0x11D8, flags | 0x0001);
    /* NMI processes synchronously in recomp — clear flag */
    bus_wram_write16(0x11D8, 0x0000);
    op_plp();
}

/*
 * $0B:B64A — Sprite compositor
 *
 * Walks the active entity list and generates OAM (sprite) data
 * for the PPU. Each entity can be a single sprite or a multi-sprite
 * group defined by a sprite list in $7F:1356.
 *
 * Writes to:
 *   $0375-$0574: OAM low table (4 bytes per sprite × 128)
 *   $0575-$0594: OAM high table (2 bits per sprite, packed)
 */
void srf_0BB64A(void) {
    op_php();
    op_sep(0x20);
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x00;
    op_rep(0x30);

    uint16_t oam_idx = 0;      /* $1205: OAM low table write index */
    uint16_t oam_hi_idx = 0;   /* $1207: OAM high table write index */
    bus_wram_write16(0x1205, 0);
    bus_wram_write16(0x1207, 0);

    int16_t bit_count = 7;     /* $11DF: bits remaining in current high byte */
    uint16_t hi_accum = 0;     /* $11E1: high table accumulator */
    bus_wram_write16(0x11E1, 0);

    int16_t ent_y = (int16_t)bus_wram_read16(0x11F7) - 2;

    while (ent_y >= 0) {
        uint16_t ent_base = bus_wram_read16(0x1239 + (uint16_t)ent_y);

        /* Check if entity is active */
        uint16_t active = bus_wram_read16(0x0010 + ent_base);
        if ((uint16_t)(active + 1) == 0) goto next_entity;  /* $FFFF = inactive */

        uint16_t sprite_list = bus_wram_read16(0x0012 + ent_base);
        if (sprite_list == 0xFFFF) goto next_entity;

        if (sprite_list != 0) {
            /* ── Multi-sprite entity ──────────────── */
            uint16_t base_x = bus_wram_read16(0x0002 + ent_base);
            int16_t rel_x = (int16_t)(base_x - bus_wram_read16(0x11ED));
            uint16_t base_y_val = bus_wram_read16(0x0006 + ent_base);
            int16_t rel_y = (int16_t)(base_y_val - bus_wram_read16(0x11EF));
            bus_wram_write16(0x11FD, (uint16_t)rel_x);
            bus_wram_write16(0x11FF, (uint16_t)rel_y);

            uint16_t list_ptr = sprite_list;
            uint16_t sprite_count = bus_read16(0x7F, 0x1356 + list_ptr);
            bus_wram_write16(0x11E3, sprite_count);
            list_ptr += 2;

            uint16_t spr_flag = 0;
            oam_idx = bus_wram_read16(0x1205);

            while ((int16_t)bus_wram_read16(0x11E3) >= 0) {
                if (oam_idx >= 0x0200) break;

                int16_t sx = (int16_t)(bus_read16(0x7F, 0x1356 + list_ptr) + (uint16_t)rel_x);
                if (sx < -16 || sx >= 256) goto skip_sub;
                if (sx < 0) spr_flag = 1; else spr_flag = 0;

                bus_wram_write16(0x0375 + oam_idx, (uint16_t)sx);

                int16_t sy = (int16_t)(bus_read16(0x7F, 0x1358 + list_ptr) + (uint16_t)rel_y);
                if (sy < -16 || sy >= 224) goto skip_sub;

                bus_wram_write16(0x0376 + oam_idx, (uint16_t)sy);
                oam_idx += 2;

                /* Tile/attribute data */
                uint16_t tile_attr = bus_read16(0x7F, 0x135A + list_ptr);
                bus_wram_write16(0x0375 + oam_idx, tile_attr);
                oam_idx += 2;
                bus_wram_write16(0x1205, oam_idx);

                /* Build high table bits */
                hi_accum = bus_wram_read16(0x11E1);
                hi_accum = (hi_accum >> 1) | ((spr_flag & 1) << 15);
                uint16_t size_bit = bus_read16(0x7F, 0x135C + list_ptr);
                hi_accum = (hi_accum >> 1) | ((size_bit & 1) << 15);
                bus_wram_write16(0x11E1, hi_accum);

                bit_count--;
                if (bit_count < 0) {
                    oam_hi_idx = bus_wram_read16(0x1207);
                    bus_wram_write16(0x0575 + oam_hi_idx, hi_accum);
                    oam_hi_idx += 2;
                    bus_wram_write16(0x1207, oam_hi_idx);
                    bit_count = 7;
                }

            skip_sub:
                list_ptr += 8;
                bus_wram_write16(0x11E3, bus_wram_read16(0x11E3) - 1);
            }
        } else {
            /* ── Single-sprite entity ──────────────── */
            uint16_t spr_flag = 0;
            oam_idx = bus_wram_read16(0x1205);
            if (oam_idx >= 0x0200) goto next_entity;

            int16_t sx = (int16_t)(bus_wram_read16(0x0002 + ent_base) - bus_wram_read16(0x11ED));
            if (sx < -16) goto next_entity;
            if (sx < 0) { spr_flag = 1; }
            else if (sx >= 256) goto next_entity;

            bus_wram_write16(0x0375 + oam_idx, (uint16_t)sx);

            int16_t sy = (int16_t)(bus_wram_read16(0x0006 + ent_base) - bus_wram_read16(0x11EF));
            if (sy < -16 || sy >= 224) goto next_entity;

            bus_wram_write16(0x0376 + oam_idx, (uint16_t)sy);

            /* Tile and attribute */
            uint16_t tile = bus_wram_read16(0x000E + ent_base) | bus_wram_read16(0x0010 + ent_base);
            bus_wram_write16(0x0377 + oam_idx, tile);
            oam_idx += 4;
            bus_wram_write16(0x1205, oam_idx);

            /* Build high table bits */
            hi_accum = bus_wram_read16(0x11E1);
            hi_accum = (hi_accum >> 1) | ((spr_flag & 1) << 15);
            uint16_t oam_size = bus_wram_read16(0x000C + ent_base);
            hi_accum = (hi_accum >> 1) | ((oam_size >> 8) << 15);
            bus_wram_write16(0x11E1, hi_accum);

            bit_count--;
            if (bit_count < 0) {
                bit_count = 7;
                oam_hi_idx = bus_wram_read16(0x1207);
                bus_wram_write16(0x0575 + oam_hi_idx, hi_accum);
                oam_hi_idx += 2;
                bus_wram_write16(0x1207, oam_hi_idx);
            }
        }

    next_entity:
        ent_y -= 2;
    }

    /* Flush remaining high table bits */
    if (bit_count != 7) {
        hi_accum = bus_wram_read16(0x11E1);
        while (bit_count >= 0) {
            hi_accum = (hi_accum >> 1) | 0x8000;
            hi_accum >>= 1;
            bit_count--;
        }
        oam_hi_idx = bus_wram_read16(0x1207);
        bus_wram_write16(0x0575 + oam_hi_idx, hi_accum);
        oam_hi_idx += 2;
        bus_wram_write16(0x1207, oam_hi_idx);
    }

    /* Fill remaining OAM entries with offscreen ($EFEF) */
    oam_idx = bus_wram_read16(0x1205);
    uint16_t oam_max = bus_wram_read16(0x1201);
    while (oam_idx < oam_max) {
        bus_wram_write16(0x0375 + oam_idx, 0xEFEF);
        oam_idx += 2;
    }
    bus_wram_write16(0x1201, bus_wram_read16(0x1205));

    /* Fill remaining high table with $5555 */
    oam_hi_idx = bus_wram_read16(0x1207);
    uint16_t hi_max = bus_wram_read16(0x1203);
    while (oam_hi_idx < hi_max) {
        bus_wram_write16(0x0575 + oam_hi_idx, 0x5555);
        oam_hi_idx += 2;
    }
    bus_wram_write16(0x1203, bus_wram_read16(0x1207));

    g_cpu.DB = saved_db;
    op_plp();
}

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

    /* $02:DAD6 — Camera setup from P1 object */
    srf_02DAD6();

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

    /* Palette/color updates */
    srf_02DB59();   /* GSU palette program */
    srf_02D55F();   /* palette fade */
    srf_02D53D();   /* palette copy */

    /* Camera angle calculation */
    srf_03D306();

    /* ── Player 2 viewport ──────────────────────────── */
    op_rep(0x30);
    uint16_t p2_state = bus_wram_read16(0x034A);
    uint16_t p2_obj = bus_read16(0x7E, 0x204F + p2_state);
    p2_obj = bus_read16(0x7E, 0x204F + p2_obj);
    g_cpu.X = p2_obj;

    /* $02:DAD6 — Camera setup from P2 object */
    srf_02DAD6();

    /* Set render-done flag for P2 */
    op_sep(0x20);
    bus_wram_write8(0x1F07, 0x01);

    /* Clear animation counter */
    op_rep(0x20);
    bus_wram_write16(0x1AE8, 0x0000);

    /* Audio sync via $7F:112F */
    op_rep(0x30);
    uint16_t audio_sync = bus_read16(0x70, 0x2476);
    CPU_SET_A16(audio_sync);
    srf_7F112F();

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
 * $03:863D / $03:8648 / $03:8653 — Scene change variants
 *
 * Each sets the scene config table offset ($1A57) and jumps to
 * $03:8683 (scene config loader). Used by the attract mode
 * auto-cycling to switch between demo scenes:
 *   $863D: offset $0000 (demo scene A)
 *   $8648: offset $006B (demo scene B)
 *   $8653: offset $00FC (demo scene C)
 */
void srf_03863D(void) {
    op_rep(0x30);
    bus_wram_write16(0x1A57, 0x0000);
    srf_038683();
}

void srf_038648(void) {
    op_rep(0x30);
    bus_wram_write16(0x1A57, 0x006B);
    srf_038683();
}

void srf_038653(void) {
    op_rep(0x30);
    bus_wram_write16(0x1A57, 0x00FC);
    srf_038683();
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
