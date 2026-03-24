/*
 * Stunt Race FX — SPC700 audio engine upload
 *
 * The game uploads its audio engine to the SPC700 via the IPL
 * boot protocol using APU ports $2140-$2143. This is the standard
 * SNES SPC700 transfer sequence used by most games.
 *
 * $04:D44C — Audio init entry point (called from $03:8AA9 init)
 * $04:D720 — IPL transfer routine (uploads data blocks to SPC700)
 *
 * The audio data is stored at $04:C27C in ROM (pointed to by
 * the table at that address).
 */

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>
#include <srf/cpu_ops.h>
#include <srf/functions.h>

/*
 * $04:D788 — Read next byte from audio data stream
 *
 * Reads a byte from [DP:$FB], increments pointer.
 * Used by the IPL transfer to stream data to the SPC700.
 */
static uint8_t srf_read_audio_byte(void) {
    uint16_t ptr = bus_wram_read16(0x00FB);
    uint8_t bank = bus_wram_read8(0x00FD);
    uint8_t val = bus_read8(bank, ptr);
    ptr++;
    if (ptr == 0) bank++;  /* handle bank wrap */
    bus_wram_write16(0x00FB, ptr);
    bus_wram_write8(0x00FD, bank);
    return val;
}

/*
 * $04:D720 — SPC700 IPL transfer
 *
 * Implements the standard SNES IPL boot protocol to upload
 * audio data to the SPC700. The protocol:
 * 1. Wait for SPC700 ready signal ($AA at $2140)
 * 2. Send $CC to $2140 to begin transfer
 * 3. Stream data blocks: send address, then bytes one at a time
 * 4. Each byte is acknowledged by the SPC700 echoing back
 *
 * A = block count (or command), Y = data pointer
 */
void srf_04D720(void) {
    op_sep(0x20);

    /* Store data pointer and bank */
    bus_wram_write16(0x00FB, g_cpu.Y);  /* data address */
    bus_wram_write8(0x00FD, (uint8_t)(g_cpu.C & 0xFF));  /* bank in A low */

    /* WORKAROUND: Skip the SPC700 IPL transfer for now.
     * The LakeSnes APU isn't clocked during the init chain, so the
     * SPC700 can't respond to the handshake protocol. The audio
     * engine will be uploaded once the frame loop starts and the
     * APU begins running. For now, just return to avoid blocking. */
    return;
#if 0  /* Original IPL transfer code — re-enable when APU timing is fixed */

    /* Wait for SPC700 ready: $2140 == $AA */
    /* In recomp, the APU boots through LakeSnes, so the IPL
     * handshake may already be complete. We perform the
     * transfer by writing to APU ports and letting LakeSnes
     * handle the SPC700 side. */
    uint8_t counter = 0;

    /* Wait for SPC700 to signal ready.
     * In recomp, the SPC700 IPL may not have booted yet since the
     * APU only runs during snesrecomp_end_frame(). Use a short
     * timeout and proceed regardless — the bus writes go directly
     * to the SPC700 input ports via LakeSnes. */
    int timeout = 1000;
    while (timeout-- > 0) {
        if (bus_read8(0x00, 0x2140) == 0xAA) break;
    }

    /* Send $CC to begin transfer */
    counter = 0xCC;

    /* Read transfer blocks from the audio data stream */
    /* Each block: 2 bytes size, 2 bytes dest addr, then data bytes */
    while (1) {
        /* Read block header */
        uint8_t size_lo = srf_read_audio_byte();
        uint8_t size_hi = srf_read_audio_byte();
        uint16_t block_size = (uint16_t)(size_lo | (size_hi << 8));

        if (block_size == 0) break;  /* end of transfer */

        /* Read destination address in SPC700 RAM */
        uint8_t addr_lo = srf_read_audio_byte();
        uint8_t addr_hi = srf_read_audio_byte();

        /* Write destination to APU ports $2142-$2143 */
        bus_write8(0x00, 0x2142, addr_lo);
        bus_write8(0x00, 0x2143, addr_hi);

        /* Transfer data bytes */
        /* First byte */
        uint8_t data = srf_read_audio_byte();
        bus_write8(0x00, 0x2141, data);
        bus_write8(0x00, 0x2140, counter);

        /* Wait for acknowledgment */
        timeout = 1000;
        while (timeout-- > 0) {
            if (bus_read8(0x00, 0x2140) == counter) break;
        }
        counter++;

        /* Remaining bytes */
        for (uint16_t i = 1; i < block_size; i++) {
            data = srf_read_audio_byte();
            bus_write8(0x00, 0x2141, data);
            bus_write8(0x00, 0x2140, counter);

            timeout = 100000;
            while (timeout-- > 0) {
                if (bus_read8(0x00, 0x2140) == counter) break;
            }
            counter++;
            if (counter == 0) counter++;  /* skip zero */
        }
    }

    /* Read execution address */
    uint8_t exec_lo = srf_read_audio_byte();
    uint8_t exec_hi = srf_read_audio_byte();

    /* Send execution command */
    bus_write8(0x00, 0x2142, exec_lo);
    bus_write8(0x00, 0x2143, exec_hi);
    bus_write8(0x00, 0x2141, 0x00);

    /* Final handshake — send counter+2 to start execution */
    counter += 2;
    if (counter == 0) counter = 2;
    bus_write8(0x00, 0x2140, counter);

    /* Wait for final ack */
    timeout = 1000;
    while (timeout-- > 0) {
        if (bus_read8(0x00, 0x2140) == counter) break;
    }
#endif  /* Original IPL transfer code */
}

