/*
 * Stunt Race FX — Display configuration & screen setup
 *
 * These routines handle:
 * - Brightness control (fade in/out)
 * - Display mode configuration (PPU register setup)
 * - VRAM tile/tilemap DMA transfers
 * - GSU framebuffer → VRAM transfer
 * - Scene transition management
 *
 * The game uses a state variable at $0D2B to index into a jump
 * table for different display configurations (title, menus, race).
 */

#include <stdio.h>
#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * $02:DF79 — Pseudo-random number generator
 *
 * 32-bit LFSR-style PRNG using DP $E6-$E9 as state.
 * Called from the object system for various random behaviors
 * (enemy AI, particle effects, etc.). Returns result in A.
 */
void srf_02DF79(void) {
    op_php();
    op_sep(0x20);

    uint8_t e6 = bus_wram_read8(0x00E6);
    uint8_t e7 = bus_wram_read8(0x00E7);
    uint8_t e8 = bus_wram_read8(0x00E8);
    uint8_t e9 = bus_wram_read8(0x00E9);

    /* LFSR feedback: each byte feeds back into the next */
    uint8_t new_e7 = (uint8_t)(e6 - e7);  /* CLC then SBC is ADD with borrow */
    uint8_t new_e8 = (uint8_t)(new_e7 - e8);
    uint8_t new_e9 = (uint8_t)(new_e8 - e9);
    uint8_t new_e6 = (uint8_t)(new_e9 - e6);

    bus_wram_write8(0x00E7, new_e7);
    bus_wram_write8(0x00E8, new_e8);
    bus_wram_write8(0x00E9, new_e9);
    bus_wram_write8(0x00E6, new_e6);

    CPU_SET_A8(new_e6);
    op_plp();
}

/*
 * $02:DAD6 — Camera setup from object data
 *
 * Reads the current object's position and rotation data,
 * stores to camera variables, and prepares GSU parameters.
 *
 * Input: X = object base address (in $7E:204F table)
 *
 * Reads from object:
 *   +$08/$0A/$0C: position X/Y/Z → DP $C5/$C7/$C9
 *   +$0E/$10/$12: rotation → $0664/$0666/$0668
 *   +$02: linked object index → $70:247A (GSU param)
 *
 * Then computes camera-to-object transform via $03:B011
 * and stores results to $1EF5-$1EFF.
 */
void srf_02DAD6(void) {
    op_rep(0x30);

    uint16_t obj_x = g_cpu.X;

    /* Copy object position to camera variables */
    bus_wram_write16(0x00C5, bus_wram_read16(obj_x + 8));
    bus_wram_write16(0x00C7, bus_wram_read16(obj_x + 10));
    bus_wram_write16(0x00C9, bus_wram_read16(obj_x + 12));
    bus_wram_write16(0x0664, bus_wram_read16(obj_x + 14));
    bus_wram_write16(0x0666, bus_wram_read16(obj_x + 16));
    bus_wram_write16(0x0668, bus_wram_read16(obj_x + 18));

    /* Write linked object ID to GSU RAM */
    uint16_t linked_obj = bus_wram_read16(obj_x + 2);
    uint16_t obj_id = bus_read16(0x7E, 0x2022 + linked_obj);
    bus_write16(0x70, 0x247A, obj_id);

    /* Compute primary camera transform */
    /* Copy position to DP work area */
    bus_wram_write16(0x0002, bus_wram_read16(0x00C5));
    bus_wram_write16(0x0094, bus_wram_read16(0x00C9));

    /* Copy angles to rotation matrix work area */
    bus_wram_write16(0x065E, bus_wram_read16(0x0664));
    bus_wram_write16(0x0660, bus_wram_read16(0x0666));
    bus_wram_write16(0x0662, bus_wram_read16(0x0668));

    /* Build rotation matrix via GSU */
    srf_03B011();

    /* Store primary transform results */
    op_rep(0x20);
    bus_wram_write16(0x1EF5, bus_wram_read16(0x0608));
    bus_wram_write16(0x1EF7, bus_wram_read16(0x0002));
    bus_wram_write16(0x1EF9, bus_wram_read16(0x0094));

    /* Check if secondary transform needed */
    uint16_t obj_flags = bus_wram_read16(obj_x + 0);
    if (obj_flags & 0x2000) {
        /* Use primary transform for secondary too */
        bus_wram_write16(0x1EFB, bus_wram_read16(0x1EF5));
        bus_wram_write16(0x1EFD, bus_wram_read16(0x1EF7));
        bus_wram_write16(0x1EFF, bus_wram_read16(0x1EF9));
    } else {
        /* Compute secondary transform from linked object position */
        uint16_t link_x = bus_read16(0x7E, 0x200C + linked_obj);
        uint16_t link_z = bus_read16(0x7E, 0x2010 + linked_obj);
        bus_wram_write16(0x0002, link_x);
        bus_wram_write16(0x0094, link_z);

        /* Store secondary results */
        bus_wram_write16(0x1EFB, bus_wram_read16(0x0608));
        bus_wram_write16(0x1EFD, bus_wram_read16(0x0002));
        bus_wram_write16(0x1EFF, bus_wram_read16(0x0094));
    }
}

