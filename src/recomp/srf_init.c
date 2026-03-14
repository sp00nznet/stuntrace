/*
 * Stunt Race FX — Hardware initialization & main loop
 *
 * Recompiled from the actual ROM disassembly.
 *
 * Init chain: $00:FE88 → $03:8AA9 → $03:89B4 (HW init)
 *                                  → $03:8CF6 (WRAM clear)
 *                                  → DMA code/data to WRAM
 *                                  → GSU setup ($3037-$3039)
 *                                  → main loop at $03:8C63
 */

#include <string.h>
#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <snesrecomp/func_table.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * Register all recompiled functions in the dispatch table.
 */
void srf_register_all(void) {
    func_table_register(0x00FE88, srf_00FE88);  /* reset vector */
    func_table_register(0x028000, srf_028000);  /* real NMI handler */
    func_table_register(0x0389B4, srf_0389B4);  /* PPU/register init */
    func_table_register(0x038AA9, srf_038AA9);  /* full init entry */
    func_table_register(0x038CF6, srf_038CF6);  /* WRAM DMA clear */
    func_table_register(0x038C63, srf_038C63);  /* main loop */
    func_table_register(0x04D44C, srf_04D44C);  /* SPC700 audio upload */
    func_table_register(0x04D720, srf_04D720);  /* IPL transfer */
    func_table_register(0x02D65A, srf_02D65A);  /* brightness control */
    func_table_register(0x02D7AB, srf_02D7AB);  /* scanline wait */
    func_table_register(0x02CF45, srf_02CF45);  /* screen setup */
    func_table_register(0x03DD1B, srf_03DD1B);  /* display mode dispatch */
    func_table_register(0x03EB0E, srf_03EB0E);  /* PPU mode setup */
    func_table_register(0x03EB83, srf_03EB83);  /* VRAM DMA engine */
    func_table_register(0x03D996, srf_03D996);  /* title setup wrapper */
    func_table_register(0x03D9B9, srf_03D9B9);  /* title scene builder */
    func_table_register(0x02E0A9, srf_02E0A9);  /* per-frame dispatch */
    func_table_register(0x02D7CD, srf_02D7CD);  /* attract frame body */
    func_table_register(0x0BAE0A, srf_0BAE0A);  /* title state machine */
    func_table_register(0x0BAE8F, srf_0BAE8F);  /* input check */
    func_table_register(0x03D306, srf_03D306);  /* camera angle calc */
    func_table_register(0x03D388, srf_03D388);  /* object processing */
    func_table_register(0x08C5A5, srf_08C5A5);  /* object system main */
    func_table_register(0x09ECE0, srf_09ECE0);  /* WRAM patches */
    func_table_register(0x02E289, srf_02E289);  /* display mode setup */
    func_table_register(0x08B893, srf_08B893);  /* viewport config */
    func_table_register(0x0BFB26, srf_0BFB26);  /* gameplay frame body */
    func_table_register(0x038683, srf_038683);  /* scene config loader */
    func_table_register(0x03865E, srf_03865E);  /* scene reset */
    func_table_register(0x038C86, srf_038C86);  /* full game restart */
}

/*
 * $03:89B4 — Full PPU / CPU register initialization
 *
 * Sets $2100 = $8F (forced blank + max brightness)
 * Zeroes all PPU registers $2101-$2133
 * Sets VRAM increment mode, Mode 7 identity matrix
 * Zeroes all CPU I/O registers $4200-$420D
 */