/*
 * $04:D6E1 — Audio state clear
 *
 * Clears all audio state variables and APU sync ports.
 * Called during display mode transitions to reset sound state.
 */
void srf_04D6E1(void) {
    op_php();
    op_sep(0x30);

    bus_wram_write8(0x0E4E, 0x00);
    bus_wram_write8(0x0DD0, 0x00);
    bus_wram_write8(0x0DD2, 0x00);
    bus_wram_write8(0x0DC8, 0x00);
    bus_wram_write8(0x0DCA, 0x00);
    bus_wram_write8(0x0DCE, 0x00);
    bus_wram_write8(0x0DCC, 0x00);
    bus_wram_write8(0x0E1B, 0x00);
    bus_wram_write8(0x0E1C, 0x00);
    bus_write8(0x00, 0x2143, 0x00);
    bus_write8(0x00, 0x2142, 0x00);

    op_plp();
}

/*
 * $04:D649 — Audio command queue (enqueue to ring buffer)
 *
 * Adds an audio command byte (A) to the circular ring buffer
 * at $0DA8 (16 entries). The NMI audio sync handler ($B112)
 * reads from this buffer and sends commands to the SPC700.
 *
 * Ring buffer state:
 *   $0DC8 = read index (consumer)
 *   $0DCC = count of pending commands
 *   $0DA8-$0DB7 = 16-byte ring buffer
 *
 * Skips if $0E4E != 0 (audio muted) or buffer full (>= 16).
 */
void srf_04D649(void) {
    op_php();
    op_sep(0x30);

    uint8_t cmd = CPU_A8();

    /* Check mute flag */
    if (bus_wram_read8(0x0E4E) != 0) {
        op_plp();
        return;
    }

    uint8_t count = bus_wram_read8(0x0DCC);

    if (count == 0) {
        /* Buffer empty — start fresh */
        bus_wram_write8(0x0DCC, 1);
        uint8_t idx = bus_wram_read8(0x0DC8);
        bus_wram_write8(0x0DA8 + idx, cmd);
    } else if (count < 0x10) {
        /* Append to ring buffer */
        uint8_t write_idx = (count + bus_wram_read8(0x0DC8)) & 0x0F;
        bus_wram_write8(0x0DA8 + write_idx, cmd);
        bus_wram_write8(0x0DCC, count + 1);
    }
    /* If count >= 16, buffer full — drop command */

    op_plp();
}

/*
 * $04:D43C — Send stop command to SPC700 + busy-wait
 *
 * Writes $05 to APU port $2141 (stop command), then waits $4000 cycles.
 */
static void audio_send_stop_wait(void) {
    op_sep(0x20);
    bus_write8(0x00, 0x2141, 0x05);
    /* Busy-wait $4000 iterations (skip in recomp) */
}

/*
 * $04:D443 — Busy-wait helper
 *
 * Waits $4000 iterations. Used between SPC700 transfers.
 */
static void audio_busy_wait(void) {
    /* Skip busy-wait in recomp */
}

/*
 * $04:D0DB — Audio/music reload
 *
 * Handles audio engine reloading and music track changes during
 * scene transitions. Two main paths:
 *
 * Path 1 ($10DC != 0): Full audio engine re-upload
 *   - Re-uploads SPC700 engine from ROM
 *   - Copies instrument data from $1E:9B2C → $70:6000
 *   - Uploads instrument data to SPC700
 *   - Uploads additional music data from $04:B8A0
 *
 * Path 2 ($10DC == 0): Music track loading
 *   - Looks up song/SFX data from tables at $04:D322 indexed by $0E3D
 *   - Uploads multiple SPC700 data blocks
 *   - Waits for APU ports to clear
 */