/*
 * $02:D53D — Palette copy from ROM table
 *
 * Copies color values from ROM table at $02:D65A+X to WRAM
 * palette buffer at $07A2+Y. Uses $0CF2 (source index) and
 * $0CF6 (dest index), decrementing both per call.
 */
void srf_02D53D(void) {
    op_rep(0x30);
    uint16_t count = bus_wram_read16(0x0CF6);
    if (count == 0) return;

    uint16_t src_idx = bus_wram_read16(0x0CF2);
    uint16_t dst_idx = count;

    uint16_t color = bus_read16(0x02, 0xD65A + src_idx);
    bus_wram_write16(0x07A2 + dst_idx, color);

    bus_wram_write16(0x0CF2, src_idx - 2);
    bus_wram_write16(0x0CF6, dst_idx - 2);
}

/*
 * $02:D55F — Palette fade (per-frame color interpolation)
 *
 * Smoothly interpolates the current palette ($0722) toward
 * a target palette ($02:D65A) one step per frame.
 * Each RGB channel is stepped by $0400 toward the target.
 * $0CFA holds the number of remaining fade frames.
 */
void srf_02D55F(void) {
    op_sep(0x20);
    uint8_t frames_left = bus_wram_read8(0x0CFA);
    if (frames_left == 0) return;

    frames_left--;
    bus_wram_write8(0x0CFA, frames_left);

    op_rep(0x30);
    uint16_t idx = bus_wram_read16(0x1168);

    /* Process each palette entry — interpolate B, G, R channels */
    uint16_t current = bus_wram_read16(0x0722 + idx);
    uint16_t target = bus_read16(0x02, 0xD65A + idx);

    /* Blue channel (bits 10-14) */
    uint16_t cur_b = current & 0x7C00;
    uint16_t tgt_b = target & 0x7C00;
    if (cur_b != tgt_b) {
        if (tgt_b > cur_b) cur_b += 0x0400;
        else cur_b -= 0x0400;
    }

    /* Green channel (bits 5-9) */
    uint16_t cur_g = current & 0x03E0;
    uint16_t tgt_g = target & 0x03E0;
    if (cur_g != tgt_g) {
        if (tgt_g > cur_g) cur_g += 0x0020;
        else cur_g -= 0x0020;
    }

    /* Red channel (bits 0-4) */
    uint16_t cur_r = current & 0x001F;
    uint16_t tgt_r = target & 0x001F;
    if (cur_r != tgt_r) {
        if (tgt_r > cur_r) cur_r += 0x0001;
        else cur_r -= 0x0001;
    }

    bus_wram_write16(0x0722 + idx, cur_b | cur_g | cur_r);
}

/*
 * $02:DB59 — GSU palette program launch
 *
 * Copies camera rotation angles to work area, then launches
 * GSU program $01:AEF8 which processes palette/color updates
 * based on the 3D camera orientation.
 */
void srf_02DB59(void) {
    /* $02:DF24 — Copy camera angles to work area + build matrix */
    op_rep(0x20);
    bus_wram_write16(0x065E, bus_wram_read16(0x0664));
    bus_wram_write16(0x0660, bus_wram_read16(0x0666));
    bus_wram_write16(0x0662, bus_wram_read16(0x0668));

    /* Build rotation matrix via GSU */
    op_sep(0x30);
    srf_03B011();

    /* Copy matrix results to palette work area */
    op_rep(0x20);
    bus_wram_write16(0x064C, bus_wram_read16(0x0608));
    bus_wram_write16(0x064E, bus_wram_read16(0x060A));
    bus_wram_write16(0x0650, bus_wram_read16(0x060C));
    bus_wram_write16(0x0652, bus_wram_read16(0x060E));
    bus_wram_write16(0x0654, bus_wram_read16(0x0610));

    /* Launch GSU palette program $01:AEF8 */
    op_sep(0x20);
    op_rep(0x10);
    CPU_SET_A8(0x01);
    g_cpu.X = 0xAEF8;
    srf_GSU_launch();
}

/*
 * $7E:E258 — P2 GSU render pipeline (ROM $02:BF7F)
 *
 * Sets up and launches the GSU for Player 2 rendering.
 * Launches GSU program $01:AB13 (P2 3D render), then
 * performs post-render processing for the second viewport.
 *
 * The original also calls several WRAM-resident sub-routines
 * for rendering setup ($E2C9, $E303, $EE7B, $E1E1, $E1CB, $EF16).
 * For the recomp, we handle the core GSU launch.
 */
