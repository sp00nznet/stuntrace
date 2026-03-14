/*
 * Stunt Race FX — Recompiled function declarations
 *
 * Naming convention:  srf_BBAAAA
 *   BB   = SNES bank (hex)
 *   AAAA = address within bank (hex)
 *
 * ROM vectors:
 *   Reset: $FE88  (mapped to srf_008000 for bootstrap)
 *   NMI:   $0108  (mapped to srf_008018 for NMI handler)
 *   IRQ:   $010C
 */
#ifndef SRF_FUNCTIONS_H
#define SRF_FUNCTIONS_H

#include <snesrecomp/func_table.h>

/* Register all recompiled functions in the function table */
void srf_register_all(void);

/* === Boot / Core === */
void srf_008000(void);  /* Reset vector bootstrap (actual vector at $FE88) */
void srf_008018(void);  /* NMI handler (actual vector at $0108) */

/* === Bank $00 — Initialization === */
void srf_00804A(void);  /* Hardware init: PPU/DMA clear, enable NMI */
void srf_0080C0(void);  /* Main loop entry / state dispatcher */

#endif /* SRF_FUNCTIONS_H */
