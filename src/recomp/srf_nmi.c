/*
 * Stunt Race FX — NMI work routine (state machine)
 *
 * ROM source: $02:8000 (copied to WRAM $7E:A2D9 at runtime)
 * NMI work entry: $7E:A305 = ROM $02:802C
 *
 * The NMI handler uses a state machine indexed by WRAM $0D3F.
 * Dispatch table at ROM $02:8034 (12 entries, 2 bytes each):
 *
 *   $0D3F  WRAM    ROM        Purpose
 *   ─────  ─────── ────────   ─────────────────────────────────
 *   $00    $A330   $028057    Title/attract brightness
 *   $02    $A34D   $028074    Title/attract force blank + DMA
 *   $04    $A4F5   $02821C    Race VBlank: brightness + OAM
 *   $06    $A56B   $028292    Race force blank + color math
 *   $08    $A5B6   $0282DD    Race 2P brightness + config
 *   $0A    $A60F   $028336    Race 2P force blank + scroll
 *   $0C    $A681   $0283A8    Race brightness + DMA helper
 *   $0E    $A6B0   $0283D7    Race force blank + BG/sprite DMA
 *   $10    $A375   $02809C    Gameplay brightness + scroll
 *   $12    $A3A8   $0280CF    Gameplay force blank + VRAM DMA
 *   $14    $A53D   $028264    IRQ mid-screen state
 *   $16    $A32F   $028056    No-op (RTL)
 *
 * State cycles:
 *   Title/attract:  $00 ↔ $02
 *   Gameplay:       $10 ↔ $12
 *   Race (1P):      $04 → $14 → $06 → $04
 *   Race (2P):      $08 → $0A → $0C → $0E → $08
 */

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/* ── NMI helper subroutines (WRAM-resident in original) ────────── */

/*
 * $7E:A2FD (ROM $02:8024)
 * Wait for H-blank by checking $4212 bit 6.
 * For recomp, we skip the busy-wait.
 */
static void nmi_wait_hblank(void) {
    /* No-op in recomp — no cycle-level timing needed */
}

/*
 * $7E:ACF7 (ROM $02:8A1E) — Low-level OAM DMA
 * Transfers X bytes from WRAM offset X to OAM via $2104.
 * Parameters: X = source addr, Y = byte count
 *
 * $7E:AD16 (ROM $02:8A3D) — OAM DMA wrapper
 * Calls ACF7 with X=$0375, Y=$0220
 */
static void nmi_oam_dma(void) {
    op_sep(0x20);
    op_rep(0x10);

    /* Set OAM address to 0 */
    bus_write8(0x00, 0x2102, 0x00);
    bus_write8(0x00, 0x2103, 0x00);

    /* DMA ch0: mode $04 (1-byte to $2104), bank $00, src $0375, count $0220 */
    bus_write16(0x00, 0x4300, 0x0400);  /* mode + dest register */
    bus_write16(0x00, 0x4302, 0x0375);  /* source addr */
    bus_write8(0x00, 0x4304, 0x00);     /* source bank */
    bus_write16(0x00, 0x4305, 0x0220);  /* byte count */
    bus_write8(0x00, 0x420B, 0x01);     /* trigger DMA ch0 */
}

/*
 * $7E:B1AE (ROM $02:8ED5) — Joypad auto-read + edge detection
 * Reads $4218/$421A (auto-joypad), computes new-press edges.
 */
static void nmi_joypad_read(void) {
    op_rep(0x20);

    /* Player 1 */
    uint16_t joy1 = bus_read16(0x00, 0x4218);
    uint16_t prev1 = bus_wram_read16(0x0309);
    uint16_t edge1 = (joy1 ^ prev1) & joy1;
    bus_wram_write16(0x0311, edge1);
    bus_wram_write16(0x0D1A, edge1);

    /* Player 2 */
    uint16_t joy2 = bus_read16(0x00, 0x421A);
    uint16_t prev2 = bus_wram_read16(0x030D);
    uint16_t edge2 = (joy2 ^ prev2) & joy2;
    bus_wram_write16(0x0313, edge2);
    bus_wram_write16(0x0D1C, edge2);

    /* Store current as previous */
    bus_wram_write16(0x030D, joy2);
    bus_wram_write16(0x0309, joy1);
}