void srf_7EE258(void) {
    op_sep(0x20);
    op_rep(0x10);

    /* Clear cycle counters */
    bus_wram_write8(0x00FE, 0x00);
    bus_wram_write8(0x00FF, 0x00);

    /* Launch GSU program $01:AB13 (P2 render) */
    CPU_SET_A8(0x01);
    g_cpu.X = 0xAB13;
    srf_GSU_launch();

    /* Store cycle count */
    op_rep(0x20);
    uint16_t cycles = bus_wram_read16(0x00FE);
    bus_wram_write16(0x1F21, cycles);
}

/*
 * $7F:112F — Gameplay audio sync (ROM $0B:892F)
 *
 * Handles per-frame audio synchronization during gameplay.
 * Checks audio processing flags and dispatches to the
 * appropriate audio handler. Stores frame audio state to $0E49.
 *
 * Called from the gameplay frame body with the audio sync
 * value in A (from GSU RAM $70:2476).
 */
void srf_7F112F(void) {
    op_php();
    op_rep(0x30);

    /* Save parameter */
    uint16_t param = g_cpu.C;
    bus_wram_write16(0x0002, param);

    op_sep(0x20);

    /* Check audio processing mode */
    uint8_t audio_flag = bus_read8(0x00, 0x0D53);
    uint8_t audio_mode = bus_read8(0x00, 0x0DD6);

    if (audio_flag != 0) {
        /* Active audio processing */
        if (audio_mode >= 6) {
            /* Special mode — just store */
        }
        /* Basic audio handler — queue commands based on game state */
    }

    /* Store audio frame state */
    op_sep(0x20);
    bus_write8(0x00, 0x0E49, (uint8_t)(param & 0xFF));

    op_plp();
}

/*
 * $02:D65A — Store brightness value
 *
 * Writes A to three brightness control bytes in WRAM $7E:BC2B/2D/2F.
 * These are used by HDMA for per-scanline brightness effects.
 */
void srf_02D65A(void) {
    op_php();
    op_sep(0x20);
    uint8_t val = CPU_A8();
    bus_write8(0x7E, 0xBC2B, val);
    bus_write8(0x7E, 0xBC2D, val);
    bus_write8(0x7E, 0xBC2F, val);
    op_plp();
}

/*
 * $02:D7AB — Wait for specific scanline
 *
 * Busy-waits until the PPU reaches scanline stored at $06E3.
 * Used for timing during screen transitions.
 * In recomp, we skip the busy-wait.
 */
void srf_02D7AB(void) {
    op_php();
    op_rep(0x30);
    op_sep(0x20);

    bus_wram_write8(0x06E4, 0x00);

    /* The original reads $2137 (software latch) then $213D/$213D
     * to get the current V-counter and compares to $06E3.
     * For recomp, we don't need cycle-accurate scanline timing. */

    op_rep(0x30);
    op_plp();
}

/*
 * $02:CF45 — Full screen setup for new scene
 *
 * Called when transitioning to a new game state (title, menu, race).
 * Disables display, configures PPU, loads tiles/palettes, starts GSU.
 */
void srf_02CF45(void) {
    op_php();
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x00;
    OP_SEI();

    op_sep(0x20);

    /* Disable HDMA */
    bus_write8(0x00, 0x420C, 0x00);

    /* Force blank + store brightness */
    CPU_SET_A8(0x80);
    srf_02D65A();
    bus_write8(0x00, 0x2100, 0x80);

    /* Clear input state */
    bus_wram_write8(0x084B, 0x00);
    bus_wram_write8(0x084C, 0x00);

    /* Wait for scanline $6E, then $64 */
    CPU_SET_A8(0x6E);
    bus_wram_write8(0x06E3, 0x6E);
    srf_02D7AB();

    op_sep(0x20);
    op_rep(0x10);
    CPU_SET_A8(0x64);
    bus_wram_write8(0x06E3, 0x64);
    srf_02D7AB();

    /* Clear game state variables */
    op_rep(0x30);
    bus_wram_write16(0x0309, 0x0000);
    bus_wram_write16(0x1168, 0x0000);

    op_sep(0x20);
    bus_wram_write8(0x08EF, 0x00);
    bus_wram_write8(0x05D5, 0x00);

    /* Initialize GSU control structure */
    /* $02:CFC6 — write marker $1234 to $70:01D0, set GSU control flag */
    op_rep(0x20);
    bus_write16(0x70, 0x01D0, 0x1234);
    bus_write16(0x70, 0x01C4, 0x0001);
    op_sep(0x20);
    uint8_t display_mode = bus_wram_read8(0x0D62);
    bus_write8(0x70, 0x108C, display_mode);

    /* $02:D928 — input device scan setup */
    op_sep(0x20);
    op_rep(0x10);
    bus_wram_write8(0x1F2C, 0x00);
    uint8_t d5 = bus_wram_read8(0x05D5);
    bus_wram_write8(0x1F2B, d5);

    /* $03:DD1B — PPU display configuration dispatch */
    srf_03DD1B();

    /* Start GSU rendering via WRAM routine ($7E:E1F5) */
    /* A = $01, X = $D1EB — this starts the GSU with a specific program */
    op_sep(0x20);
    op_rep(0x10);
    CPU_SET_A8(0x01);
    g_cpu.X = 0xD1EB;
    /* JSL $7E:E1F5 — GSU program launcher */
    srf_GSU_launch();

    /* Restore and return */
    g_cpu.DB = saved_db;
    op_plp();
    OP_CLI();
}

