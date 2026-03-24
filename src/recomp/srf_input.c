/*
 * Stunt Race FX — Input handling & controller reading
 *
 * The game reads controllers via SNES auto-joypad ($4218-$421F)
 * and processes button presses into game state variables.
 *
 * Controller data locations:
 *   $0309 — P1 current buttons (new presses)
 *   $030D — P2 current buttons
 *   $18FB — Combined input for menus
 *   $0346 — Current state index into object table
 *   $034A — Secondary state index
 */

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * $0B:AE8F — Check for input (Start button press)
 *
 * Reads P1 ($0309) and P2 ($030D) button states plus
 * the menu input register ($18FB). Returns:
 *   A = $0000 if no relevant input (keep waiting)
 *   A = $FFFF if input detected (transition to next state)
 *
 * The title screen loops on this until Start is pressed.
 */
void srf_0BAE8F(void) {
    op_rep(0x30);

    /* Check P1 buttons for Start/A/B */
    uint16_t p1 = bus_wram_read16(0x0309);
    if (p1 & 0x000F) {
        /* Input detected — return $FFFF */
        CPU_SET_A16(0xFFFF);
        return;
    }

    /* Check P2 buttons */
    uint16_t p2 = bus_wram_read16(0x030D);
    if (p2 & 0x000F) {
        CPU_SET_A16(0xFFFF);
        return;
    }

    /* Check menu input register */
    uint16_t menu = bus_wram_read16(0x18FB);
    if (menu != 0) {
        CPU_SET_A16(0xFFFF);
        return;
    }

    /* No input — return $0000 */
    CPU_SET_A16(0x0000);
}

/*
 * $0B:AE0A — Title screen state machine
 *
 * This is the main loop for the title screen. It:
 * 1. Runs per-frame processing ($0B:E390 via JSR, $0B:B450, $0B:B64A)
 * 2. Sets input mask ($11D7 |= $0F)
 * 3. Waits for $11EB to become non-zero (NMI has run)
 * 4. Checks for input ($AE8F)
 * 5. If input detected, disables display and returns
 * 6. If no input, loops back to step 1
 *
 * For recomp, we execute one iteration per call and return.
 * The launcher's frame loop drives this.
 */
void srf_0BAE0A(void) {
    op_rep(0x30);

    /* Per-frame processing — increment frame counter */
    uint8_t fc = bus_wram_read8(0x05E9);
    bus_wram_write8(0x05E9, fc + 1);

    /* Set input enable mask */
    op_sep(0x20);
    uint8_t input_mask = bus_wram_read8(0x11D7);
    bus_wram_write8(0x11D7, input_mask | 0x0F);

    /* Check for input */
    op_rep(0x30);
    srf_0BAE8F();

    if (g_cpu.C == 0xFFFF) {
        /* Input detected — disable display for transition */
        op_sep(0x20);
        bus_write8(0x00, 0x2100, 0x80);  /* force blank */
        bus_write8(0x00, 0x4200, 0x00);  /* disable NMI */
        bus_write8(0x00, 0x420C, 0x00);  /* disable HDMA */
        bus_write8(0x00, 0x420B, 0x00);  /* disable DMA */
    }
}

/*
 * $03:D306 — Camera angle calculation
 *
 * Converts 3D rotation values ($0664/$0666) to screen-space
 * coordinates ($06CD/$06CF) for the camera system.
 *
 * The rotation values are fixed-point numbers. The routine
 * performs arithmetic shift right (sign-extending ROR) 6 times,
 * negates, clamps to [-56, +232] range, and adds screen center
 * offset ($0098 = 152).
 */
void srf_03D306(void) {
    uint16_t saved_y = g_cpu.Y;
    op_php();
    op_rep(0x30);

    /* X rotation → screen Y coordinate */
    int16_t rot_x = (int16_t)bus_wram_read16(0x0664);

    /* Arithmetic shift right 6 times (divide by 64, sign-extend) */
    rot_x >>= 6;

    /* Negate */
    rot_x = -rot_x;

    /* Clamp to [-56, +232] */
    if (rot_x > 0xE8) rot_x = 0xE8;
    if (rot_x < -56) rot_x = -56;

    /* Add screen center offset and mask to 9 bits */
    uint16_t screen_y = ((uint16_t)(rot_x + 0x98)) & 0x01FF;
    bus_wram_write16(0x06CD, screen_y);

    /* Y rotation → screen X coordinate */
    int16_t rot_y = (int16_t)bus_wram_read16(0x0666);

    /* Arithmetic shift right 5 times */
    rot_y >>= 5;

    /* Mask to 11 bits */
    uint16_t screen_x = ((uint16_t)rot_y) & 0x07FF;
    bus_wram_write16(0x06CF, screen_x);

    op_sep(0x20);
    op_plp();
    g_cpu.Y = saved_y;
}