/*
 * $7E:B112 (ROM $02:8E39) — APU port sync / audio command dispatch
 * Sends pending audio commands to SPC700 via APU ports $2140-$2143.
 */
static void nmi_audio_sync(void) {
    op_sep(0x30);

    /* Check for priority audio command on port $2140 */
    uint8_t cmd0 = bus_wram_read8(0x10E2);
    if (cmd0 != 0) {
        bus_write8(0x00, 0x2140, cmd0);
        bus_wram_write8(0x10E2, 0x00);
        return;
    }

    /* Check for priority audio command on port $2141 */
    uint8_t cmd1 = bus_wram_read8(0x10E3);
    if (cmd1 != 0) {
        bus_write8(0x00, 0x2141, cmd1);
        bus_wram_write8(0x10E3, 0x00);
        return;
    }

    /* Frame-throttled audio command ring buffer */
    uint8_t da = bus_wram_read8(0x10DA);
    uint8_t dc = bus_wram_read8(0x10DC);
    if (da != 0 || dc != 0) return;

    uint8_t fc = bus_wram_read8(0x0E3B);
    if ((fc & 0x01) != 0) return;

    /* Process ring buffer if APU port $2140 is idle */
    uint8_t apu0 = bus_read8(0x00, 0x2140);
    if (apu0 != 0) {
        bus_write8(0x00, 0x2140, 0x00);
    } else {
        uint8_t cmd_count = bus_wram_read8(0x0DCC);
        if (cmd_count != 0) {
            uint8_t idx = bus_wram_read8(0x0DC8);
            uint8_t cmd = bus_wram_read8(0x0DA8 + idx);
            bus_wram_write8(0x0DCC, cmd_count - 1);
            if (cmd_count - 1 != 0) {
                idx++;
                if (idx >= 0x10) idx = 0x00;
                bus_wram_write8(0x0DC8, idx);
            }
            bus_write8(0x00, 0x2140, cmd);
        }
    }
}

/*
 * $7E:AC4D (ROM $02:8974) — Brightness fade-in ramp
 * Increments $0D60 by $0098 each frame, clamped to $0F00.
 * Used by state $10 (gameplay brightness).
 */
static void nmi_brightness_fade_in(void) {
    uint8_t fade_dir = bus_wram_read8(0x0E3A);
    if (fade_dir != 0) {
        /* Fade out */
        op_rep(0x20);
        int16_t val = (int16_t)bus_wram_read16(0x0D60);
        if (val >= 0) {
            val -= 0x0058;
            if (val < 0) val = (int16_t)0x8000;
        }
        bus_wram_write16(0x0D60, (uint16_t)val);
        op_sep(0x20);
        return;
    }

    /* Fade in */
    op_rep(0x20);
    uint16_t brightness = bus_wram_read16(0x0D60);
    brightness += 0x0098;
    if (brightness >= 0x0F00) brightness = 0x0F00;
    bus_wram_write16(0x0D60, brightness);
    op_sep(0x20);
}

/*
 * $7E:ABD7 (ROM $02:88FE) — VRAM DMA from GSU RAM
 * Parameters: A=count, X=src addr (in bank $70), Y=VRAM dest
 * Releases GSU bus (SCMR) during transfer, then restores.
 */
static void nmi_vram_dma_gsu(uint16_t count, uint16_t src, uint16_t vram_dst) {
    op_rep(0x20);
    bus_write16(0x00, 0x4305, count);
    bus_write16(0x00, 0x4302, src);
    bus_write16(0x00, 0x2116, vram_dst);
    bus_write16(0x00, 0x4300, 0x1801);  /* word mode to $2118 */
    op_sep(0x20);
    bus_write8(0x00, 0x4304, 0x70);     /* source bank = GSU RAM */
    bus_write8(0x00, 0x2115, 0x80);     /* VRAM increment mode */

    /* Release GSU RAM bus for DMA */
    uint8_t scmr = bus_wram_read8(0x0374);
    bus_write8(0x00, 0x303A, scmr & 0xF7);  /* clear RAN */
    bus_write8(0x00, 0x420B, 0x01);          /* trigger DMA */
    bus_write8(0x00, 0x303A, scmr | 0x18);   /* restore RON+RAN */
}

