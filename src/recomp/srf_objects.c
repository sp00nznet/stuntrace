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

    /* Process P1 objects */
    uint16_t p1_state = bus_wram_read16(0x0346);

    /* $08:C60F — Update object state for player
     * Reads object flags at $7E:2025+X, checks bit 2
     * If set, reads object ID from $7E:2022+X and
     * updates GSU object table at $70:0040+ */
    op_sep(0x20);
    op_rep(0x10);
    uint8_t obj_flags = bus_read8(0x7E, 0x2025 + p1_state);
    if (obj_flags & 0x04) {
        op_rep(0x20);
        uint16_t obj_id = bus_read16(0x7E, 0x2022 + p1_state);
        if (obj_id != 0) {
            /* Set object visibility flag in GSU RAM */
            uint16_t gsu_flags = bus_read16(0x70, 0x0040 + obj_id);
            bus_write16(0x70, 0x0040 + obj_id, gsu_flags | 0x0200);
        }
    }

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

        /* Process P2 objects */
        uint16_t p2_state = bus_wram_read16(0x034A);
        uint8_t p2_flags = bus_read8(0x7E, 0x2025 + p2_state);
        if (p2_flags & 0x04) {
            op_rep(0x20);
            uint16_t p2_obj = bus_read16(0x7E, 0x2022 + p2_state);
            if (p2_obj != 0) {
                uint16_t gsu_f = bus_read16(0x70, 0x0040 + p2_obj);
                bus_write16(0x70, 0x0040 + p2_obj, gsu_f | 0x0200);
            }
        }
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