void srf_0389B4(void) {
    op_sep(0x30);  /* 8-bit A, 8-bit X/Y */

    /* $2100 = $8F — force blank, brightness 15 */
    bus_write8(0x00, 0x2100, 0x8F);

    /* Zero PPU registers $2101-$2114 (twice for double-write regs) */
    uint16_t ppu_regs[] = {
        0x2101, 0x2102, 0x2103, 0x2105, 0x2106,
        0x2107, 0x2108, 0x2109, 0x210A, 0x210B, 0x210C,
    };
    for (int i = 0; i < 11; i++) {
        bus_write8(0x00, ppu_regs[i], 0x00);
    }

    /* BG scroll registers — write twice (low/high) */
    for (uint16_t reg = 0x210D; reg <= 0x2114; reg++) {
        bus_write8(0x00, reg, 0x00);
        bus_write8(0x00, reg, 0x00);
    }

    /* VRAM increment mode = $80 (word access, inc after $2119) */
    bus_write8(0x00, 0x2115, 0x80);

    /* VRAM address = $0000 */
    bus_write8(0x00, 0x2116, 0x00);
    bus_write8(0x00, 0x2117, 0x00);

    /* Mode 7 registers */
    bus_write8(0x00, 0x211A, 0x00);

    /* Mode 7 matrix: identity (A=1, B=0, C=0, D=1) */
    bus_write8(0x00, 0x211B, 0x00);  /* M7A low  = $00 */
    bus_write8(0x00, 0x211B, 0x01);  /* M7A high = $01 → A = $0100 = 1.0 */
    bus_write8(0x00, 0x211C, 0x00);  /* M7B = $0000 */
    bus_write8(0x00, 0x211C, 0x00);
    bus_write8(0x00, 0x211D, 0x00);  /* M7C = $0000 */
    bus_write8(0x00, 0x211D, 0x00);
    bus_write8(0x00, 0x211E, 0x00);  /* M7D low  = $00 */
    bus_write8(0x00, 0x211E, 0x01);  /* M7D high = $01 → D = $0100 = 1.0 */
    bus_write8(0x00, 0x211F, 0x00);  /* M7X = $0000 */
    bus_write8(0x00, 0x211F, 0x00);
    bus_write8(0x00, 0x2120, 0x00);  /* M7Y = $0000 */
    bus_write8(0x00, 0x2120, 0x00);

    /* CGRAM address = 0 */
    bus_write8(0x00, 0x2121, 0x00);

    /* Window / color math registers */
    for (uint16_t reg = 0x2122; reg <= 0x212B; reg++) {
        bus_write8(0x00, reg, 0x00);
    }
    bus_write8(0x00, 0x212C, 0x00);  /* main screen designation */
    bus_write8(0x00, 0x212D, 0x00);  /* sub screen designation */

    /* Color math */
    bus_write8(0x00, 0x212E, 0x00);
    bus_write8(0x00, 0x2130, 0x30);  /* color add/sub settings */
    bus_write8(0x00, 0x2131, 0x00);
    bus_write8(0x00, 0x2132, 0xE0);  /* fixed color = black */
    bus_write8(0x00, 0x2133, 0x00);  /* screen mode = normal */

    /* CPU I/O registers */
    bus_write8(0x00, 0x4200, 0x00);  /* NMI/IRQ disable */
    bus_write8(0x00, 0x4201, 0xFF);  /* I/O port = $FF */
    for (uint16_t reg = 0x4202; reg <= 0x420D; reg++) {
        bus_write8(0x00, reg, 0x00);
    }
    /* RTL */
}

/*
 * $03:8CF6 — WRAM DMA clear utility
 *
 * Clears a block of WRAM via DMA channel 0.
 * A = bank, X = WRAM address, Y = byte count
 *
 * Sets up DMA ch0: fixed source → WMDATA ($2180)
 */
void srf_038CF6(void) {
    uint8_t bank = CPU_A8();
    uint16_t wram_addr = g_cpu.X;
    uint16_t count = g_cpu.Y;

    /* set WRAM target address */
    bus_write8(0x00, 0x2181, (uint8_t)(wram_addr & 0xFF));
    bus_write8(0x00, 0x2182, (uint8_t)(wram_addr >> 8));
    bus_write8(0x00, 0x2183, bank & 0x01);

    /* DMA ch0: mode = $08 (fixed, A→B), B-bus = $80 (WMDATA) */
    bus_write8(0x00, 0x4300, 0x08);  /* transfer mode: fixed */
    bus_write8(0x00, 0x4301, 0x80);  /* B-bus address: WMDATA */

    /* source address — use address $0000 in bank $00 as zero source */
    bus_write8(0x00, 0x4302, 0x00);
    bus_write8(0x00, 0x4303, 0x00);
    bus_write8(0x00, 0x4304, 0x03);  /* source bank */

    /* transfer count */
    bus_write8(0x00, 0x4305, (uint8_t)(count & 0xFF));
    bus_write8(0x00, 0x4306, (uint8_t)(count >> 8));

    /* trigger DMA */
    bus_write8(0x00, 0x420B, 0x01);
}

