/*
 * Stunt Race FX — Boot sequence & NMI handler
 *
 * Recompiled from WDC 65C816 + Super FX 2 (GSU-2) assembly.
 * ROM: Stunt Race FX (USA) — LoROM + Super FX, 1 MB
 *
 * ROM vectors (from header at $7FEA-$7FFF):
 *   Reset: $FE88
 *   NMI:   $0108  → JSL $7E:A2D9 (WRAM, copied from $02:8000)
 *   IRQ:   $010C
 *
 * The NMI vector in ROM points to a WRAM trampoline at $0108,
 * which was set up by the init code copying a small stub from
 * $03:8D16. The real NMI handler lives at $7E:A2D9, which was
 * DMA-copied from ROM bank $02:$8000 (0x54A6 bytes).
 */

#include <string.h>
#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * $00:FE88 — Reset vector entry point
 *
 *   FE88: 78        SEI
 *   FE89: 18        CLC
 *   FE8A: FB        XCE          ; switch to native mode
 *   FE8B: D8        CLD          ; clear decimal mode
 *   FE8C: 5C A9 8A 03  JML $038AA9  ; jump to full init
 */
void srf_00FE88(void) {
    OP_SEI();
    OP_CLC();
    op_xce();
    OP_CLD();
    srf_038AA9();
}

/*
 * $02:8000 — Real NMI handler (lives in WRAM at $7E:A2D9 at runtime)
 *
 * Acknowledges NMI via $4210, preserves registers, dispatches
 * to NMI sub-handler at $7E:A305.
 *
 *   02:8000  C2 30     REP #$30       ; 16-bit A/X/Y
 *   02:8002  48        PHA
 *   02:8003  AF 10 42 00  LDA $004210  ; acknowledge NMI
 *   02:8007  30 06     BMI $800F      ; if NMI flag set, handle it
 *   02:8009  AF 30 30 00  LDA $003030  ; read GSU SFR (status)
 *   02:800D  68        PLA
 *   02:800E  6B        RTL            ; false NMI — return
 *
 *   02:800F  8B        PHB
 *   02:8010  78        SEI
 *   02:8011  DA        PHX
 *   02:8012  5A        PHY
 *   02:8013  E2 30     SEP #$30
 *   02:8015  A9 00     LDA #$00
 *   02:8017  48        PHA
 *   02:8018  AB        PLB            ; DB = $00
 *   02:8019  22 05 A3 7E  JSL $7EA305  ; → NMI work routine
 *   02:801D  C2 30     REP #$30
 *   02:801F  7A        PLY
 *   02:8020  FA        PLX
 *   02:8021  AB        PLB
 *   02:8022  68        PLA
 *   02:8023  6B        RTL
 */
void srf_028000(void) {
    op_rep(0x30);
    op_pha16();

    /* acknowledge NMI — read $4210 */
    uint16_t nmi_status = bus_read16(0x00, 0x4210);
    CPU_SET_A16(nmi_status);

    if (!(g_cpu.C & 0x8000)) {
        /* false NMI — just read GSU SFR and return */
        uint16_t sfr = bus_read16(0x00, 0x3030);
        CPU_SET_A16(sfr);
        op_pla16();
        return;
    }

    /* real NMI */
    /* PHB, SEI, PHX, PHY */
    uint8_t saved_db = g_cpu.DB;
    OP_SEI();
    uint16_t saved_x = g_cpu.X;
    uint16_t saved_y = g_cpu.Y;

    op_sep(0x30);
    g_cpu.DB = 0x00;

    /* JSL $7E:A305 — NMI work subroutine */
    /* This is the sub-handler that does the actual per-frame work.
     * It's in WRAM so we call it via the function table or
     * directly once we've recompiled it. For now, call the stub.
     */
    srf_NMI_work();

    /* restore */
    op_rep(0x30);
    g_cpu.Y = saved_y;
    g_cpu.X = saved_x;
    g_cpu.DB = saved_db;
    op_pla16();
}

/*
 * NMI work routine — placeholder for the actual NMI sub-handler
 * at $7E:A305 (ROM source offset $02:802C relative to $7E:A2D9).
 *
 * The real routine handles:
 * - Screen brightness update ($2100)
 * - OAM DMA transfer
 * - BG scroll register updates
 * - HDMA channel setup
 * - Frame counter increment
 * - Input polling
 * - GSU status check
 */
void srf_NMI_work(void) {
    /* Increment frame counter */
    uint8_t fc = bus_wram_read8(0x05E9);
    bus_wram_write8(0x05E9, fc + 1);
}