/*
 * $7E:A46B (ROM $02:8192) — Window/HDMA table configuration
 * Builds HDMA parameter tables from viewport config at $7F:3400+.
 */
static void nmi_window_config(void) {
    op_sep(0x20);

    /* Compute horizontal window range */
    uint8_t right = bus_read8(0x7F, 0x3438);
    uint8_t left  = bus_read8(0x7F, 0x3434);
    uint8_t width = right - left;
    bus_write8(0x7F, 0x3402, width);
    uint8_t center = (width >> 1) + left;
    bus_write8(0x7F, 0x3406, center);

    /* Vertical half-height */
    uint8_t v_range = bus_read8(0x7F, 0x3435);
    uint8_t v_half = v_range >> 1;
    bus_write8(0x7F, 0x3410, v_half);
    bus_write8(0x7F, 0x3413, v_half + 1);

    /* Window positions — cascading from $7F:3433 base */
    uint8_t base_v = bus_read8(0x7F, 0x3433);
    bus_write8(0x7F, 0x3420, base_v);
    bus_write8(0x7F, 0x3423, base_v);
    bus_write8(0x7F, 0x341D, base_v + 1);
    bus_write8(0x7F, 0x3426, base_v + 1);
    bus_write8(0x7F, 0x341A, base_v + 2);
    bus_write8(0x7F, 0x3429, base_v + 2);
    bus_write8(0x7F, 0x3417, base_v + 3);
    bus_write8(0x7F, 0x342C, base_v + 3);

    /* Window positions from $7F:3437 base */
    uint8_t base_h = bus_read8(0x7F, 0x3437);
    bus_write8(0x7F, 0x3421, base_h);
    bus_write8(0x7F, 0x3424, base_h);
    bus_write8(0x7F, 0x341E, base_h - 1);
    bus_write8(0x7F, 0x3427, base_h - 1);
    bus_write8(0x7F, 0x341B, base_h - 2);
    bus_write8(0x7F, 0x342A, base_h - 2);
    bus_write8(0x7F, 0x3418, base_h - 3);
    bus_write8(0x7F, 0x342D, base_h - 3);

    /* Vertical span computation */
    uint8_t v_end = bus_read8(0x7F, 0x3439);
    uint8_t v_span = ((v_end - v_range - 6) >> 1);
    bus_write8(0x7F, 0x341F, v_span);
    bus_write8(0x7F, 0x3422, v_span);
}

/* ── NMI state handlers ─────────────────────────────────────────── */

/*
 * State $00 — Title/attract brightness (ROM $02:8057)
 *
 * Sets screen brightness, H-timer, increments frame counters.
 * Next state: $02
 */
static void nmi_state_00(void) {
    nmi_wait_hblank();

    bus_write8(0x00, 0x2100, bus_wram_read8(0x0D61));
    bus_write8(0x00, 0x4209, 0xC8);
    bus_read8(0x00, 0x4211);

    bus_wram_write8(0x0D3F, 0x02);

    uint8_t fc1 = bus_wram_read8(0x0E3B);
    bus_wram_write8(0x0E3B, fc1 + 1);
    uint8_t fc2 = bus_wram_read8(0x0306);
    bus_wram_write8(0x0306, fc2 + 1);
}

/*
 * State $02 — Title/attract force blank + DMA (ROM $02:8074)
 *
 * Force blank, joypad read, OAM DMA, audio sync.
 * Next state: $00
 */
static void nmi_state_02(void) {
    nmi_wait_hblank();

    bus_write8(0x00, 0x2100, 0x80);     /* force blank */
    bus_write8(0x00, 0x4209, 0x17);     /* H-timer */
    bus_read8(0x00, 0x4211);            /* clear IRQ */

    bus_wram_write8(0x0D3F, 0x00);      /* next state */
    bus_wram_write8(0x0D52, 0xFF);      /* frame ready */
    bus_write8(0x00, 0x420C, 0x02);     /* HDMA */

    /* Increment frame counter $05E9 */
    uint8_t fc = bus_wram_read8(0x05E9);
    bus_wram_write8(0x05E9, fc + 1);

    nmi_joypad_read();
    nmi_oam_dma();
    nmi_audio_sync();
}

/*
 * State $04 — Race mode brightness + OAM (ROM $02:821C)
 *
 * Wait for H-blank via $4212 directly (not via helper).
 * Brightness, H/V timers, OAM DMA, scroll, sprite DMA.
 * Next state: $14
 */