/*
 * $03:8AA9 — Full initialization entry point
 *
 * This is where the reset vector jumps to. It:
 * 1. Enters native mode, sets stack/DP/DB
 * 2. Calls $03:89B4 for PPU/register init
 * 3. Clears direct page ($0000-$02FF) with a loop
 * 4. Clears WRAM banks $7E:2000-$7E:FFFF and $7F:0000-$7F:FFFF via DMA
 * 5. Clears GSU work RAM ($70:0000-$70:27FF) via block move
 * 6. Copies NMI handler code to WRAM via DMA ($02:8000 → $7E:A2D9)
 * 7. Copies vector table stub to $00:0101 (NMI trampoline)
 * 8. Sets up GSU registers (CFGR, SCBR, CLSR)
 * 9. Enters main loop
 */
void srf_038AA9(void) {
    /* SEP #$30 / SEI / CLC / XCE — already done by reset vector */
    op_sep(0x30);
    OP_SEI();
    OP_CLC();
    op_xce();

    /* PHK / PLB — set DB to current bank ($03) */
    g_cpu.DB = 0x03;

    /* REP #$30 — 16-bit A/X/Y */
    op_rep(0x30);

    /* LDX #$02FF / TXS — stack at $02FF */
    g_cpu.X = 0x02FF;
    g_cpu.S = g_cpu.X;

    /* LDA #$0000 / TCD — direct page at $0000 */
    CPU_SET_A16(0x0000);
    g_cpu.DP = g_cpu.C;

    /* ── Call PPU/register init ──────────────────────── */
    srf_0389B4();

    /* CLI — enable interrupts */
    OP_CLI();

    /* ── Clear direct page $0000-$02FE ──────────────── */
    op_sep(0x20);   /* 8-bit A */
    op_rep(0x10);   /* 16-bit X/Y */
    for (uint16_t i = 0; i <= 0x02FE; i++) {
        bus_wram_write8(i, 0x00);
    }

    /* ── Reset stack and DP again ────────────────────── */
    op_rep(0x20);
    CPU_SET_A16(0x0000);
    g_cpu.DP = g_cpu.C;
    CPU_SET_A16(0x02FF);
    g_cpu.S = g_cpu.C;
    g_cpu.DB = 0x03;  /* PHK/PLB */

    /* ── WRAM clear: $7E:2000-$7E:FFFF ──────────────── */
    op_sep(0x20);
    op_rep(0x10);
    CPU_SET_A8(0x00);
    g_cpu.X = 0x0300;
    g_cpu.Y = 0x2100;
    srf_038CF6();  /* clear PPU register area */

    CPU_SET_A8(0x7E);
    g_cpu.X = 0x2000;
    g_cpu.Y = 0xE000;
    srf_038CF6();  /* clear WRAM $7E:2000-$7E:FFFF */

    CPU_SET_A8(0x7F);
    g_cpu.X = 0x0000;
    g_cpu.Y = 0xFFFF;
    srf_038CF6();  /* clear WRAM $7F:0000-$7F:FFFF */

    /* ── Clear GSU work RAM $70:0000-$70:27FF ────────── */
    /* MVN $70,$70 — block move */
    op_rep(0x30);
    bus_write8(0x70, 0x0000, 0x00);  /* seed first byte */
    /* The original uses MVN to fill; we just memset via bus */
    for (uint32_t i = 1; i <= 0x27FF; i++) {
        bus_write8(0x70, (uint16_t)i, 0x00);
    }

    /* ── Upload SPC700 audio engine ────────────────────── */
    /* JSL $04D44C */
    srf_04D44C();

    /* ── Copy vector table stub to WRAM $00:0101 ─────── */
    /* Source: $03:8D16, 16 bytes → WRAM $0101 */
    /* This creates the NMI trampoline: NOP*3 + JSL $7EA2D9 + RTI */
    op_rep(0x30);
    const uint8_t vector_stub[] = {
        0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0xEA, 0x40,  /* IRQ: 7xNOP + RTI */
        0xEA, 0xEA, 0xEA, 0x22, 0xD9, 0xA2, 0x7E, 0x40   /* NMI: 3xNOP + JSL $7EA2D9 + RTI */
    };
    for (int i = 0; i < 16; i++) {
        bus_wram_write8(0x0101 + i, vector_stub[i]);
    }

    /* ── DMA: copy NMI handler code to WRAM ──────────── */
    /* $02:8000 (ROM) → $7E:A2D9 (WRAM), 0x54A6 bytes */
    op_sep(0x20);
    op_rep(0x10);

    /* DMA ch0: A→B, fixed increment */
    bus_write8(0x00, 0x4300, 0x00);  /* mode: inc A, write B */
    bus_write8(0x00, 0x4301, 0x80);  /* B-bus: WMDATA ($2180) */

    /* WRAM target = $7E:A2D9 */
    bus_write8(0x00, 0x2181, 0xD9);
    bus_write8(0x00, 0x2182, 0xA2);
    CPU_SET_A8(0x7E);
    bus_write8(0x00, 0x2183, 0x00);  /* bank bit (0 = $7E) */

    /* Source = $02:8000 */
    g_cpu.X = 0x8000;
    bus_write8(0x00, 0x4302, 0x00);
    bus_write8(0x00, 0x4303, 0x80);
    CPU_SET_A8(0x02);
    bus_write8(0x00, 0x4304, 0x02);

    /* Count = $54A6 */
    g_cpu.Y = 0x54A6;
    bus_write8(0x00, 0x4305, 0xA6);
    bus_write8(0x00, 0x4306, 0x54);

    /* Trigger DMA */
    bus_write8(0x00, 0x420B, 0x01);

    /* ── DMA: copy more data to WRAM $7F:0800 ────────── */
    bus_write8(0x00, 0x4300, 0x00);
    bus_write8(0x00, 0x4301, 0x80);
    bus_write8(0x00, 0x2181, 0x00);
    bus_write8(0x00, 0x2182, 0x08);
    bus_write8(0x00, 0x2183, 0x01);  /* bank bit 1 = $7F */
    bus_write8(0x00, 0x4302, 0x00);
    bus_write8(0x00, 0x4303, 0x80);
    bus_write8(0x00, 0x4304, 0x0B);  /* source bank $0B */
    bus_write8(0x00, 0x4305, 0x00);
    bus_write8(0x00, 0x4306, 0x2C);  /* count = $2C00 */
    bus_write8(0x00, 0x420B, 0x01);

    /* ── Store game state flags ──────────────────────── */
    op_sep(0x20);
    bus_wram_write8(0x0374, 0x01);

    /* ── GSU setup ───────────────────────────────────── */
    /* $3039 = CLSR = $01 (high speed clock) */
    bus_write8(0x00, 0x3039, 0x01);

    /* $3037 = CFGR = $20 */
    bus_write8(0x00, 0x3037, 0x20);

    /* $3038 = SCBR = $0B (screen base at $0B * 1024 in GSU RAM) */
    bus_write8(0x00, 0x3038, 0x0B);

    /* ── Continue to main game init and loop ─────────── */
    srf_038C63();
}

