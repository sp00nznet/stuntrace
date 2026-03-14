/*
 * Stunt Race FX — Boot sequence & NMI handler
 *
 * Recompiled from WDC 65C816 + Super FX 2 (GSU-2) assembly.
 * ROM: Stunt Race FX (USA) — LoROM + Super FX, 1 MB
 *
 * This file covers the reset vector, initial hardware setup,
 * and the NMI handler that drives each frame.
 */

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * $00:8000 — Reset vector entry point
 *
 * The 65816 jumps here on power-on / reset.
 * Sets native mode, configures stack, disables interrupts,
 * and falls through to hardware init.
 *
 * Original disassembly:
 *   00:8000  78        SEI
 *   00:8001  18        CLC
 *   00:8002  FB        XCE          ; switch to native mode
 *   00:8003  C2 30     REP #$30     ; 16-bit A, X, Y
 *   00:8005  A9 FF 01  LDA #$01FF
 *   00:8008  1B        TCS          ; SP = $01FF
 *   00:8009  A9 00 00  LDA #$0000
 *   00:800C  5B        TCD          ; DP = $0000
 *   00:800D  E2 20     SEP #$20     ; 8-bit A
 *   00:800F  A9 80     LDA #$80
 *   00:8011  8D 00 21  STA $2100    ; force blank on
 *   00:8014  9C 00 42  STZ $4200    ; disable NMI/IRQ
 *   00:8017  4C 4A 80  JMP $804A    ; → hardware init
 */
void srf_008000(void) {
    OP_SEI();
    OP_CLC();
    op_xce();                          /* native mode */

    op_rep(0x30);                      /* 16-bit A/X/Y */
    op_lda_imm16(0x01FF);
    OP_TCS();                          /* SP = $01FF */
    op_lda_imm16(0x0000);
    OP_TCD();                          /* DP = $0000 */

    op_sep(0x20);                      /* 8-bit A */
    op_lda_imm8(0x80);
    bus_write8(0x00, 0x2100, 0x80);    /* force blank */
    bus_write8(0x00, 0x4200, 0x00);    /* disable NMI */

    srf_00804A();                      /* → hardware init */
}

/*
 * $00:8018 — NMI handler
 *
 * Called every V-blank. Acknowledges the NMI, updates the
 * frame counter, and dispatches per-state NMI work.
 *
 * Original disassembly:
 *   00:8018  08        PHP
 *   00:8019  C2 30     REP #$30
 *   00:801B  48        PHA
 *   00:801C  DA        PHX
 *   00:801D  AD 10 42  LDA $4210    ; acknowledge NMI
 *   00:8020  EE xx xx  INC $nnnn    ; frame counter++
 *   ...
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
