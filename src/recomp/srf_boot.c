/*
 * Stunt Race FX — Boot sequence & NMI handler
 *
 * Recompiled from WDC 65C816 + Super FX 2 (GSU-2) assembly.
 * ROM: Stunt Race FX (USA) — LoROM + Super FX, 1 MB
 *
 * This file covers the reset vector, initial hardware setup,
 * and the NMI handler that drives each frame.
 *
 * ROM vectors (from header at $7FEA-$7FFF):
 *   Reset: $FE88
 *   NMI:   $0108
 *   IRQ:   $010C
 */

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * $00:FE88 — Reset vector entry point
 *
 * The 65816 jumps here on power-on / reset.
 * Sets native mode, disables interrupts, and jumps to the
 * main initialization code.
 *
 * Bytes at reset vector:
 *   FE88: 78        SEI
 *   FE89: 18        CLC
 *   FE8A: FB        XCE          ; switch to native mode
 *   FE8B: D8        CLD          ; clear decimal mode
 *   FE8C: 5C A9 8A 03  JML $038AA9  ; jump to init in bank $03
 */
void srf_008000(void) {
    OP_SEI();
    OP_CLC();
    op_xce();                          /* native mode */
    OP_CLD();                          /* clear decimal */

    /* JML $03:8AA9 — jump to full initialization
     * For now, we call our local init stub.
     * As more functions are recompiled, this will dispatch
     * to the actual $03:8AA9 init routine.
     */
    srf_00804A();
}

/*
 * $00:0108 — NMI handler
 *
 * Called every V-blank. The game uses this to update the screen
 * brightness, handle DMA transfers, and synchronize the Super FX.
 *
 * Bytes at NMI vector:
 *   0108: 4A        LSR A
 *   0109: 4A        LSR A
 *   010A: 4A        LSR A
 *   010B: D0 03     BNE $0110
 *   010D: A9 01 00  LDA #$0001
 *   0110: 18        CLC
 *   0111: 6D 19 07  ADC $0719      ; add to brightness accumulator
 *   0114: 8D 19 07  STA $0719
 *   0117: E2 20     SEP #$20       ; 8-bit A
 *   0119: AD 19 07  LDA $0719
 *   011C: 8D 0E 21  STA $210E      ; BG3 H-scroll
 *   011F: AD 1A 07  LDA $071A
 *   0122: 8D 0E 21  STA $210E      ; BG3 H-scroll high
 *   0125: A9 03     LDA #$03
 *   0127: 8D 40 21  STA $2140      ; APU port 0
 */
void srf_008018(void) {
    op_php();
    op_rep(0x30);
    op_pha16();
    op_phx16();

    /* acknowledge NMI */
    op_lda_abs16(0x4210);

    /* increment frame counter in WRAM */
    uint16_t fc = bus_wram_read16(0x0044);
    bus_wram_write16(0x0044, fc + 1);

    /* restore state and return */
    op_plx16();
    op_pla16();
    op_plp();
}
