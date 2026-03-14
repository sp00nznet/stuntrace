/*
 * Stunt Race FX — Title screen & VRAM DMA engine
 *
 * The title screen uses BG Mode 3 (256-color BG1 + 4-color BG2).
 * Graphics data is prepared in GSU work RAM at bank $70 by
 * running a GSU program, then DMA'd to VRAM during forced blank.
 *
 * Key routines:
 *   $03:EB0E — PPU register setup for title screen mode
 *   $03:EB83 — VRAM DMA engine (table-driven)
 *   $03:D9B9 — Title screen scene setup (loads tiles/tilemaps/palettes)
 */

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * $03:EB0E — PPU mode setup for title screen
 *
 * Configures PPU for BG Mode 3:
 *   $2105 = $03 — Mode 3 (256-color BG1 + 4-color BG2)
 *   $2101 = $03 — OAM: 16x16 / 32x32 sprites, name base $03
 *   $2107 = $7A — BG1 tilemap at VRAM word addr $3D00
 *   $2108 = $5A — BG2 tilemap at VRAM word addr $2D00
 *   $210B = $60 — BG1 chars at $0000, BG2 chars at $6000
 *   Zeroes all window/color math registers
 */
void srf_03EB0E(void) {
    op_php();
    op_sep(0x20);
    op_rep(0x10);

    /* BG Mode 3 */
    bus_write8(0x00, 0x2105, 0x03);
    bus_write8(0x00, 0x2106, 0x00);

    /* OAM config */
    bus_write8(0x00, 0x2101, 0x03);

    /* Tilemap addresses */
    bus_write8(0x00, 0x2107, 0x7A);  /* BG1 tilemap */
    bus_write8(0x00, 0x2108, 0x5A);  /* BG2 tilemap */

    /* Character base addresses */
    bus_write8(0x00, 0x210B, 0x60);

    /* Clear CGRAM address and write first color (black) */
    bus_write8(0x00, 0x2121, 0x00);
    bus_write8(0x00, 0x2122, 0x00);
    bus_write8(0x00, 0x2122, 0x00);

    /* Clear sub-screen, window mask, and color math */
    bus_write8(0x00, 0x212D, 0x00);
    bus_write8(0x00, 0x212E, 0x00);
    bus_write8(0x00, 0x212F, 0x00);
    bus_write8(0x00, 0x2123, 0x00);
    bus_write8(0x00, 0x2124, 0x00);
    bus_write8(0x00, 0x2125, 0x00);
    bus_write8(0x00, 0x212A, 0x00);
    bus_write8(0x00, 0x212B, 0x00);
    bus_write8(0x00, 0x2130, 0x00);
    bus_write8(0x00, 0x2131, 0x00);
    bus_write8(0x00, 0x2132, 0x00);
    bus_write8(0x00, 0x2133, 0x00);

    /* Clear H/V timer */
    bus_write8(0x00, 0x4207, 0x00);
    bus_write8(0x00, 0x4208, 0x00);
    bus_write8(0x00, 0x4209, 0x00);

    op_plp();
}

/*
 * $03:EB83 — VRAM DMA engine
 *
 * Table-driven DMA transfer from GSU RAM ($70:3000) to VRAM.
 *
 * Input: X = pointer to DMA descriptor table (in bank $03)
 *        Y = pointer to parameter table
 *
 * Each DMA descriptor is 7 bytes:
 *   [0] = VRAM increment mode ($2115)
 *   [1] = VRAM address low ($2116)
 *   [2] = VRAM address high ($2117)
 *   [3] = DMA transfer mode ($4300)
 *   [4] = DMA B-bus register ($4301)
 *   [5] = DMA byte count low ($4305)
 *   [6] = DMA byte count high ($4306)
 *
 * Source is always from $70:3000 (GSU RAM).
 * The routine writes the parameter table pointer to GSU RAM
 * at $70:0068, then runs a GSU program to prepare the data,
 * and finally DMA's it to VRAM.
 */
