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

/* === Bank $02 — Display / Screen === */
void srf_02D65A(void);  /* Store brightness to HDMA table */
void srf_02D7AB(void);  /* Wait for scanline */
void srf_02CF45(void);  /* Full screen setup for scene transition */

/* === Bank $03 — Display Config === */
void srf_03DD1B(void);  /* Display mode DMA dispatcher */

/* === Bank $03 — Title Screen === */
void srf_03EB0E(void);  /* PPU mode setup (Mode 3 for title screen) */
void srf_03EB83(void);  /* VRAM DMA engine (table-driven from GSU RAM) */
void srf_03D996(void);  /* Title/attract setup (outer wrapper) */
void srf_03D9B9(void);  /* Title screen scene builder */

/* === Bank $02 — Attract Mode === */
void srf_02E0A9(void);  /* Per-frame dispatch (fade management) */
void srf_02D7CD(void);  /* Attract mode frame body (main render loop) */

/* === Bank $0B — Title State Machine / Input === */
void srf_0BAE0A(void);  /* Title screen state machine */
void srf_0BAE8F(void);  /* Check for input (Start press) */

/* === Bank $03 — Camera / Objects === */
void srf_03D306(void);  /* Camera angle calculation (3D → screen) */
void srf_03D388(void);  /* Object/animation linked list processing */

/* === GSU === */
void srf_GSU_launch(void);  /* GSU program launcher ($7E:E1F5) */

#endif /* SRF_FUNCTIONS_H */
