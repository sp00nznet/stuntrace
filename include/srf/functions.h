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
void srf_04D649(void);  /* Audio command queue */
void srf_04D6E1(void);  /* Audio state clear */
void srf_04D0DB(void);  /* Audio/music reload */
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

/* === Bank $03 — 3D Math === */
void srf_03B011(void);  /* Rotation matrix setup via GSU ($01:8325) */

/* === Bank $02 — Utility === */
void srf_02DF79(void);  /* PRNG (pseudo-random number generator) */

/* === Bank $02 — Camera === */
void srf_02DAD6(void);  /* Camera setup from object data */

/* === Bank $02 — Palette / Color === */
void srf_02D53D(void);  /* Palette copy from ROM table */
void srf_02D55F(void);  /* Palette fade (per-frame interpolation) */
void srf_02DB59(void);  /* GSU palette program launch ($01:AEF8) */

/* === Bank $02 — Attract Mode === */
void srf_02E0A9(void);  /* Per-frame dispatch (fade management) */
void srf_02D7CD(void);  /* Attract mode frame body (main render loop) */

/* === Bank $0B — Title State Machine / Input === */
void srf_0BAE0A(void);  /* Title screen state machine */
void srf_0BAE8F(void);  /* Check for input (Start press) */

/* === Bank $03 — Scene State === */
void srf_03B48C(void);  /* Object table setup for scenes */
void srf_03F02B(void);  /* Palette/GSU config checksum */
void srf_03D8B3(void);  /* Display-mode-specific initialization */
void srf_03B3DA(void);  /* Scene state initialization */

/* === Bank $03 — Camera / Objects === */
void srf_03CB5C(void);  /* Allocate object slot from free list */
void srf_03CB25(void);  /* Initialize object slot */
void srf_03B8A1(void);  /* Deallocate object (active + render list removal) */
void srf_03D306(void);  /* Camera angle calculation (3D → screen) */
void srf_03D388(void);  /* Object/animation linked list processing */

/* === Bank $08 — Object System / Viewport === */
void srf_088364(void);  /* Insert object into render list */
void srf_088392(void);  /* Remove object from render list */
void srf_08801C(void);  /* Object command dispatcher (bytecode interpreter) */
void srf_08B863(void);  /* Viewport render order setup */
void srf_0894A1(void);  /* Vehicle race mode animation callback */
void srf_08951B(void);  /* Vehicle collision animation callback */
void srf_0883CC(void);  /* Render list rehash (reposition in spatial hash) */
void srf_08D070(void);  /* Object validity check + GSU flag setup */
void srf_08D8C2(void);  /* GSU camera position sync (3 values) */
void srf_08D86F(void);  /* Object → GSU data sync (write all fields) */
void srf_08CD25(void);  /* Object state flag + callback chain setup */
void srf_08CC7C(void);  /* Vehicle object creation (link to GSU) */
void srf_0888C7(void);  /* Vehicle model setup from ROM table */
void srf_08CF41(void);  /* Collision response (walk GSU chain) */
void srf_08CF92(void);  /* Collision check (standalone) */
void srf_08CE02(void);  /* Collision state sync (GSU → WRAM) */
void srf_08CCA3(void);  /* GSU object flag setup (set bit 4, clear bit 8) */
void srf_08CCBE(void);  /* GSU animation frame set ($70:001A = 2) */
void srf_08CCD2(void);  /* GSU position write (DP $02/$08/$94 → $70:000C-0010) */
void srf_08CCF1(void);  /* GSU flag OR ($70:0016 |= DP $02) */
void srf_08C60F(void);  /* Object state update (per-player) */
void srf_08B4C6(void);  /* Object rendering/callback setup */
void srf_08C5A5(void);  /* Object system main update (P1/P2 processing) */
void srf_08B893(void);  /* Viewport configuration init */

/* === Bank $0B — Gameplay / Entity System === */
void srf_0BB479(void);  /* Entity system initialization */
void srf_0BB4C3(void);  /* Entity allocator */
void srf_0BB450(void);  /* Entity callback dispatcher */
void srf_0BB64A(void);  /* Sprite compositor (entity → OAM) */
void srf_0BE390(void);  /* VBlank wait (simplified for recomp) */
void srf_0BFA24(void);  /* Gameplay scene setup (OAM, sprites, GSU, NMI) */
void srf_0BFB26(void);  /* Gameplay/menu frame body (2P capable) */

/* === Bank $03 — Scene Management === */
void srf_03863D(void);  /* Scene change: config offset $0000 */
void srf_038648(void);  /* Scene change: config offset $006B */
void srf_038653(void);  /* Scene change: config offset $00FC */
void srf_038683(void);  /* Scene config loader from ROM table */
void srf_03865E(void);  /* Scene reset (3 scene configs) */
void srf_038C86(void);  /* Full game restart */

/* === Bank $09 — WRAM Patches === */
void srf_09ECE0(void);  /* Copy jump table patches to WRAM */

/* === Bank $02 — Display Mode === */
void srf_02E289(void);  /* Display mode setup (RGB→BGR, NMI config) */

/* === GSU === */
void srf_GSU_launch(void);  /* GSU program launcher ($7E:E1F5) */

/* === WRAM-resident === */
void srf_7EE258(void);  /* P2 GSU render pipeline ($7E:E258) */
void srf_7F112F(void);  /* Gameplay audio sync ($7F:112F) */

#endif /* SRF_FUNCTIONS_H */