static void nmi_state_04(void) {
    /* Wait H-blank + set brightness */
    bus_write8(0x00, 0x2100, bus_wram_read8(0x0D61));

    bus_write8(0x00, 0x4207, 0x30);     /* H-timer low */
    bus_write8(0x00, 0x4209, 0xC0);     /* H-timer high */
    bus_read8(0x00, 0x4211);            /* clear IRQ */

    bus_wram_write8(0x0D3F, 0x14);      /* next state */

    uint8_t fc1 = bus_wram_read8(0x0E3B);
    bus_wram_write8(0x0E3B, fc1 + 1);
    uint8_t fc2 = bus_wram_read8(0x0306);
    bus_wram_write8(0x0306, fc2 + 1);

    bus_wram_write8(0x0D52, 0xFF);      /* frame ready */

    /* Sub-calls: $AC69 (fade), $B1E1, $B112 (audio), $B072 */
    /* Brightness fade */
    {
        uint8_t fade_dir = bus_wram_read8(0x0E3A);
        if (fade_dir == 0) {
            /* Table-based fade */
            uint8_t br = bus_wram_read8(0x0D61);
            op_rep(0x20);
            uint16_t val = bus_wram_read16(0x0D60);
            /* Simplified: use fixed increment */
            val += 0x0098;
            if (val >= 0x0F00) val = 0x0F00;
            bus_wram_write16(0x0D60, val);
            op_sep(0x20);
        } else {
            op_rep(0x20);
            int16_t val = (int16_t)bus_wram_read16(0x0D60);
            if (val >= 0) {
                val -= 0x0058;
                if (val < 0) val = (int16_t)0x8000;
            }
            bus_wram_write16(0x0D60, (uint16_t)val);
            op_sep(0x20);
        }
    }

    nmi_joypad_read();
    nmi_audio_sync();

    /* $0E60 = $01D8 for scroll/sprite positioning */
    op_rep(0x20);
    bus_wram_write16(0x0E60, 0x01D8);
    op_sep(0x20);
}

/*
 * State $06 — Race mode force blank + color math (ROM $02:8292)
 *
 * Force blank, HDMA, color math, fixed color registers.
 * Next state: $04
 */
static void nmi_state_06(void) {
    bus_write8(0x00, 0x2100, 0x80);     /* force blank */

    op_sep(0x20);
    bus_write8(0x00, 0x4207, 0xE0);
    bus_write8(0x00, 0x4209, 0x10);
    bus_read8(0x00, 0x4211);

    bus_write8(0x00, 0x212C, 0x13);     /* main screen */
    bus_write8(0x00, 0x420C, bus_wram_read8(0x0D45));  /* HDMA */

    bus_wram_write8(0x0D3F, 0x04);      /* next state */

    /* Fixed color from $19AC-$19AE */
    bus_write8(0x00, 0x2132, bus_wram_read8(0x19AC));
    bus_write8(0x00, 0x2132, bus_wram_read8(0x19AD));
    bus_write8(0x00, 0x2132, bus_wram_read8(0x19AE));
}

/*
 * State $08 — Race 2P brightness + config (ROM $02:82DD)
 *
 * Brightness, timer setup, frame counters, OAM/audio.
 * Next state: $0A
 */
