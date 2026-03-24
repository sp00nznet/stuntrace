/*
 * Stunt Race FX — Object system & display mode setup
 *
 * $08:C5A5 — Object system main update
 * $02:E289 — Display mode setup from save data
 * $09:ECE0 — Copy jump table patches to WRAM
 */

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * $08:C5A5 — Object system main update
 *
 * Processes all active game objects for the current frame.
 * Handles both player 1 and player 2 (if 2-player mode).
 *
 * For each player:
 * 1. Stores current input to $1AF0/$1AF2
 * 2. Calls $08:C60F (object state update)
 * 3. Calls $08:B4C6 (object rendering/collision)
 *
 * After processing, saves previous input and clears current.
 */
void srf_08C5A5(void) {
    op_sep(0x20);
    op_rep(0x10);

    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x7E;

    /* Store P1 input */
    op_rep(0x20);
    uint16_t p1_input = bus_wram_read16(0x0309);
    bus_wram_write16(0x1AF0, p1_input);
    uint16_t p1_prev = bus_wram_read16(0x1F23);
    bus_wram_write16(0x1AF2, p1_prev);

    /* Clear 2-player flag */
    op_sep(0x20);
    bus_wram_write8(0x1A5D, 0x00);

    /* $08:C60F — Process P1 object state */
    uint16_t p1_state = bus_wram_read16(0x0346);
    g_cpu.X = p1_state;
    srf_08C60F();

    /* Check for 2-player mode */
    op_sep(0x20);
    uint8_t game_mode = bus_wram_read8(0x0D62);
    if (game_mode != 0) {
        /* Store P2 input */
        op_rep(0x20);
        uint16_t p2_input = bus_wram_read16(0x030D);
        bus_wram_write16(0x1AF0, p2_input);
        uint16_t p2_prev = bus_wram_read16(0x1F25);
        bus_wram_write16(0x1AF2, p2_prev);

        op_sep(0x20);
        bus_wram_write8(0x1A5D, 0x01);

        /* $08:C60F — Process P2 object state */
        uint16_t p2_state = bus_wram_read16(0x034A);
        g_cpu.X = p2_state;
        srf_08C60F();
    }

    /* Save previous frame input for edge detection */
    op_rep(0x20);
    bus_wram_write16(0x1F23, bus_wram_read16(0x0309));
    bus_wram_write16(0x1F25, bus_wram_read16(0x030D));

    /* Clear current frame input (will be refreshed by auto-joypad) */
    bus_wram_write16(0x0309, 0x0000);
    bus_wram_write16(0x030D, 0x0000);

    op_sep(0x20);
    g_cpu.DB = saved_db;
}

/*
 * $03:B6E6 / $03:B6F9 — Scene config index → display config value
 *
 * Lookup tables converting scene config indices to 16-bit
 * display configuration values used by the display pipeline.
 */
static uint16_t scene_config_lookup_b6e6(uint16_t index) {
    static const uint16_t table[] = { 0x853D, 0x8411, 0x81B9, 0x82E5 };
    if (index < 4) return table[index];
    return table[0];
}

static uint16_t scene_config_lookup_b6f9(uint16_t index) {
    static const uint16_t table[] = { 0x8AA8, 0x8990, 0x876E, 0x0E20 };
    if (index < 4) return table[index];
    return table[0];
}

/*
 * $03:B011 — Rotation matrix setup via GSU
 *
 * Copies camera Euler angles from $065E-$0662 to GSU RAM
 * $70:0020-$0024, launches GSU program $01:8325 (rotation
 * matrix calculator), then reads back the resulting 3x3 matrix
 * from $70:00E4-$70:00F4 to WRAM $0608-$0618.
 *
 * The GSU computes:
 *   ┌            ┐   ┌               ┐
 *   │ $0608 $060A│   │ cos·cos  ...  │
 *   │ $060C $060E│ = │  ...     ...  │
 *   │ $0610 $0612│   │  ...     ...  │
 *   │ $0614 $0616│   │  ...     ...  │
 *   │ $0618      │   │  ...          │
 *   └            ┘   └               ┘
 */
void srf_03B011(void) {
    /* Write camera angles to GSU RAM */
    op_rep(0x20);
    bus_write16(0x70, 0x0020, bus_wram_read16(0x065E));
    bus_write16(0x70, 0x0022, bus_wram_read16(0x0660));
    bus_write16(0x70, 0x0024, bus_wram_read16(0x0662));

    /* Launch GSU rotation matrix program $01:8325 */
    op_sep(0x20);
    op_rep(0x10);
    CPU_SET_A8(0x01);
    g_cpu.X = 0x8325;
    srf_GSU_launch();

    /* Read back 3x3 rotation matrix from GSU RAM */
    op_rep(0x20);
    bus_wram_write16(0x0608, bus_read16(0x70, 0x00E4));
    bus_wram_write16(0x060A, bus_read16(0x70, 0x00E6));
    bus_wram_write16(0x060C, bus_read16(0x70, 0x00E8));
    bus_wram_write16(0x060E, bus_read16(0x70, 0x00EA));
    bus_wram_write16(0x0610, bus_read16(0x70, 0x00EC));
    bus_wram_write16(0x0612, bus_read16(0x70, 0x00EE));
    bus_wram_write16(0x0614, bus_read16(0x70, 0x00F0));
    bus_wram_write16(0x0616, bus_read16(0x70, 0x00F2));
    bus_wram_write16(0x0618, bus_read16(0x70, 0x00F4));

    op_sep(0x30);
}

/*
 * $03:B48C — Object table setup for non-attract scenes
 *
 * Reads scene config values from $18CB/$18C9/$18CD/$18CF/$18D1,
 * converts them to display config values via lookup tables,
 * stores to $18DB/$18DD/$18DF/$18E1. Then configures GSU RAM
 * palette parameters at $70:FC0C/$FC0D based on scene type.
 */
void srf_03B48C(void) {
    op_php();
    op_rep(0x30);

    /* Convert scene config indices to display values */
    uint16_t cfg_cb = bus_read16(0x00, 0x18CB);
    bus_write16(0x00, 0x18DB, scene_config_lookup_b6e6(cfg_cb));

    uint16_t cfg_c9 = bus_read16(0x00, 0x18C9);
    bus_write16(0x00, 0x18DD, scene_config_lookup_b6e6(cfg_c9));

    uint16_t cfg_cd = bus_read16(0x00, 0x18CD);
    bus_write16(0x00, 0x18DD, scene_config_lookup_b6f9(cfg_cd));

    uint16_t cfg_cf = bus_read16(0x00, 0x18CF);
    bus_write16(0x00, 0x18DF, scene_config_lookup_b6f9(cfg_cf));

    uint16_t cfg_d1 = bus_read16(0x00, 0x18D1);
    bus_write16(0x00, 0x18E1, scene_config_lookup_b6f9(cfg_d1));

    /* Palette config lookup table at $03:B540: {1, 0, 2, 3} */
    static const uint16_t pal_table[] = { 0x0001, 0x0000, 0x0002, 0x0003 };

    /* Configure GSU palette based on scene type */
    uint16_t scene_type = bus_read16(0x00, 0x18C3);

    if (scene_type == 0x0003) {
        /* Race mode: write palette index to $70:FC0D */
        uint16_t pal_idx = (cfg_cb & 0x0003) * 2;
        uint16_t pal_val = (pal_idx < 8) ? pal_table[pal_idx / 2] : 0;
        op_sep(0x20);
        bus_write8(0x70, 0xFC0D, (uint8_t)pal_val);
        op_rep(0x20);
    } else if (scene_type == 0x0001) {
        /* Menu mode: shift and combine with existing $70:FC0C */
        uint16_t existing = bus_read16(0x70, 0xFC0C) & 0x0003;
        uint16_t pal_idx = cfg_cb * 2;
        uint16_t pal_val = (pal_idx < 8) ? pal_table[pal_idx / 2] : 0;
        pal_val = (pal_val << 4) | existing;
        op_sep(0x20);
        bus_write8(0x70, 0xFC0C, (uint8_t)pal_val);
        op_rep(0x20);
    } else {
        /* Title/attract: combine with existing $70:FC0C bits 4-5 */
        uint16_t existing = bus_read16(0x70, 0xFC0C) & 0x0030;
        uint16_t pal_idx = cfg_cb * 2;
        uint16_t pal_val = (pal_idx < 8) ? pal_table[pal_idx / 2] : 0;
        pal_val |= existing;
        op_sep(0x20);
        bus_write8(0x70, 0xFC0C, (uint8_t)pal_val);
        op_rep(0x20);
    }

    /* Update palette checksum */
    srf_03F02B();

    op_plp();
}