/*
 * $03:8C63 — Main game loop
 *
 * This is the top-level frame loop. Each iteration:
 * 1. Calls the per-frame game logic routine ($02:E0A9)
 * 2. Checks game state and dispatches accordingly
 * 3. Loops until frame counter reaches threshold
 *
 * For the recompilation, we return after one frame so the
 * launcher's frame loop can drive rendering.
 */
void srf_038C63(void) {
    op_sep(0x20);
    op_rep(0x10);

    /* Read $4211 to clear IRQ flag */
    bus_read8(0x00, 0x4211);

    /* Enable NMI + auto-joypad read */
    bus_write8(0x00, 0x4200, 0x81);

    /* Per-frame dispatch — manages fade timing */
    srf_02E0A9();

    /* Dispatch based on game mode:
     * $0D62 == 0: attract mode → $02:D7CD
     * $0D62 != 0: gameplay/menu → $0B:FB26 */
    op_sep(0x20);
    uint8_t display_mode = bus_wram_read8(0x0D62);
    if (display_mode != 0) {
        srf_0BFB26();
    } else {
        srf_02D7CD();
    }

    /* Check frame counter threshold for timeout logic
     * Original: if $05E9 >= $1E (30 frames), clear $0713 */
    op_sep(0x20);
    uint8_t fc = bus_wram_read8(0x05E9);
    if (fc >= 0x1E) {
        bus_wram_write8(0x0713, 0x00);
    }
}