static void nmi_state_08(void) {
    uint8_t brightness = bus_wram_read8(0x0D61);
    nmi_wait_hblank();
    bus_write8(0x00, 0x2100, brightness);

    bus_write8(0x00, 0x4209, 0x6F);
    bus_write8(0x00, 0x4208, 0x00);
    bus_write8(0x00, 0x420A, 0x00);
    bus_write8(0x00, 0x4207, 0x10);
    bus_read8(0x00, 0x4211);

    bus_wram_write8(0x0D3F, 0x0A);      /* next state */

    uint8_t fc1 = bus_wram_read8(0x0E3B);
    bus_wram_write8(0x0E3B, fc1 + 1);
    uint8_t fc2 = bus_wram_read8(0x0306);
    bus_wram_write8(0x0306, fc2 + 1);

    bus_wram_write8(0x0D52, 0xFF);      /* frame ready */

    /* Brightness fade */
    {
        uint8_t fade_dir = bus_wram_read8(0x0E3A);
        if (fade_dir == 0) {
            op_rep(0x20);
            uint16_t val = bus_wram_read16(0x0D60);
            val += 0x0098;
            if (val >= 0x0F00) val = 0x0F00;
            bus_wram_write16(0x0D60, val);
            op_sep(0x20);
        } else {
            op_rep(0x20);
            int16_t val = (int16_t)bus_wram_read16(0x0D60);
            if (val >= 0) {
                val -= 0x0058;
                if (val < 0) val = (int16_t)0x8000;
            }
            bus_wram_write16(0x0D60, (uint16_t)val);
            op_sep(0x20);
        }
    }

    bus_write8(0x00, 0x212C, 0x13);     /* main screen */

    op_rep(0x20);
    bus_wram_write16(0x0E60, 0x0100);
    op_sep(0x20);

    nmi_joypad_read();
    nmi_audio_sync();
}

/*
 * State $0A — Race 2P force blank + scroll (ROM $02:8336)
 *
 * Force blank, fixed color, BG scroll, tilemap setup.
 * Next state: $0C
 */
static void nmi_state_0a(void) {
    bus_write8(0x00, 0x2100, 0x80);     /* force blank */
    nmi_wait_hblank();

    bus_write8(0x00, 0x4209, 0x77);
    bus_read8(0x00, 0x4211);

    bus_wram_write8(0x0D3F, 0x0C);      /* next state */

    /* Fixed color from $19AF-$19B1 */
    bus_write8(0x00, 0x2132, bus_wram_read8(0x19AF));
    bus_write8(0x00, 0x2132, bus_wram_read8(0x19B0));
    bus_write8(0x00, 0x2132, bus_wram_read8(0x19B1));

    /* BG4 scroll conditional on $0D5D */
    uint8_t scroll_flag = bus_wram_read8(0x0D5D);
    if (scroll_flag != 0) {
        if ((int8_t)scroll_flag > 0) {
            bus_write8(0x00, 0x2110, 0x80);
            bus_write8(0x00, 0x2110, 0x01);
        } else {
            bus_write8(0x00, 0x2110, 0x00);
            bus_write8(0x00, 0x2110, 0x00);
        }
        /* BG3 scroll */
        bus_write8(0x00, 0x210F, 0xA0);
        bus_write8(0x00, 0x210F, 0x01);
        /* BG tilemap */
        bus_write8(0x00, 0x2108, 0x78);
        bus_write8(0x00, 0x210B, 0x70);
    } else {
        uint8_t sc_flag = bus_wram_read8(0x0D5C);
        if (sc_flag != 0) {
            bus_write8(0x00, 0x210B, 0x40);
        }
        bus_write8(0x00, 0x2108, 0x3F);
    }
}

/*
 * State $0C — Race brightness + DMA helper (ROM $02:83A8)
 *
 * Brightness, H-timer, DMA helper call.
 * Next state: $0E
 */
static void nmi_state_0c(void) {
    bus_write8(0x00, 0x2100, bus_wram_read8(0x0D61));
    nmi_wait_hblank();

    bus_write8(0x00, 0x4209, 0xD7);
    bus_read8(0x00, 0x4211);

    bus_wram_write8(0x0D3F, 0x0E);      /* next state */

    op_rep(0x20);
    bus_wram_write16(0x0E60, 0x0000);
    op_sep(0x20);
}

/*
 * State $0E — Race force blank + BG/sprite DMA (ROM $02:83D7)
 *
 * Force blank, OAM DMA, sprite/BG DMA, scroll, HDMA, color math.
 * Next state: $08
 */