/*
 * $03:F02B — Palette/GSU config checksum
 *
 * Computes a checksum from GSU RAM config values at $70:FC06-FC0E
 * and stores to $70:FC14/$FC15. Used for change detection.
 */
void srf_03F02B(void) {
    op_php();
    op_sep(0x20);
    uint8_t sum = 0;
    sum += bus_read8(0x70, 0xFC0B);
    sum += bus_read8(0x70, 0xFC06);
    sum += bus_read8(0x70, 0xFC07);
    sum += bus_read8(0x70, 0xFC0C);
    sum += bus_read8(0x70, 0xFC0D);
    sum += bus_read8(0x70, 0xFC0E);
    sum += bus_read8(0x70, 0xFC08);
    sum += bus_read8(0x70, 0xFC09);
    sum += bus_read8(0x70, 0xFC0A);
    bus_write8(0x70, 0xFC14, sum);
    bus_write8(0x70, 0xFC15, sum);
    op_plp();
}

/*
 * $03:D8B3 — Display-mode-specific initialization
 *
 * Sets up display state variables based on the current mode ($0D2B):
 * - Race mode ($03): reads GSU RAM flags, sets $10CD/$0E3D/$0D4A
 * - Title/attract: clears all display state to defaults
 * - Common: clears $0D4C/$0D4E/$0D48/$0D44 and many state flags,
 *   sets $0F05-$0F10 palette indices, lookups $18DB→$0D8C/$0D8B
 */
void srf_03D8B3(void) {
    op_php();
    op_sep(0x20);

    uint8_t mode = bus_wram_read8(0x0D2B);

    if (mode == 0x03) {
        /* Race mode */
        uint8_t gsu_flags = bus_read8(0x70, 0xFC0B);
        if ((gsu_flags & 0x07) == 0) {
            bus_wram_write8(0x10CD, 0xFF);
            bus_wram_write8(0x0E3D, 0x1B);
            bus_wram_write8(0x0E3E, 0x00);
            op_rep(0x20);
            uint16_t cfg = bus_read16(0x00, 0x18DB);
            bus_wram_write16(0x0D8C, cfg);
            /* $EC43 inline: convert config to index */
            uint8_t idx;
            if (cfg == 0x81B9) idx = 0;
            else if (cfg == 0x853D) idx = 1;
            else if (cfg == 0x8411) idx = 2;
            else if (cfg == 0x82E5) idx = 3;
            else idx = 4;
            op_sep(0x30);
            bus_wram_write8(0x0D8B, idx);
            op_plp();
            return;
        }
        /* GSU flags non-zero: set $0D4A based on bit 0 */
        bus_wram_write8(0x0D4A, (gsu_flags & 0x01) ? 0x00 : 0x01);
    } else {
        /* Non-race modes */
        bus_wram_write8(0x0D4A, 0x00);
    }

    /* Common path */
    bus_wram_write8(0x10CD, 0x00);
    op_rep(0x20);
    bus_wram_write16(0x0E37, 0x0000);

    uint16_t cfg = bus_read16(0x00, 0x18DB);
    bus_wram_write16(0x0D8C, cfg);
    uint8_t idx;
    if (cfg == 0x81B9) idx = 0;
    else if (cfg == 0x853D) idx = 1;
    else if (cfg == 0x8411) idx = 2;
    else if (cfg == 0x82E5) idx = 3;
    else idx = 4;
    op_sep(0x30);
    bus_wram_write8(0x0D8B, idx);

    /* Clear display state variables */
    bus_wram_write8(0x0D4C, 0x00);
    bus_wram_write8(0x0D4E, 0x00);
    bus_wram_write8(0x10CD, 0x00);
    bus_wram_write8(0x0D48, 0x00);
    bus_wram_write8(0x0E3E, 0x00);
    bus_wram_write8(0x0D63, 0x00);
    bus_wram_write8(0x0D44, 0x00);
    bus_wram_write8(0x0EED, 0x00);
    bus_wram_write8(0x0EEE, 0x00);
    bus_wram_write8(0x0EEF, 0x00);
    bus_wram_write8(0x0D24, 0x00);
    bus_wram_write8(0x0D25, 0x00);

    /* Set palette indices: $0F05-$0F10 = 4 for title, 0 for others */
    uint8_t pal_val = (mode == 0x00) ? 0x04 : 0x00;
    for (int x = 0x0B; x >= 0; x--) {
        bus_wram_write8(0x0F05 + x, pal_val);
    }

    op_rep(0x20);
    bus_wram_write16(0x0E6C, 0x0000);
    bus_wram_write16(0x0E6E, 0x0000);
    op_sep(0x20);

    op_plp();
}

/*
 * $03:B3DA — Scene state initialization
 *
 * Sets up the game scene state for the current mode:
 * 1. Clears render/animation flags
 * 2. Checks $1917 game state for first-boot vs transition
 * 3. Calls scene-specific setup ($0B:AFAC or $0B:AFBC)
 * 4. Sets display mode ($0D2B) from scene config ($18C3)
 * 5. Loads RGB color components from scene data
 * 6. Sets NMI to disabled state during setup
 *
 * For the recomp, the complex scene transition loops
 * ($0B:AEF2/$0B:AFAC/$0B:AFBC) are simplified since they
 * contain their own frame loops for transition effects.
 */
void srf_03B3DA(void) {
    op_php();
    op_sep(0x20);

    /* Clear render flag */
    bus_wram_write8(0x1F07, 0x00);

    op_rep(0x20);
    bus_wram_write16(0x18C3, 0x0000);  /* scene type */
    bus_wram_write16(0x1168, 0x0000);

    /* Check game state — first boot or scene transition */
    uint16_t game_state = bus_read16(0x00, 0x1917);
    if (game_state != 0) {
        /* Scene transition — $0B:AFAC handles music/fade
         * For recomp, the key output is $18C3 being set */
    } else {
        /* First boot — $0B:AFBC handles initial setup
         * Sets $11DA/$11DB for controller config */
        op_sep(0x20);
        bus_wram_write8(0x11DA, 0x0F);
        bus_wram_write8(0x11DB, 0x0F);
    }

    /* Set display mode from scene config */
    op_rep(0x20);
    uint16_t scene_type = bus_read16(0x00, 0x18C3);
    bus_wram_write16(0x0D2B, scene_type);

    /* Mark as initialized */
    bus_write16(0x00, 0x1917, 0x0001);

    /* Scene-specific object init based on mode */
    if (scene_type != 0x0002) {
        /* Non-attract modes: $03:B48C object table setup */
        srf_03B48C();
    }

    /* Load RGB color components from scene data */
    op_sep(0x20);
    uint8_t r = bus_read8(0x00, 0x18EF);
    uint8_t g = bus_read8(0x00, 0x18F1);
    uint8_t b = bus_read8(0x00, 0x18F3);
    bus_wram_write8(0x1184, r);
    bus_wram_write8(0x1185, g);
    bus_wram_write8(0x1186, b);
    bus_write8(0x70, 0xFC08, r);
    bus_write8(0x70, 0xFC09, g);
    bus_write8(0x70, 0xFC0A, b);

    /* $03:F02B — palette/GSU config checksum */
    srf_03F02B();

    /* Disable VBlank processing during scene setup */
    op_sep(0x20);
    bus_wram_write8(0x0D3F, 0x1C);  /* invalid NMI state → default handler */
    bus_write8(0x00, 0x4200, 0x00); /* disable NMI */
    bus_write8(0x00, 0x420C, 0x00); /* disable HDMA */
    bus_write8(0x00, 0x2100, 0x80); /* force blank */

    op_plp();
}

