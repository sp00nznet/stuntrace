#!/usr/bin/env python3
"""
Stunt Race FX — 65C816 disassembler for LoROM + Super FX mapping.

Usage: python disasm.py <rom.sfc> <bank:addr> [count]
  e.g. python disasm.py "Stunt Race FX (USA).sfc" 03:8AA9 64
"""

import sys, struct

# 65C816 opcode table: (mnemonic, addressing_mode, base_size)
# Size depends on M/X flags for some opcodes; base_size is for 8-bit mode
# Modes: imp=implied, imm=immediate, dp=direct_page, abs=absolute,
#         lng=long, rel8=relative8, rel16=relative16, etc.

OPCODES = {
    0x00: ("BRK", "imm8", 2), 0x01: ("ORA", "dpxi", 2), 0x02: ("COP", "imm8", 2),
    0x03: ("ORA", "sr", 2), 0x04: ("TSB", "dp", 2), 0x05: ("ORA", "dp", 2),
    0x06: ("ASL", "dp", 2), 0x07: ("ORA", "dpil", 2), 0x08: ("PHP", "imp", 1),
    0x09: ("ORA", "immM", 2), 0x0A: ("ASL", "acc", 1), 0x0B: ("PHD", "imp", 1),
    0x0C: ("TSB", "abs", 3), 0x0D: ("ORA", "abs", 3), 0x0E: ("ASL", "abs", 3),
    0x0F: ("ORA", "lng", 4),
    0x10: ("BPL", "rel8", 2), 0x11: ("ORA", "dpiy", 2), 0x12: ("ORA", "dpi", 2),
    0x13: ("ORA", "sriy", 2), 0x14: ("TRB", "dp", 2), 0x15: ("ORA", "dpx", 2),
    0x16: ("ASL", "dpx", 2), 0x17: ("ORA", "dpily", 2), 0x18: ("CLC", "imp", 1),
    0x19: ("ORA", "absy", 3), 0x1A: ("INC", "acc", 1), 0x1B: ("TCS", "imp", 1),
    0x1C: ("TRB", "abs", 3), 0x1D: ("ORA", "absx", 3), 0x1E: ("ASL", "absx", 3),
    0x1F: ("ORA", "lngx", 4),
    0x20: ("JSR", "abs", 3), 0x21: ("AND", "dpxi", 2), 0x22: ("JSL", "lng", 4),
    0x23: ("AND", "sr", 2), 0x24: ("BIT", "dp", 2), 0x25: ("AND", "dp", 2),
    0x26: ("ROL", "dp", 2), 0x27: ("AND", "dpil", 2), 0x28: ("PLP", "imp", 1),
    0x29: ("AND", "immM", 2), 0x2A: ("ROL", "acc", 1), 0x2B: ("PLD", "imp", 1),
    0x2C: ("BIT", "abs", 3), 0x2D: ("AND", "abs", 3), 0x2E: ("ROL", "abs", 3),
    0x2F: ("AND", "lng", 4),
    0x30: ("BMI", "rel8", 2), 0x31: ("AND", "dpiy", 2), 0x32: ("AND", "dpi", 2),
    0x33: ("AND", "sriy", 2), 0x34: ("BIT", "dpx", 2), 0x35: ("AND", "dpx", 2),
    0x36: ("ROL", "dpx", 2), 0x37: ("AND", "dpily", 2), 0x38: ("SEC", "imp", 1),
    0x39: ("AND", "absy", 3), 0x3A: ("DEC", "acc", 1), 0x3B: ("TSC", "imp", 1),
    0x3C: ("BIT", "absx", 3), 0x3D: ("AND", "absx", 3), 0x3E: ("ROL", "absx", 3),
    0x3F: ("AND", "lngx", 4),
    0x40: ("RTI", "imp", 1), 0x41: ("EOR", "dpxi", 2), 0x42: ("WDM", "imm8", 2),
    0x43: ("EOR", "sr", 2), 0x44: ("MVP", "mv", 3), 0x45: ("EOR", "dp", 2),
    0x46: ("LSR", "dp", 2), 0x47: ("EOR", "dpil", 2), 0x48: ("PHA", "imp", 1),
    0x49: ("EOR", "immM", 2), 0x4A: ("LSR", "acc", 1), 0x4B: ("PHK", "imp", 1),
    0x4C: ("JMP", "abs", 3), 0x4D: ("EOR", "abs", 3), 0x4E: ("LSR", "abs", 3),
    0x4F: ("EOR", "lng", 4),
    0x50: ("BVC", "rel8", 2), 0x51: ("EOR", "dpiy", 2), 0x52: ("EOR", "dpi", 2),
    0x53: ("EOR", "sriy", 2), 0x54: ("MVN", "mv", 3), 0x55: ("EOR", "dpx", 2),
    0x56: ("LSR", "dpx", 2), 0x57: ("EOR", "dpily", 2), 0x58: ("CLI", "imp", 1),
    0x59: ("EOR", "absy", 3), 0x5A: ("PHY", "imp", 1), 0x5B: ("TCD", "imp", 1),
    0x5C: ("JML", "lng", 4), 0x5D: ("EOR", "absx", 3), 0x5E: ("LSR", "absx", 3),
    0x5F: ("EOR", "lngx", 4),
    0x60: ("RTS", "imp", 1), 0x61: ("ADC", "dpxi", 2), 0x62: ("PER", "rel16", 3),
    0x63: ("ADC", "sr", 2), 0x64: ("STZ", "dp", 2), 0x65: ("ADC", "dp", 2),
    0x66: ("ROR", "dp", 2), 0x67: ("ADC", "dpil", 2), 0x68: ("PLA", "imp", 1),
    0x69: ("ADC", "immM", 2), 0x6A: ("ROR", "acc", 1), 0x6B: ("RTL", "imp", 1),
    0x6C: ("JMP", "absi", 3), 0x6D: ("ADC", "abs", 3), 0x6E: ("ROR", "abs", 3),
    0x6F: ("ADC", "lng", 4),
    0x70: ("BVS", "rel8", 2), 0x71: ("ADC", "dpiy", 2), 0x72: ("ADC", "dpi", 2),
    0x73: ("ADC", "sriy", 2), 0x74: ("STZ", "dpx", 2), 0x75: ("ADC", "dpx", 2),
    0x76: ("ROR", "dpx", 2), 0x77: ("ADC", "dpily", 2), 0x78: ("SEI", "imp", 1),
    0x79: ("ADC", "absy", 3), 0x7A: ("PLY", "imp", 1), 0x7B: ("TDC", "imp", 1),
    0x7C: ("JMP", "absxi", 3), 0x7D: ("ADC", "absx", 3), 0x7E: ("ROR", "absx", 3),
    0x7F: ("ADC", "lngx", 4),
    0x80: ("BRA", "rel8", 2), 0x81: ("STA", "dpxi", 2), 0x82: ("BRL", "rel16", 3),
    0x83: ("STA", "sr", 2), 0x84: ("STY", "dp", 2), 0x85: ("STA", "dp", 2),
    0x86: ("STX", "dp", 2), 0x87: ("STA", "dpil", 2), 0x88: ("DEY", "imp", 1),
    0x89: ("BIT", "immM", 2), 0x8A: ("TXA", "imp", 1), 0x8B: ("PHB", "imp", 1),
    0x8C: ("STY", "abs", 3), 0x8D: ("STA", "abs", 3), 0x8E: ("STX", "abs", 3),
    0x8F: ("STA", "lng", 4),
    0x90: ("BCC", "rel8", 2), 0x91: ("STA", "dpiy", 2), 0x92: ("STA", "dpi", 2),
    0x93: ("STA", "sriy", 2), 0x94: ("STY", "dpx", 2), 0x95: ("STA", "dpx", 2),
    0x96: ("STX", "dpy", 2), 0x97: ("STA", "dpily", 2), 0x98: ("TYA", "imp", 1),
    0x99: ("STA", "absy", 3), 0x9A: ("TXS", "imp", 1), 0x9B: ("TXY", "imp", 1),
    0x9C: ("STZ", "abs", 3), 0x9D: ("STA", "absx", 3), 0x9E: ("STZ", "absx", 3),
    0x9F: ("STA", "lngx", 4),
    0xA0: ("LDY", "immX", 2), 0xA1: ("LDA", "dpxi", 2), 0xA2: ("LDX", "immX", 2),
    0xA3: ("LDA", "sr", 2), 0xA4: ("LDY", "dp", 2), 0xA5: ("LDA", "dp", 2),
    0xA6: ("LDX", "dp", 2), 0xA7: ("LDA", "dpil", 2), 0xA8: ("TAY", "imp", 1),
    0xA9: ("LDA", "immM", 2), 0xAA: ("TAX", "imp", 1), 0xAB: ("PLB", "imp", 1),
    0xAC: ("LDY", "abs", 3), 0xAD: ("LDA", "abs", 3), 0xAE: ("LDX", "abs", 3),
    0xAF: ("LDA", "lng", 4),
    0xB0: ("BCS", "rel8", 2), 0xB1: ("LDA", "dpiy", 2), 0xB2: ("LDA", "dpi", 2),
    0xB3: ("LDA", "sriy", 2), 0xB4: ("LDY", "dpx", 2), 0xB5: ("LDA", "dpx", 2),
    0xB6: ("LDX", "dpy", 2), 0xB7: ("LDA", "dpily", 2), 0xB8: ("CLV", "imp", 1),
    0xB9: ("LDA", "absy", 3), 0xBA: ("TSX", "imp", 1), 0xBB: ("TYX", "imp", 1),
    0xBC: ("LDY", "absx", 3), 0xBD: ("LDA", "absx", 3), 0xBE: ("LDX", "absy", 3),
    0xBF: ("LDA", "lngx", 4),
    0xC0: ("CPY", "immX", 2), 0xC1: ("CMP", "dpxi", 2), 0xC2: ("REP", "imm8", 2),
    0xC3: ("CMP", "sr", 2), 0xC4: ("CPY", "dp", 2), 0xC5: ("CMP", "dp", 2),
    0xC6: ("DEC", "dp", 2), 0xC7: ("CMP", "dpil", 2), 0xC8: ("INY", "imp", 1),
    0xC9: ("CMP", "immM", 2), 0xCA: ("DEX", "imp", 1), 0xCB: ("WAI", "imp", 1),
    0xCC: ("CPY", "abs", 3), 0xCD: ("CMP", "abs", 3), 0xCE: ("DEC", "abs", 3),
    0xCF: ("CMP", "lng", 4),
    0xD0: ("BNE", "rel8", 2), 0xD1: ("CMP", "dpiy", 2), 0xD2: ("CMP", "dpi", 2),
    0xD3: ("CMP", "sriy", 2), 0xD4: ("PEI", "dpi", 2), 0xD5: ("CMP", "dpx", 2),
    0xD6: ("DEC", "dpx", 2), 0xD7: ("CMP", "dpily", 2), 0xD8: ("CLD", "imp", 1),
    0xD9: ("CMP", "absy", 3), 0xDA: ("PHX", "imp", 1), 0xDB: ("STP", "imp", 1),
    0xDC: ("JMP", "absil", 3), 0xDD: ("CMP", "absx", 3), 0xDE: ("DEC", "absx", 3),
    0xDF: ("CMP", "lngx", 4),
    0xE0: ("CPX", "immX", 2), 0xE1: ("SBC", "dpxi", 2), 0xE2: ("SEP", "imm8", 2),
    0xE3: ("SBC", "sr", 2), 0xE4: ("CPX", "dp", 2), 0xE5: ("SBC", "dp", 2),
    0xE6: ("INC", "dp", 2), 0xE7: ("SBC", "dpil", 2), 0xE8: ("INX", "imp", 1),
    0xE9: ("SBC", "immM", 2), 0xEA: ("NOP", "imp", 1), 0xEB: ("XBA", "imp", 1),
    0xEC: ("CPX", "abs", 3), 0xED: ("SBC", "abs", 3), 0xEE: ("INC", "abs", 3),
    0xEF: ("SBC", "lng", 4),
    0xF0: ("BEQ", "rel8", 2), 0xF1: ("SBC", "dpiy", 2), 0xF2: ("SBC", "dpi", 2),
    0xF3: ("SBC", "sriy", 2), 0xF4: ("PEA", "abs", 3), 0xF5: ("SBC", "dpx", 2),
    0xF6: ("INC", "dpx", 2), 0xF7: ("SBC", "dpily", 2), 0xF8: ("SED", "imp", 1),
    0xF9: ("SBC", "absy", 3), 0xFA: ("PLX", "imp", 1), 0xFB: ("XCE", "imp", 1),
    0xFC: ("JSR", "absxi", 3), 0xFD: ("SBC", "absx", 3), 0xFE: ("INC", "absx", 3),
    0xFF: ("SBC", "lngx", 4),
}

