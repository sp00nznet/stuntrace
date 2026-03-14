/*
 * Stunt Race FX — Recompiled function declarations
 *
 * Naming convention:  srf_BBAAAA
 *   BB   = SNES bank (hex)
 *   AAAA = address within bank (hex)
 */
#ifndef SRF_FUNCTIONS_H
#define SRF_FUNCTIONS_H

/* Register all recompiled functions in the function table */
void srf_register_all(void);

/* === Bank $00 === */
void srf_008000(void);  /* Reset vector entry */
void srf_008018(void);  /* NMI handler */

/* === Bank $00 — Initialization === */
void srf_00804A(void);  /* Hardware init: disable IRQ/NMI, force blank */
void srf_0080C0(void);  /* Main loop entry */

#endif /* SRF_FUNCTIONS_H */
