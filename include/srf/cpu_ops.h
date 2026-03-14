/*
 * Stunt Race FX — 65816 instruction helpers
 *
 * Thin wrappers around snesrecomp's CPU and bus primitives.
 * Each macro / inline function corresponds to a single 65816 opcode
 * so recompiled functions read almost like the original disassembly.
 */
#ifndef SRF_CPU_OPS_H
#define SRF_CPU_OPS_H

#include <snesrecomp/cpu.h>
#include <snesrecomp/bus.h>

/* ── processor flag manipulation ─────────────────────────── */
#define OP_SEI()  (g_cpu.flag_I = 1)
#define OP_CLI()  (g_cpu.flag_I = 0)
#define OP_SEC()  (g_cpu.flag_C = 1)
#define OP_CLC()  (g_cpu.flag_C = 0)
#define OP_SED()  (g_cpu.flag_D = 1)
#define OP_CLD()  (g_cpu.flag_D = 0)
#define OP_CLV()  (g_cpu.flag_V = 0)

/* ── transfer helpers ────────────────────────────────────── */
#define OP_TCS()  (g_cpu.SP = g_cpu.A)
#define OP_TSC()  (g_cpu.A  = g_cpu.SP)
#define OP_TCD()  (g_cpu.DP = g_cpu.A)
#define OP_TDC()  (g_cpu.A  = g_cpu.DP)

/* ── data bank ───────────────────────────────────────────── */
#define OP_SET_DB(b)  (g_cpu.DB = (b))

/* ── REP / SEP ───────────────────────────────────────────── */
static inline void op_rep(uint8_t mask) {
    if (mask & 0x20) g_cpu.flag_M = 0;   /* 16-bit A */
    if (mask & 0x10) g_cpu.flag_X = 0;   /* 16-bit X/Y */
    if (mask & 0x01) g_cpu.flag_C = 0;
    if (mask & 0x02) g_cpu.flag_Z = 0;
    if (mask & 0x80) g_cpu.flag_N = 0;
    if (mask & 0x40) g_cpu.flag_V = 0;
}

static inline void op_sep(uint8_t mask) {
    if (mask & 0x20) g_cpu.flag_M = 1;   /* 8-bit A */
    if (mask & 0x10) g_cpu.flag_X = 1;   /* 8-bit X/Y */
    if (mask & 0x01) g_cpu.flag_C = 1;
    if (mask & 0x02) g_cpu.flag_Z = 1;
    if (mask & 0x80) g_cpu.flag_N = 1;
    if (mask & 0x40) g_cpu.flag_V = 1;
}

/* ── XCE (exchange carry & emulation) ────────────────────── */
static inline void op_xce(void) {
    uint8_t tmp  = g_cpu.flag_E;
    g_cpu.flag_E = g_cpu.flag_C;
    g_cpu.flag_C = tmp;
}

/* ── XBA (exchange B and A bytes) ────────────────────────── */
static inline void op_xba(void) {
    uint16_t lo = g_cpu.A & 0xFF;
    uint16_t hi = (g_cpu.A >> 8) & 0xFF;
    g_cpu.A = (lo << 8) | hi;
    g_cpu.flag_N = (hi & 0x80) ? 1 : 0;
    g_cpu.flag_Z = (hi == 0) ? 1 : 0;
}

/* ── load immediate ──────────────────────────────────────── */
static inline void op_lda_imm8(uint8_t v) {
    g_cpu.A = (g_cpu.A & 0xFF00) | v;
    g_cpu.flag_N = (v & 0x80) ? 1 : 0;
    g_cpu.flag_Z = (v == 0) ? 1 : 0;
}

static inline void op_lda_imm16(uint16_t v) {
    g_cpu.A = v;
    g_cpu.flag_N = (v & 0x8000) ? 1 : 0;
    g_cpu.flag_Z = (v == 0) ? 1 : 0;
}

static inline void op_ldx_imm16(uint16_t v) {
    g_cpu.X = v;
    g_cpu.flag_N = (v & 0x8000) ? 1 : 0;
    g_cpu.flag_Z = (v == 0) ? 1 : 0;
}

static inline void op_ldy_imm16(uint16_t v) {
    g_cpu.Y = v;
    g_cpu.flag_N = (v & 0x8000) ? 1 : 0;
    g_cpu.flag_Z = (v == 0) ? 1 : 0;
}

/* ── store absolute ──────────────────────────────────────── */
static inline void op_sta_abs8(uint16_t addr) {
    bus_write8(g_cpu.DB, addr, (uint8_t)(g_cpu.A & 0xFF));
}

static inline void op_sta_abs16(uint16_t addr) {
    bus_write8(g_cpu.DB, addr,     (uint8_t)(g_cpu.A & 0xFF));
    bus_write8(g_cpu.DB, addr + 1, (uint8_t)((g_cpu.A >> 8) & 0xFF));
}