def lorom_offset(bank, addr):
    """Convert LoROM bank:addr to ROM file offset."""
    bank &= 0x7F
    if addr >= 0x8000:
        return (bank * 0x8000) + (addr & 0x7FFF)
    # For banks >= 0x40, full 64K mapping
    if bank >= 0x40:
        return ((bank - 0x40) * 0x10000) + addr
    return None

def disasm(rom, bank, addr, count, flag_m=True, flag_x=True):
    """Disassemble 'count' instructions starting at bank:addr."""
    lines = []
    for _ in range(count):
        off = lorom_offset(bank, addr)
        if off is None or off >= len(rom):
            lines.append(f"  ${bank:02X}:{addr:04X}  [unmapped]")
            break

        op = rom[off]
        mnem, mode, size = OPCODES.get(op, ("???", "imp", 1))

        # Adjust size for 16-bit immediate modes
        if mode == "immM" and not flag_m:
            size = 3  # 16-bit accumulator
        elif mode == "immX" and not flag_x:
            size = 3  # 16-bit index

        # Track M/X flag changes
        if op == 0xC2:  # REP
            mask = rom[off + 1] if off + 1 < len(rom) else 0
            if mask & 0x20: flag_m = False
            if mask & 0x10: flag_x = False
        elif op == 0xE2:  # SEP
            mask = rom[off + 1] if off + 1 < len(rom) else 0
            if mask & 0x20: flag_m = True
            if mask & 0x10: flag_x = True

        # Read operand bytes
        operand_bytes = rom[off+1:off+size] if size > 1 else b""
        hex_bytes = " ".join(f"{rom[off+i]:02X}" for i in range(size))

        # Format operand
        operand_str = ""
        if mode == "imp" or mode == "acc":
            operand_str = ""
        elif mode == "imm8":
            operand_str = f"#${operand_bytes[0]:02X}"
        elif mode in ("immM", "immX"):
            if size == 2:
                operand_str = f"#${operand_bytes[0]:02X}"
            else:
                val = operand_bytes[0] | (operand_bytes[1] << 8)
                operand_str = f"#${val:04X}"
        elif mode == "dp":
            operand_str = f"${operand_bytes[0]:02X}"
        elif mode == "dpx":
            operand_str = f"${operand_bytes[0]:02X},X"
        elif mode == "dpy":
            operand_str = f"${operand_bytes[0]:02X},Y"
        elif mode == "dpi":
            operand_str = f"(${operand_bytes[0]:02X})"
        elif mode == "dpxi":
            operand_str = f"(${operand_bytes[0]:02X},X)"
        elif mode == "dpiy":
            operand_str = f"(${operand_bytes[0]:02X}),Y"
        elif mode == "dpil":
            operand_str = f"[${operand_bytes[0]:02X}]"
        elif mode == "dpily":
            operand_str = f"[${operand_bytes[0]:02X}],Y"
        elif mode == "sr":
            operand_str = f"${operand_bytes[0]:02X},S"
        elif mode == "sriy":
            operand_str = f"(${operand_bytes[0]:02X},S),Y"
        elif mode == "abs":
            val = operand_bytes[0] | (operand_bytes[1] << 8)
            operand_str = f"${val:04X}"
        elif mode == "absx":
            val = operand_bytes[0] | (operand_bytes[1] << 8)
            operand_str = f"${val:04X},X"
        elif mode == "absy":
            val = operand_bytes[0] | (operand_bytes[1] << 8)
            operand_str = f"${val:04X},Y"
        elif mode == "absi":
            val = operand_bytes[0] | (operand_bytes[1] << 8)
            operand_str = f"(${val:04X})"
        elif mode == "absxi":
            val = operand_bytes[0] | (operand_bytes[1] << 8)
            operand_str = f"(${val:04X},X)"
        elif mode == "absil":
            val = operand_bytes[0] | (operand_bytes[1] << 8)
            operand_str = f"[${val:04X}]"
        elif mode == "lng":
            val = operand_bytes[0] | (operand_bytes[1] << 8) | (operand_bytes[2] << 16)
            operand_str = f"${val:06X}"
        elif mode == "lngx":
            val = operand_bytes[0] | (operand_bytes[1] << 8) | (operand_bytes[2] << 16)
            operand_str = f"${val:06X},X"
        elif mode == "rel8":
            offset_val = operand_bytes[0]
            if offset_val & 0x80:
                offset_val -= 256
            target = (addr + size + offset_val) & 0xFFFF
            operand_str = f"${target:04X}"
        elif mode == "rel16":
            offset_val = operand_bytes[0] | (operand_bytes[1] << 8)
            if offset_val & 0x8000:
                offset_val -= 0x10000
            target = (addr + size + offset_val) & 0xFFFF
            operand_str = f"${target:04X}"
        elif mode == "mv":
            operand_str = f"${operand_bytes[0]:02X},${operand_bytes[1]:02X}"

        line = f"  ${bank:02X}:{addr:04X}  {hex_bytes:<12s}  {mnem} {operand_str}"
        lines.append(line)

        # Stop on terminal instructions
        if op in (0x60, 0x6B, 0x40):  # RTS, RTL, RTI
            lines.append("")
            addr += size
            continue
        if op == 0x4C or op == 0x5C or op == 0x80 or op == 0x82:  # JMP/JML/BRA/BRL
            lines.append("")

        addr += size
        # Handle bank wrap
        if addr > 0xFFFF:
            bank += 1
            addr &= 0xFFFF

    return "\n".join(lines), flag_m, flag_x

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <rom.sfc> <bank:addr> [count] [m=0|1] [x=0|1]")
        sys.exit(1)

    rom_path = sys.argv[1]
    parts = sys.argv[2].split(":")
    bank = int(parts[0], 16)
    addr = int(parts[1], 16)
    count = int(sys.argv[3]) if len(sys.argv) > 3 else 32
    flag_m = True
    flag_x = True
    for arg in sys.argv[4:]:
        if arg.startswith("m="):
            flag_m = arg[2] == "1"
        elif arg.startswith("x="):
            flag_x = arg[2] == "1"

    with open(rom_path, "rb") as f:
        rom = f.read()

    result, _, _ = disasm(rom, bank, addr, count, flag_m, flag_x)
    print(result)

if __name__ == "__main__":
    main()