/*
 * $08:8364 — Insert object into render list
 *
 * Adds object X to the render list. Each object has a hash/list
 * index at $2040+X, with the list heads stored at $7F:0000+hash.
 * Objects are doubly-linked via $203C (next) and $203E (prev).
 */
void srf_088364(void) {
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x7E;
    uint16_t saved_y = g_cpu.Y;
    uint16_t saved_x = g_cpu.X;
    op_rep(0x20);

    uint16_t obj = saved_x;
    uint16_t list_idx = bus_read16(0x7E, 0x2040 + obj);
    bus_write16(0x7E, 0x203E + obj, 0x0000);  /* prev = null */

    /* Get current list head */
    uint16_t old_head = bus_read16(0x7F, 0x0000 + list_idx);
    bus_write16(0x7E, 0x203C + obj, old_head);  /* new.next = old head */

    if (old_head != 0) {
        /* Update old head's prev to point to new object */
        bus_write16(0x7E, 0x203E + old_head, obj);
    }

    /* Set new list head */
    bus_write16(0x7F, 0x0000 + list_idx, obj);

    g_cpu.X = saved_x;
    g_cpu.Y = saved_y;
    g_cpu.DB = saved_db;
}

/*
 * $08:8392 — Remove object from render list
 *
 * Unlinks object X from its render list by updating
 * the prev/next pointers of adjacent objects.
 */
void srf_088392(void) {
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x7E;
    uint16_t saved_y = g_cpu.Y;
    uint16_t saved_x = g_cpu.X;
    op_rep(0x20);

    uint16_t obj = saved_x;
    uint16_t prev = bus_read16(0x7E, 0x203E + obj);

    if (prev != 0) {
        /* Not first in list: prev.next = this.next */
        uint16_t next = bus_read16(0x7E, 0x203C + obj);
        bus_write16(0x7E, 0x203C + prev, next);
        if (next != 0) {
            bus_write16(0x7E, 0x203E + next, prev);
        }
    } else {
        /* First in list: update list head */
        uint16_t list_idx = bus_read16(0x7E, 0x2040 + obj);
        uint16_t next = bus_read16(0x7E, 0x203C + obj);
        bus_write16(0x7F, 0x0000 + list_idx, next);
        if (next != 0) {
            bus_write16(0x7E, 0x203E + next, 0x0000);
        }
    }

    g_cpu.X = saved_x;
    g_cpu.Y = saved_y;
    g_cpu.DB = saved_db;
}

/*
 * $03:CB5C — Allocate object slot from free list
 *
 * Removes the first slot from the free list ($032D),
 * inserts it into the active object list ($0329/$032B).
 * Returns carry set + X=slot if successful, carry clear if full.
 */
void srf_03CB5C(void) {
    op_php();
    op_sep(0x20);
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x7E;
    op_rep(0x30);

    uint16_t tail = bus_wram_read16(0x032B);
    uint16_t free_head = bus_wram_read16(0x032D);

    if (free_head == 0) {
        /* No free slots */
        g_cpu.X = tail;
        op_sep(0x20);
        g_cpu.DB = saved_db;
        op_plp();
        g_cpu.flag_C = 0;  /* carry clear = failure */
        return;
    }

    /* Remove from free list */
    uint16_t next_free = bus_read16(0x7E, 0x2000 + free_head);
    bus_wram_write16(0x032D, next_free);

    /* Insert into active list */
    if (tail == 0) {
        /* List was empty */
        uint16_t old_head = bus_wram_read16(0x0329);
        bus_write16(0x7E, 0x2000 + free_head, old_head);
        bus_write16(0x7E, 0x2002 + free_head, 0x0000);
        bus_wram_write16(0x0329, free_head);
    } else {
        /* Insert after tail (Y) */
        uint16_t tail_next = bus_read16(0x7E, 0x2000 + tail);
        bus_write16(0x7E, 0x2000 + free_head, tail_next);
        bus_write16(0x7E, 0x2000 + tail, free_head);
        bus_write16(0x7E, 0x2002 + free_head, tail);
    }

    bus_wram_write16(0x032B, free_head);

    /* Update next's prev pointer if it exists */
    uint16_t new_next = bus_read16(0x7E, 0x2000 + free_head);
    if (new_next != 0) {
        bus_write16(0x7E, 0x2002 + new_next, free_head);
    }

    g_cpu.X = free_head;
    op_sep(0x20);
    g_cpu.DB = saved_db;
    op_plp();
    g_cpu.flag_C = 1;  /* carry set = success */
}

/*
 * $03:CB25 — Initialize object slot
 *
 * Clears object data fields, inserts into render list,
 * sets animation timer to 0 and init value to $69.
 * X = object slot to initialize, Y = parameter (swapped).
 */
void srf_03CB25(void) {
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x7E;

    uint16_t param = g_cpu.X;
    uint16_t obj = g_cpu.Y;

    /* Clear object data: $5E bytes at $2004+obj */
    op_rep(0x20);
    for (uint16_t i = 0; i < 0x5E; i += 2) {
        bus_write16(0x7E, 0x2004 + obj + i, 0x0000);
    }

    /* Clear render list index */
    bus_write16(0x7E, 0x2040 + obj, 0x0000);

    /* Insert into render list */
    g_cpu.X = obj;
    srf_088364();

    /* Set animation state */
    op_sep(0x20);
    bus_write8(0x7E, 0x2036 + obj, 0x00);  /* timer = 0 */
    bus_write8(0x7E, 0x2059 + obj, 0x69);  /* init value */

    /* Restore X/Y (swapped back) */
    g_cpu.X = param;
    g_cpu.Y = obj;
    g_cpu.DB = saved_db;
}

/*
 * $03:B8A1 — Deallocate object (remove from active + render lists)
 *
 * Calls $08:8392 to remove from render list, then unlinks from
 * the active doubly-linked list and adds to the free list.
 */
void srf_03B8A1(void) {
    uint16_t saved_x = g_cpu.X;
    uint16_t saved_y = g_cpu.Y;
    op_php();

    /* Remove from render list */
    srf_088392();

    op_rep(0x20);
    uint16_t obj = saved_x;
    uint16_t prev = bus_read16(0x7E, 0x2002 + obj);

    if (prev == 0) {
        /* First in active list */
        uint16_t next = bus_read16(0x7E, 0x2000 + obj);
        bus_wram_write16(0x0329, next);
        if (next != 0) {
            bus_write16(0x7E, 0x2002 + next, 0x0000);
        }
        bus_wram_write16(0x032B, 0x0000);
    } else {
        /* Not first: unlink from doubly-linked list */
        uint16_t next = bus_read16(0x7E, 0x2000 + obj);
        bus_write16(0x7E, 0x2000 + prev, next);
        if (next != 0) {
            bus_write16(0x7E, 0x2002 + next, prev);
        } else {
            bus_wram_write16(0x032B, prev);
        }
    }

    /* Add to free list head */
    uint16_t old_free = bus_wram_read16(0x032D);
    bus_write16(0x7E, 0x2000 + obj, old_free);
    bus_wram_write16(0x032D, obj);

    op_sep(0x20);
    op_plp();
    g_cpu.Y = saved_y;
    g_cpu.X = saved_x;
}