/* ── load absolute ───────────────────────────────────────── */
static inline void op_lda_abs8(uint16_t addr) {
    uint8_t v = bus_read8(g_cpu.DB, addr);
    g_cpu.A = (g_cpu.A & 0xFF00) | v;
    g_cpu.flag_N = (v & 0x80) ? 1 : 0;
    g_cpu.flag_Z = (v == 0) ? 1 : 0;
}

static inline void op_lda_abs16(uint16_t addr) {
    uint16_t v = bus_read8(g_cpu.DB, addr) |
                 ((uint16_t)bus_read8(g_cpu.DB, addr + 1) << 8);
    g_cpu.A = v;
    g_cpu.flag_N = (v & 0x8000) ? 1 : 0;
    g_cpu.flag_Z = (v == 0) ? 1 : 0;
}

/* ── stack operations ────────────────────────────────────── */
static inline void op_php(void) {
    uint8_t p = 0;
    if (g_cpu.flag_N) p |= 0x80;
    if (g_cpu.flag_V) p |= 0x40;
    if (g_cpu.flag_M) p |= 0x20;
    if (g_cpu.flag_X) p |= 0x10;
    if (g_cpu.flag_D) p |= 0x08;
    if (g_cpu.flag_I) p |= 0x04;
    if (g_cpu.flag_Z) p |= 0x02;
    if (g_cpu.flag_C) p |= 0x01;
    bus_write8(0x00, g_cpu.SP, p);
    g_cpu.SP--;
}

static inline void op_plp(void) {
    g_cpu.SP++;
    uint8_t p = bus_read8(0x00, g_cpu.SP);
    g_cpu.flag_N = (p >> 7) & 1;
    g_cpu.flag_V = (p >> 6) & 1;
    g_cpu.flag_M = (p >> 5) & 1;
    g_cpu.flag_X = (p >> 4) & 1;
    g_cpu.flag_D = (p >> 3) & 1;
    g_cpu.flag_I = (p >> 2) & 1;
    g_cpu.flag_Z = (p >> 1) & 1;
    g_cpu.flag_C = p & 1;
}

static inline void op_pha16(void) {
    bus_write8(0x00, g_cpu.SP,     (uint8_t)((g_cpu.A >> 8) & 0xFF));
    bus_write8(0x00, g_cpu.SP - 1, (uint8_t)(g_cpu.A & 0xFF));
    g_cpu.SP -= 2;
}

static inline void op_pla16(void) {
    uint16_t lo = bus_read8(0x00, g_cpu.SP + 1);
    uint16_t hi = bus_read8(0x00, g_cpu.SP + 2);
    g_cpu.A = (hi << 8) | lo;
    g_cpu.SP += 2;
    g_cpu.flag_N = (g_cpu.A & 0x8000) ? 1 : 0;
    g_cpu.flag_Z = (g_cpu.A == 0) ? 1 : 0;
}

static inline void op_phx16(void) {
    bus_write8(0x00, g_cpu.SP,     (uint8_t)((g_cpu.X >> 8) & 0xFF));
    bus_write8(0x00, g_cpu.SP - 1, (uint8_t)(g_cpu.X & 0xFF));
    g_cpu.SP -= 2;
}

static inline void op_plx16(void) {
    uint16_t lo = bus_read8(0x00, g_cpu.SP + 1);
    uint16_t hi = bus_read8(0x00, g_cpu.SP + 2);
    g_cpu.X = (hi << 8) | lo;
    g_cpu.SP += 2;
    g_cpu.flag_N = (g_cpu.X & 0x8000) ? 1 : 0;
    g_cpu.flag_Z = (g_cpu.X == 0) ? 1 : 0;
}

/* ── compare immediate ───────────────────────────────────── */
static inline void op_cmp_imm8(uint8_t v) {
    uint8_t a = (uint8_t)(g_cpu.A & 0xFF);
    uint16_t r = (uint16_t)a - (uint16_t)v;
    g_cpu.flag_N = (r & 0x80) ? 1 : 0;
    g_cpu.flag_Z = ((r & 0xFF) == 0) ? 1 : 0;
    g_cpu.flag_C = (a >= v) ? 1 : 0;
}

static inline void op_cmp_imm16(uint16_t v) {
    uint32_t r = (uint32_t)g_cpu.A - (uint32_t)v;
    g_cpu.flag_N = (r & 0x8000) ? 1 : 0;
    g_cpu.flag_Z = ((r & 0xFFFF) == 0) ? 1 : 0;
    g_cpu.flag_C = (g_cpu.A >= v) ? 1 : 0;
}

#endif /* SRF_CPU_OPS_H */