void srf_04D0DB(void) {
    op_php();
    op_sep(0x20);

    /* If $10DA != 0, nothing to do */
    if (bus_wram_read8(0x10DA) != 0) {
        op_plp();
        return;
    }

    /* Check if audio engine reload is needed */
    uint8_t dc = bus_wram_read8(0x10DC);
    if (dc != 0) {
        /* ── Path 1: Full audio engine reload ──────────── */
        uint8_t de = bus_wram_read8(0x10DE);
        if (de != 0) {
            op_plp();
            return;
        }

        /* Set reload flag */
        bus_wram_write8(0x10DE, bus_wram_read8(0x10DE) - 1);

        /* Send stop command to SPC700 */
        op_rep(0x30);
        audio_send_stop_wait();

        /* Send stop via $2141 and wait */
        op_rep(0x30);
        bus_write16(0x00, 0x2141, 0x0005);
        audio_busy_wait();

        /* Re-upload SPC700 engine from ROM $04:8D4C */
        op_rep(0x30);
        CPU_SET_A16(0x0019);
        g_cpu.Y = 0x8D4C;
        srf_04D720();

        audio_busy_wait();
        audio_busy_wait();

        /* Send another stop + wait */
        op_rep(0x30);
        bus_write16(0x00, 0x2141, 0x0005);
        audio_busy_wait();

        /* Copy instrument data: $1E:9B2C → $70:6000 ($1518 bytes) */
        op_rep(0x20);
        for (uint16_t i = 0; i < 0x1518; i += 2) {
            uint16_t val = bus_read16(0x1E, 0x9B2C + i);
            bus_write16(0x70, 0x6000 + i, val);
        }

        /* Upload instrument data from GSU RAM to SPC700 */
        op_rep(0x30);
        g_cpu.Y = 0x6000;
        CPU_SET_A16(0x0070);
        srf_04D720();

        audio_busy_wait();

        /* Send stop + wait */
        op_rep(0x30);
        bus_write16(0x00, 0x2141, 0x0005);
        audio_busy_wait();

        /* Upload additional music data from ROM $04:B8A0 */
        op_rep(0x30);
        CPU_SET_A16(0x001B);
        g_cpu.Y = 0xB8A0;
        srf_04D720();

        audio_busy_wait();

        /* Clear audio state */
        srf_04D6E1();

        /* Send play command to APU */
        op_sep(0x20);
        bus_write8(0x00, 0x2140, 0x04);

        op_plp();
        return;
    }

    /* ── Path 2: Music track loading ───────────────────── */
    op_rep(0x30);

    /* Calculate table index from display config */
    uint16_t cfg = bus_wram_read16(0x0E3D) & 0x00FF;
    uint16_t tbl_idx = cfg * 3;

    /* Send stop command */
    audio_send_stop_wait();

    /* Look up primary song data from table at $04:D322 */
    uint8_t song_bank = bus_read8(0x04, 0xD324 + tbl_idx);
    uint16_t song_addr = bus_read16(0x04, 0xD322 + tbl_idx);

    /* Upload song data to SPC700 if valid */
    if (song_addr != 0) {
        g_cpu.Y = song_addr;
        CPU_SET_A16((uint16_t)song_bank);
        srf_04D720();
    }

    audio_busy_wait();

    /* Look up secondary audio block from table at $04:D376 */
    op_rep(0x30);
    uint16_t sec_addr = bus_read16(0x04, 0xD376 + tbl_idx);
    if (sec_addr != 0xFFFF) {
        uint8_t sec_bank = bus_read8(0x04, 0xD378 + tbl_idx);
        audio_send_stop_wait();
        g_cpu.Y = sec_addr;
        CPU_SET_A16((uint16_t)sec_bank);
        srf_04D720();
    }

    audio_busy_wait();

    /* Load instrument data based on display mode ($0D89) */
    op_rep(0x20);
    uint16_t mode_cfg = bus_wram_read16(0x0D89) & 0x000F;
    uint16_t inst_idx = mode_cfg * 3;

    /* Look up instrument block from table at $04:D41E */
    uint16_t inst_addr = bus_read16(0x04, 0xD41E + inst_idx);
    if (inst_addr != 0xFFFF) {
        uint8_t inst_bank = bus_read8(0x04, 0xD420 + inst_idx);
        audio_send_stop_wait();
        g_cpu.Y = inst_addr;
        CPU_SET_A16((uint16_t)inst_bank);
        srf_04D720();
    }

    audio_busy_wait();

    /* Look up tertiary audio block from table at $04:D3CA */
    op_rep(0x30);
    uint16_t tert_addr = bus_read16(0x04, 0xD3CA + tbl_idx);
    if (tert_addr != 0xFFFF) {
        uint8_t tert_bank = bus_read8(0x04, 0xD3CC + tbl_idx);
        audio_send_stop_wait();
        g_cpu.Y = tert_addr;
        CPU_SET_A16((uint16_t)tert_bank);
        srf_04D720();
    }

    audio_busy_wait();

    /* Wait for all APU ports to clear (handshake complete) */
    op_sep(0x20);
    /* In recomp, skip the busy-wait on APU ports — the SPC700
     * processes commands synchronously through LakeSnes */

    op_plp();
}

/*
 * $04:D44C — Audio initialization
 *
 * Called during boot. Loads the SPC700 audio engine.
 *   - Calls $04:D720 with A=$19, Y=$C27C (audio data table in bank $04)
 *   - Busy-waits $4000 iterations for SPC700 to initialize
 */
void srf_04D44C(void) {
    op_php();
    op_rep(0x30);

    /* LDA #$0019 / LDY #$C27C / JSL $04D720 */
    CPU_SET_A16(0x0019);
    g_cpu.Y = 0xC27C;
    srf_04D720();

    /* Busy-wait for SPC700 init */
    op_rep(0x20);
    for (uint32_t i = 0; i < 0x4000; i++) {
        /* spin */
    }

    op_plp();
}