/*
 * $08:801C — Object command dispatcher (bytecode interpreter)
 *
 * Reads a stream of commands from a ROM table and creates/configures
 * GSU 3D objects for the current scene. The command table address
 * is passed via A (bank) and X (offset from $8000).
 *
 * Command format: each entry starts with a command byte used as an
 * index into a jump table at $08:8000. Handlers consume variable
 * numbers of bytes and loop back for the next command.
 *
 * Key commands for the title screen:
 *   $06: Scene init — clear render hash, set display params
 *   $1A: End/return — finish processing
 */
void srf_08801C(void) {
    uint8_t cmd_bank = CPU_A8();
    uint16_t tbl_x = g_cpu.X;  /* offset from $8000 in the command table */

    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = cmd_bank;

    op_rep(0x20);

    /* Process command stream */
    int cmd_count = 0;
    while (cmd_count < 100) {  /* safety limit */
        /* Read command byte from ROM table */
        uint8_t cmd = bus_read8(cmd_bank, 0x8000 + tbl_x);
        cmd_count++;

        if (cmd == 0x06) {
            /* Command $06: Scene init
             * Clear $7F:0000-$07FF (render hash table)
             * Clear various state variables */
            op_rep(0x30);
            for (uint16_t i = 0; i < 0x0800; i += 2) {
                bus_write16(0x7F, i, 0x0000);
            }

            op_sep(0x20);
            bus_wram_write8(0x1AF4, 0x00);

            op_rep(0x20);
            bus_wram_write16(0x1ADE, 0x0000);
            bus_write16(0x7E, 0xFFB8, 0x0000);
            bus_write16(0x7E, 0xFFFD, 0x0000);
            bus_write16(0x70, 0x26F2, 0x0000);
            bus_wram_write16(0x1AEC, 0x0000);

            uint16_t cfg = bus_wram_read16(0x0E3D) & 0x00FF;
            bus_write16(0x70, 0x26F4, cfg);

            bus_wram_write16(0x0CEE, 0x0000);
            bus_wram_write16(0x1AEE, 0x0000);

            op_sep(0x20);
            bus_wram_write8(0x1F71, 0x00);
            bus_wram_write8(0x1F72, 0x00);

            /* JSL $03:D3DE — object system init helper */
            func_table_call(0x03D3DE);

            tbl_x++;  /* advance past command byte */
        } else if (cmd == 0x04) {
            /* Command $04: Store table address to GSU RAM
             * Stores the current ROM address to $70:1092/$70:1094 */
            tbl_x++;  /* advance past command */
            op_rep(0x20);
            uint16_t addr = (uint16_t)(tbl_x + 0x8000);
            bus_write16(0x70, 0x1092, addr);
            op_sep(0x20);
            bus_write16(0x70, 0x1094, (uint16_t)cmd_bank);
            /* This command returns (RTL equivalent) */
            break;
        } else if (cmd == 0x1A) {
            /* Command $1A: handler at $81E6 — likely end/return */
            break;
        } else if (cmd == 0x00) {
            /* Command $00: re-dispatch (reads next command) — skip */
            tbl_x++;
        } else {
            /* Unhandled command — skip 1 byte and continue */
            tbl_x++;
        }
    }

    g_cpu.DB = saved_db;
}

/*
 * $08:D8C2 — GSU camera position sync (3 values only)
 *
 * Writes only the camera-relative position from WRAM object
 * ($200C-$2010) to GSU RAM ($70:001E-$0022). Lighter version
 * of $08:D86F when only position needs updating.
 */
void srf_08D8C2(void) {
    op_rep(0x20);
    uint16_t obj_x = g_cpu.X;
    uint16_t obj_id = bus_read16(0x7E, 0x2022 + obj_x);
    if (obj_id != 0) {
        bus_write16(0x70, 0x001E + obj_id, bus_read16(0x7E, 0x200C + obj_x));
        bus_write16(0x70, 0x0020 + obj_id, bus_read16(0x7E, 0x200E + obj_x));
        bus_write16(0x70, 0x0022 + obj_id, bus_read16(0x7E, 0x2010 + obj_x));
    }
    op_sep(0x20);
}

/*
 * $08:CD25 — Object state flag + callback chain setup
 *
 * Sets object processing flags and installs a callback chain:
 *   - Sets bit 5 of $2024+X (processing enabled)
 *   - Clears $2027+X (accumulated flags)
 *   - Sets update callback to $08:CD7E (calls $D070 validity check)
 *   - Sets init callback to $08:CD55 (calls $CB98 anim step, ORs GSU flags)
 */
void srf_08CD25(void) {
    uint16_t obj_x = g_cpu.X;

    /* Set processing flag */
    uint8_t flags = bus_read8(0x7E, 0x2024 + obj_x);
    bus_write8(0x7E, 0x2024 + obj_x, flags | 0x20);

    /* Clear accumulated flags */
    op_rep(0x20);
    bus_write16(0x7E, 0x2027 + obj_x, 0x0000);

    /* Set update callback to $08:CD7E */
    bus_write16(0x7E, 0x201A + obj_x, 0xCD7E);
    op_sep(0x20);
    bus_write8(0x7E, 0x201C + obj_x, 0x08);

    /* Set init callback to $08:CD55 */
    op_rep(0x20);
    bus_write16(0x7E, 0x201D + obj_x, 0xCD55);
    op_sep(0x20);
    bus_write8(0x7E, 0x201F + obj_x, 0x08);
}

/*
 * $08:D86F — Object → GSU data sync (write all fields)
 *
 * Copies all object state from WRAM to GSU RAM for rendering:
 *   $2053-$2057 → $70:000C-0010 (position X/Y/Z)
 *   $2020       → $70:0014 (heading/direction)
 *   $200C-$2010 → $70:001E-0022 (camera-relative position)
 *   $2015       → $70:0024 (animation frame, byte→word)
 *   $205A       → $70:0012 (sprite/model index, masked 7-bit)
 *
 * This is the WRAM→GSU direction (inverse of $08:CE02).
 */
void srf_08D86F(void) {
    op_rep(0x20);
    uint16_t obj_x = g_cpu.X;
    uint16_t obj_id = bus_read16(0x7E, 0x2022 + obj_x);
    if (obj_id == 0) {
        op_sep(0x20);
        return;
    }

    /* Position */
    bus_write16(0x70, 0x000C + obj_id, bus_read16(0x7E, 0x2053 + obj_x));
    bus_write16(0x70, 0x000E + obj_id, bus_read16(0x7E, 0x2055 + obj_x));
    bus_write16(0x70, 0x0010 + obj_id, bus_read16(0x7E, 0x2057 + obj_x));

    /* Heading */
    bus_write16(0x70, 0x0014 + obj_id, bus_read16(0x7E, 0x2020 + obj_x));

    /* Camera-relative position */
    bus_write16(0x70, 0x001E + obj_id, bus_read16(0x7E, 0x200C + obj_x));
    bus_write16(0x70, 0x0020 + obj_id, bus_read16(0x7E, 0x200E + obj_x));
    bus_write16(0x70, 0x0022 + obj_id, bus_read16(0x7E, 0x2010 + obj_x));

    /* Animation frame (8-bit → 16-bit high byte) */
    uint16_t anim = (bus_read16(0x7E, 0x2015 + obj_x) & 0x00FF) << 8;
    bus_write16(0x70, 0x0024 + obj_id, anim);

    /* Sprite/model index (7-bit) */
    uint16_t sprite = bus_read16(0x7E, 0x205A + obj_x) & 0x007F;
    bus_write16(0x70, 0x0012 + obj_id, sprite);

    op_sep(0x20);
}