/*
 * $03:EC43 — Display config value → index lookup
 *
 * Converts the 16-bit display config value from ROM tables
 * ($18DB/$18DD) into a 0-4 index for $0D89/$0D8A.
 */
static uint8_t display_config_to_index(uint16_t val) {
    if (val == 0x81B9) return 0;
    if (val == 0x853D) return 1;
    if (val == 0x8411) return 2;
    if (val == 0x82E5) return 3;
    return 4;
}

/*
 * $03:DCEF — Display config lookup from scene data
 *
 * Reads display config values from $18DB/$18DD (set by scene
 * config loader $03:8683), converts to indices via $EC43,
 * and stores to $0D89/$0D8A/$0D8B.
 */
static void display_config_lookup(void) {
    op_rep(0x20);
    uint16_t cfg1 = bus_read16(0x00, 0x18DB);
    uint8_t idx1 = display_config_to_index(cfg1);
    op_sep(0x30);
    bus_wram_write8(0x0D89, idx1);

    uint8_t da = bus_wram_read8(0x10DA);
    uint8_t dc = bus_wram_read8(0x10DC);
    if (da != 0 || dc != 0) {
        bus_wram_write8(0x0D8B, idx1);
    }

    op_rep(0x20);
    uint16_t cfg2 = bus_read16(0x00, 0x18DD);
    uint8_t idx2 = display_config_to_index(cfg2);
    op_sep(0x20);
    bus_wram_write8(0x0D8A, idx2);
}

/*
 * $03:DCC0 — VRAM DMA from GSU RAM with offset calculation
 *
 * Parameters (16-bit): A = tile index, X = VRAM dest
 * Uses DP $F1 as GSU RAM base offset.
 * Transfers 32 bytes (16 words) from $70:(A<<5 + base) to VRAM at X.
 */
static void vram_dma_gsu_indexed(uint16_t tile_idx, uint16_t vram_dest) {
    op_rep(0x20);
    uint16_t base = bus_wram_read16(0x00F1);
    uint16_t src_addr = (tile_idx << 5) + base;

    bus_write16(0x00, 0x4302, src_addr);    /* DMA source */
    bus_write16(0x00, 0x2116, vram_dest);   /* VRAM dest */
    bus_write16(0x00, 0x4300, 0x1801);      /* word mode to $2118 */
    bus_write16(0x00, 0x4305, 0x0020);      /* 32 bytes */
    op_sep(0x20);
    bus_write8(0x00, 0x2115, 0x80);         /* VRAM inc mode */
    bus_write8(0x00, 0x4304, 0x70);         /* source bank = GSU RAM */
    bus_write8(0x00, 0x420B, 0x01);         /* trigger DMA */
    op_rep(0x20);
}

/*
 * $03:EC01 — GSU tile decompressor launcher
 *
 * Sets up parameters in GSU RAM and launches GSU program $01:CF42
 * which decompresses tile data from ROM into GSU work RAM.
 *
 * Parameters (16-bit): A = parameter value, X = offset
 */
static void gsu_tile_decompress(uint16_t param, uint16_t offset) {
    op_rep(0x20);
    bus_write16(0x70, 0x0068, param);   /* store parameter */
    bus_write16(0x70, 0x002C, 0x3000);  /* GSU work base */
    bus_write16(0x70, 0x00A2, 0x0000);  /* clear flag */
    bus_write16(0x70, 0x006A, offset);  /* store offset */

    /* Launch GSU tile decompressor at $01:CF42 */
    op_sep(0x20);
    CPU_SET_A8(0x01);
    g_cpu.X = 0xCF42;
    srf_GSU_launch();
}

/* $04:D0DB is now a full recompiled function — see srf_audio.c */

/*
 * $03:EF3F — Display mode config table lookup
 *
 * Looks up a display configuration index from ROM tables
 * based on display mode ($0D2B) and stores at $0E70.
 * Also initializes sprite timer values $0E68-$0E6B.
 */
