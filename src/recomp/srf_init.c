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

#include <stdio.h>
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
    func_table_register(0x04D649, srf_04D649);  /* audio command queue */
    func_table_register(0x04D6E1, srf_04D6E1);  /* audio state clear */
    func_table_register(0x04D0DB, srf_04D0DB);  /* audio/music reload */
    func_table_register(0x04D720, srf_04D720);  /* IPL transfer */
    func_table_register(0x03B011, srf_03B011);  /* rotation matrix */
    func_table_register(0x02DF79, srf_02DF79);  /* PRNG */
    func_table_register(0x02DAD6, srf_02DAD6);  /* camera setup */
    func_table_register(0x02D53D, srf_02D53D);  /* palette copy */
    func_table_register(0x02D55F, srf_02D55F);  /* palette fade */
    func_table_register(0x02DB59, srf_02DB59);  /* GSU palette launch */
    func_table_register(0x7EE258, srf_7EE258);  /* P2 GSU render */
    func_table_register(0x7F112F, srf_7F112F);  /* gameplay audio sync */
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
    func_table_register(0x03B48C, srf_03B48C);  /* object table setup */
    func_table_register(0x03F02B, srf_03F02B);  /* palette checksum */
    func_table_register(0x03D8B3, srf_03D8B3);  /* display-mode init */
    func_table_register(0x03B3DA, srf_03B3DA);  /* scene state init */
    func_table_register(0x03CB5C, srf_03CB5C);  /* object alloc */
    func_table_register(0x03CB25, srf_03CB25);  /* object init */
    func_table_register(0x03B8A1, srf_03B8A1);  /* object dealloc */
    func_table_register(0x03D306, srf_03D306);  /* camera angle calc */
    func_table_register(0x03D388, srf_03D388);  /* object processing */
    func_table_register(0x08801C, srf_08801C);  /* object command dispatch */
    func_table_register(0x0894A1, srf_0894A1);  /* vehicle race callback */
    func_table_register(0x08951B, srf_08951B);  /* vehicle collision callback */
    func_table_register(0x08D8C2, srf_08D8C2);  /* GSU camera sync */
    func_table_register(0x08D86F, srf_08D86F);  /* object→GSU sync */
    func_table_register(0x08CD25, srf_08CD25);  /* object callback chain */
    func_table_register(0x08CC7C, srf_08CC7C);  /* vehicle creation */
    func_table_register(0x0888C7, srf_0888C7);  /* vehicle model setup */
    func_table_register(0x08CF41, srf_08CF41);  /* collision response */
    func_table_register(0x08CF92, srf_08CF92);  /* collision check */
    func_table_register(0x0883CC, srf_0883CC);  /* render list rehash */
    func_table_register(0x08D070, srf_08D070);  /* object validity check */
    func_table_register(0x08CE02, srf_08CE02);  /* collision state sync */
    func_table_register(0x08CCA3, srf_08CCA3);  /* GSU flag setup */
    func_table_register(0x08CCBE, srf_08CCBE);  /* GSU anim frame */
    func_table_register(0x08CCD2, srf_08CCD2);  /* GSU position write */
    func_table_register(0x08CCF1, srf_08CCF1);  /* GSU flag OR */
    func_table_register(0x08B863, srf_08B863);  /* viewport render order */
    func_table_register(0x08C60F, srf_08C60F);  /* object state update */
    func_table_register(0x08B4C6, srf_08B4C6);  /* object render setup */
    func_table_register(0x088364, srf_088364);  /* render list insert */
    func_table_register(0x088392, srf_088392);  /* render list remove */
    func_table_register(0x08C5A5, srf_08C5A5);  /* object system main */
    func_table_register(0x09ECE0, srf_09ECE0);  /* WRAM patches */
    func_table_register(0x02E289, srf_02E289);  /* display mode setup */
    func_table_register(0x08B893, srf_08B893);  /* viewport config */
    func_table_register(0x0BB479, srf_0BB479);  /* entity system init */
    func_table_register(0x0BB4C3, srf_0BB4C3);  /* entity allocator */
    func_table_register(0x0BB450, srf_0BB450);  /* entity callback dispatch */
    func_table_register(0x0BB64A, srf_0BB64A);  /* sprite compositor */
    func_table_register(0x0BE390, srf_0BE390);  /* VBlank wait */
    func_table_register(0x0BFA24, srf_0BFA24);  /* gameplay scene setup */
    func_table_register(0x0BFB26, srf_0BFB26);  /* gameplay frame body */
    func_table_register(0x03863D, srf_03863D);  /* scene change A */
    func_table_register(0x038648, srf_038648);  /* scene change B */
    func_table_register(0x038653, srf_038653);  /* scene change C */
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
    printf("Init: PPU/register init...\n");
    srf_0389B4();
    printf("Init: PPU done.\n");

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

    printf("Init: WRAM clear done.\n");

    /* ── Clear GSU work RAM $70:0000-$70:27FF ────────── */
    /* MVN $70,$70 — block move */
    op_rep(0x30);
    bus_write8(0x70, 0x0000, 0x00);  /* seed first byte */
    /* The original uses MVN to fill; we just memset via bus */
    for (uint32_t i = 1; i <= 0x27FF; i++) {
        bus_write8(0x70, (uint16_t)i, 0x00);
    }

    printf("Init: GSU RAM clear done.\n");

    /* ── Upload SPC700 audio engine ────────────────────── */
    printf("Init: Uploading SPC700 audio engine...\n");
    /* JSL $04D44C */
    srf_04D44C();
    printf("Init: SPC700 upload done.\n");

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

    /* ── Game scene initialization ($8B96-$8C62) ─────── */
    printf("Init: HW setup complete, starting scene init...\n");

    /* $0B:AE0A — Title screen state machine (initial call) */
    srf_0BAE0A();

    /* Set game state flags */
    op_rep(0x30);
    bus_wram_write16(0x1915, 0x0003);
    bus_wram_write16(0x1917, 0x0000);

    /* Reset stack (no-op in recomp, stack is managed by C) */
    g_cpu.DB = 0x03;

    op_sep(0x20);
    bus_wram_write8(0x0E4E, 0x00);  /* audio unmute */
    bus_wram_write8(0x117A, 0x02);  /* controller config */
    bus_wram_write8(0x117B, 0x01);

    /* $03:B3DA — Scene state initialization */
    srf_03B3DA();

    /* $03:D8B3 — Display-mode-specific initialization */
    srf_03D8B3();

    /* $08:B893 — Viewport config init */
    srf_08B893();

    /* Disable HDMA, force blank, disable interrupts */
    g_cpu.DB = 0x03;
    op_sep(0x20);
    bus_write8(0x00, 0x420C, 0x00);
    bus_write8(0x00, 0x2100, 0x80);
    OP_SEI();

    op_sep(0x20);
    bus_wram_write8(0x0E4E, 0x00);

    /* $02:E289 — Display mode setup (NMI state, brightness) */
    srf_02E289();

    /* Force blank, disable HDMA, disable interrupts */
    op_sep(0x20);
    bus_write8(0x00, 0x420C, 0x00);
    bus_write8(0x00, 0x2100, 0x80);
    OP_SEI();

    /* $03:D996 — Title setup wrapper */
    srf_03D996();

    g_cpu.DB = 0x03;
    op_sep(0x20);
    bus_write8(0x00, 0x420C, 0x00);
    bus_write8(0x00, 0x2100, 0x80);
    OP_SEI();

    /* $09:ECE0 — WRAM jump table patches */
    srf_09ECE0();

    /* CLSR high-speed, init flag */
    op_sep(0x20);
    op_rep(0x10);
    bus_write8(0x00, 0x3039, 0x01);
    bus_write8(0x7E, 0xFF65, 0x01);

    OP_SEI();

    printf("Init: Scene state done. $0D2B=%d $0D62=%d $0D3F=$%02X\n",
           bus_wram_read8(0x0D2B), bus_wram_read8(0x0D62), bus_wram_read8(0x0D3F));

    /* $02:CF45 — Full screen setup (calls display dispatcher) */
    printf("Init: Running screen setup ($02:CF45)...\n");
    srf_02CF45();

    /* $03:EC24 — Look up scene's object command table
     * Returns: A = bank, X = ROM offset for the command table */
    op_rep(0x30);
    {
        uint8_t cfg_idx = bus_wram_read8(0x0E3D);
        uint16_t tbl_offset = (uint16_t)cfg_idx * 3;
        uint16_t cmd_addr = bus_read16(0x03, 0xF4A5 + tbl_offset) - 0x8000;
        uint8_t cmd_bank = bus_read8(0x03, 0xF4A7 + tbl_offset);
        g_cpu.X = cmd_addr;
        CPU_SET_A8(cmd_bank);
        printf("Init: Object command table at $%02X:%04X\n", cmd_bank, cmd_addr + 0x8000);
    }

    /* $08:801C — Object command dispatcher
     * Reads commands from the ROM table and creates GSU 3D objects. */
    printf("Init: Running object command dispatcher ($08:801C)...\n");
    srf_08801C();
    printf("Init: Object commands done.\n");

    /* Clear GSU state flags */
    op_sep(0x20);
    bus_write8(0x70, 0x023A, 0x00);
    bus_wram_write8(0x1A32, 0x00);
    bus_wram_write8(0x05E9, 0x00);  /* clear master frame counter */
    bus_wram_write8(0x0713, 0x01);  /* frame timeout flag */

    printf("Init: Screen setup done. $0D3F=$%02X $0D61=$%02X $2100=$%02X\n",
           bus_wram_read8(0x0D3F), bus_wram_read8(0x0D61),
           bus_read8(0x00, 0x2100));

    /* Launch GSU program $04:8800 (initial 3D scene setup) */
    printf("Init: Launching GSU $04:8800...\n");
    op_sep(0x20);
    op_rep(0x10);
    CPU_SET_A8(0x04);
    g_cpu.X = 0x8800;
    srf_GSU_launch();
    printf("Init: GSU $04:8800 complete.\n");

    /* Set processing flag, clear IRQ, enable interrupts */
    op_sep(0x20);
    bus_wram_write8(0x0357, 0x01);
    bus_read8(0x00, 0x4211);
    OP_CLI();

    printf("Init complete. Entering main loop. $0D3F=$%02X $0D62=$%02X\n",
           bus_wram_read8(0x0D3F), bus_wram_read8(0x0D62));

    /* ── Enter main game loop ──────────────────────── */
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

        /* ── Start button detection during attract ──────────
         * Check for Start press (bit 4 of $0311 = new button edges).
         * When detected, transition from attract to gameplay. */
        op_rep(0x20);
        uint16_t new_buttons = bus_wram_read16(0x0311);
        if (new_buttons & 0x1000) {  /* Start button */
            /* Disable display for transition */
            op_sep(0x20);
            bus_write8(0x00, 0x2100, 0x80);
            bus_write8(0x00, 0x4200, 0x00);
            bus_write8(0x00, 0x420C, 0x00);

            /* Set game mode to gameplay */
            bus_wram_write8(0x0D62, 0x01);
            bus_wram_write8(0x0D2B, 0x03);  /* race display mode */

            /* Reinitialize for gameplay */
            srf_03B3DA();
            srf_03D8B3();
            srf_02E289();
            srf_02CF45();

            op_sep(0x20);
            bus_wram_write8(0x0357, 0x01);
            bus_read8(0x00, 0x4211);
            OP_CLI();
        }

        /* ── Attract mode auto-cycle ($0B:AEF2 logic) ─────
         * In the original, $0B:AEF2 runs its own frame loop during
         * init. In the recomp, we check the attract timer each frame.
         * After 1024 frames (~17 sec), cycle to next demo scene.
         * Uses $1913 as frame counter, $1915 as scene index (0-2). */
        op_rep(0x20);
        uint16_t attract_timer = bus_wram_read16(0x1913);
        attract_timer++;
        bus_wram_write16(0x1913, attract_timer);

        if (attract_timer >= 0x0400) {
            /* Timer expired — advance to next demo scene */
            bus_wram_write16(0x1913, 0x0000);

            op_sep(0x20);
            bus_wram_write8(0x10DA, 0xFF);  /* flag for scene transition */

            op_rep(0x20);
            uint16_t scene_idx = bus_read16(0x00, 0x1915);
            scene_idx = (scene_idx + 1) & 0x0003;
            if (scene_idx == 0x0003) scene_idx = 0x0000;
            bus_write16(0x00, 0x1915, scene_idx);

            /* Call appropriate scene change function */
            switch (scene_idx) {
            case 0: srf_03863D(); break;
            case 1: srf_038648(); break;
            case 2: srf_038653(); break;
            }

            /* Reinitialize scene after change */
            srf_03B3DA();
            srf_03D8B3();
            srf_02E289();
            srf_02CF45();

            /* Re-launch GSU for new scene */
            op_sep(0x20);
            op_rep(0x10);
            CPU_SET_A8(0x04);
            g_cpu.X = 0x8800;
            srf_GSU_launch();

            op_sep(0x20);
            bus_wram_write8(0x0357, 0x01);
            bus_read8(0x00, 0x4211);
            OP_CLI();
        }
    }

    /* Check frame counter threshold for timeout logic
     * Original: if $05E9 >= $1E (30 frames), clear $0713 */
    op_sep(0x20);
    uint8_t fc = bus_wram_read8(0x05E9);
    if (fc >= 0x1E) {
        bus_wram_write8(0x0713, 0x00);
    }
}