/*
 * $08:CC7C — Vehicle object creation (link to GSU)
 *
 * Assigns a GSU 3D object to the game object at X.
 * Reads the next free GSU object ID from $70:238E,
 * writes the model address from $1F4C to GSU $70:000A+ID,
 * then calls $08:D86F to sync all object data to GSU RAM.
 *
 * Returns: A=0 if success, A=$FF if no free GSU object
 */
void srf_08CC7C(void) {
    op_rep(0x20);
    uint16_t obj_x = g_cpu.X;

    /* Read next free GSU object ID */
    uint16_t gsu_id = bus_read16(0x70, 0x238E);
    bus_write16(0x7E, 0x2022 + obj_x, gsu_id);

    if (gsu_id == 0) {
        op_sep(0x20);
        CPU_SET_A8(0xFF);
        return;
    }

    /* Write model address to GSU object */
    uint16_t model_addr = bus_wram_read16(0x1F4C);
    bus_write16(0x70, 0x000A + gsu_id, model_addr);

    /* Sync all object data to GSU RAM */
    g_cpu.X = obj_x;
    srf_08D86F();

    op_sep(0x20);
    CPU_SET_A8(0x00);
}

/*
 * $08:88C7 — Vehicle model setup from ROM table
 *
 * Loads vehicle model data from the ROM table at $08:85B9.
 * Y = model table offset (e.g., $BB for a specific vehicle).
 * Computes the table index, then calls $08:88D8 which reads
 * model type and initializes position/rotation accordingly.
 *
 * Model types at $08:85B9+index:
 *   Type 3: Load 3 position values to linked object
 *   Type 1/2: Different initialization patterns
 */
void srf_0888C7(void) {
    op_rep(0x20);
    uint16_t obj_x = g_cpu.X;
    uint16_t model_offset = g_cpu.Y;

    /* Compute table index: Y - $85B9 */
    uint16_t table_idx = model_offset - 0x85B9;
    bus_write16(0x7E, 0x2029 + obj_x, table_idx);

    /* Read model type from ROM table */
    uint16_t model_type = bus_read16(0x08, 0x85B9 + table_idx);

    if (model_type == 0x0003) {
        /* Type 3: Load 3 position values to linked object */
        uint16_t linked = bus_read16(0x7E, 0x204F + obj_x);

        uint16_t val1 = bus_read16(0x08, 0x85BB + table_idx);
        bus_wram_write16(0x0020 + linked, val1);
        bus_wram_write16(0x001A + linked, val1);

        uint16_t val2 = bus_read16(0x08, 0x85BD + table_idx);
        bus_wram_write16(0x0022 + linked, val2);
        bus_wram_write16(0x001C + linked, val2);

        uint16_t val3 = bus_read16(0x08, 0x85BF + table_idx);
        bus_wram_write16(0x0024 + linked, val3);
        bus_wram_write16(0x001E + linked, val3);

        /* Clear velocity/rotation */
        bus_write16(0x7E, 0x202E + obj_x, 0x0000);
        bus_write16(0x7E, 0x2030 + obj_x, 0x0000);
        bus_write16(0x7E, 0x2032 + obj_x, 0x0000);
        bus_write16(0x7E, 0x2027 + obj_x, 0x0000);

        /* Advance model index */
        bus_write16(0x7E, 0x2029 + obj_x, table_idx + 8);
    }

    op_sep(0x20);
}

/*
 * $08:CF41 — Collision response (walk GSU collision chain)
 *
 * Walks the GSU collision linked list starting from $1F3C
 * (set by $08:CE02). For each collision pair:
 * - Reads partner object from $70:000C+X
 * - Checks collision type from $70:0010+X (bit 1 = active)
 * - Reads partner flags from $70:0016 and collision data from $70:0064
 * - Accumulates collision response flags in $1F36/$1F38/$1F3A
 *
 * Returns: carry set if collision found, carry clear if no collision
 */
void srf_08CF41(void) {
    op_rep(0x20);
    uint16_t chain = bus_wram_read16(0x1F3C);
    if (chain == 0) {
        op_sep(0x20);
        g_cpu.flag_C = 0;
        return;
    }

    uint16_t saved_x = g_cpu.X;
    uint16_t x = chain;

    /* Walk collision linked list */
    while (x != 0) {
        uint16_t type = bus_read16(0x70, 0x0010 + x);
        if (!(type & 0x0002)) {
            /* Not an active collision — follow next pointer */
            x = bus_read16(0x70, 0x000E + x);
            continue;
        }

        /* Active collision found */
        uint16_t partner = bus_read16(0x70, 0x000C + x);
        uint16_t next = bus_read16(0x70, 0x000E + x);
        bus_wram_write16(0x1F3C, next);

        /* Read partner collision data */
        bus_wram_write16(0x1F3A, 0x0000);
        uint16_t partner_flags = bus_read16(0x70, 0x0016 + partner);
        if (partner_flags & 0x0002) {
            uint16_t col_data = bus_read16(0x70, 0x0064 + partner);
            bus_wram_write16(0x1F3A, col_data);
        }
        bus_wram_write16(0x1F36, partner_flags);
        bus_wram_write16(0x1F38, 0x0000);

        g_cpu.X = saved_x;
        op_sep(0x20);
        g_cpu.flag_C = 1;
        return;
    }

    g_cpu.X = saved_x;
    op_sep(0x20);
    g_cpu.flag_C = 0;
}

/*
 * $08:CF92 — Collision check (standalone)
 *
 * Checks for collisions on the object at X by reading the GSU
 * collision chain at $70:0018+obj_id. Accumulates flags from
 * all collision partners and calls the collision handler.
 *
 * Returns: A = collision type flags
 */
void srf_08CF92(void) {
    op_rep(0x20);
    uint16_t obj_x = g_cpu.X;
    uint16_t obj_id = bus_read16(0x7E, 0x2022 + obj_x);
    if (obj_id == 0) {
        op_sep(0x20);
        CPU_SET_A8(0x00);
        return;
    }

    uint16_t saved_x = g_cpu.X;

    /* Check GSU collision flags */
    bus_wram_write16(0x1F38, 0x0000);
    uint16_t gsu_lock = bus_read16(0x70, 0x001C + obj_id);
    if (gsu_lock & 0x0001) {
        bus_wram_write16(0x1F38, bus_wram_read16(0x1F38) | 0x0010);
        /* Check global collision flag */
        uint16_t global = bus_read16(0x70, 0x26F2);
        if (global & 0x0020) {
            bus_wram_write16(0x1F38, bus_wram_read16(0x1F38) | 0x0080);
        }
    }

    /* Check object-specific collision data */
    uint16_t obj_col = bus_read16(0x70, 0x002C + obj_id);
    uint16_t flags38 = bus_wram_read16(0x1F38) | (obj_col & 0x0020);
    bus_wram_write16(0x1F38, flags38);

    /* Walk collision chain */
    bus_wram_write16(0x1F36, 0x0000);
    bus_wram_write16(0x1F3A, 0x0000);

    uint16_t chain = bus_read16(0x70, 0x0018 + obj_id);
    while (chain != 0) {
        uint16_t type = bus_read16(0x70, 0x0010 + chain);
        if (type & 0x0002) {
            /* Active collision */
            uint16_t partner = bus_read16(0x70, 0x000C + chain);
            uint16_t pf = bus_read16(0x70, 0x0016 + partner);
            if (pf & 0x0002) {
                uint16_t pd = bus_read16(0x70, 0x0064 + partner);
                bus_wram_write16(0x1F3A, bus_wram_read16(0x1F3A) | pd);
            }
            bus_wram_write16(0x1F36, bus_wram_read16(0x1F36) | pf);
        }
        chain = bus_read16(0x70, 0x000E + chain);
    }

    g_cpu.X = saved_x;
    op_sep(0x20);
}