static void display_mode_config_lookup(void) {
    op_sep(0x30);

    /* Init sprite timer values */
    bus_wram_write8(0x0E68, 0x99);
    bus_wram_write8(0x0E69, 0x99);
    bus_wram_write8(0x0E6A, 0x99);
    bus_wram_write8(0x0E6B, 0x99);

    uint8_t mode = bus_wram_read8(0x0D2B);
    uint8_t result = 0;

    if (mode == 0) {
        /* Title mode */
        uint8_t flag = bus_wram_read8(0x0D4C);
        if (flag != 0) {
            uint8_t idx = bus_wram_read8(0x0D4A);
            result = bus_read8(0x03, 0xD810 + idx);
        } else {
            uint8_t cfg = bus_wram_read8(0x0D89);
            /* Table lookup from $03:D813 indexed by cfg */
            result = bus_read8(0x03, 0xD813 + cfg);
        }
    } else if (mode == 1) {
        uint8_t flag = bus_wram_read8(0x0D4E);
        if (flag != 0) {
            result = 0;
        } else {
            /* Table lookup based on $0E3D and $0D89 */
            uint8_t cfg_idx = bus_wram_read8(0x0E3D);
            uint8_t offset = ((cfg_idx - 0x0C) << 2) + bus_wram_read8(0x0D89);
            result = bus_read8(0x03, 0xD8A3 + offset);
        }
    } else {
        /* Modes 2+ (attract, race) → result = 0 */
        result = 0;
    }

    bus_wram_write8(0x0E70, result);
    bus_wram_write8(0x0E71, 0x00);
}

/*
 * $03:DD1B — Display mode DMA dispatcher
 *
 * Complete display pipeline for scene transitions. Performs:
 * 1. Audio state clear
 * 2. Display config lookup from ROM tables
 * 3. Audio/music reload check
 * 4. GSU tile decompression (program $01:CF42)
 * 5. Mode-specific VRAM DMA (jump table at $DD56)
 * 6. Common post-dispatch:
 *    - Additional VRAM DMA from GSU RAM
 *    - VRAM DMA engine call ($03:EB83)
 *    - Second GSU tile decompress pass
 *    - GSU framebuffer copy: MVN $70:3000 → $7F:63D7 (12 KB)
 *    - Display config table lookups
 *    - Attract/gameplay path dispatch
 *
 * Jump table at $DD56:
 *   Mode 0 ($DDA4): Title — DMA from $70:30A0+offset to VRAM $62E0
 *   Mode 1 ($DDCE): Transition — if $0D4E, use mode 3; else common
 *   Mode 2 ($DD94): Attract — DMA from $70:3000+offset to VRAM $62F0
 *   Mode 3 ($DD5E): Race — DMA GSU framebuffer $70:3080 → VRAM $63F0
 */
