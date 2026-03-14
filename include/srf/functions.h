/*
 * Stunt Race FX — Recompiled function declarations
 *
 * Naming convention:  srf_BBAAAA
 *   BB   = SNES bank (hex)
 *   AAAA = address within bank (hex)
 *
 * ROM vectors:
 *   Reset: $FE88  → JML $03:8AA9
 *   NMI:   $0108  → JSL $7E:A2D9 (WRAM copy of $02:8000)
 *   IRQ:   $010C  → NOP*7 + RTI (stub)
 */
#ifndef SRF_FUNCTIONS_H
#define SRF_FUNCTIONS_H

#include <snesrecomp/func_table.h>

/* Register all recompiled functions in the function table */
void srf_register_all(void);

/* === Boot === */
void srf_00FE88(void);  /* Reset vector: SEI/CLC/XCE/CLD → JML $03:8AA9 */

/* === NMI === */
void srf_028000(void);  /* Real NMI handler (copied to $7E:A2D9 at runtime) */
void srf_NMI_work(void);/* NMI work subroutine (stub for $7E:A305) */

/* === Bank $03 — Initialization === */
void srf_0389B4(void);  /* PPU/register init: zero $2100-$2133, $4200-$420D */
void srf_038AA9(void);  /* Full init: HW init, WRAM clear, DMA, GSU setup */
void srf_038CF6(void);  /* WRAM DMA clear: A=bank, X=addr, Y=count */
void srf_038C63(void);  /* Main game loop entry */

/* === Bank $04 — Audio === */
void srf_04D44C(void);  /* SPC700 audio engine upload */
void srf_04D720(void);  /* IPL transfer routine */

#endif /* SRF_FUNCTIONS_H */