/*
 * $08:83CC — Render list rehash
 *
 * Reads object position, computes spatial hash, and if the hash
 * changed, removes from old render list and inserts into new one.
 * Uses $08:8392 (remove) and $08:8364 (insert).
 */
void srf_0883CC(void) {
    op_php();
    op_rep(0x20);
    uint16_t obj_x = g_cpu.X;

    /* Read position from object */
    uint16_t pos_x = bus_read16(0x7E, 0x200C + obj_x);
    uint16_t pos_z = bus_read16(0x7E, 0x2010 + obj_x);

    /* Simple spatial hash from position (fixed-point) */
    bus_wram_write16(0x0002, pos_x);
    bus_wram_write16(0x0094, pos_z);

    /* Compute hash — simplified: use high byte of position */
    uint16_t hash = (pos_x >> 8) + (pos_z >> 5);
    hash &= 0x00FE;  /* even alignment */

    /* Check if hash changed */
    uint16_t old_hash = bus_read16(0x7E, 0x2040 + obj_x);
    if (hash != old_hash) {
        /* Rehash: remove from old list, update hash, insert into new */
        g_cpu.X = obj_x;
        srf_088392();
        bus_write16(0x7E, 0x2040 + obj_x, hash);
        g_cpu.X = obj_x;
        srf_088364();
    }

    g_cpu.X = obj_x;
    op_plp();
}

/*
 * $08:D070 — Object validity check + GSU flag setup
 *
 * Checks if object at X has a valid GSU object ID ($2022+X).
 * If invalid: sets timer $2036+X = $FF, returns A=$FF.
 * If valid: sets GSU flag bit 2 at $70:001C+ID, returns A=$00.
 */
void srf_08D070(void) {
    uint16_t obj_x = g_cpu.X;

    /* Check if object ID is valid */
    uint8_t id_lo = bus_read8(0x7E, 0x2022 + obj_x);
    uint8_t id_hi = bus_read8(0x7E, 0x2023 + obj_x);
    if ((id_lo | id_hi) == 0) {
        /* No object — set timer and return $FF */
        bus_write8(0x7E, 0x2036 + obj_x, 0xFF);
        CPU_SET_A8(0xFF);
        return;
    }

    /* Valid object — set GSU flag bit 2 */
    op_rep(0x20);
    uint16_t obj_id = bus_read16(0x7E, 0x2022 + obj_x);
    if (obj_id != 0) {
        uint16_t gsu_flags = bus_read16(0x70, 0x001C + obj_id);
        bus_write16(0x70, 0x001C + obj_id, gsu_flags | 0x0004);
    }
    op_sep(0x20);
    CPU_SET_A8(0x00);
}

/*
 * $08:CE02 — Collision state sync (GSU → WRAM)
 *
 * Syncs the GSU-computed collision/position state back to the
 * WRAM object table. Reads GSU RAM fields and writes to the
 * object's WRAM entries for position, animation, and collision.
 *
 * Returns: carry clear + A=collision status if object exists,
 *          carry set if no object
 */
void srf_08CE02(void) {
    uint16_t obj_x = g_cpu.X;

    /* Rehash render list position */
    srf_0883CC();
    g_cpu.X = obj_x;

    /* Validity check + GSU flag */
    srf_08D070();
    g_cpu.X = obj_x;

    /* Clear collision flag */
    uint8_t flags = bus_read8(0x7E, 0x2024 + obj_x);
    bus_write8(0x7E, 0x2024 + obj_x, flags & 0xFD);

    /* Read object ID */
    op_rep(0x20);
    uint16_t obj_id = bus_read16(0x7E, 0x2022 + obj_x);
    if (obj_id == 0) {
        bus_wram_write16(0x1F3C, 0x0000);
        op_sep(0x20);
        CPU_SET_A8(0x00);
        g_cpu.flag_C = 1;  /* carry set = no object */
        return;
    }

    /* Read collision chain head from GSU RAM */
    uint16_t collision = bus_read16(0x70, 0x0018 + obj_id);
    bus_wram_write16(0x1F3C, collision);

    /* Write animation/state data to GSU object */
    uint8_t anim_val = bus_read8(0x7E, 0x2015 + obj_x);
    bus_write16(0x70, 0x0024 + obj_id, (uint16_t)anim_val << 8);

    uint16_t sprite_val = bus_read16(0x7E, 0x205A + obj_x) & 0x007F;
    bus_write16(0x70, 0x0012 + obj_id, sprite_val);

    /* Read position back from GSU to WRAM object */
    bus_write16(0x7E, 0x200C + obj_x, bus_read16(0x70, 0x001E + obj_id));
    bus_write16(0x7E, 0x200E + obj_x, bus_read16(0x70, 0x0020 + obj_id));
    bus_write16(0x7E, 0x2010 + obj_x, bus_read16(0x70, 0x0022 + obj_id));

    /* Check GSU collision flag */
    uint16_t gsu_collision = bus_read16(0x70, 0x001C + obj_id);
    op_sep(0x20);
    if (gsu_collision & 0x0002) {
        uint8_t f = bus_read8(0x7E, 0x2024 + obj_x);
        bus_write8(0x7E, 0x2024 + obj_x, f | 0x02);
    }

    /* Return collision status */
    uint16_t chain = bus_wram_read16(0x1F3C);
    if (chain != 0) {
        CPU_SET_A8(0x01);
    } else {
        CPU_SET_A8(0x00);
    }
    g_cpu.flag_C = 0;  /* carry clear = object exists */
}

/*
 * $08:CCA3 — GSU object flag setup
 *
 * Sets bit 4 and clears bit 8 of GSU object flags at $70:0016+ID.
 * Used to mark objects as "processing" during state updates.
 */
void srf_08CCA3(void) {
    op_rep(0x20);
    uint16_t obj_id = bus_read16(0x7E, 0x2022 + g_cpu.X);
    if (obj_id != 0) {
        uint16_t flags = bus_read16(0x70, 0x0016 + obj_id);
        flags = (flags | 0x0010) & 0xFEFF;
        bus_write16(0x70, 0x0016 + obj_id, flags);
    }
    op_sep(0x20);
}

/*
 * $08:CCBE — GSU animation frame set
 *
 * Writes value 2 to GSU object parameter at $70:001A+ID.
 * Sets the animation frame index for the GSU renderer.
 */
void srf_08CCBE(void) {
    op_rep(0x20);
    uint16_t obj_id = bus_read16(0x7E, 0x2022 + g_cpu.X);
    if (obj_id != 0) {
        bus_write16(0x70, 0x001A + obj_id, 0x0002);
    }
    op_sep(0x20);
}

/*
 * $08:CCD2 — GSU position write
 *
 * Writes 3D position from DP $02 (X), $08 (Y), $94 (Z)
 * to GSU object at $70:000C/000E/0010+ID.
 */
void srf_08CCD2(void) {
    op_rep(0x20);
    uint16_t obj_id = bus_read16(0x7E, 0x2022 + g_cpu.X);
    if (obj_id != 0) {
        bus_write16(0x70, 0x000C + obj_id, bus_wram_read16(0x0002));
        bus_write16(0x70, 0x000E + obj_id, bus_wram_read16(0x0008));
        bus_write16(0x70, 0x0010 + obj_id, bus_wram_read16(0x0094));
    }
    op_sep(0x20);
}

/*
 * $08:CCF1 — GSU flag OR
 *
 * ORs the value at DP $02 into GSU object flags at $70:0016+ID.
 */
void srf_08CCF1(void) {
    op_rep(0x20);
    uint16_t obj_id = bus_read16(0x7E, 0x2022 + g_cpu.X);
    if (obj_id != 0) {
        uint16_t flags = bus_read16(0x70, 0x0016 + obj_id);
        flags |= bus_wram_read16(0x0002);
        bus_write16(0x70, 0x0016 + obj_id, flags);
    }
    op_sep(0x20);
}