void srf_03DD1B(void) {
    op_php();
    op_sep(0x20);
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x03;

    /* ── Pre-dispatch ──────────────────────────────────── */

    /* $04:D6E1 — clear audio state */
    srf_04D6E1();

    op_sep(0x20);
    uint8_t mode_4c = bus_wram_read8(0x0D4C);
    uint8_t mode_2b = bus_wram_read8(0x0D2B);

    /* Display config override or ROM lookup */
    if (mode_4c != 0 && mode_2b == 0) {
        bus_wram_write8(0x0D89, 0x04);
    } else {
        /* $03:DCEF — lookup from ROM scene tables */
        display_config_lookup();
    }

    /* $04:D0DB — audio/music reload */
    srf_04D0DB();

    /* GSU tile decompress pass 1: A=$8078, X=$001D */
    op_rep(0x30);
    gsu_tile_decompress(0x8078, 0x001D);

    /* ── Mode-specific dispatch ─────────────────────── */
    op_rep(0x30);
    uint16_t mode = bus_wram_read16(0x0D2B);

    switch (mode) {
    case 0x0000:
        /* Title screen — DMA 32 bytes from GSU RAM to VRAM */
        op_rep(0x30);
        bus_wram_write16(0x00F1, 0x30A0);  /* GSU RAM base */
        {
            uint16_t tile_idx = bus_wram_read16(0x18E5);
            vram_dma_gsu_indexed(tile_idx, 0x62E0);
        }
        break;

    case 0x0001:
        /* Transition mode — check $0D4E flag */
        op_sep(0x20);
        if (bus_wram_read8(0x0D4E) != 0) {
            /* Use race mode DMA (same as mode 3) */
            goto mode3_dma;
        }
        /* Otherwise fall through to common */
        break;

    case 0x0002:
        /* Attract mode — DMA 32 bytes to VRAM $62F0 */
        op_rep(0x30);
        bus_wram_write16(0x00F1, 0x3000);
        {
            uint16_t tile_idx = bus_wram_read16(0x18E5);
            vram_dma_gsu_indexed(tile_idx, 0x62F0);
        }
        break;

    case 0x0003:
    mode3_dma:
        /* Race mode — DMA GSU framebuffer $70:3080 → VRAM $63F0 */
        op_rep(0x20);
        bus_write16(0x00, 0x4300, 0x1801);
        bus_write16(0x00, 0x4302, 0x3080);
        bus_write16(0x00, 0x2116, 0x63F0);
        bus_write16(0x00, 0x4305, 0x0020);
        op_sep(0x20);
        bus_write8(0x00, 0x2115, 0x80);
        bus_write8(0x00, 0x4304, 0x70);
        bus_write8(0x00, 0x420B, 0x01);
        break;
    }

    /* ── Common post-dispatch ($DDD5) ──────────────── */

    /* Set GSU RAM base for common DMA */
    op_rep(0x30);
    bus_wram_write16(0x00F1, 0x3000);

    /* DMA from GSU RAM: tile_idx from $18E3 → VRAM $62E0 */
    {
        uint16_t tile_idx = bus_wram_read16(0x18E3);
        vram_dma_gsu_indexed(tile_idx, 0x62E0);
    }

    /* VRAM DMA engine ($03:EB83) with Y=$F845, X=$F804 */
    op_rep(0x30);
    g_cpu.Y = 0xF845;
    g_cpu.X = 0xF804;
    srf_03EB83();

    /* GSU tile decompress pass 2: A=$D920, X=$0018 */
    op_rep(0x30);
    gsu_tile_decompress(0xD920, 0x0018);

    /* ── GSU framebuffer copy: $70:3000 → $7F:63D7 ── */
    /* MVN $7F,$70 — copies $3000 bytes (12 KB)
     * This transfers the GSU-rendered framebuffer from GSU work RAM
     * to WRAM where the NMI VBlank handler can DMA it to VRAM */
    op_rep(0x30);
    for (uint32_t i = 0; i <= 0x2FFF; i++) {
        uint8_t val = bus_read8(0x70, 0x3000 + (uint16_t)i);
        bus_write8(0x7F, 0x63D7 + (uint16_t)i, val);
    }

    /* $04:D6E1 — audio clear again */
    op_sep(0x20);
    srf_04D6E1();

    /* $EF3F — display mode config table lookup */
    op_sep(0x20);
    display_mode_config_lookup();

    /* Read display config and store to GSU RAM */
    op_rep(0x30);
    uint8_t cfg_idx = bus_wram_read8(0x0E3D);
    uint16_t cfg_tbl_val = bus_read16(0x03, 0xF799 + (uint16_t)(cfg_idx * 2));
    bus_write16(0x70, 0x0050, cfg_tbl_val);

    /* Dispatch based on game mode for attract/gameplay-specific setup */
    op_sep(0x20);
    uint8_t display_mode = bus_wram_read8(0x0D62);
    if (display_mode != 0) {
        /* Gameplay path ($E6FA) — initial GSU/SCMR setup then full scene */
        op_rep(0x30);
        bus_write16(0x70, 0x004E, 0xA545);
        op_sep(0x20);
        bus_wram_write8(0x0374, 0x01);
        op_rep(0x20);
        bus_write16(0x70, 0x01C4, 0x0002);

        /* $0B:FA24 — full gameplay scene setup */
        srf_0BFA24();
    } else {
        /* ── Attract path ($DECA) ──────────────────────── */

        /* $03:EDDF — queue music command based on $0D89 table */
        op_sep(0x20);
        {
            /* Table at $03:EDEF: { $06, $0C, $09, $0F, $12 } */
            uint8_t music_idx = bus_wram_read8(0x0D89);
            uint8_t music_cmd = bus_read8(0x03, 0xEDEF + music_idx);
            CPU_SET_A8(music_cmd);
            srf_04D649();
        }

        /* Set GSU parameter */
        op_rep(0x30);
        bus_write16(0x70, 0x004E, 0xA325);

        /* $03:ECD1 — Clear OAM buffer (all sprites offscreen) */
        op_rep(0x30);
        for (uint16_t x = 0; x <= 0x01FC; x += 4) {
            bus_write16(0x00, 0x0375 + x, 0xF0F0);
        }
        for (uint16_t x = 0; x <= 0x001E; x += 2) {
            bus_write16(0x00, 0x0575 + x, 0x0000);
        }

        /* $03:E9FE — Copy sprite positions from ROM table $F883 */
        op_sep(0x20);
        op_rep(0x10);
        {
            uint16_t src_y = 0x0000;
            uint16_t dst_x = 0x0000;
            while (1) {
                uint8_t val = bus_read8(0x03, 0xF883 + src_y);
                if (val == 0xFF) break;
                if (val == 0xFE) {
                    bus_wram_write8(0x0376 + dst_x, val);
                } else {
                    bus_wram_write8(0x0375 + dst_x, val);
                    bus_wram_write8(0x0376 + dst_x, bus_read8(0x03, 0xF884 + src_y));
                    bus_wram_write8(0x0377 + dst_x, bus_read8(0x03, 0xF885 + src_y));
                    bus_wram_write8(0x0378 + dst_x, bus_read8(0x03, 0xF886 + src_y));
                    src_y += 3;
                }
                src_y++;
                dst_x += 4;
            }

            /* Second sprite block: X=$0200, Y=$01AA */
            src_y = 0x01AA;
            dst_x = 0x0200;
            while (1) {
                uint8_t val2 = bus_read8(0x03, 0xF883 + src_y);
                if (val2 == 0xFF) break;
                if (val2 == 0xFE) {
                    bus_wram_write8(0x0376 + dst_x, val2);
                } else {
                    bus_wram_write8(0x0375 + dst_x, val2);
                    bus_wram_write8(0x0376 + dst_x, bus_read8(0x03, 0xF884 + src_y));
                    bus_wram_write8(0x0377 + dst_x, bus_read8(0x03, 0xF885 + src_y));
                    bus_wram_write8(0x0378 + dst_x, bus_read8(0x03, 0xF886 + src_y));
                    src_y += 3;
                }
                src_y++;
                dst_x += 4;
            }
        }

        /* ── Title screen mode handler ($E4DD) ─────────── */
        op_sep(0x30);
        uint8_t mode_2b_final = bus_wram_read8(0x0D2B);
        if (mode_2b_final == 0) {
            /* Mode 0: Title screen */
            bus_wram_write8(0x0E18, 0x8C);
            bus_wram_write8(0x0E16, 0xEC);

            uint8_t skip_flag = bus_wram_read8(0x10CD);
            if (skip_flag != 0) {
                /* $10CD set — simplified sprite setup */
                bus_wram_write8(0x046F, 0x3F);
                bus_wram_write8(0x0467, 0x3F);
                op_sep(0x20);
                bus_wram_write8(0x046D, 0xAA);
                bus_wram_write8(0x046E, 0xC0);
                bus_wram_write8(0x0470, 0x36);
                bus_wram_write8(0x0465, 0xE6);
                bus_wram_write8(0x0466, 0xC0);
                bus_wram_write8(0x0468, 0x36);

                /* $DE39 — set OAM high-table entries offscreen */
                bus_wram_write8(0x0546, 0xF0);
                bus_wram_write8(0x054A, 0xF0);
                bus_wram_write8(0x054E, 0xF0);
                bus_wram_write8(0x0552, 0xF0);
            } else {
                /* Normal title screen sprite setup */
                uint8_t car_tile = bus_wram_read8(0x0DF4);
                bus_wram_write8(0x046F, car_tile * 2);
                bus_wram_write8(0x0473, car_tile * 2 + 1);
                op_sep(0x20);
                bus_wram_write8(0x046D, 0xAA);
                bus_wram_write8(0x046E, 0xB8);
                bus_wram_write8(0x0470, 0x36);
                bus_wram_write8(0x0471, 0xAA);
                bus_wram_write8(0x0472, 0xC0);
                bus_wram_write8(0x0474, 0x36);

                uint8_t logo_tile = bus_wram_read8(0x0DF8);
                bus_wram_write8(0x0467, logo_tile * 2);
                bus_wram_write8(0x046B, logo_tile * 2 + 1);
                op_sep(0x20);
                bus_wram_write8(0x0465, 0xE6);
                bus_wram_write8(0x0466, 0xB8);
                bus_wram_write8(0x0468, 0x36);
                bus_wram_write8(0x0469, 0xE6);
                bus_wram_write8(0x046A, 0xC0);
                bus_wram_write8(0x046C, 0x36);
            }
        }

        /* ── Title screen final setup ($E5B4) ─────────── */

        /* Set background color to white */
        op_rep(0x20);
        bus_write16(0x7F, 0x3470, 0x7FFF);

        /* OAM DMA */
        op_sep(0x20);
        op_rep(0x10);
        /* Inline OAM DMA: $0375 → OAM, $0220 bytes */
        bus_write8(0x00, 0x2102, 0x00);
        bus_write8(0x00, 0x2103, 0x00);
        bus_write16(0x00, 0x4300, 0x0400);
        bus_write16(0x00, 0x4302, 0x0375);
        bus_write8(0x00, 0x4304, 0x00);
        bus_write16(0x00, 0x4305, 0x0220);
        bus_write8(0x00, 0x420B, 0x01);

        /* BG scroll registers */
        op_sep(0x30);
        bus_write8(0x00, 0x210D, 0x00);
        bus_write8(0x00, 0x210D, 0x00);
        bus_write8(0x00, 0x210E, 0xEF);
        bus_write8(0x00, 0x210E, 0xFF);
        bus_write8(0x00, 0x210F, 0x00);
        bus_write8(0x00, 0x210F, 0x00);
        bus_write8(0x00, 0x2110, 0x80);
        bus_write8(0x00, 0x2110, 0x00);

        /* Set NMI state = $04 (race VBlank cycle for 3D title) */
        bus_wram_write8(0x0D3F, 0x04);

        /* Display timing variables */
        bus_wram_write8(0x0E46, 0xFF);
        bus_wram_write8(0x0E45, 0xFF);
        bus_wram_write8(0x0D7D, 0x00);

        /* Frame counter thresholds */
        bus_wram_write8(0x0E12, 0xC4);
        bus_wram_write8(0x0E14, 0x93);
        bus_wram_write8(0x0E09, 0x2A);
        bus_wram_write8(0x0E0C, 0x7A);
        bus_wram_write8(0x0E0B, 0x00);

        /* Viewport dimensions */
        bus_wram_write8(0x0D57, 0xE0);
        bus_wram_write8(0x0D58, 0x20);
        bus_wram_write8(0x0E0E, 0x50);
        bus_wram_write8(0x0E10, 0xB8);
    }

    op_plp();
    g_cpu.DB = saved_db;
}