void srf_03EB83(void) {
    uint16_t desc_x = g_cpu.X;
    uint16_t param_y = g_cpu.Y;

    /* Store parameter table pointer at GSU RAM $70:0068 */
    /* The Y register points to a table of addresses within bank $03 */
    bus_write8(0x70, 0x0068, (uint8_t)(desc_x & 0xFF));

    /* Store zeros at $70:002C and $70:00A2 */
    bus_write8(0x70, 0x002C, 0x00);
    bus_write8(0x70, 0x00A2, 0x00);

    /* Launch GSU program to prepare data in bank $70 */
    /* The GSU decompresses/processes graphics data from ROM
     * into its work RAM starting at $70:3000 */
    CPU_SET_A8(0x01);
    g_cpu.X = 0x42CF;  /* GSU program address */
    srf_GSU_launch();

    /* Now DMA from GSU RAM to VRAM */
    op_sep(0x20);
    op_rep(0x10);

    /* Restore X to descriptor table */
    g_cpu.X = desc_x;

    /* Read DMA descriptor and perform transfer */
    uint8_t vram_inc  = bus_read8(0x03, desc_x);
    uint8_t vram_lo   = bus_read8(0x03, desc_x + 1);
    uint8_t vram_hi   = bus_read8(0x03, desc_x + 2);
    uint8_t dma_mode  = bus_read8(0x03, desc_x + 3);
    uint8_t dma_bbus  = bus_read8(0x03, desc_x + 4);
    uint8_t count_lo  = bus_read8(0x03, desc_x + 5);
    uint8_t count_hi  = bus_read8(0x03, desc_x + 6);

    /* Set VRAM address and increment mode */
    bus_write8(0x00, 0x2115, vram_inc);
    bus_write8(0x00, 0x2116, vram_lo);
    bus_write8(0x00, 0x2117, vram_hi);

    /* Set DMA ch0 registers */
    bus_write8(0x00, 0x4300, dma_mode);
    bus_write8(0x00, 0x4301, dma_bbus);

    /* Source: $70:3000 (GSU RAM output buffer) */
    bus_write8(0x00, 0x4302, 0x00);
    bus_write8(0x00, 0x4303, 0x30);
    bus_write8(0x00, 0x4304, 0x70);

    /* Transfer count */
    bus_write8(0x00, 0x4305, count_lo);
    bus_write8(0x00, 0x4306, count_hi);

    /* Fire DMA */
    bus_write8(0x00, 0x420B, 0x01);
}

/*
 * $03:D996 — Title/attract screen setup (outer wrapper)
 *
 * Checks game state at $0D24 and clears some variables,
 * then calls $03:D9B9 for the actual setup.
 */
void srf_03D996(void) {
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x03;

    op_sep(0x20);
    uint8_t state = bus_wram_read8(0x0D24);

    if (state >= 0x03) {
        bus_wram_write8(0x0EED, 0x00);
        bus_wram_write8(0x0EEE, 0x00);
        bus_wram_write8(0x0EEF, 0x00);
        op_rep(0x20);
        bus_wram_write16(0x0E6C, 0x0000);
        bus_wram_write16(0x0E6E, 0x0000);
    }

    srf_03D9B9();
    g_cpu.DB = saved_db;
}

/*
 * $03:D9B9 — Title screen scene builder
 *
 * Sets up PPU registers, loads tile graphics, tilemaps, and
 * palettes by running GSU programs and DMA'ing results to VRAM.
 *
 * The loading sequence:
 * 1. $03:EB0E — Set PPU to BG Mode 3
 * 2. $03:EC6D — Disable NMI, read GSU status, set game state
 * 3. $03:DCEF — Set display config mode
 * 4. $03:EB83 — DMA tiles/tilemaps (multiple calls with different tables)
 * 5. $03:EC01 — Load and run GSU display program
 * 6. Various palette and OAM setup
 */
void srf_03D9B9(void) {
    op_php();
    op_sep(0x20);
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x03;

    /* Set PPU registers for title mode */
    srf_03EB0E();

    /* Look up display config from table */
    op_rep(0x30);
    uint8_t cfg_index = bus_wram_read8(0x0E3D);
    uint16_t tbl_offset = (uint16_t)cfg_index * 3;

    /* Load background tiles and tilemaps via DMA engine */
    /* First call: X = $F7DA, Y = $F80D — BG tile data */
    g_cpu.X = 0xF7DA;
    g_cpu.Y = 0xF80D;
    srf_03EB83();

    /* Second call: config-relative tiles */
    op_rep(0x30);
    uint16_t cfg_addr = 0xF4F9 + tbl_offset;
    g_cpu.X = cfg_addr;
    g_cpu.Y = 0xF853;
    srf_03EB83();

    /* Third call: X = $F7DD, Y = $F814 — more tile data */
    op_rep(0x30);
    g_cpu.X = 0xF7DD;
    g_cpu.Y = 0xF814;
    srf_03EB83();

    /* Load and run display GSU program */
    /* $03:EC01 — writes to $70:0068, $70:002C, $70:006A
     *            then calls $7E:E1F5 with A=$01, X=$42CF */
    op_rep(0x30);
    CPU_SET_A16(0xD970);
    g_cpu.X = 0x001D;

    /* Store address at GSU RAM */
    bus_write8(0x70, 0x0068, (uint8_t)(g_cpu.C & 0xFF));
    bus_write8(0x70, 0x0069, (uint8_t)((g_cpu.C >> 8) & 0xFF));
    bus_write8(0x70, 0x002C, 0x00);
    bus_write16(0x70, 0x006A, g_cpu.X);

    op_sep(0x20);
    CPU_SET_A8(0x01);
    g_cpu.X = 0x42CF;
    srf_GSU_launch();

    /* Enable NMI + auto-joypad */
    op_sep(0x20);
    bus_write8(0x00, 0x4200, 0x81);

    g_cpu.DB = saved_db;
    op_plp();
}