/*
 * $08:94A1 — Vehicle race mode animation callback
 *
 * Called through the animation handler dispatch when a vehicle
 * enters race mode. Sets the next-frame update callback to $08:94DD,
 * configures vehicle parameters, and sets GSU visibility flag.
 */
void srf_0894A1(void) {
    uint16_t obj_x = g_cpu.X;

    /* Set next-frame update callback to $08:94DD */
    op_rep(0x20);
    bus_write16(0x7E, 0x201A + obj_x, 0x94DD);
    op_sep(0x20);
    bus_write8(0x7E, 0x201C + obj_x, 0x08);

    /* Read linked object, set parameter */
    uint16_t linked = bus_read16(0x7E, 0x204F + obj_x);
    op_rep(0x20);
    bus_wram_write16(0x0000 + linked, 0x0655);
    op_sep(0x20);

    /* Set GSU object flag: visibility + active */
    op_rep(0x20);
    uint16_t parent_idx = bus_read16(0x7E, 0x2006 + obj_x);
    uint16_t parent_obj_id = bus_read16(0x7E, 0x2022 + parent_idx);
    if (parent_obj_id != 0) {
        uint16_t gsu_vis = bus_read16(0x70, 0x0040 + parent_obj_id);
        bus_write16(0x70, 0x0040 + parent_obj_id, gsu_vis | 0x0004);
    }
    op_sep(0x20);
}

/*
 * $08:951B — Vehicle collision animation callback
 *
 * Called through the animation handler dispatch when a vehicle
 * enters collision state. Sets update callback to $08:953C,
 * starts a 10-frame collision animation timer, configures
 * GSU collision flags.
 */
void srf_08951B(void) {
    uint16_t obj_x = g_cpu.X;

    /* Set next-frame update callback to $08:953C */
    op_rep(0x20);
    bus_write16(0x7E, 0x201A + obj_x, 0x953C);
    op_sep(0x20);
    bus_write8(0x7E, 0x201C + obj_x, 0x08);

    /* Set collision animation timer = 10 frames */
    bus_write8(0x7E, 0x2048 + obj_x, 0x0A);

    /* Set linked object collision parameter */
    uint16_t linked = bus_read16(0x7E, 0x204F + obj_x);
    op_rep(0x20);
    bus_wram_write16(0x0037 + linked, 0x8000);
    op_sep(0x20);

    /* Set GSU collision flags ($0410) on parent's object */
    uint16_t parent_idx = bus_read16(0x7E, 0x2006 + obj_x);
    op_rep(0x20);
    uint16_t parent_obj_id = bus_read16(0x7E, 0x2022 + parent_idx);
    if (parent_obj_id != 0) {
        bus_write16(0x70, 0x0042 + parent_obj_id, 0x0410);
    }
    op_sep(0x20);

    /* Decrement timer for collision animation */
    uint8_t timer = bus_read8(0x7E, 0x2048 + obj_x);
    if (timer > 0) {
        bus_write8(0x7E, 0x2048 + obj_x, timer - 1);
    }
}

/*
 * $08:B863 — Viewport render order setup
 *
 * Reads the viewport count from $7E:FFB8, then for each viewport
 * at $7E:FFAE+X, reads the linked object's GSU parameter at
 * $70:0088+obj_id and stores to $7E:FFA6+X.
 * This sets up the rendering priority order for multi-viewport.
 */
void srf_08B863(void) {
    uint16_t saved_x = g_cpu.X;
    uint16_t saved_y = g_cpu.Y;
    op_php();
    op_rep(0x30);

    uint16_t count = bus_read16(0x7E, 0xFFB8);
    /* Signed divide by 2 (CMP #$8000 + ROR) */
    count = (count >> 1);
    if (count == 0) { op_plp(); return; }

    for (uint16_t i = 0; i < count; i++) {
        uint16_t viewport_obj = bus_read16(0x7E, 0xFFAE + i * 2);
        uint16_t obj_id = bus_read16(0x7E, 0x2022 + viewport_obj);
        uint16_t gsu_param = bus_read16(0x70, 0x0088 + obj_id);
        bus_write16(0x7E, 0xFFA6 + i * 2, gsu_param);
    }

    op_plp();
    g_cpu.Y = saved_y;
    g_cpu.X = saved_x;
}

/*
 * $08:C60F — Object state update (per-player)
 *
 * Updates a single player's object state. Two paths based on
 * object flag bit 2 at $2025+X:
 *
 * Path A (bit 2 set): Simple GSU visibility update
 *   - Reads object ID from $2022+X
 *   - Sets visibility bit ($0200) in GSU RAM $70:0040+ID
 *   - Clears non-essential bits in $70:0042+ID
 *
 * Path B (bit 2 clear): Full vehicle processing
 *   - Checks input ($1AF0), game mode ($0D62)
 *   - Processes vehicle speed, position, collision flags
 *   - Updates object state in both WRAM and GSU RAM
 *
 * Called from $08:C5A5 for each active player.
 */
void srf_08C60F(void) {
    op_sep(0x20);
    op_rep(0x10);
    uint16_t obj_x = g_cpu.X;

    /* Save object index */
    bus_wram_write16(0x1F18, obj_x);

    uint8_t flags = bus_read8(0x7E, 0x2025 + obj_x);

    if (flags & 0x04) {
        /* Path A: Simple GSU visibility update */
        op_rep(0x20);
        uint16_t obj_id = bus_read16(0x7E, 0x2022 + obj_x);
        if (obj_id != 0) {
            uint16_t gsu_vis = bus_read16(0x70, 0x0040 + obj_id);
            bus_write16(0x70, 0x0040 + obj_id, gsu_vis | 0x0200);

            uint16_t gsu_flags = bus_read16(0x70, 0x0042 + obj_id);
            if (!(gsu_flags & 0x1000)) {
                bus_write16(0x70, 0x0042 + obj_id, gsu_flags & 0xD800);
            }
        }
        op_sep(0x20);
        return;
    }

    /* Path B: Full vehicle/object processing */
    op_rep(0x20);
    uint16_t obj_id = bus_read16(0x7E, 0x2022 + obj_x);
    if (obj_id == 0) return;

    /* Check GSU object lock flag */
    uint16_t gsu_lock = bus_read16(0x70, 0x001C + obj_id);
    if (gsu_lock & 0x0010) return;

    /* Check input for L button (select/debug) */
    uint16_t input = bus_wram_read16(0x1AF0);
    if (!(input & 0x2000)) {
        /* No L button — skip to game mode check */
    } else {
        /* L button processing */
        bus_wram_write16(0x1A33, bus_wram_read16(0x1A33) + 1);

        uint16_t prev_input = bus_wram_read16(0x1AF2);
        if (prev_input & 0x2000) {
            /* L was already held — set object flags */
            uint16_t linked = bus_read16(0x7E, 0x204F + obj_x);
            op_sep(0x20);
            uint8_t lf = bus_read8(0x7E, 0x2025 + linked);
            bus_write8(0x7E, 0x2025 + linked, lf | 0x01);

            /* Check for R button combo */
            uint8_t inp_lo = (uint8_t)(input & 0xFF);
            if (inp_lo & 0x20) {
                if (inp_lo & 0x10) {
                    bus_write8(0x7E, 0x2025 + linked, lf | 0x03);
                }
            }
            op_rep(0x20);
        }
    }

    /* Game mode check — different processing for attract vs gameplay */
    op_rep(0x20);
    uint8_t game_mode = bus_wram_read8(0x0D62);
    if (game_mode == 0) {
        /* Attract mode — minimal processing */
        return;
    }

    /* Gameplay mode — vehicle state processing
     * Full implementation needed for race physics:
     * - Speed calculation from input
     * - Position updates
     * - Collision response
     * - GSU object parameter writes
     * These will be recompiled with the vehicle physics system. */
}

