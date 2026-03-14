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

    /* Wait for SPC700 ready: $2140 == $AA */
    /* In recomp, the APU boots through LakeSnes, so the IPL
     * handshake may already be complete. We perform the
     * transfer by writing to APU ports and letting LakeSnes
     * handle the SPC700 side. */
    uint8_t counter = 0;

    /* Wait for SPC700 to signal ready */
    int timeout = 100000;
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
        timeout = 100000;
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
    timeout = 100000;
    while (timeout-- > 0) {
        if (bus_read8(0x00, 0x2140) == counter) break;
    }
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
