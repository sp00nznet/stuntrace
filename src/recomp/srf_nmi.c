/*
 * Stunt Race FX — NMI work routine (state machine)
 *
 * ROM source: $02:8000 (copied to WRAM $7E:A2D9 at runtime)
 * NMI work entry: $7E:A305 = ROM $02:802C
 *
 * The NMI handler uses a state machine indexed by WRAM $0D3F.
 * Each state handles different V-blank work:
 *   $00 → State 0: Title/attract VBlank (brightness, frame counter)
 *   $02 → State 1: Force blank + HDMA/DMA setup
 *   $04+ → Various gameplay states (OAM DMA, scroll, GSU sync)
 *
 * The dispatch works via indirect JMP through a table at ROM $02:8034
 * (WRAM $7E:A30D). The table contains 16-bit addresses that point
 * to handlers within the WRAM-resident code block.
 */

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * Helper: $7E:A2FD (ROM $02:8024)
 * Wait for H-blank by checking $4212 bit 6
 * Actually this just returns immediately for recomp since we
 * don't need cycle-level hblank timing.
 */
static void srf_wait_hblank(void) {
    /* In the original, this spins on $4212 bit 6.
     * For recomp, we skip the busy-wait. */
}

/*
 * NMI State 0: $7E:A309 = ROM $02:8030
 * Index $0D3F = $00
 *
 * This is the "idle" VBlank state used during title screen / attract.
 *   - Waits for H-blank
 *   - Sets screen brightness from $0D61
 *   - Sets H-timer to $C8
 *   - Clears IRQ flag
 *   - Sets NMI state to $02 (next VBlank will do DMA work)
 *   - Increments frame counters $0E3B and $0306
 */
static void srf_nmi_state0(void) {
    srf_wait_hblank();

    /* Set screen brightness from game variable */
    uint8_t brightness = bus_wram_read8(0x0D61);
    bus_write8(0x00, 0x2100, brightness);

    /* Set H-timer IRQ at H=$C8 (for mid-screen effects) */
    bus_write8(0x00, 0x4209, 0xC8);

    /* Clear IRQ flag */
    bus_read8(0x00, 0x4211);

    /* Set next NMI state = $02 */
    bus_wram_write8(0x0D3F, 0x02);

    /* Increment frame counters */
    uint8_t fc1 = bus_wram_read8(0x0E3B);
    bus_wram_write8(0x0E3B, fc1 + 1);
    uint8_t fc2 = bus_wram_read8(0x0306);
    bus_wram_write8(0x0306, fc2 + 1);
}

/*
 * NMI State 1: $7E:A34D = ROM $02:8074
 * Index $0D3F = $02
 *
 * Force blank state — used for safe VRAM/CGRAM/OAM DMA transfers.
 *   - Force blank ($2100 = $80)
 *   - Set H-timer to $17
 *   - Clear IRQ, reset NMI state to $00
 *   - Set up HDMA channels ($420C)
 *   - Call DMA transfer routine
 *   - Call OAM/scroll update routine
 */
static void srf_nmi_state1(void) {
    srf_wait_hblank();

    /* Force blank */
    bus_write8(0x00, 0x2100, 0x80);

    /* H-timer = $17 */
    bus_write8(0x00, 0x4209, 0x17);

    /* Clear IRQ */
    bus_read8(0x00, 0x4211);

    /* Reset NMI state to $00 */
    bus_wram_write8(0x0D3F, 0x00);

    /* Mark frame as ready */
    bus_wram_write8(0x0D52, 0xFF);

    /* Enable HDMA channels from game variable */
    bus_write8(0x00, 0x420C, 0x02);

    /* Increment frame counter $05E9 */
    uint8_t fc = bus_wram_read8(0x05E9);
    bus_wram_write8(0x05E9, fc + 1);
}

/*
 * NMI State 4: $7E:A375 = ROM $02:809C
 * Index $0D3F = $04
 *
 * Gameplay VBlank — BG layers, scroll, OAM, frame counter.
 *   - Set main screen designation ($212C = $13 = BG1+BG2+OBJ)
 *   - Update BG4 scroll from game variables
 *   - H-timer = $D8, clear IRQ
 *   - Set NMI state = $12 (next frame does DMA work)
 *   - OAM DMA + scroll register updates
 */