/*
 * $08:B4C6 — Object rendering / animation callback setup
 *
 * Sets up animation callbacks for objects based on game mode.
 * Checks object flags, audio state, and assigns appropriate
 * vehicle animation handlers at $08:94A1 or $08:951B.
 *
 * Also manages GSU object visibility flags at $70:0042+ID.
 *
 * Called from the object system main update ($08:C5A5).
 */
void srf_08B4C6(void) {
    op_sep(0x20);
    uint16_t obj_x = g_cpu.X;
    uint16_t obj_y = g_cpu.Y;

    /* Check if already rendered (bit 3) */
    uint8_t flags = bus_read8(0x7E, 0x2025 + obj_x);
    if (flags & 0x08) return;  /* already done */

    /* Check audio/game mode */
    uint8_t mode = bus_wram_read8(0x0DD6 + obj_y);

    if (mode == 0x08) {
        /* Collision mode — clear GSU object flags */
        op_rep(0x20);
        uint16_t obj_id = bus_read16(0x7E, 0x2022 + obj_x);
        if (obj_id != 0) {
            uint16_t gsu_flags = bus_read16(0x70, 0x0042 + obj_id);
            bus_write16(0x70, 0x0042 + obj_id, gsu_flags & 0xFBCF);
        }
        op_sep(0x20);

        /* Check game event flag */
        uint8_t evt = bus_wram_read8(0x19ED);
        if (evt == 0x08) {
            /* Set vehicle animation callback to $08:951B */
            uint16_t linked = bus_read16(0x7E, 0x204F + obj_x);
            op_rep(0x20);
            bus_write16(0x7E, 0x201A + linked, 0x951B);
            op_sep(0x20);
            bus_write8(0x7E, 0x201C + linked, 0x08);
        }
    } else if (mode == 0x0C || mode == 0x0A) {
        /* Race modes — clear GSU object flags */
        op_rep(0x20);
        uint16_t obj_id = bus_read16(0x7E, 0x2022 + obj_x);
        if (obj_id != 0) {
            uint16_t gsu_flags = bus_read16(0x70, 0x0042 + obj_id);
            bus_write16(0x70, 0x0042 + obj_id, gsu_flags & 0xFBCF);
        }
        op_sep(0x20);

        /* Check special event flag */
        if (bus_wram_read8(0x0E59) != 0) {
            /* Set vehicle animation callback to $08:94A1 */
            uint16_t linked = bus_read16(0x7E, 0x204F + obj_x);
            op_rep(0x20);
            bus_write16(0x7E, 0x201A + linked, 0x94A1);
            op_sep(0x20);
            bus_write8(0x7E, 0x201C + linked, 0x08);
        }
    } else {
        /* Other modes — check $19ED */
        uint8_t evt = bus_wram_read8(0x19ED);
        if (evt != 0x08) return;  /* skip */

        /* Set callback to $08:951B */
        uint16_t linked = bus_read16(0x7E, 0x204F + obj_x);
        op_rep(0x20);
        bus_write16(0x7E, 0x201A + linked, 0x951B);
        op_sep(0x20);
        bus_write8(0x7E, 0x201C + linked, 0x08);
    }

    /* $B59A — Post-render: optionally call $08:B863, set rendered flag */
    op_rep(0x20);
    uint16_t render_flag = bus_wram_read16(0x1AEE);
    if ((render_flag & 0x0001) == 0) {
        /* Would call $08:B863 for additional render setup */
    }
    op_sep(0x20);
    uint8_t f = bus_read8(0x7E, 0x2025 + obj_x);
    bus_write8(0x7E, 0x2025 + obj_x, f | 0x08);
}

/*
 * $02:E289 — Display mode setup
 *
 * Converts RGB color components from save data ($1184-$1186)
 * into SNES 15-bit BGR color format and stores at $0F11.
 * Then looks up display mode from table at $02:E338 indexed
 * by game state $0D2B, sets NMI state, and waits for VBlank.
 *
 * SNES color format: 0bbb bbgg gggr rrrr
 *   R = $1184 & $1F (bits 0-4)
 *   G = ($1185 << 5) & $03E0 (bits 5-9)
 *   B = ($1186 << 10) & $7C00 (bits 10-14)
 */
void srf_02E289(void) {
    op_sep(0x20);

    /* Check skip flag */
    uint8_t skip = bus_wram_read8(0x10CD);
    if (skip != 0) return;

    /* Build 15-bit BGR color from RGB components */
    op_rep(0x20);
    uint16_t r = bus_wram_read16(0x1184) & 0x001F;
    uint16_t g = (bus_wram_read16(0x1185) << 5) & 0x03E0;
    uint16_t b = ((uint16_t)bus_wram_read8(0x1186) << 10) & 0x7C00;
    uint16_t color = r | g | b;
    bus_wram_write16(0x0F11, color);

    /* Look up display mode from table */
    op_sep(0x30);
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x02;

    uint8_t mode_idx = bus_wram_read8(0x0D2B);
    uint8_t display_mode = bus_read8(0x02, 0xE338 + mode_idx);
    bus_wram_write8(0x0D62, display_mode);

    /* Set NMI state = $10 (gameplay force-blank) */
    op_sep(0x20);
    bus_wram_write8(0x0D3F, 0x10);

    /* Clear various display state variables */
    bus_wram_write8(0x0D61, 0x00);  /* brightness = 0 */
    bus_wram_write8(0x0E3A, 0x00);
    bus_wram_write8(0x0D43, 0x00);
    bus_wram_write8(0x0D40, 0x00);
    bus_wram_write8(0x0D50, 0x00);

    /* Enable NMI + auto-joypad + H-IRQ ($4200 = $31) */
    bus_write8(0x00, 0x4200, 0x31);
    OP_CLI();

    /* Wait for NMI to fire — spin until $0D52 is set */
    /* In recomp, the NMI fires within snesrecomp_end_frame(),
     * so we just check once and proceed */
    op_sep(0x20);
    bus_wram_write8(0x0D52, 0x00);

    g_cpu.DB = saved_db;
}

/*
 * $09:ECE0 — Copy jump table patches to WRAM
 *
 * Copies small code/data patches from ROM to WRAM at
 * $7F:F857, $7F:FFD1, $7F:FFD5, $7F:FFD9, $7F:FFDD
 * (4 bytes each from tables at $09:F999+).
 *
 * Then copies 16 bytes from $09:F163 to $7F:F85B.
 * These are likely jump table entries or interrupt vectors
 * used by the game's state machine in WRAM bank $7F.
 */
void srf_09ECE0(void) {
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x09;
    op_php();
    op_sep(0x30);

    /* Copy 5 x 4-byte patches from ROM to WRAM */
    struct { uint32_t src; uint32_t dst; } patches[] = {
        { 0x09F999, 0x7FF857 },
        { 0x09F99D, 0x7FFFD1 },
        { 0x09F9A1, 0x7FFFD5 },
        { 0x09F9A5, 0x7FFFD9 },
        { 0x09F9A9, 0x7FFFDD },
    };

    for (int p = 0; p < 5; p++) {
        for (int i = 0; i < 4; i++) {
            uint8_t val = bus_read8(
                (uint8_t)(patches[p].src >> 16),
                (uint16_t)(patches[p].src + i));
            bus_write8(
                (uint8_t)(patches[p].dst >> 16),
                (uint16_t)(patches[p].dst + i), val);
        }
    }

    /* Copy 16 bytes from $09:F163 to $7F:F85B */
    for (int i = 0; i < 16; i++) {
        uint8_t val = bus_read8(0x09, 0xF163 + i);
        bus_write8(0x7F, 0xF85B + i, val);
    }

    op_plp();
    g_cpu.DB = saved_db;
}