/*
 * $7E:E1F5 — GSU program launcher (ROM $02:BF1C)
 *
 * Starts a GSU program execution:
 *   A = program bank (PBR)
 *   X = program start address (written to R15)
 *
 * Protocol:
 * 1. Write PBR to $3034
 * 2. Set SCMR with RON+RAN to give GSU bus access
 * 3. Clear SFR
 * 4. Write X to R15 ($301E/$301F) — triggers execution
 * 5. Spin until GSU GO flag clears (STOP instruction reached)
 * 6. Restore SCMR without RON+RAN (return bus to 65816)
 *
 * Original disassembly (ROM $02:BF1C):
 *   BF1C: 8F 34 30 00  STA $003034     ; PBR = A
 *   BF20: 8B           PHB
 *   BF21: A9 00        LDA #$00
 *   BF23: 48/AB        PHB/PLB         ; DB = $00
 *   BF25: AD 74 03     LDA $0374
 *   BF28: 09 18        ORA #$18        ; set RON + RAN
 *   BF2A: 8D 3A 30     STA $303A       ; SCMR
 *   BF2D: 9C 30 30     STZ $3030       ; clear SFR low
 *   BF30: 8E 1E 30     STX $301E       ; R15 = X (triggers GO!)
 *   BF33: E6 FE        INC $FE         ; cycle counter
 *   BF35: D0 02        BNE $BF39
 *   BF37: E6 FF        INC $FF
 *   BF39: AD 30 30     LDA $3030       ; read SFR low
 *   BF3C: 29 20        AND #$20        ; check GO flag
 *   BF3E: D0 F3        BNE $BF33       ; loop while running
 *   BF40: AD 74 03     LDA $0374
 *   BF43: 8D 3A 30     STA $303A       ; restore SCMR
 *   BF46: AB           PLB
 *   BF47: 6B           RTL
 */