static void nmi_state_0e(void) {
    bus_write8(0x00, 0x2100, 0x80);     /* force blank */
    nmi_wait_hblank();

    nmi_oam_dma();

    /* BG4 scroll conditional on $0D5C/$0D5D */
    op_sep(0x30);
    uint8_t sf = bus_wram_read8(0x0D5C);
    uint8_t sd = bus_wram_read8(0x0D5D);
    if (sd != 0) {
        if ((int8_t)sd > 0) {
            bus_write8(0x00, 0x2110, 0xE8);
            bus_write8(0x00, 0x2110, 0x01);
        } else {
            bus_write8(0x00, 0x2110, 0x68);
            bus_write8(0x00, 0x2110, 0x00);
        }
        bus_write8(0x00, 0x210F, 0xF0);
        bus_write8(0x00, 0x210F, 0x01);
        bus_write8(0x00, 0x2108, 0x78);
        bus_write8(0x00, 0x210B, 0x70);
    } else {
        if (sf != 0) {
            bus_write8(0x00, 0x210B, 0x40);
        }
        bus_write8(0x00, 0x2107, 0x1C);
        bus_write8(0x00, 0x2108, 0x3F);
    }

    /* PPU mode + main screen + HDMA */
    bus_write8(0x00, 0x2105, 0x01);
    bus_write8(0x00, 0x212C, 0x03);
    bus_write8(0x00, 0x420C, 0x1C);

    /* Fixed color from $19AC-$19AE */
    bus_write8(0x00, 0x2132, bus_wram_read8(0x19AC));
    bus_write8(0x00, 0x2132, bus_wram_read8(0x19AD));
    bus_write8(0x00, 0x2132, bus_wram_read8(0x19AE));

    /* H-timer + next state */
    bus_write8(0x00, 0x4209, 0x0F);
    bus_read8(0x00, 0x4211);
    bus_wram_write8(0x0D3F, 0x08);      /* next state */
}

/*
 * State $10 — Gameplay brightness + scroll (ROM $02:809C)
 *
 * Main screen, BG4 scroll, frame counters, joypad, fade, audio.
 * Next state: $12
 */
static void nmi_state_10(void) {
    op_sep(0x30);
    nmi_wait_hblank();

    /* Main screen = BG1 + BG2 + OBJ */
    bus_write8(0x00, 0x212C, 0x13);

    /* BG4 scroll from game variables */
    bus_write8(0x00, 0x2110, bus_wram_read8(0x0E37));
    bus_write8(0x00, 0x2110, bus_wram_read8(0x0E34));

    /* H-timer = $D8, clear IRQ */
    bus_write8(0x00, 0x4209, 0xD8);
    bus_read8(0x00, 0x4211);

    /* Next state = $12 */
    bus_wram_write8(0x0D3F, 0x12);

    /* Frame counters */
    uint8_t fc1 = bus_wram_read8(0x0E3B);
    bus_wram_write8(0x0E3B, fc1 + 1);
    uint8_t fc2 = bus_wram_read8(0x0306);
    bus_wram_write8(0x0306, fc2 + 1);

    /* Joypad read ($B1AE) */
    nmi_joypad_read();

    /* Brightness fade ($AC4D) */
    nmi_brightness_fade_in();

    /* Audio sync ($B112) */
    nmi_audio_sync();
}

/*
 * State $12 — Gameplay force blank + VRAM DMA (ROM $02:80CF)
 *
 * Force blank, HDMA, color math, OAM DMA, BG4 scroll,
 * fixed color, VRAM DMA from $7F:93D7 → VRAM $5000,
 * GSU sprite DMA (conditional), window config, brightness.
 * Next state: $10
 */