/*
 * $03:D388 — Object/animation processing
 *
 * Walks a linked list of active objects starting from the
 * object table head at $7E:2000+$0329. Each object has:
 *   +$00/$01: next pointer (link)
 *   +$22/$23: object ID
 *   +$36: animation timer/state
 *
 * For each object:
 * - If timer > 0: decrement and call animation update ($03:CAB8)
 * - If timer < 0: initialize to 3 and call init handler ($03:CAEB)
 * - If timer = 0: skip (inactive)
 *
 * Also increments the master frame counter at $05E9.
 */
void srf_03D388(void) {
    op_sep(0x20);

    /* Increment frame counter */
    uint16_t fc = bus_wram_read16(0x05E9);
    bus_wram_write16(0x05E9, fc + 1);

    op_rep(0x10);

    /* Save DB, set to $7E for object table access */
    uint8_t saved_db = g_cpu.DB;
    g_cpu.DB = 0x7E;

    /* Walk linked list starting at $7E:2000 + $0329 */
    uint16_t obj_idx = bus_wram_read16(0x0329);
    bus_wram_write8(0x035C, 0x00);

    while (obj_idx != 0) {
        uint8_t timer = bus_read8(0x7E, 0x2036 + obj_idx);

        if (timer == 0) {
            /* Inactive — follow link */
        } else if ((int8_t)timer < 0) {
            /* Negative — initialize to 3, call init handler ($03:CAEB)
             * Reads callback from $7E:201D+X (addr) / $201F+X (bank).
             * If addr is non-zero and $2022+X is zero, dispatch. */
            bus_write8(0x7E, 0x2036 + obj_idx, 0x03);
            op_rep(0x20);
            uint16_t init_addr = bus_read16(0x7E, 0x201D + obj_idx);
            if (init_addr != 0) {
                uint16_t obj_id = bus_read16(0x7E, 0x2022 + obj_idx);
                if (obj_id == 0) {
                    uint8_t init_bank = bus_read8(0x7E, 0x201F + obj_idx);
                    uint32_t callback = ((uint32_t)init_bank << 16) | init_addr;
                    bus_wram_write16(0x034C, obj_idx);
                    g_cpu.X = obj_idx;
                    func_table_call(callback);
                }
            }
            op_sep(0x20);
        } else {
            /* Positive — decrement timer */
            timer--;
            if (timer == 0) {
                /* Timer expired — clear object */
                bus_write8(0x7E, 0x2022 + obj_idx, 0x00);
                bus_write8(0x7E, 0x2023 + obj_idx, 0x00);
                bus_write8(0x7E, 0x2036 + obj_idx, 0x00);
            } else {
                bus_write8(0x7E, 0x2036 + obj_idx, timer);
                /* Call animation update handler ($03:CAB8)
                 * Reads callback from $7E:201A+X (addr) / $201C+X (bank).
                 * If addr is non-zero, dispatch. */
                op_rep(0x20);
                uint16_t upd_addr = bus_read16(0x7E, 0x201A + obj_idx);
                if (upd_addr != 0) {
                    uint8_t upd_bank = bus_read8(0x7E, 0x201C + obj_idx);
                    uint32_t callback = ((uint32_t)upd_bank << 16) | upd_addr;
                    bus_wram_write16(0x034C, obj_idx);
                    bus_wram_write16(0x0354, upd_addr);
                    bus_wram_write8(0x0356, upd_bank);
                    g_cpu.X = obj_idx;
                    func_table_call(callback);
                }
                op_sep(0x20);
            }
        }

        /* Follow link to next object */
        uint16_t next = bus_read16(0x7E, 0x2000 + obj_idx);
        obj_idx = next;
    }

    g_cpu.DB = saved_db;
}