static void srf_nmi_state4(void) {
    op_sep(0x30);
    srf_wait_hblank();

    /* Main screen = BG1 + BG2 + OBJ */
    bus_write8(0x00, 0x212C, 0x13);

    /* BG4 scroll from game variables */
    uint8_t scroll_h = bus_wram_read8(0x0E37);
    bus_write8(0x00, 0x2110, scroll_h);
    uint8_t scroll_v = bus_wram_read8(0x0E34);
    bus_write8(0x00, 0x2110, scroll_v);

    /* H-timer = $D8 */
    bus_write8(0x00, 0x4209, 0xD8);
    bus_read8(0x00, 0x4211);

    /* NMI state = $12 */
    bus_wram_write8(0x0D3F, 0x12);

    /* Frame counters */
    uint8_t fc1 = bus_wram_read8(0x0E3B);
    bus_wram_write8(0x0E3B, fc1 + 1);
    uint8_t fc2 = bus_wram_read8(0x0306);
    bus_wram_write8(0x0306, fc2 + 1);
}

/*
 * NMI State 8: $7E:A3A8 = ROM $02:80CF
 * Index $0D3F = $08 (or $10)
 *
 * Gameplay force-blank state — VRAM transfers, HDMA, color math.
 * Similar to state 1 but with gameplay-specific registers.
 */
static void srf_nmi_state8(void) {
    op_sep(0x30);
    srf_wait_hblank();

    /* Force blank */
    bus_write8(0x00, 0x2100, 0x80);

    /* H-timer = $37 */
    bus_write8(0x00, 0x4209, 0x37);
    bus_read8(0x00, 0x4211);

    /* V-timer disable */
    bus_write8(0x00, 0x420A, 0x00);

    /* HDMA channels from game variable */
    uint8_t hdma = bus_wram_read8(0x0D45);
    bus_write8(0x00, 0x420C, hdma);

    /* Color math from game variable */
    uint8_t colmath = bus_wram_read8(0x0D47);
    bus_write8(0x00, 0x2131, colmath);

    /* NMI state = $10 */
    bus_wram_write8(0x0D3F, 0x10);

    /* Mark frame ready */
    bus_wram_write8(0x0D52, 0xFF);

    /* BG4 scroll */
    uint8_t s1 = bus_wram_read8(0x0E33);
    bus_write8(0x00, 0x2110, s1);
    uint8_t s2 = bus_wram_read8(0x0E34);
    bus_write8(0x00, 0x2110, s2);

    /* Fixed color register setup */
    uint8_t fc = bus_read8(0x7F, 0x340D);
    bus_write8(0x00, 0x2132, fc | 0x80);  /* blue component */
    bus_write8(0x00, 0x2132, (fc & 0x7F) | 0x40);  /* green component */
    bus_write8(0x00, 0x2132, (fc & 0x1F) | 0x20);  /* red component */
}

/*
 * $7E:A305 — NMI work dispatch (ROM $02:802C)
 *
 * Reads NMI state from $0D3F, dispatches to appropriate handler.
 */
void srf_NMI_work(void) {
    op_sep(0x30);

    uint8_t state = bus_wram_read8(0x0D3F);

    switch (state) {
    case 0x00:
        srf_nmi_state0();
        break;
    case 0x02:
        srf_nmi_state1();
        break;
    case 0x04:
        srf_nmi_state4();
        break;
    case 0x08:
    case 0x10:
        srf_nmi_state8();
        break;
    default:
        /* States we haven't recompiled yet — just increment
         * the frame counter so the main loop doesn't hang */
        {
            uint8_t fc = bus_wram_read8(0x05E9);
            bus_wram_write8(0x05E9, fc + 1);
            uint8_t fc2 = bus_wram_read8(0x0306);
            bus_wram_write8(0x0306, fc2 + 1);
        }
        break;
    }
}