static void nmi_state_12(void) {
    op_sep(0x30);
    nmi_wait_hblank();

    /* Force blank */
    bus_write8(0x00, 0x2100, 0x80);

    /* H-timer = $37, clear IRQ */
    bus_write8(0x00, 0x4209, 0x37);
    bus_read8(0x00, 0x4211);

    /* V-timer disable */
    bus_write8(0x00, 0x420A, 0x00);

    /* HDMA channels from game variable */
    bus_write8(0x00, 0x420C, bus_wram_read8(0x0D45));

    /* Color math from game variable */
    bus_write8(0x00, 0x2131, bus_wram_read8(0x0D47));

    /* Next state = $10, frame ready */
    bus_wram_write8(0x0D3F, 0x10);
    bus_wram_write8(0x0D52, 0xFF);

    /* OAM DMA ($7E:AD16) */
    nmi_oam_dma();

    /* BG4 scroll */
    bus_write8(0x00, 0x2110, bus_wram_read8(0x0E33));
    bus_write8(0x00, 0x2110, bus_wram_read8(0x0E34));

    /* Fixed color register setup from $7F:340D */
    uint8_t fc = bus_read8(0x7F, 0x340D);
    bus_write8(0x00, 0x2132, fc | 0x80);          /* blue */
    bus_write8(0x00, 0x2132, (fc & 0x7F) | 0x40); /* green */
    bus_write8(0x00, 0x2132, (fc & 0x1F) | 0x20); /* red */

    /* Main screen = BG1 + BG2 */
    bus_write8(0x00, 0x212C, 0x03);

    /* ── VRAM DMA: $7F:93D7 → VRAM $5000, $1000 bytes ────── */
    op_rep(0x20);
    bus_write16(0x00, 0x4305, 0x1000);  /* count */
    bus_write16(0x00, 0x4302, 0x93D7);  /* source addr */
    bus_write16(0x00, 0x2116, 0x5000);  /* VRAM addr */
    bus_write16(0x00, 0x4300, 0x1801);  /* word mode to $2118 */
    op_sep(0x20);
    bus_write8(0x00, 0x4304, 0x7F);     /* source bank */
    bus_write8(0x00, 0x2115, 0x80);     /* VRAM inc mode */

    /* Release GSU RAM bus for DMA */
    uint8_t scmr = bus_wram_read8(0x0374);
    bus_write8(0x00, 0x303A, scmr & 0xF7);  /* clear RAN */
    bus_write8(0x00, 0x420B, 0x01);          /* trigger DMA */
    bus_write8(0x00, 0x303A, scmr | 0x18);   /* restore RON+RAN */

    /* ── Conditional sprite VRAM DMA from GSU RAM ────────── */
    op_rep(0x30);
    uint16_t sprite_flag = bus_wram_read16(0x0D7F);
    if (sprite_flag != 0) {
        bus_wram_write16(0x0D7F, 0x0000);  /* clear flag */

        /* DMA from $70:2C00 → VRAM $0400, $0800 bytes */
        nmi_vram_dma_gsu(0x0800, 0x2C00, 0x0400);

        op_rep(0x20);
        bus_write16(0x00, 0x0D81, 0xFFFF);
    }

    /* ── Window/HDMA table configuration ($A46B) ─────────── */
    nmi_window_config();

    /* Set brightness from game variable */
    op_sep(0x20);
    bus_write8(0x00, 0x2100, bus_wram_read8(0x0D61));
}

/*
 * State $14 — IRQ mid-screen state (ROM $02:8264)
 *
 * Sets up H-timer for mid-screen IRQ, calls WRAM-resident
 * rendering helpers for sprite/BG compositing.
 * Next state: $06
 */
static void nmi_state_14(void) {
    bus_write8(0x00, 0x4209, 0xD0);
    bus_read8(0x00, 0x4211);

    bus_wram_write8(0x0D3F, 0x06);      /* next state */

    /* JSL targets for rendering pipeline — these are WRAM-resident
     * routines for sprite compositing, BG layer management, etc.
     * They operate on GSU framebuffer data in $7F bank.
     * For now we skip these — they'll be recompiled with the
     * race mode rendering pipeline. */
}

/* ── NMI dispatch ───────────────────────────────────────────────── */

/*
 * $7E:A305 — NMI work dispatch (ROM $02:802C)
 *
 * Reads NMI state from $0D3F, dispatches to appropriate handler.
 */
void srf_NMI_work(void) {
    op_sep(0x30);

    uint8_t state = bus_wram_read8(0x0D3F);

    switch (state) {
    case 0x00: nmi_state_00(); break;
    case 0x02: nmi_state_02(); break;
    case 0x04: nmi_state_04(); break;
    case 0x06: nmi_state_06(); break;
    case 0x08: nmi_state_08(); break;
    case 0x0A: nmi_state_0a(); break;
    case 0x0C: nmi_state_0c(); break;
    case 0x0E: nmi_state_0e(); break;
    case 0x10: nmi_state_10(); break;
    case 0x12: nmi_state_12(); break;
    case 0x14: nmi_state_14(); break;
    case 0x16: break;  /* no-op (RTL) */
    default:
        /* Unknown state — increment frame counters to prevent hangs */
        {
            uint8_t fc = bus_wram_read8(0x05E9);
            bus_wram_write8(0x05E9, fc + 1);
            uint8_t fc2 = bus_wram_read8(0x0306);
            bus_wram_write8(0x0306, fc2 + 1);
        }
        break;
    }
}