static int s_gsu_launch_count = 0;

void srf_GSU_launch(void) {
    uint8_t program_bank = CPU_A8();
    uint16_t program_addr = g_cpu.X;

    /* Debug: log first GSU launches with status */
    if (s_gsu_launch_count < 20) {
        printf("  GSU launch #%d: $%02X:%04X", s_gsu_launch_count, program_bank, program_addr);
    }

    /* Write PBR (program bank register) */
    bus_write8(0x00, 0x3034, program_bank);

    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x00;

    /* Set SCMR: enable RON + RAN (bits 4,3) to give GSU bus access */
    uint8_t scmr_base = bus_wram_read8(0x0374);
    bus_write8(0x00, 0x303A, scmr_base | 0x18);

    /* Clear SFR low byte */
    bus_write8(0x00, 0x3030, 0x00);

    /* Write program address to R15 — this triggers GSU execution!
     * The original uses STX $301E (16-bit write) which writes both
     * bytes atomically. Writing the high byte ($301F) sets the GO flag.
     * Use bus_write16 to match the original 16-bit store behavior. */
    bus_write16(0x00, 0x301E, program_addr);

    /* The GSU is now running. In the original hardware, the 65816
     * spins checking the GO flag in SFR ($3030 bit 5).
     * In our emulation, gsu_write triggers gsu_run() which executes
     * until STOP, so by the time we get here the GSU has finished. */

    /* Verify GSU has stopped (read SFR, check GO flag) */
    uint8_t sfr = bus_read8(0x00, 0x3030);

    if (s_gsu_launch_count < 20) {
        uint8_t sfr_hi = bus_read8(0x00, 0x3031);
        /* Read R15 (PC) after execution to see how far it got */
        uint16_t r15_after = bus_read8(0x00, 0x301E) | (bus_read8(0x00, 0x301F) << 8);
        /* Read R0 to check computation results */
        uint16_t r0 = bus_read8(0x00, 0x3000) | (bus_read8(0x00, 0x3001) << 8);
        uint16_t ram3000 = bus_read16(0x70, 0x3000);
        printf(" → SFR=$%02X%02X R15=$%04X R0=$%04X RAM=$%04X\n",
               sfr_hi, sfr, r15_after, r0, ram3000);
        s_gsu_launch_count++;
    }

    /* Restore SCMR without RON+RAN (return bus to 65816) */
    bus_write8(0x00, 0x303A, scmr_base);

    g_cpu.DB = saved_db;
}
