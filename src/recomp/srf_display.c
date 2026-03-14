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
void srf_GSU_launch(void) {
    uint8_t program_bank = CPU_A8();
    uint16_t program_addr = g_cpu.X;

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
     * STX $301E writes both low and high bytes of R15.
     * Writing the high byte ($301F) sets the GO flag. */
    bus_write8(0x00, 0x301E, (uint8_t)(program_addr & 0xFF));
    bus_write8(0x00, 0x301F, (uint8_t)(program_addr >> 8));

    /* The GSU is now running. In the original hardware, the 65816
     * spins checking the GO flag in SFR ($3030 bit 5).
     * In our emulation, gsu_write triggers gsu_run() which executes
     * until STOP, so by the time we get here the GSU has finished. */

    /* Verify GSU has stopped (read SFR, check GO flag) */
    uint8_t sfr = bus_read8(0x00, 0x3030);
    /* GO flag should be clear after STOP */

    /* Restore SCMR without RON+RAN (return bus to 65816) */
    bus_write8(0x00, 0x303A, scmr_base);

    g_cpu.DB = saved_db;
}
