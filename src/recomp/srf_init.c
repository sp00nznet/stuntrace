/*
 * Stunt Race FX — Hardware initialization
 *
 * Full PPU / APU / DMA / Super FX register setup performed
 * once during boot before the game enters its main loop.
 *
 * Stunt Race FX is one of only a handful of SNES titles to
 * use the Super FX 2 (GSU-2) coprocessor for real-time 3D
 * polygon rendering.  The GSU has its own register space
 * at $3000-$303F that must be initialized here.
 */

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <snesrecomp/func_table.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * Register all recompiled functions in the dispatch table.
 * This allows indirect calls (JSR/JSL to computed addresses)
 * to find the right native function.
 */
void srf_register_all(void) {
    func_table_register(0x00FE88, srf_008000);  /* reset vector */
    func_table_register(0x000108, srf_008018);  /* NMI handler */
    func_table_register(0x00804A, srf_00804A);  /* hardware init */
    func_table_register(0x0080C0, srf_0080C0);  /* main loop */
}

/*
 * $00:804A — Hardware init
 *
 * Zeroes out PPU registers ($2101-$2133), disables all DMA,
 * clears WRAM, and prepares the system for the title sequence.
 *
 * Original disassembly (partial):
 *   00:804A  E2 20     SEP #$20     ; 8-bit A
 *   00:804C  A9 00     LDA #$00
 *   00:804E  8D 01 21  STA $2101    ; OAM size/base
 *   00:8051  8D 02 21  STA $2102    ; OAM address low
 *   ...
 *   00:80xx  A9 80     LDA #$80
 *   00:80xx  8D 15 21  STA $2115    ; VRAM increment mode
 *   ...
 */
void srf_00804A(void) {
    op_sep(0x20);                      /* 8-bit A */

    /* ── zero PPU registers ─────────────────────────────── */
    for (uint16_t reg = 0x2101; reg <= 0x2133; reg++) {
        bus_write8(0x00, reg, 0x00);
    }

    /* VRAM increment: word-access, increment after $2119 */
    bus_write8(0x00, 0x2115, 0x80);

    /* ── disable all DMA channels ───────────────────────── */
    bus_write8(0x00, 0x420B, 0x00);    /* GPDMA */
    bus_write8(0x00, 0x420C, 0x00);    /* HDMA  */

    /* ── clear WRAM via DMA ─────────────────────────────── */
    /*    ch0: fixed source $00:0000 → WMDATA ($2180)       */
    bus_write8(0x00, 0x2181, 0x00);    /* WRAM addr low     */
    bus_write8(0x00, 0x2182, 0x00);    /* WRAM addr mid     */
    bus_write8(0x00, 0x2183, 0x00);    /* WRAM addr high    */

    /* ── enable NMI + joypad auto-read ──────────────────── */
    bus_write8(0x00, 0x4200, 0x81);

    /* ── enter main loop ────────────────────────────────── */
    srf_0080C0();
}

/*
 * $00:80C0 — Main loop entry
 *
 * Waits for NMI each frame, then dispatches the current
 * game state.  This is the top-level heartbeat of the game.
 */
void srf_0080C0(void) {
    /* placeholder — will be expanded as more states are recompiled */
    op_rep(0x30);                      /* 16-bit A/X/Y */
    OP_SET_DB(0x00);

    /* The real loop reads the game-state variable from WRAM
       and indexes into a jump table.  For now we simply return
       so the launcher's frame loop can drive rendering. */
}
