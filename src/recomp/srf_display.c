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

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

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
 * $03:DD1B — Display mode DMA dispatcher
 *
 * Sets up PPU registers and performs VRAM DMA based on the
 * current display mode ($0D2B). Handles loading tile graphics,
 * tilemaps, and palettes appropriate for the current scene.
 *
 * The jump table at $DD56 dispatches to:
 *   $0D2B=0: $DDA4 — Title screen / mode 0
 *   $0D2B=1: $DDCE — ???
 *   $0D2B=2: $DD94 — Attract / demo
 *   $0D2B=3: $DD5E — Race mode (GSU framebuffer DMA)
 */
void srf_03DD1B(void) {
    op_php();
    op_sep(0x20);
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x03;

    /* $04:D6E1 — another audio/system call */
    /* For now skip this sub-call */

    uint8_t mode_4c = bus_wram_read8(0x0D4C);
    uint8_t mode_2b = bus_wram_read8(0x0D2B);

    /* Set display params based on mode */
    if (mode_4c != 0 && mode_2b == 0) {
        bus_wram_write8(0x0D89, 0x04);
    }

    /* Look up display config from table at $03:F4A5 */
    /* $03:EC24 returns bank in A, offset in X */
    op_rep(0x30);
    uint8_t cfg_index = bus_wram_read8(0x0E3D);
    uint16_t tbl_offset = (uint16_t)cfg_index * 3;
    uint16_t cfg_addr = bus_read16(0x03, 0xF4A5 + tbl_offset);
    uint8_t  cfg_bank = bus_read8(0x03, 0xF4A7 + tbl_offset);

    /* Dispatch based on display mode $0D2B */
    op_rep(0x30);
    uint16_t mode = bus_wram_read16(0x0D2B);

    switch (mode) {
    case 0x0000:
        /* Title screen mode — basic tile DMA */
        break;

    case 0x0003:
        /* Race mode — DMA GSU framebuffer from $70:3080 to VRAM $63F0 */
        {
            op_rep(0x20);
            /* DMA ch0: mode $01/$18 → VRAM data port */
            bus_write16(0x00, 0x4300, 0x1801);
            /* Source: $70:3080 */
            bus_write16(0x00, 0x4302, 0x3080);
            /* VRAM address = $63F0 */
            bus_write16(0x00, 0x2116, 0x63F0);
            /* Transfer count = $0020 (32 bytes) */
            bus_write16(0x00, 0x4305, 0x0020);
            op_sep(0x20);
            /* VRAM increment mode */
            bus_write8(0x00, 0x2115, 0x80);
            /* Source bank */
            bus_write8(0x00, 0x4304, 0x70);
            /* Trigger DMA */
            bus_write8(0x00, 0x420B, 0x01);
        }
        break;

    default:
        /* Other modes — stub for now */
        break;
    }

    /* Common post-dispatch setup */
    /* Set BG mode, tilemap addresses, etc. from config table */
    /* This will be expanded as more display modes are recompiled */

    op_plp();
    g_cpu.DB = saved_db;
}

/*
 * GSU program launcher — stub for $7E:E1F5
 *
 * The original routine in WRAM sets up GSU registers and writes
 * R15 high byte to start GSU program execution. The GSU then
 * renders the current frame to its work RAM at bank $70.
 *
 * A = program index, X = parameter table address
 */
void srf_GSU_launch(void) {
    uint8_t program = CPU_A8();
    uint16_t params = g_cpu.X;

    /* The full implementation would:
     * 1. Look up GSU program address from table
     * 2. Set ROMBR, RAMBR
     * 3. Write program address to R15 (low then high)
     * 4. Writing R15 high byte triggers GSU execution
     * 5. Wait for GSU to STOP
     *
     * For now, trigger GSU via bus API if available */
    if (bus_has_gsu()) {
        /* Set PBR = program bank */
        bus_gsu_write(0x3034, program);

        /* The actual program addresses and setup will be
         * recompiled as we trace the GSU launch routine
         * from $7E:E1F5 */
    }
}
