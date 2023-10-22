#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gb.h"

#include "platform.c"

const Color PALETTE[] = {0xE0F8D0FF, 0x88C070FF, 0x346856FF, 0x081820FF};
//const Color PALETTE[] = {0xFFFFFFFF, 0xC0C0C0FF, 0x404040FF, 0x000000FF};

static int gb_clock_freq(int clock_select)
{
    int freq[] = {4096, 262144, 65536, 16384};
    return freq[clock_select & 3];
}


#define make_inst1(b0)         make_inst_internal(1, b0, -1, -1)
#define make_inst2(b0, b1)     make_inst_internal(2, b0, b1, -1)
#define make_inst3(b0, b1, b2) make_inst_internal(3, b0, b1, b2)

static Inst make_inst_internal(u8 n, u8 b0, u8 b1, u8 b2)
{
    assert(n >= 1 && n <= 3);
    Inst inst = {0};
    inst.size = n;
    inst.data[0] = b0;
    inst.data[1] = b1;
    inst.data[2] = b2;
    return inst;
}

Inst gb_fetch_internal(const u8 *data, u8 flags, bool exit_illegal_inst)
{
    u8 b = data[0];
    u8 z = (flags >> 7) & 1;
    u8 c = (flags >> 4) & 1;
    u8 cc = (b >> 3) & 3; // 0 -> Z=0 | 1 -> Z=1 | 2 -> C=0 | 3 -> C=1
    bool taken = (cc == 0 && z == 0) || (cc == 1 && z == 1) || (cc == 2 && c == 0) || (cc == 3 && c == 1);

    if (b == 0xd3 || b == 0xdb || b == 0xdd || b == 0xe3 || b == 0xe4 ||
        b == 0xeb || b == 0xec || b == 0xed || b == 0xf4 || b == 0xfc || b == 0xfd)
    {
        //gb_dump(gb);
        if (exit_illegal_inst) {
            fprintf(stderr, "Illegal Instruction 0x%02X\n", b);
            exit(1);
        } else {
            return (Inst){.data = {b}, .size = 1};
        }
    }

    // 1-byte instructions
    if (b == 0x00 || b == 0x76 || b == 0xf3 || b == 0xfb) { // NOP, HALT, DI, EI
        return (Inst){.data = {b}, .size = 1, .cycles = 4};
    } else if (b == 0x02 || b == 0x12 || b == 0x22 || b == 0x32) { // LD (BC),A | LD (DE),A | LD (HL-),A
        return (Inst){.data = {b}, .size = 1, .cycles = 8};
    } else if (b == 0x09 || b == 0x19 || b == 0x29 || b == 0x39) { // ADD HL,n (n = BC,DE,HL,SP)
        return (Inst){.data = {b}, .size = 1, .cycles = 8};
    } else if ((b >> 6) == 0 && (b & 7) == 4) { // INC reg8: 00|xxx|100
        return (Inst){.data = {b}, .size = 1, .cycles = 4};
    } else if ((b >> 6) == 0 && (b & 7) == 5) { // DEC reg8: 00|xxx|101
        return (Inst){.data = {b}, .size = 1, .cycles = 4};
    } else if ((b >> 6) == 0 && (b & 7) == 7) { // RLCA|RRCA|RLA|RRA|DAA|CPL|SCF|CCF: 00|xxx|111
        return (Inst){.data = {b}, .size = 1, .cycles = 4};
    } else if ((b >> 6) == 0 && (b & 7) == 3) { // INC reg16|DEC reg16: 00|xxx|011
        return (Inst){.data = {b}, .size = 1, .cycles = 8};
    } else if ((b >> 6) == 0 && (b & 7) == 2) { // LD (reg16),A|LD A,(reg16): 00|xxx|010
        return (Inst){.data = {b}, .size = 1, .cycles = 8};
    } else if (b >= 0x40 && b <= 0x7f) {
        bool is_ld_r8_hl = (b >> 6) == 1 && (b & 7) == 6;
        bool is_ld_hl_r8 = b >= 0x70 && b <= 0x77;
        bool is_ld_hl = is_ld_r8_hl || is_ld_hl_r8;
        u8 cycles = is_ld_hl ? 8 : 4;
        return (Inst){.data = {b}, .size = 1, .cycles = cycles};
    } else if (b >= 0x80 && b <= 0xbf) {
        bool reads_hl = (b >> 6) == 2 && (b & 7) == 6;
        u8 cycles = reads_hl ? 8 : 4;
        return (Inst){.data = {b}, .size = 1, .cycles = cycles};
    } else if (b == 0xc0 || b == 0xc8 || b == 0xd0 || b == 0xd8) { // RET NZ|RET Z|RET NC|RET C
        return (Inst){.data = {b}, .size = 1, .cycles = taken ? 20 : 8};
    } else if (b == 0xc1 || b == 0xd1 || b == 0xe1 || b == 0xf1) { // POP reg16: 11|xx|0001
        return (Inst){.data = {b}, .size = 1, .cycles = 12};
    } else if (b == 0xc5 || b == 0xd5 || b == 0xe5 || b == 0xf5) { // PUSH reg16: 11|xx|0101
        return (Inst){.data = {b}, .size = 1, .cycles = 16};
    } else if ((b >> 6) == 3 && (b & 7) == 7) { // RST xx: 11|xxx|111
        return (Inst){.data = {b}, .size = 1, .cycles = 16};
    } else if (b == 0xc9 || b == 0xd9) { // RET|RETI
        return (Inst){.data = {b}, .size = 1, .cycles = 16};
    } else if (b == 0xe2 || b == 0xf2) { // LD (C),A|LD A,(C)
        return (Inst){.data = {b}, .size = 1, .cycles = 8};
    } else if (b == 0xe9) { // JP (HL)
        return (Inst){.data = {b}, .size = 1, .cycles = 4};
    } else if (b == 0xf9) { // LD SP,HL
        return (Inst){.data = {b}, .size = 1, .cycles = 8};
    }

    // 2-byte instructions
    else if (b == 0x10) { // STOP
        return (Inst){.data = {b, data[1]}, .size = 2, .cycles = 4};
    } else if ((b >> 6) == 0 && (b & 7) == 6) { // LD reg8,d8|LD (HL),d8
        u8 cycles = b == 0x36 ? 12 : 8;
        return (Inst){.data = {b, data[1]}, .size = 2, .cycles = cycles};
    } else if (b == 0x18) { // JR r8
        return (Inst){.data = {b, data[1]}, .size = 2, .cycles = 12};
    } else if (b == 0x20 || b == 0x28 || b == 0x30 || b == 0x38) { // JR NZ,r8|JP Z,r8|JP NC,r8|JP C,r8
        return (Inst){.data = {b, data[1]}, .size = 2, .cycles = taken ? 12 : 8};
    } else if ((b >> 6) == 3 && (b & 7) == 6) { // ADD|ADC|SUB|SBC|AND|XOR|OR|CP d8: 11|xxx|110
        return (Inst){.data = {b, data[1]}, .size = 2, .cycles = 8};
    } else if (b == 0xE0 || b == 0xF0) { // LDH (a8),A|LDH A,(a8)
        return (Inst){.data = {b, data[1]}, .size = 2, .cycles = 12};
    } else if (b == 0xE8) { // ADD SP,r8
        return (Inst){.data = {b, data[1]}, .size = 2, .cycles = 16};
    } else if (b == 0xF8) { // LD HL,SP+r8
        return (Inst){.data = {b, data[1]}, .size = 2, .cycles = 12};
    }

    // Prefix CB
    else if (b == 0xCB) {
        u8 b2 = data[1];
        //u8 b2 = gb_mem_read(gb, gb->PC+1);
        u8 cycles = (b2 & 7) == 6 ? 16 : 8;
        return (Inst){.data = {b, data[1]}, .size = 2, .cycles = cycles};
    }

    // 3-byte instructions
    else if (b == 0x01 || b == 0x11 || b == 0x21 || b == 0x31) { // LD r16,d16
        return (Inst){.data = {b, data[1], data[2]}, .size = 3, .cycles = 12};
    } else if (b == 0x08) { // LD (a16),SP
        return (Inst){.data = {b, data[1], data[2]}, .size = 3, .cycles = 20};
    } else if (b == 0xc3) { // JP a16
        return (Inst){.data = {b, data[1], data[2]}, .size = 3, .cycles = 16};
    } else if (b == 0xc4 || b == 0xcc || b == 0xd4 || b == 0xdc) { // CALL NZ,a16|CALL Z,a16|CALL NC,a16|CALL C,a16
        return (Inst){.data = {b, data[1], data[2]}, .size = 3, .cycles = taken ? 24 : 12};
    } else if (b == 0xc2 || b == 0xca || b == 0xd2 || b == 0xda) { // JP NZ,a16|JP Z,a16|JP NC,a16|JP C,a16
        return (Inst){.data = {b, data[1], data[2]}, .size = 3, .cycles = taken ? 16 : 12};
    } else if (b == 0xcd) { // CALL a16
        return (Inst){.data = {b, data[1], data[2]}, .size = 3, .cycles = 24};
    } else if (b == 0xea || b == 0xfa) { // LD (a16),A|LD A,(a16)
        return (Inst){.data = {b, data[1], data[2]}, .size = 3, .cycles = 16};
    }

    printf("%02X\n", b);
    assert(0 && "Not implemented");
}

Inst gb_fetch(const GameBoy *gb)
{
    return gb_fetch_internal(gb->memory + gb->PC, gb->F, false);
}

const char *gb_decode(Inst inst, char *buf, size_t size)
{
    u8 b = inst.data[0];
    if (inst.size == 1) {
        if (b == 0x00) {
            snprintf(buf, size, "NOP");
        } else if (b == 0x09 || b == 0x19 || b == 0x29 || b == 0x39) {
            Reg16 r16 = (b >> 4) & 0x3;
            snprintf(buf, size, "ADD HL,%s", gb_reg16_to_str(r16));
        } else if (
            b == 0x04 || b == 0x14 || b == 0x24 || b == 0x34 ||
            b == 0x0C || b == 0x1C || b == 0x2C || b == 0x3C
        ) {
            Reg8 r8 = (b >> 3) & 7;
            snprintf(buf, size, "INC %s", gb_reg8_to_str(r8));
        } else if (
            b == 0x05 || b == 0x15 || b == 0x25 || b == 0x35 ||
            b == 0x0D || b == 0x1D || b == 0x2D || b == 0x3D
        ) {
            Reg8 r8 = (b >> 3) & 7;
            snprintf(buf, size, "DEC %s", gb_reg8_to_str(r8));
        } else if (b == 0x07) {
            snprintf(buf, size, "RLCA");
        } else if (b == 0x0F) {
            snprintf(buf, size, "RRCA");
        } else if (b == 0x17) {
            snprintf(buf, size, "RLA");
        } else if (b == 0x1F) {
            snprintf(buf, size, "RRA");
        } else if (b == 0x27) {
            snprintf(buf, size, "DAA");
        } else if (b == 0x2F) {
            snprintf(buf, size, "CPL");
        } else if (b == 0x37) {
            snprintf(buf, size, "SCF");
        } else if (b == 0x3F) {
            snprintf(buf, size, "CCF");
        } else if (b == 0x0A || b == 0x1A) {
            Reg16 r16 = (b >> 4) & 0x3;
            snprintf(buf, size, "LD A,(%s)", gb_reg16_to_str(r16));
        } else if (b == 0x02 || b == 0x12) {
            Reg16 r16 = (b >> 4) & 0x3;
            snprintf(buf, size, "LD (%s),A", gb_reg16_to_str(r16));
        } else if (b == 0x22 || b == 0x32) {
            snprintf(buf, size, "LD (HL%c),A", b == 0x22 ? '+' : '-');
        } else if (b == 0x0A || b == 0x1A) {
            Reg16 r16 = (b >> 4) & 0x3;
            snprintf(buf, size, "LD A,(%s)", gb_reg16_to_str(r16));
        } else if (b == 0x2A || b == 0x3A) {
            snprintf(buf, size, "LD A,(HL%c)", b == 0x2A ? '+' : '-');
        } else if (b == 0x03 || b == 0x13 || b == 0x23 || b == 0x33) { // INC reg16
            Reg16 r16 = (b >> 4) & 0x3;
            snprintf(buf, size, "INC %s", gb_reg16_to_str(r16));
        } else if (b == 0x0B || b == 0x1B || b == 0x2B || b == 0x3B) {
            Reg16 r16 = (b >> 4) & 0x3;
            snprintf(buf, size, "DEC %s", gb_reg16_to_str(r16));
        } else if (b >= 0x40 && b <= 0x7F) {
            if (b == 0x76) {
                snprintf(buf, size, "HALT");
            }
            Reg8 src = b & 7;
            Reg8 dst = (b >> 3) & 0x7;
            snprintf(buf, size, "LD %s,%s", gb_reg8_to_str(dst), gb_reg8_to_str(src));
        } else if (b >= 0x80 && b <= 0x87) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "ADD A,%s", gb_reg8_to_str(r8));
        } else if (b >= 0x88 && b <= 0x8F) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "ADC A,%s", gb_reg8_to_str(r8));
        } else if (b >= 0x90 && b <= 0x97) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "SUB A,%s", gb_reg8_to_str(r8));
        } else if (b >= 0x98 && b <= 0x9F) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "SBC A,%s", gb_reg8_to_str(r8));
        } else if (b >= 0xB0 && b <= 0xB7) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "OR %s", gb_reg8_to_str(r8));
        } else if (b >= 0xB8 && b <= 0xBF) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "CP %s", gb_reg8_to_str(r8));
        } else if (b >= 0xA0 && b <= 0xA7) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "AND %s", gb_reg8_to_str(r8));
        } else if (b >= 0xA8 && b <= 0xAF) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "XOR %s", gb_reg8_to_str(r8));
        } else if (b == 0xE2) {
            snprintf(buf, size, "LD (C),A");
        } else if (b == 0xF2) {
            snprintf(buf, size, "LD A,(C)");
        } else if (b == 0xF3 || b == 0xFB) {
            snprintf(buf, size, b == 0xF3 ? "DI" : "EI");
        } else if (b == 0xC0 || b == 0xD0 || b == 0xC8 || b == 0xD8) {
            Flag f = (b >> 3) & 0x3;
            snprintf(buf, size, "RET %s", gb_flag_to_str(f));
        } else if (b == 0xC9) {
            snprintf(buf, size, "RET");
        } else if (b == 0xD9) {
            snprintf(buf, size, "RETI");
        } else if (b == 0xC1 || b == 0xD1 || b == 0xE1) {
            Reg16 reg = (b >> 4) & 0x3;
            snprintf(buf, size, "POP %s", gb_reg16_to_str(reg));
        } else if (b == 0xF1) {
            snprintf(buf, size, "POP AF");
        } else if (b == 0xC5 || b == 0xD5 || b == 0xE5) {
            Reg16 reg = (b >> 4) & 0x3;
            snprintf(buf, size, "PUSH %s", gb_reg16_to_str(reg));
        } else if (b == 0xF5) {
            snprintf(buf, size, "PUSH AF");
        } else if (
            b == 0xC7 || b == 0xD7 || b == 0xE7 || b == 0xF7 ||
            b == 0xCF || b == 0xDF || b == 0xEF || b == 0xFF
        ) {
            u8 n = ((b >> 3) & 0x7)*8;
            snprintf(buf, size, "RST %02XH", n);
        } else if (b == 0xE9) {
            snprintf(buf, size, "JP HL");
        } else if (b == 0xF9) {
            snprintf(buf, size, "LD SP,HL");
        } else {
            snprintf(buf, size, "???");
            //assert(0 && "Instruction not implemented");
        }
    }
    // 2-byte instructions
    else if (inst.size == 2 && b != 0xCB) {
        u8 b2 = inst.data[1];
        if (b == 0x10) {
            snprintf(buf, size, "STOP");
        } else if (b == 0x18) {
            int r8 = b2 >= 0x80 ? (int8_t)b2 : b2;
            snprintf(buf, size, "JR %d", r8);
        } else if ( // LD reg,d8
            b == 0x06 || b == 0x16 || b == 0x26 || b == 0x36 ||
            b == 0x0E || b == 0x1E || b == 0x2E || b == 0x3E
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            snprintf(buf, size, "LD %s,0x%02X", gb_reg8_to_str(reg), b2);
        } else if (b == 0x20 || b == 0x30 || b == 0x28 || b == 0x38) {
            Flag f = (b >> 3) & 0x3;
            snprintf(buf, size, "JR %s,0x%02X", gb_flag_to_str(f), b2);
        } else if (b == 0x20) {
            snprintf(buf, size, "JR NZ,0x%02X", b2);
        } else if (b == 0xE0) {
            snprintf(buf, size, "LDH (FF00+%02X),A", b2);
        } else if (b == 0xC6) {
            snprintf(buf, size, "ADD A,0x%02X", b2);
        } else if (b == 0xCE) {
            snprintf(buf, size, "ADC A,0x%02X", b2);
        } else if (b == 0xD6) {
            snprintf(buf, size, "SUB A,0x%02X", b2);
        } else if (b == 0xDE) {
            snprintf(buf, size, "SBC A,0x%02X", b2);
        } else if (b == 0xE6) {
            snprintf(buf, size, "AND A,0x%02X", b2);
        } else if (b == 0xE8) {
            snprintf(buf, size, "ADD SP,0x%02X", b2);
        } else if (b == 0xF0) {
            snprintf(buf, size, "LDH A,(FF00+%02X)", b2);
        } else if (b == 0xF6) {
            snprintf(buf, size, "OR A,0x%02X", b2);
        } else if (b == 0xF8) {
            snprintf(buf, size, "LD HL,SP+%d", (int8_t)b2);
        } else if (b == 0xEE) {
            snprintf(buf, size, "XOR 0x%02X", b2);
        } else if (b == 0xFE) {
            snprintf(buf, size, "CP 0x%02X", b2);
        } else {
            assert(0 && "Instruction not implemented");
        }
    }

    // Prefix CB
    else if (inst.size == 2 && inst.data[0] == 0xCB) {
        u8 b2 = inst.data[1];
        Reg8 reg = b2 & 7;
        u8 bit = (b2 >> 3) & 7;
        if (b2 <= 0x07) {
            snprintf(buf, size, "RLC %s", gb_reg8_to_str(reg));
        } else if (b2 >= 0x08 && b2 <= 0x0F) {
            snprintf(buf, size, "RRC %s", gb_reg8_to_str(reg));
        } else if (b2 >= 0x10 && b2 <= 0x17) {
            snprintf(buf, size, "RL %s", gb_reg8_to_str(reg));
        } else if (b2 >= 0x18 && b2 <= 0x1F) {
            snprintf(buf, size, "RR %s", gb_reg8_to_str(reg));
        } else if (b2 >= 0x20 && b2 <= 0x27) {
            snprintf(buf, size, "SLA %s", gb_reg8_to_str(reg));
        } else if (b2 >= 0x28 && b2 <= 0x2F) {
            snprintf(buf, size, "SRA %s", gb_reg8_to_str(reg));
        } else if (b2 >= 0x30 && b2 <= 0x37) {
            snprintf(buf, size, "SWAP %s", gb_reg8_to_str(reg));
        } else if (b2 >= 0x38 && b2 <= 0x3F) {
            snprintf(buf, size, "SRL %s", gb_reg8_to_str(reg));
        } else if (b2 >= 0x40 && b2 <= 0x7F) {
            snprintf(buf, size, "BIT %d,%s", bit, gb_reg8_to_str(reg));
        } else if (b2 >= 0x80 && b2 <= 0xBF) {
            snprintf(buf, size, "RES %d,%s", bit, gb_reg8_to_str(reg));
        } else if (b2 >= 0xC0) {
            snprintf(buf, size, "SET %d,%s", bit, gb_reg8_to_str(reg));
        } else {
            assert(0 && "Instruction not implemented");
        }
    }

    // 3-byte instructions
    else if (inst.size == 3) {
        u16 n = inst.data[1] | (inst.data[2] << 8);
        Flag f = (b >> 3) & 3;
        if (b == 0xC3) {
            snprintf(buf, size, "JP 0x%04X", n);
        } else if (b == 0x01 || b == 0x11 || b == 0x21 || b == 0x31) {
            Reg16 reg = b >> 4;
            snprintf(buf, size, "LD %s,0x%04X", gb_reg16_to_str(reg), n);
        } else if (b == 0x08) {
            snprintf(buf, size, "LD (0x%04X),SP", n);
        } else if (b == 0xC2 || b == 0xCA || b == 0xD2 || b == 0xDA) {
            snprintf(buf, size, "JP %s,0x%04X", gb_flag_to_str(f), n);
        } else if (b == 0xC4 || b == 0xD4 || b == 0xCC || b == 0xDC) {
            snprintf(buf, size, "CALL %s,0x%04X", gb_flag_to_str(f), n);
        } else if (b == 0xCD) {
            snprintf(buf, size, "CALL 0x%04X", n);
        } else if (b == 0xEA) {
            snprintf(buf, size, "LD (0x%04X),A", n);
        } else if (b == 0xFA) {
            snprintf(buf, size, "LD A,(0x%04X)", n);
        } else {
            assert(0 && "Not implemented");
        }
    }
    return buf;
}

static bool gb_button_down(GameBoy *gb)
{
    return gb->button_a || gb->button_b || gb->button_start || gb->button_select ||
        gb->dpad_up || gb->dpad_down || gb->dpad_left || gb->dpad_right;
}

void gb_timer_write(GameBoy *gb, u16 addr, u8 value);
int gb_exec(GameBoy *gb, Inst inst)
{
    assert(gb->PC <= 0x7FFF || gb->PC >= 0xFF80 || (gb->PC >= 0xA000 && gb->PC <= 0xDFFF));

    if (gb->boot_mode && gb->PC == 0x100) {
        memcpy(gb->memory, gb->rom, 0x100);
        gb->boot_mode = false;
    }

    if (gb->stopped) {
        if (gb_button_down(gb)) {
            gb->stopped = false;
            gb->PC += 2;
            gb_timer_write(gb, rDIV, 0);
            return 0;
        }
    }

    u8 IE = gb->memory[rIE];
    u8 IF = gb->memory[rIF];
    if (gb->halted) {
        if ((IF & IE) == 0) return -1;
        gb->halted = false;
    }

    if (gb->IME) {
        // VBlank Interrupt
        if (((IF & 0x01) == 0x01) && ((IE & 0x01) == 0x01)) {
            gb->SP -= 2;
            gb_mem_write(gb, gb->SP+0, gb->PC & 0xff);
            gb_mem_write(gb, gb->SP+1, gb->PC >> 8);
            gb->PC = 0x0040;

            // Clear IME and corresponding bit of IF
            if ((IF & 0x01) == 0x01) {
                gb->IME = 0;
                gb->memory[rIF] &= ~0x01;
            }
            return 0;
        }

        // STAT Interrupt
        if (((IF & 0x02) == 0x02) && ((IE & 0x02) == 0x02)) {
            gb->SP -= 2;
            gb_mem_write(gb, gb->SP+0, gb->PC & 0xff);
            gb_mem_write(gb, gb->SP+1, gb->PC >> 8);
            gb->PC = 0x0048;

            // Clear IME and corresponding bit of IF
            if ((IF & 0x02) == 0x02) {
                gb->IME = 0;
                gb->memory[rIF] &= ~0x02;
            }
            return 0;
        }

        // Timer Interrupt
        if (((IF & 0x04) == 0x04) && ((IE & 0x04) == 0x04)) {
            if (gb->halted) gb->halted = false;

            gb->SP -= 2;
            gb_mem_write(gb, gb->SP+0, gb->PC & 0xff);
            gb_mem_write(gb, gb->SP+1, gb->PC >> 8);
            gb->PC = 0x0050;

            // Clear IME and corresponding bit of IF
            if ((IF & 0x04) == 0x04) {
                gb->IME = 0;
                gb->memory[rIF] &= ~0x04;
            }
            return 0;
        }

        // Joypad Interrupt
        if (((IF & 0x1F) == 0x1F) && ((IE & 0x1F) == 0x1F)) {
            gb->SP -= 2;
            gb_mem_write(gb, gb->SP+0, gb->PC & 0xff);
            gb_mem_write(gb, gb->SP+1, gb->PC >> 8);
            gb->PC = 0x0060;

            // Clear IME and corresponding bit of IF
            if ((IF & 0x1F) == 0x1F) {
                gb->IME = 0;
                gb->memory[rIF] &= ~0x1F;
            }
            return 0;
        }
    }

    int cycles = inst.cycles;

    u8 b = inst.data[0];
    // 1-byte instructions
    if (inst.size == 1) {
        if (b == 0x00) {
            gb_log_inst("NOP");
            gb->PC += inst.size;
        } else if (b == 0x09 || b == 0x19 || b == 0x29 || b == 0x39) {
            Reg16 src = (b >> 4) & 0x3;
            gb_log_inst("ADD HL,%s", gb_reg16_to_str(src));
            u16 hl_prev = gb_get_reg16(gb, REG_HL);
            u16 reg_prev = gb_get_reg16(gb, src);
            u16 res = hl_prev + reg_prev;
            gb_set_reg16(gb, REG_HL, res);
            int h = ((hl_prev & 0xFFF) + (reg_prev & 0xFFF)) > 0xFFF;
            int c = ((hl_prev & 0xFFFF) + (reg_prev & 0xFFFF)) > 0xFFFF;
            gb_set_flags(gb, UNCHANGED, 0, h, c);
            gb->PC += inst.size;
        } else if ( // INC reg
            b == 0x04 || b == 0x14 || b == 0x24 || b == 0x34 ||
            b == 0x0C || b == 0x1C || b == 0x2C || b == 0x3C
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            gb_log_inst("INC %s", gb_reg8_to_str(reg));
            u8 prev = gb_get_reg8(gb, reg);
            u8 res = prev + 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, (res & 0xF) < (prev & 0xF), UNCHANGED);
            gb->PC += inst.size;
        } else if ( // DEC reg
            b == 0x05 || b == 0x15 || b == 0x25 || b == 0x35 ||
            b == 0x0D || b == 0x1D || b == 0x2D || b == 0x3D
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            gb_log_inst("DEC %s", gb_reg8_to_str(reg));
            u8 prev = gb_get_reg8(gb, reg);
            int res = prev - 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 1, (res & 0xF) > (prev & 0xF), UNCHANGED);
            gb->PC += inst.size;
        } else if (b == 0x07) {
            gb_log_inst("RLCA");
            u8 c = gb->A >> 7;
            gb->A = (gb->A << 1) | c;
            gb_set_flags(gb, 0, 0, 0, c);
            gb->PC += inst.size;
        } else if (b == 0x0F) {
            gb_log_inst("RRCA");
            u8 c = gb->A & 1;
            gb->A = (gb->A >> 1) | (c << 7);
            gb_set_flags(gb, 0, 0, 0, c);
            gb->PC += inst.size;
        } else if (b == 0x17) {
            gb_log_inst("RLA");
            u8 c = gb->A >> 7;
            gb->A = (gb->A << 1) | gb_get_flag(gb, Flag_C);
            gb_set_flags(gb, 0, 0, 0, c);
            gb->PC += inst.size;
        } else if (b == 0x1F) {
            gb_log_inst("RRA");
            u8 c = gb->A & 1;
            gb->A = (gb->A >> 1) | (gb_get_flag(gb, Flag_C) << 7);
            gb_set_flags(gb, 0, 0, 0, c);
            gb->PC += inst.size;
        } else if (b == 0x27) {
            gb_log_inst("DAA");
            u8 n = gb_get_flag(gb, Flag_N);
            u8 prev_h = gb_get_flag(gb, Flag_H);
            u8 prev_c = gb_get_flag(gb, Flag_C);
            u8 res = gb->A;
            u8 c = 0;
            if (n) {
                // Previous operation was a subtraction
                if (prev_c) {
                    res -= 0x60;
                    c = 1;
                }
                if (prev_h) {
                    res -= 0x06;
                }
            } else {
                // Previous operation was an addition
                if (((gb->A & 0xF) > 0x09) || prev_h) {
                    res += 0x06;
                }
                if ((gb->A > 0x99) | prev_c) {
                    res += 0x60;
                    c = 1;
                }
            }
            gb->A = res;
            gb_set_flags(gb, res == 0, UNCHANGED, 0, c);
            gb->PC += inst.size;
        } else if (b == 0x2F) {
            gb_log_inst("CPL");
            gb->A = ~gb->A;
            gb_set_flags(gb, UNCHANGED, 1, 1, UNCHANGED);
            gb->PC += inst.size;
        } else if (b == 0x37) {
            gb_log_inst("SCF");
            gb_set_flags(gb, UNCHANGED, 0, 0, 1);
            gb->PC += inst.size;
        } else if (b == 0x3F) {
            gb_log_inst("CCF");
            gb_set_flags(gb, UNCHANGED, 0, 0, !gb_get_flag(gb, Flag_C));
            gb->PC += inst.size;
        } else if (b == 0x0A || b == 0x1A) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("LD A,(%s)", gb_reg16_to_str(reg));
            u8 value = gb_mem_read(gb, gb_get_reg16(gb, reg));
            gb->A = value;
            gb->PC += inst.size;
        } else if (b == 0x02 || b == 0x12) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("LD (%s),A", gb_reg16_to_str(reg));
            u16 addr = gb_get_reg16(gb, reg);
            gb_mem_write(gb, addr, gb->A);
            gb->PC += inst.size;
        } else if (b == 0x22 || b == 0x32) {
            gb_log_inst("LD (HL%c),A", b == 0x22 ? '+' : '-');
            gb_mem_write(gb, gb->HL, gb->A);
            if (b == 0x22) gb->HL += 1;
            else gb->HL -= 1;
            gb->PC += inst.size;
        } else if (b == 0x0A || b == 0x1A) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("LD A,(%s)", gb_reg16_to_str(reg));
            u16 addr = gb_get_reg16(gb, reg);
            gb->A = gb_mem_read(gb, addr);
            gb->PC += inst.size;
        } else if (b == 0x2A || b == 0x3A) {
            gb_log_inst("LD A,(HL%c)", b == 0x2A ? '+' : '-');
            gb->A = gb_mem_read(gb, gb->HL);
            if (b == 0x2A) gb->HL += 1;
            else gb->HL -= 1;
            gb->PC += inst.size;
        } else if (b == 0x03 || b == 0x13 || b == 0x23 || b == 0x33) { // INC reg16
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("INC %s", gb_reg16_to_str(reg));
            gb_set_reg16(gb, reg, gb_get_reg16(gb, reg) + 1);
            gb->PC += inst.size;
        } else if (b == 0x0B || b == 0x1B || b == 0x2B || b == 0x3B) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("DEC %s", gb_reg16_to_str(reg));
            gb_set_reg16(gb, reg, gb_get_reg16(gb, reg) - 1);
            gb->PC += inst.size;
        } else if (b >= 0x40 && b <= 0x7F) {
            if (b == 0x76) {
                gb_log_inst("HALT");
                gb->halted = true;
                gb->PC += inst.size;
                return 0;
            }
            Reg8 src = b & 0x7;
            Reg8 dst = (b >> 3) & 0x7;
            gb_log_inst("LD %s,%s", gb_reg8_to_str(dst), gb_reg8_to_str(src));
            u8 value = gb_get_reg8(gb, src);
            gb_set_reg(gb, dst, value);
            gb->PC += inst.size;
        } else if (b >= 0x80 && b <= 0x87) {
            Reg8 reg = b & 0x7;
            gb_log_inst("ADD A,%s", gb_reg8_to_str(reg));
            u8 r = gb_get_reg8(gb, reg);
            u8 res = gb->A + r;
            int h = ((gb->A & 0xF) + (r & 0xF)) > 0xF;
            int c = ((gb->A & 0xFF) + (r & 0xFF)) > 0xFF;
            gb->A = res;
            gb_set_flags(gb, res == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b >= 0x88 && b <= 0x8F) {
            Reg8 reg = b & 0x7;
            gb_log_inst("ADC A,%s", gb_reg8_to_str(reg));
            u8 r = gb_get_reg8(gb, reg);
            u8 prev_c = gb_get_flag(gb, Flag_C);
            u8 res = prev_c + gb->A + r;
            int h = ((gb->A & 0xF) + (r & 0xF) + prev_c) > 0xF;
            int c = ((gb->A & 0xFF) + (r & 0xFF) + prev_c) > 0xFF;
            gb->A = res;
            gb_set_flags(gb, res == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b >= 0x90 && b <= 0x97) {
            Reg8 reg = b & 0x7;
            gb_log_inst("SUB A,%s", gb_reg8_to_str(reg));
            int res = gb->A - gb_get_reg8(gb, reg);
            u8 c = gb->A >= gb_get_reg8(gb, reg) ? 0 : 1;
            u8 h = (gb->A & 0xF) < (res & 0xF);
            gb->A = res;
            gb_set_flags(gb, res == 0, 1, h, c);
            gb->PC += inst.size;
        } else if (b >= 0x98 && b <= 0x9F) {
            Reg8 reg = b & 0x7;
            gb_log_inst("SBC A,%s", gb_reg8_to_str(reg));
            u8 r = gb_get_reg8(gb, reg);
            u8 prev_c = gb_get_flag(gb, Flag_C);
            int h = ((gb->A & 0xF) - (r & 0xF) - prev_c) < 0;
            int c = ((gb->A & 0xFF) - (r & 0xFF) - prev_c) < 0;
            gb->A = (u8)(gb->A - prev_c - r);
            gb_set_flags(gb, gb->A == 0, 1, h, c);
            gb->PC += inst.size;
        } else if (b >= 0xB0 && b <= 0xB7) {
            Reg8 reg = b & 0x7;
            gb_log_inst("OR %s", gb_reg8_to_str(reg));
            gb->A = gb->A | gb_get_reg8(gb, reg);
            gb_set_flags(gb, gb->A == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (b >= 0xB8 && b <= 0xBF) {
            Reg8 reg = b & 0x7;
            gb_log_inst("CP %s", gb_reg8_to_str(reg));
            int a = (int)gb_get_reg8(gb, REG_A);
            int n = (int)gb_get_reg8(gb, reg);
            int res = a - n;
            u8 h = (a & 0xF) < (res & 0xF);
            gb_set_flags(gb, res == 0, 1, h, a < n);
            gb->PC += inst.size;
        } else if (b >= 0xA0 && b <= 0xA7) {
            Reg8 reg = b & 0x7;
            gb_log_inst("AND %s", gb_reg8_to_str(reg));
            gb->A = gb->A & gb_get_reg8(gb, reg);
            gb_set_flags(gb, gb->A == 0, 0, 1, 0);
            gb->PC += inst.size;
        } else if (b >= 0xA8 && b <= 0xAF) {
            Reg8 reg = b & 0x7;
            gb_log_inst("XOR %s", gb_reg8_to_str(reg));
            gb->A = gb->A ^ gb_get_reg8(gb, reg);
            gb_set_flags(gb, gb->A == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (b == 0xE2) {
            gb_log_inst("LD(0xFF00+%02X),%02X", gb_get_reg8(gb, REG_C), gb_get_reg8(gb, REG_A));
            u8 c = gb_get_reg8(gb, REG_C);
            gb_mem_write(gb, 0xFF00 + c, gb->A);
            gb->PC += inst.size;
        } else if (b == 0xF2) {
            gb_log_inst("LD A,(C)");
            u8 c = gb_get_reg8(gb, REG_C);
            gb->A = gb_mem_read(gb, 0xFF00 + c);
            gb->PC += inst.size;
        } else if (b == 0xF3 || b == 0xFB) {
            gb_log_inst(b == 0xF3 ? "DI" : "EI");
            gb->IME = b == 0xF3 ? 0 : 1;
            if (gb->IME) gb->ime_cycles = 1;
            gb->PC += inst.size;
        } else if (b == 0xC0 || b == 0xD0 || b == 0xC8 || b == 0xD8) {
            Flag f = (b >> 3) & 0x3;
            u8 low = gb_mem_read(gb, gb->SP + 0);
            u8 high = gb_mem_read(gb, gb->SP + 1);
            u16 addr = (high << 8) | low;
            gb_log_inst("RET %s", gb_flag_to_str(f));
            if (gb_get_flag(gb, f)) {
                gb->SP += 2;
                gb->PC = addr;
            } else {
                gb->PC += inst.size;
            }
        } else if (b == 0xC9) {
            u8 low = gb_mem_read(gb, gb->SP + 0);
            u8 high = gb_mem_read(gb, gb->SP + 1);
            u16 addr = (high << 8) | low;
            gb_log_inst("RET");
            gb->SP += 2;
            gb->PC = addr;
        } else if (b == 0xD9) {
            u8 low = gb_mem_read(gb, gb->SP + 0);
            u8 high = gb_mem_read(gb, gb->SP + 1);
            u16 addr = (high << 8) | low;
            gb_log_inst("RETI");
            gb->SP += 2;
            gb->IME = 1;
            gb->PC = addr;
        } else if (b == 0xC1 || b == 0xD1 || b == 0xE1) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("POP %s", gb_reg16_to_str(reg));
            u8 low = gb_mem_read(gb, gb->SP + 0);
            u8 high = gb_mem_read(gb, gb->SP + 1);
            gb_set_reg16(gb, reg, (high << 8) | low);
            gb->SP += 2;
            gb->PC += inst.size;
        } else if (b == 0xF1) {
            gb_log_inst("POP AF");
            gb->F = gb_mem_read(gb, gb->SP + 0) & 0xF0; // Clear the lower 4-bits
            gb->A = gb_mem_read(gb, gb->SP + 1);
            gb->SP += 2;
            gb->PC += inst.size;
        } else if (b == 0xC5 || b == 0xD5 || b == 0xE5) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("PUSH %s", gb_reg16_to_str(reg));
            u16 value = gb_get_reg16(gb, reg);
            gb->SP -= 2;
            gb_mem_write(gb, gb->SP + 0, value & 0xff);
            gb_mem_write(gb, gb->SP + 1, value >> 8);
            gb->PC += inst.size;
        } else if (b == 0xF5) {
            gb_log_inst("PUSH AF");
            gb->SP -= 2;
            gb_mem_write(gb, gb->SP + 0, gb->F);
            gb_mem_write(gb, gb->SP + 1, gb->A);
            gb->PC += inst.size;
        } else if (
            b == 0xC7 || b == 0xD7 || b == 0xE7 || b == 0xF7 ||
            b == 0xCF || b == 0xDF || b == 0xEF || b == 0xFF
        ) {
            u8 n = ((b >> 3) & 0x7)*8;
            gb_log_inst("RST %02XH", n);
            gb->SP -= 2;
            u16 ret_addr = gb->PC + inst.size;
            gb_mem_write(gb, gb->SP + 0, ret_addr & 0xff);
            gb_mem_write(gb, gb->SP + 1, ret_addr >> 8);
            gb->PC = n;
        } else if (b == 0xE9) {
            gb_log_inst("JP HL");
            u16 pc = gb->HL;
            gb->PC = pc;
        } else if (b == 0xF9) {
            gb_log_inst("LD SP,HL");
            gb->SP = gb->HL;
            gb->PC += inst.size;
        } else {
            gb_log_inst("%02X", inst.data[0]);
            assert(0 && "Instruction not implemented");
        }
    }
    // 2-byte instructions
    else if (inst.size == 2 && b != 0xCB) {
        if (b == 0x10) {
            gb_log_inst("STOP");
            gb->stopped = true;
        } else if (b == 0x18) {
            static bool infinite_loop = false;
            int r8 = inst.data[1] >= 0x80 ? (int8_t)inst.data[1] : inst.data[1];
            if (!infinite_loop) {
                gb_log_inst("JR %d", r8);
            }
            if (r8 == -2 && !infinite_loop) {
                //fprintf(stderr, "Detected infinite loop...\n");
                //infinite_loop = true;
            }
            gb->PC = (gb->PC + inst.size) + r8;
        } else if ( // LD reg,d8
            b == 0x06 || b == 0x16 || b == 0x26 || b == 0x36 ||
            b == 0x0E || b == 0x1E || b == 0x2E || b == 0x3E
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            gb_log_inst("LD %s,0x%02X", gb_reg8_to_str(reg), inst.data[1]);
            gb_set_reg(gb, reg, inst.data[1]);
            gb->PC += inst.size;
        } else if (b == 0x20 || b == 0x30 || b == 0x28 || b == 0x38) {
            Flag f = (b >> 3) & 0x3;
            static bool infinite_loop = false;
            if (!infinite_loop) {
                gb_log_inst("JR %s,0x%02X", gb_flag_to_str(f), inst.data[1]);
            }
            if (gb_get_flag(gb, f)) {
                int offset = inst.data[1] >= 0x80 ? (int8_t)inst.data[1] : inst.data[1];
                if (offset == -2 && !infinite_loop) {
                    //infinite_loop = true;
                    //printf("Detected infinite loop...\n");
                }
                gb->PC = (gb->PC + inst.size) + offset;
            } else {
                gb->PC += inst.size;
            }
        } else if (b == 0xE0) {
            gb_log_inst("LDH (FF00+%02X),A", inst.data[1]);
            gb_mem_write(gb, 0xFF00 + inst.data[1], gb->A);
            gb->PC += inst.size;
        } else if (b == 0xC6) {
            gb_log_inst("ADD A,0x%02X", inst.data[1]);
            u8 res = gb->A + inst.data[1];
            u8 c = res < gb->A ? 1 : 0;
            u8 h = (gb->A & 0xF) > (res & 0xF);
            gb->A = res;
            gb_set_flags(gb, gb->A == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b == 0xCE) {
            gb_log_inst("ADC A,0x%02X", inst.data[1]);
            u8 prev_c = gb_get_flag(gb, Flag_C);
            int h = ((gb->A & 0xF) + (inst.data[1] & 0xF) + prev_c) > 0xF;
            int c = ((gb->A & 0xFF) + (inst.data[1] & 0xFF) + prev_c) > 0xFF;
            gb->A = gb->A + inst.data[1] + prev_c;
            gb_set_flags(gb, gb->A == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b == 0xD6) {
            gb_log_inst("SUB A,0x%02X", inst.data[1]);
            u8 res = gb->A - inst.data[1];
            u8 c = gb->A < inst.data[1] ? 1 : 0;
            u8 h = (gb->A & 0xF) < (res & 0xF);
            gb->A = res;
            gb_set_flags(gb, gb->A == 0, 1, h, c);
            gb->PC += inst.size;
        } else if (b == 0xDE) {
            gb_log_inst("SBC A,0x%02X", inst.data[1]);
            int n = inst.data[1];
            int prev_c = gb_get_flag(gb, Flag_C);
            int h = ((gb->A & 0xF) - (n & 0xF) - prev_c) < 0;
            int c = ((gb->A & 0xFF) - (n & 0xFF) - prev_c) < 0;
            gb->A = (u8)(gb->A - prev_c - n);
            gb_set_flags(gb, gb->A == 0, 1, h, c);
            gb->PC += inst.size;
        } else if (b == 0xE6) {
            gb_log_inst("AND A,0x%02X", inst.data[1]);
            gb->A = gb->A & inst.data[1];
            gb_set_flags(gb, gb->A == 0, 0, 1, 0);
            gb->PC += inst.size;
        } else if (b == 0xE8) {
            gb_log_inst("ADD SP,0x%02X", inst.data[1]);
            int h = ((gb->SP & 0xF) + (inst.data[1] & 0xF)) > 0xF;
            int c = ((gb->SP & 0xFF) + (inst.data[1] & 0xFF)) > 0xFF;
            gb_set_flags(gb, 0, 0, h, c);
            gb->SP = gb->SP + (int8_t)inst.data[1];
            gb->PC += inst.size;
        } else if (b == 0xF0) {
            gb_log_inst("LDH A,(FF00+%02X)", inst.data[1]);
            gb->A = gb_mem_read(gb, 0xFF00 + inst.data[1]);
            gb->PC += inst.size;
        } else if (b == 0xF6) {
            gb_log_inst("OR A,0x%02X", inst.data[1]);
            gb->A = gb->A | inst.data[1];
            gb_set_flags(gb, gb->A == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (b == 0xF8) {
            gb_log_inst("LD HL,SP+%d", (int8_t)inst.data[1]);
            gb->HL = gb->SP + (int8_t)inst.data[1];
            int h = ((gb->SP & 0xF) + (inst.data[1] & 0xF)) > 0xF;
            int c = ((gb->SP & 0xFF) + (inst.data[1] & 0xFF)) > 0xFF;
            gb_set_flags(gb, 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b == 0xEE) {
            gb_log_inst("XOR 0x%02X", inst.data[1]);
            gb->A = gb->A ^ inst.data[1];
            gb_set_flags(gb, gb->A == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (b == 0xFE) {
            gb_log_inst("CP 0x%02X", inst.data[1]);
            int a = (int)gb->A;
            int n = (int)inst.data[1];
            u8 res = (u8)(a - n);
            u8 h = (a & 0xF) < (res & 0xF);
            gb_set_flags(gb, res == 0, 1, h, a < n);
            gb->PC += inst.size;
        } else {
            printf("%02X\n", b);
            assert(0 && "Instruction not implemented");
        }
    }

    // Prefix CB
    else if (inst.size == 2 && inst.data[0] == 0xCB) {
        if (inst.data[1] <= 0x07) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RLC %s", gb_reg8_to_str(reg));
            u8 value = gb_get_reg8(gb, reg);
            u8 res = (value << 1) | (value >> 7);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value >> 7);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x08 && inst.data[1] <= 0x0F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RRC %s", gb_reg8_to_str(reg));
            u8 value = gb_get_reg8(gb, reg);
            u8 res = (value >> 1) | (value << 7);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x10 && inst.data[1] <= 0x17) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RL %s", gb_reg8_to_str(reg));
            u8 value = gb_get_reg8(gb, reg);
            u8 c = gb_get_flag(gb, Flag_C);
            u8 res = (value << 1) | (c & 1);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value >> 7);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x18 && inst.data[1] <= 0x1F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RR %s", gb_reg8_to_str(reg));
            u8 value = gb_get_reg8(gb, reg);
            u8 c = gb_get_flag(gb, Flag_C);
            u8 res = (value >> 1) | (c << 7);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x20 && inst.data[1] <= 0x27) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SLA %s", gb_reg8_to_str(reg));
            u8 value = gb_get_reg8(gb, reg);
            u8 res = value << 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value >> 7);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x28 && inst.data[1] <= 0x2F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SRA %s", gb_reg8_to_str(reg));
            u8 value = gb_get_reg8(gb, reg);
            u8 res = (value & 0x80) | (value >> 1);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x30 && inst.data[1] <= 0x37) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SWAP %s", gb_reg8_to_str(reg));
            u8 value = gb_get_reg8(gb, reg);
            u8 res = ((value & 0xF) << 4) | ((value & 0xF0) >> 4);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x38 && inst.data[1] <= 0x3F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SRL %s", gb_reg8_to_str(reg));
            u8 value = gb_get_reg8(gb, reg);
            u8 res = value >> 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 0x1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x40 && inst.data[1] <= 0x7F) {
            u8 b = (inst.data[1] >> 3) & 0x7;
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("BIT %d,%s", b, gb_reg8_to_str(reg));
            u8 value = (gb_get_reg8(gb, reg) >> b) & 1;
            gb_set_flags(gb, value == 0, 0, 1, UNCHANGED);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x80 && inst.data[1] <= 0xBF) {
            u8 b = (inst.data[1] >> 3) & 0x7;
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RES %d,%s", b, gb_reg8_to_str(reg));
            u8 value = gb_get_reg8(gb, reg) & ~(1 << b);
            gb_set_reg(gb, reg, value);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0xC0) {
            u8 b = (inst.data[1] >> 3) & 0x7;
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SET %d,%s", b, gb_reg8_to_str(reg));
            u8 value = gb_get_reg8(gb, reg) | (1 << b);
            gb_set_reg(gb, reg, value);
            gb->PC += inst.size;
        } else {
            printf("%02X %02X\n", inst.data[0], inst.data[1]);
            assert(0 && "Instruction not implemented");
        }
    }

    // 3-byte instructions
    else if (inst.size == 3) {
        u16 n = inst.data[1] | (inst.data[2] << 8);
        if (b == 0xC3) {
            static bool infinite_loop = false;
            if (!infinite_loop) {
                gb_log_inst("JP 0x%04X", n);
            }
            if (n == gb->PC && !infinite_loop) {
                //printf("Detected infinite loop...\n");
                //infinite_loop = true;
            }
            gb->PC = n;
        } else if (b == 0x01 || b == 0x11 || b == 0x21 || b == 0x31) {
            Reg16 reg = b >> 4;
            gb_log_inst("LD %s,0x%04X", gb_reg16_to_str(reg), n);
            gb_set_reg16(gb, reg, n);
            gb->PC += inst.size;
        } else if (b == 0x08) {
            gb_log_inst("LD (0x%04X),SP", n);
            gb_mem_write(gb, n+0, gb->SP & 0xff);
            gb_mem_write(gb, n+1, gb->SP >> 8);
            gb->PC += inst.size;
        } else if (b == 0xC2 || b == 0xCA || b == 0xD2 || b == 0xDA) {
            Flag f = (b >> 3) & 0x3;
            gb_log_inst("JP %s,0x%04X", gb_flag_to_str(f), n);
            if (gb_get_flag(gb, f)) {
                gb->PC = n;
            } else {
                gb->PC += inst.size;
            }
        } else if (b == 0xC4 || b == 0xD4 || b == 0xCC || b == 0xDC) {
            Flag f = (b >> 3) & 0x3;
            gb_log_inst("CALL %s,0x%04X", gb_flag_to_str(f), n);
            if (gb_get_flag(gb, f)) {
                gb->SP -= 2;
                u16 ret_addr = gb->PC + inst.size;
                gb_mem_write(gb, gb->SP+0, ret_addr & 0xff);
                gb_mem_write(gb, gb->SP+1, ret_addr >> 8);
                gb->PC = n;
            } else {
                gb->PC += inst.size;
            }
        } else if (b == 0xCD) {
            gb_log_inst("CALL 0x%04X", n);
            gb->SP -= 2;
            u16 ret_addr = gb->PC + inst.size;
            gb_mem_write(gb, gb->SP+0, ret_addr & 0xff);
            gb_mem_write(gb, gb->SP+1, ret_addr >> 8);
            gb->PC = n;
        } else if (b == 0xEA) {
            gb_log_inst("LD (0x%04X),A", n);
            gb_mem_write(gb, n, gb->A);
            gb->PC += inst.size;
        } else if (b == 0xFA) {
            gb_log_inst("LD A,(0x%04X)", n);
            gb->A = gb_mem_read(gb, n);
            gb->PC += inst.size;
        } else {
            assert(0 && "Not implemented");
        }
    }

    assert(gb->PC <= 0xFFFF);
    gb->inst_executed += 1;

    return cycles;
}

static size_t gb_tile_coord_to_pixel(int row, int col)
{
    assert(row >= 0 && row < 32);
    assert(col >= 0 && col < 32);
    int row_pixel = row*TILE_PIXELS; // (0, 8, 16, ..., 248)
    int col_pixel = col*TILE_PIXELS; // (0, 8, 16, ..., 248)
    return row_pixel*SCRN_VX + col_pixel;
}

static void fill_solid_tile(GameBoy *gb, int x, int y, Color color)
{
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            gb->display[(row+y)*256 + (col+x)] = color;
        }
    }
}

// Render Heart tile at 0,0
//u8 tile[] = {
//    0x00, 0x00, 0x6C, 0x6C, 0xFE, 0xFA, 0xFE, 0xFE,
//    0xFE, 0xFE, 0x7C, 0x7C, 0x38, 0x38, 0x10, 0x10,
//};
//fill_tile(gb, 0, 0, tile);
// x - pixel coordinate
// y - pixel coordinate
static void fill_tile(GameBoy *gb, int x, int y, u8 *tile, bool transparency, u8 plt)
{
    // tile is 16 bytes
    //u8 bgp = gb->memory[rBGP];
    u8 bgp = plt;
    u8 bgp_tbl[] = {(bgp >> 0) & 3, (bgp >> 2) & 3, (bgp >> 4) & 3, (bgp >> 6) & 3};
    for (int row = 0; row < 8; row++) {
        // 64 pixels, but only 16 bytes (2 bytes per row)
        u8 low_bitplane = tile[row*2+0];
        u8 high_bitplane = tile[row*2+1];
        for (int col = 0; col < 8; col++) {
            u8 bit0 = (low_bitplane & 0x80) >> 7;
            u8 bit1 = (high_bitplane & 0x80) >> 7;
            low_bitplane <<= 1;
            high_bitplane <<= 1;
            u8 color_idx = (bit1 << 1) | bit0; // 0-3
            u8 palette_idx = bgp_tbl[color_idx];
            Color color = PALETTE[palette_idx];
            if (!transparency || color_idx != 0) {
                int r = (row+y) % 256;
                int c = (col+x) % 256;
                gb->display[r*256 + c] = color;
            }
        }
    }
}

static u8 flip_horz(u8 b)
{
    // 01234567 => 7654321
    return 0 |
        (((b >> 0) & 1) << 7) |
        (((b >> 1) & 1) << 6) |
        (((b >> 2) & 1) << 5) |
        (((b >> 3) & 1) << 4) |
        (((b >> 4) & 1) << 3) |
        (((b >> 5) & 1) << 2) |
        (((b >> 6) & 1) << 1) |
        (((b >> 7) & 1) << 0);
}

static void flip_vert(u8 *tile)
{
    for (int i = 0; i < 4; i++) {
        u8 t = tile[2*i+0];
        tile[2*i+0] = tile[14-2*i];
        tile[14-2*i] = t;

        t = tile[2*i+1];
        tile[2*i+1] = tile[14-(2*i)+1];
        tile[14-(2*i)+1] = t;
    }
}

static void dump_tile(u8 *tile)
{
    for (int i = 0; i < 8; i++) {
        printf("%02X %02X\n", tile[i*2+0], tile[i*2+1]);
    }
}

static void gb_render_row(GameBoy *gb, int px_row)
{
    u8 lcdc = gb->memory[rLCDC];
    if (
        (lcdc & LCDCF_ON)   != LCDCF_ON ||
        (lcdc & LCDCF_BGON) != LCDCF_BGON
    ) {
        Color color = 0xFFFFFFFF;
        for (int px_col = 0; px_col < 256; px_col++) {
            gb->display[px_row*256 + px_col] = color;
        }
        return;
    }

    u16 scx = gb->memory[rSCX];
    u16 scy = gb->memory[rSCY];
    (void)scx;
    (void)scy;

    u16 bg_win_td_off = (lcdc & LCDCF_BG8000) == LCDCF_BG8000 ?  _VRAM8000 : _VRAM9000;
    u16 bg_tm_off = (lcdc & LCDCF_BG9C00) == LCDCF_BG9C00 ? _SCRN1 : _SCRN0;

    u8 bgp = gb->memory[rBGP];
    u8 bgp_tbl[] = {(bgp >> 0) & 3, (bgp >> 2) & 3, (bgp >> 4) & 3, (bgp >> 6) & 3};

    int tile_row = px_row / 8; // 0-31
    for (int px_col = 0; px_col < 256; px_col++) {
        //px_col = (px_col+scx) % 256;
        int tile_col = px_col / 8; // 0-31

        int tile_idx = gb->memory[bg_tm_off + tile_row*32 + tile_col]; // 0-383
        if (bg_win_td_off == _VRAM9000) tile_idx = (int8_t)tile_idx;

        u8 *tile = gb->memory + bg_win_td_off + tile_idx*16; // 8x8 pixels (16-bytes)

        u8 char_row = px_row % 8; // 0-7
        u8 low_bitplane  = tile[char_row*2+0];
        u8 high_bitplane = tile[char_row*2+1];

        u8 bit0 = (low_bitplane  >> (7 - (px_col % 8))) & 1;
        u8 bit1 = (high_bitplane >> (7 - (px_col % 8))) & 1;

        u8 color_idx = (bit1 << 1) | bit0; // 0-3 (2bpp)
        u8 palette_idx = bgp_tbl[color_idx];
        Color color = PALETTE[palette_idx];

        //gb->display[px_row*256 + ((px_col + scx) % 256)] = color;
        gb->display[px_row*256 + px_col] = color;
    }
}

static void gb_render_sprites(GameBoy *gb)
{
    u8 lcdc = gb->memory[rLCDC];
    if ((lcdc & LCDCF_OBJON) != LCDCF_OBJON) return;
    u16 scx = gb->memory[rSCX];
    u16 scy = gb->memory[rSCY];
    for (int i = 0; i < OAM_COUNT; i++) {
        u8 y = gb->memory[_OAMRAM + i*4 + 0] - 16;
        u8 x = gb->memory[_OAMRAM + i*4 + 1] - 8;
        u8 tile_idx = gb->memory[_OAMRAM + i*4 + 2];
        u8 attribs  = gb->memory[_OAMRAM + i*4 + 3];

        x = (x+scx) % 256;
        y = (y+scy) % 256;

        u8 bg_win_over = (attribs >> 7) & 1;
        u8 yflip = (attribs >> 6) & 1;
        u8 xflip = (attribs >> 5) & 1;
        u8 plt_idx = (attribs >> 4) & 1;

        // TODO: Support BG/Win over OBJ
        //assert(bg_win_over == 0);
        (void)bg_win_over;

        u8 *tile_start = gb->memory + _VRAM8000 + tile_idx*16;
        u8 tile[16];
        memcpy(tile, tile_start, 16);
        if (xflip) {
            for (int i = 0; i < 16; i++) {
                u8 *b = tile + i;
                *b = flip_horz(*b);
            }
        }
        if (yflip) {
            flip_vert(tile);
        }

        fill_tile(gb, x, yflip ? (y + 8) : y, tile, true, gb->memory[rOBP0+plt_idx]);

        if(lcdc & LCDCF_OBJ16) {
            u8 *tile_start = gb->memory + _VRAM8000 + (tile_idx+1)*16;
            u8 tile[16];
            memcpy(tile, tile_start, 16);

            if (xflip) {
                for (int i = 0; i < 16; i++) {
                    u8 *b = tile + i;
                    *b = flip_horz(*b);
                }
            }
            if (yflip) {
                flip_vert(tile);
            }

            fill_tile(gb, x, yflip ? y : (y + 8), tile, true, gb->memory[rOBP0+plt_idx]);
        }
    }
}

static void gb_render_window(GameBoy *gb)
{
    u8 lcdc = gb->memory[rLCDC];
    if ((lcdc & LCDCF_WINON) != LCDCF_WINON) return;
#if 0
    //u8 wx = gb->memory[rWX] - 7;
    //u8 wy = gb->memory[rWY];
    //printf("WX: %3d, WY: %3d\n", wx, wy);
    //assert(0 && "Window rendering is not implemented");

    u16 bg_tm_off = (lcdc & LCDCF_BG9C00) == LCDCF_BG9C00 ? _SCRN1 : _SCRN0;

    for (int row = wy; row < SCRN_VY_B; row++) {
        for (int col = wx; col < SCRN_VX_B; col++) {
            int tile_idx = gb->memory[bg_tm_off + row*32 + col];
            if (bg_win_td_off == _VRAM9000) tile_idx = (int8_t)tile_idx;
            fill_solid_tile(gb, col*8, row*8, 0xff);
            u8 *tile = gb->memory + bg_win_td_off + tile_idx*16;
            fill_tile(gb, col*8, row*8, tile, false, gb->memory[rBGP]);
        }
    }
#endif
}

void gb_render(GameBoy *gb)
{
    u8 lcdc = gb->memory[rLCDC];
    if ((lcdc & LCDCF_ON) != LCDCF_ON) return;

    // Render the Background
    if ((lcdc & LCDCF_BGON) == LCDCF_BGON) {
        for (int px_row = 0; px_row < 256; px_row++) {
            gb_render_row(gb, px_row);
        }
    }

    // Render the Window
    gb_render_window(gb);

    // Render the Sprites (OBJ)
    gb_render_sprites(gb);
}

void gb_load_boot_rom(GameBoy *gb)
{
    gb->boot_mode = true;
    memcpy(gb->boot_rom, BOOT_ROM, sizeof(BOOT_ROM));
    memcpy(gb->memory, BOOT_ROM, sizeof(BOOT_ROM));
    memcpy(gb->memory+0x104, NINTENDO_LOGO, sizeof(NINTENDO_LOGO));
    //gb->memory[0x134] = 0xe7; // $19 + $e7 = $00 => Don't lock up

    gb->PC = 0;
}

void gb_load_rom(GameBoy *gb, u8 *raw, size_t size)
{
    assert(size > 0x14F);
    ROM_Header *header = (ROM_Header*)(raw + 0x100);
    bool log_rom_info = false;
    if (log_rom_info) {
        printf("  ROM Size:          $%lx (%ld KiB, %ld bytes)\n", size, size / 1024, size);
        printf("  ROM Banks:         #%zu\n", size / 0x4000);
        if (strlen(header->title)) {
            printf("  Title:             %s\n", header->title);
        } else {
            printf("  Title:");
            for (int i = 0; i < 16; i++) printf(" %02X", header->title[i]);
            printf("\n");
        }
        printf("  CGB:               $%02X\n", (u8)header->title[15]);
        printf("  New licensee code: $%02X $%02X\n", header->new_licensee[0], header->new_licensee[1]);
        printf("  SGB:               $%02X\n", header->sgb);
        printf("  Cartridge Type:    $%02X\n", header->cart_type);
        printf("  ROM size:          $%02X %d KiB\n", header->rom_size, 32*(1 << header->rom_size));
        printf("  Destination code:  $%02X\n", header->dest_code);
        printf("  Old licensee code: $%02X\n", header->old_licensee);
        printf("  Mask ROM version number: $%02X\n", header->mask_version);
        printf("  Header checksum:   $%02X\n", header->header_check);
        printf("  Global checksum:   $%02X $%02X\n", header->global_check[0], header->global_check[1]);
        printf("\n");
    }

    if (memcmp(header->logo, NINTENDO_LOGO, sizeof(NINTENDO_LOGO)) != 0) {
        fprintf(stderr, "Nintendo Logo does NOT match\n");
        exit(1);
    }

    u8 checksum = 0;
    for (u16 addr = 0x0134; addr <= 0x014C; addr++) {
        checksum = checksum - raw[addr] - 1;
    }
    if (header->header_check != checksum) {
        fprintf(stderr, "    Checksum does NOT match: %02X vs. %02X\n", header->header_check, checksum);
        exit(1);
    }

    //assert(header->cart_type == 0);

    gb->cart_type = header->cart_type;

    // TODO: Handle MBC1
    gb->rom = malloc(size);
    assert(gb->rom);
    if (gb->cart_type == 0) {
        assert(size == 32*1024);
        memcpy(gb->rom, raw, size);
        gb->rom_bank_count = 2;

        memcpy(gb->memory, raw, size);
    } else if (gb->cart_type == 1 || gb->cart_type == 3 || gb->cart_type == 0x13) {
        memcpy(gb->rom, raw, size);
        gb->rom_bank_count = size / (16*1024);
        if (log_rom_info) {
            printf("Size: %ld, ROM Bank Count: %d\n", size, gb->rom_bank_count);
        }

        memcpy(gb->memory, raw, 32*1024); // Copy only the first 2 banks
    } else {
        assert(0 && "MBC not implemented yet!");
    }

#if 0
    gb_load_boot_rom(gb);
#else
    gb->AF = 0x01B0;
    gb->BC = 0x0013;
    gb->DE = 0x00D8;
    gb->HL = 0x014D;
    gb->SP = 0xFFFE;

    gb->PC = 0x0100;
    gb->SP = 0xFFFE;

    gb->memory[rLY] = 0x90;
    //gb->memory[rP1] = 0x00;
    gb->memory[rLCDC] = 0x91;
    gb->memory[rSTAT] = 0x85;
#endif

    gb->timer_div = (1000.0 / 16384.0);
}

void gb_load_rom_file(GameBoy *gb, const char *path)
{
    //printf("Loading ROM \"%s\"...\n", path);
    size_t size;
    u8 *raw = read_entire_file(path, &size);
    gb_load_rom(gb, raw, size);
    free(raw);
}

void gb_tick_ms(GameBoy *gb, f64 dt_ms)
{
    if (gb->paused) return;

    static f64 dt_cycle = 0.0;
    dt_cycle += dt_ms;
    while (dt_cycle > (1000.0 / CPU_FREQ)) {
        gb->elapsed_cycles += 1;
        dt_cycle -= (1000.0 / CPU_FREQ);
    }

    gb->elapsed_ms += dt_ms;
    gb->timer_div  -= dt_ms;

    while (gb->timer_div <= 0) {
        gb->timer_div += (1000.0 / 16384.0);
        gb->memory[rDIV] += 1;
    }

    if (gb->memory[rTAC] & 0x04) {
        gb->timer_tima -= dt_ms;
        int freq = gb_clock_freq(gb->memory[rTAC]);
        while (gb->timer_tima <= 0) {
            gb->timer_tima += (1000.0 / freq);
            gb->memory[rTIMA] += 1;
            if (gb->memory[rTIMA] == 0) {
                gb->memory[rTIMA] = gb->memory[rTMA];
                gb->memory[rIF] |= 0x04;
            }
        }
    }

    static f64 dt = 0.0;
    dt += dt_ms;

    int n = 0;
    while (dt > 0) {
        Inst inst = gb_fetch(gb);
        if (((1000.0 / CPU_FREQ) * inst.cycles) > dt) {
            break;
        }

        int cycles = gb_exec(gb, inst);
        if (cycles <= 0) {
            break;
        }
        if (n > 1000) {
            break;
        }
        break;

        dt -= ((1000.0 / CPU_FREQ) * cycles);
        n += 1;
    }
}

///////////////////////////////////////////////////////////////////////////////
//                          Timer                                            //
///////////////////////////////////////////////////////////////////////////////
#include <sys/time.h>

static uint64_t get_ticks(void)
{
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec*1000*1000 + t.tv_usec;
}

void timer_init(Timer *timer)
{
    timer->iterations = 0;
    timer->dt_ticks = 0;
    timer->elapsed_ticks = 0;
    timer->prev_ticks = get_ticks();
    timer->curr_ticks = timer->prev_ticks;
}

void timer_update(Timer *timer)
{
    timer->iterations += 1;

    u64 ticks = get_ticks();
    u64 dt_ticks = ticks - timer->prev_ticks;
    timer->dt_ticks = dt_ticks;
    timer->curr_ticks = ticks;
    timer->elapsed_ticks += dt_ticks;
    timer->prev_ticks = timer->curr_ticks;
}

///////////////////////////////////////////////////////////////////////////////
//                          GameBoy                                          //
///////////////////////////////////////////////////////////////////////////////
void gb_init_with_args(GameBoy *gb, int argc, char **argv)
{
    // Command line options:
    // - Start in step-debug mode
    // - Duration to run for
    // - ...
    // - path to ROM (last)
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <ROM path>\n", argv[0]);
        exit(1);
    }

    gb->running = true;
    gb->printf = printf;

    gb_load_rom_file(gb, argv[argc - 1]);
}

void gb_init(GameBoy *gb)
{
    timer_init(&gb->timer);
    ppu_init(&gb->ppu);
}

void gb_clock_step(GameBoy *gb)
{
    gb->clk = (gb->clk + 1) % 4194304;
    gb->phi = gb->clk >> 2;

    if (gb->prev_inst.m == 0) {
        gb->mem_rw = RW_R_OPCODE;
        gb->mem_rw_addr = gb->PC;
        gb->mem_rw_value = gb->memory[gb->PC];
        gb->prev_inst = gb_fetch(gb);
    }

    if ((gb->clk % 4) == 0) {
        // Exec last cycle of prev inst.
        if (gb->prev_inst.size != 0) {
            gb->A += 1;
            gb->PC += gb->prev_inst.size;
        }

        // M1: fetch (opcode)
        if (gb->prev_inst.m == 0) {
            gb->mem_rw = RW_R_OPCODE;
            gb->mem_rw_addr = gb->PC;
            gb->mem_rw_value = gb->memory[gb->PC];
            gb->prev_inst = gb_fetch(gb);
            gb->prev_inst.m = gb->prev_inst.cycles / 4;
        }
    }
}

void gb_update(GameBoy *gb)
{
    static bool initialized = false;
    if (!initialized) {
        gb_init(gb);
        initialized = true;
    }

    timer_update(&gb->timer);
    ppu_update(gb);
    cpu_update(gb);

    gb_render(gb);
#if 0
    gb_render_row(gb, gb->memory[rLY]); // LY 0-153
    if (gb->memory[rLY] == 144) {
        for (int row = 154; row < 256; row++) {
            gb_render_row(gb, row);
        }

        gb_render_sprites(gb);
    } else if ((gb->memory[rLCDC] & LCDCF_ON) == 0) {
        for (int row = 0; row < 256; row++) {
            gb_render_row(gb, row);
        }
    }
#endif
}

///////////////////////////////////////////////////////////////////////////////
//                          CPU                                              //
///////////////////////////////////////////////////////////////////////////////
void cpu_update(GameBoy *gb)
{
    f64 dt_ms = TICKS_TO_MS(gb->timer.dt_ticks);
    gb_tick_ms(gb, dt_ms);
}

u8 gb_get_flag(const GameBoy *gb, Flag flag)
{
    switch (flag) {
        case Flag_Z:  return ((gb->F >> 7) & 1) == 1;
        case Flag_NZ: return ((gb->F >> 7) & 1) == 0;
        case Flag_N:  return ((gb->F >> 6) & 1) == 1;
        case Flag_H:  return ((gb->F >> 5) & 1) == 1;
        case Flag_C:  return ((gb->F >> 4) & 1) == 1;
        case Flag_NC: return ((gb->F >> 4) & 1) == 0;
        default: assert(0 && "Invalid flag");
    }
}

void gb_set_flag(GameBoy *gb, Flag flag, u8 value)
{
    switch (flag) {
        case Flag_Z: BIT_ASSIGN(gb->F, 7, value); break;
        case Flag_N: BIT_ASSIGN(gb->F, 6, value); break;
        case Flag_H: BIT_ASSIGN(gb->F, 5, value); break;
        case Flag_C: BIT_ASSIGN(gb->F, 4, value); break;
        default: assert(0 && "Invalid flag");
    }
}

void gb_set_flags(GameBoy *gb, int z, int n, int h, int c)
{
    if (z >= 0) gb_set_flag(gb, Flag_Z, z);
    if (n >= 0) gb_set_flag(gb, Flag_N, n);
    if (h >= 0) gb_set_flag(gb, Flag_H, h);
    if (c >= 0) gb_set_flag(gb, Flag_C, c);
}

void gb_set_reg(GameBoy *gb, Reg8 r8, u8 value)
{
    switch (r8) {
        case REG_B: gb->B = value; break;
        case REG_C: gb->C = value; break;
        case REG_D: gb->D = value; break;
        case REG_E: gb->E = value; break;
        case REG_H: gb->H = value; break;
        case REG_L: gb->L = value; break;
        case REG_HL_IND: gb_mem_write(gb, gb->HL, value); break;
        case REG_A: gb->A = value; break;
        default: assert(0 && "Invalid register");
    }
}

u8 gb_get_reg8(const GameBoy *gb, Reg8 r8)
{
    switch (r8) {
        case REG_B: return gb->B;
        case REG_C: return gb->C;
        case REG_D: return gb->D;
        case REG_E: return gb->E;
        case REG_H: return gb->H;
        case REG_L: return gb->L;
        case REG_HL_IND: return gb_mem_read(gb, gb->HL);
        case REG_A: return gb->A;
        default: assert(0 && "Invalid register");
    }
}

void gb_set_reg16(GameBoy *gb, Reg16 r16, u16 value)
{
    switch (r16) {
        case REG_BC: gb->BC = value; break;
        case REG_DE: gb->DE = value; break;
        case REG_HL: gb->HL = value; break;
        case REG_SP: gb->SP = value; break;
        default: assert(0 && "Invalid register provided");
    }
}

u16 gb_get_reg16(const GameBoy *gb, Reg16 r16)
{
    switch (r16) {
        case REG_BC: return gb->BC;
        case REG_DE: return gb->DE;
        case REG_HL: return gb->HL;
        case REG_SP: return gb->SP;
        default: assert(0 && "Invalid register provided");
    }
}

///////////////////////////////////////////////////////////////////////////////
//                          Memory Bus                                       //
///////////////////////////////////////////////////////////////////////////////
void gb_joypad_write(GameBoy *gb, u16 addr, u8 value)
{
    assert(addr == rP1);
    gb->memory[rP1] |= 0xC0;

    if (value == P1F_GET_BTN) {
        if (gb->button_a) {
            gb->memory[rP1] &= ~0x01;
        } else {
            gb->memory[rP1] |= 0x01;
        }
        if (gb->button_b) {
            gb->memory[rP1] &= ~0x02;
        } else {
            gb->memory[rP1] |= 0x02;
        }
        if (gb->button_select) {
            gb->memory[rP1] &= ~0x04;
        } else {
            gb->memory[rP1] |= 0x04;
        }
        if (gb->button_start) {
            gb->memory[rP1] &= ~0x08;
        } else {
            gb->memory[rP1] |= 0x08;
        }
    } else if (value == P1F_GET_DPAD) {
        if (gb->dpad_right) {
            gb->memory[rP1] &= ~0x01;
        } else {
            gb->memory[rP1] |= 0x01;
        }
        if (gb->dpad_left) {
            gb->memory[rP1] &= ~0x02;
        } else {
            gb->memory[rP1] |= 0x02;
        }
        if (gb->dpad_up) {
            gb->memory[rP1] &= ~0x04;
        } else {
            gb->memory[rP1] |= 0x04;
        }
        if (gb->dpad_down) {
            gb->memory[rP1] &= ~0x08;
        } else {
            gb->memory[rP1] |= 0x08;
        }
    } else if (value == P1F_GET_NONE) {
        gb->memory[rP1] |= 0x0F;
    }
}

void gb_serial_write(GameBoy *gb, u16 addr, u8 value)
{
    assert(addr == rSB || addr == rSC);
    if (0) {}
    else if (addr == rSB) gb->memory[addr] = value;
    else if (addr == rSC) {
        if (value == 0x81) {
            gb->serial_buffer[gb->serial_idx++] = gb->memory[rSB];
        }
        gb->memory[addr] = 0xff;
    }
}

void gb_timer_write(GameBoy *gb, u16 addr, u8 value)
{
    assert(addr == rDIV || addr == rTIMA || addr == rTMA || addr == rTAC);
    if (0) {}
    else if (addr == rDIV)  gb->memory[addr] = 0;
    else if (addr == rTIMA) gb->memory[addr] = value;
    else if (addr == rTMA)  gb->memory[addr] = value;
    else if (addr == rTAC) {
        if (value & 0x04) {
            int freq = gb_clock_freq(value);
            gb->timer_tima = (1000.0 / freq);
        }
        gb->memory[addr] = value;
    }
}

void gb_apu_write(GameBoy *gb, u16 addr, u8 value)
{
    bool is_ch1 = addr >= rNR10 && addr <= rNR14;
    bool is_ch2 = addr >= rNR21 && addr <= rNR24;
    bool is_ch3 = addr >= rNR30 && addr <= rNR34;
    bool is_ch4 = addr >= rNR41 && addr <= rNR44;
    bool is_wave = addr >= 0xff30 && addr <= 0xff3f;
    assert(is_ch1 || is_ch2 || is_ch3 || is_ch4 || is_wave ||
        addr == rNR50 || addr == rNR51 || addr == rNR52);

    gb->memory[addr] = value;

    if (addr == rNR52 && value == 0 && gb->serial_idx > 0) {
        for (int i = 0; i < gb->serial_idx; i++) {
            char c = gb->serial_buffer[i];
            if (c < ' ' || c > '~') c = '.';
            fprintf(stderr, "%c", c);
        }
        fprintf(stderr, "\n");

        size_t len = strlen("Passed\n");
        const char *passed_part = gb->serial_buffer + gb->serial_idx - len;
        if (gb->serial_idx > len && strncmp(passed_part, "Passed\n", len) == 0) {
            printf("Passed\n");
            exit(0);
        } else {
            printf("Failed at $%04x\n", gb->PC);
            //exit(1);
        }
    }
}

void gb_ppu_write(GameBoy *gb, u16 addr, u8 value)
{
    assert(addr == rLCDC || addr == rSTAT || addr == rSCY || addr == rSCX ||
        addr == rLY || addr == rLYC || addr == rDMA ||
        addr == rBGP || addr == rOBP0 || addr == rOBP1 || addr == rWY || addr == rWX);

    if (addr == rLCDC || addr == rSTAT) {
        gb->memory[addr] = value;
    } else if (addr == rSCY || addr == rSCX) {
        gb->memory[addr] = value;
    } else if (addr == rLY) {
        gb->memory[addr] = value;
    } else if (addr == rDMA) {
        assert(value <= 0xdf);
        u16 src = value << 8;
        memcpy(gb->memory + 0xfe00, gb->memory + src, 0x9f);
        gb->memory[addr] = value;
    } else if (addr == rBGP || addr == rOBP0 || addr == rOBP1) {
        gb->memory[addr] = value;
    } else if (addr == rWY || addr == rWX) {
        gb->memory[addr] = value;
    }
}

void gb_io_write(GameBoy *gb, u16 addr, u8 value)
{
    assert((addr >= 0xff00 && addr <= 0xff7f) || addr == 0xffff);

    switch (addr) {
        // Joypad
        case rP1: gb_joypad_write(gb, addr, value); break;

        // Serial transfer
        case rSB:
        case rSC: gb_serial_write(gb, addr, value); break;

        // Timer
        case rDIV:
        case rTIMA:
        case rTMA:
        case rTAC: gb_timer_write(gb, addr, value); break;

        // Interrupt flag (interrupt request)
        case rIF: gb->memory[addr] = value; break;

        // APU
        case rNR10:
        case rNR11:
        case rNR12:
        case rNR13:
        case rNR14: gb_apu_write(gb, addr, value); break;

        case rNR21:
        case rNR22:
        case rNR23:
        case rNR24: gb_apu_write(gb, addr, value); break;

        case rNR30:
        case rNR31:
        case rNR32:
        case rNR33:
        case rNR34: gb_apu_write(gb, addr, value); break;

        case rNR41:
        case rNR42:
        case rNR43:
        case rNR44: gb_apu_write(gb, addr, value); break;

        case rNR50:
        case rNR51:
        case rNR52: gb_apu_write(gb, addr, value); break;

        case 0xff30:
        case 0xff31:
        case 0xff32:
        case 0xff33:
        case 0xff34:
        case 0xff35:
        case 0xff36:
        case 0xff37:
        case 0xff38:
        case 0xff39:
        case 0xff3a:
        case 0xff3b:
        case 0xff3c:
        case 0xff3d:
        case 0xff3e:
        case 0xff3f: gb_apu_write(gb, addr, value); break;

        // PPU
        case rLCDC:
        case rSTAT:
        case rSCY:
        case rSCX:
        case rLY:
        case rLYC:
        case rDMA:
        case rBGP:
        case rOBP0:
        case rOBP1:
        case rWY:
        case rWX: gb_ppu_write(gb, addr, value); break;

        // CGB specific
        case 0xff4d: break; // KEY1
        case 0xff4f: break; // VBK
        case 0xff51: break; // HDMA1
        case 0xff52: break; // HDMA2
        case 0xff53: break; // HDMA3
        case 0xff54: break; // HDMA4
        case 0xff55: break; // HDMA5
        case 0xff56: break; // RP
        case 0xff68: break; // BCPS/BGPI
        case 0xff69: break; // BCPD/BGPD
        case 0xff6a: break; // OCPS/OBPI
        case 0xff6b: break; // OCPD/OBPD
        case 0xff6c: break; // OBPRI
        case 0xff70: break; // SVBK
        case 0xff76: break; // PCM12
        case 0xff77: break; // PCM34

        // Interrupt enable
        case rIE: gb->memory[addr] = value; break;

        default: break; // assert(0 && "Unreachable");
    }
}

u8 gb_mem_read(const GameBoy *gb, u16 addr)
{
    //if (addr == 0xFF44) return 0x90;
    return gb->memory[addr];
}

void gb_mem_write(GameBoy *gb, u16 addr, u8 value)
{
    // No MBC (32 KiB ROM only)
    if (gb->cart_type == 0) {
        if (addr <= 0x7FFF) return;
        assert(addr >= 0x8000);
    }

    // MBC1
    else if (gb->cart_type == 1 || gb->cart_type == 3 || gb->cart_type == 0x13) {
        if (addr <= 0x1FFF) {
            if ((value & 0xF) == 0xA) {
                gb->ram_enabled = true;
            } else {
                gb->ram_enabled = false;
            }
        } else if (addr <= 0x3FFF) {
            value = value & 0x1F; // Consider only lower 5-bits
            if (value == 0) value = 1;
            gb->rom_bank_num = value % gb->rom_bank_count;
            memcpy(gb->memory+0x4000, gb->rom + gb->rom_bank_num*0x4000, 0x4000);
        } else if (addr <= 0x5FFF) {
            //fprintf(stderr, "RAM Bank Number\n");
        } else if (addr <= 0x7FFF) {
            //fprintf(stderr, "Banking Mode Select\n");
        }

        if (addr <= 0x7FFF) return;
    }

    else {
        assert(0 && "MBC X is not supported yet!");
    }

    if (0) {}
    // 16 KiB ROM bank 00
    else if (addr >= 0x0000 && addr <= 0x3fff) {
        assert(0 && "Unreachable");
    }
    // 16 KiB ROM Bank 01~NN
    else if (addr >= 0x4000 && addr <= 0x7fff) {
        assert(0 && "Unreachable");
    }
    // 8 KiB Video RAM (VRAM)
    else if (addr >= 0x8000 && addr <= 0x9fff) {
        gb->memory[addr] = value;
    }
    // 8 KiB External RAM (Cartridge)
    else if (addr >= 0xa000 && addr <= 0xbfff) {
        gb->memory[addr] = value;
    }
    // 4 KiB Work RAM (WRAM)
    else if (addr >= 0xc000 && addr <= 0xcfff) {
        gb->memory[addr] = value;
    }
    // 4 KiB Work RAM (WRAM)
    else if (addr >= 0xd000 && addr <= 0xdfff) {
        gb->memory[addr] = value;
    }
    // Mirror of $c000~$ddff (ECHO RAM)
    else if (addr >= 0xe000 && addr <= 0xfdff) {
        gb->memory[addr] = value;
    }
    // Object attribute memory (OAM)
    else if (addr >= 0xfe00 && addr <= 0xfe9f) {
        gb->memory[addr] = value;
    }
    // Not Usable
    else if (addr >= 0xfea0 && addr <= 0xfeff) {
        //assert(0 && "Not Usable memory");
    }
    // I/O Registers
    else if (addr >= 0xff00 && addr <= 0xff7f) {
        gb_io_write(gb, addr, value);
    }
    // High RAM (HRAM)
    else if (addr >= 0xff80 && addr <= 0xfffe) {
        gb->memory[addr] = value;
    }
    // Interrupt Enable register (IE)
    else if (addr >= 0xffff && addr <= 0xffff) {
        gb_io_write(gb, addr, value);
    }
}

///////////////////////////////////////////////////////////////////////////////
//                          PPU                                              //
///////////////////////////////////////////////////////////////////////////////
// OAM Search           =>    20 clocks
// Pixel transfer       =>    43 clocks
// H-Blank              =>    51 clocks (114 clocks / line)
// Line                 =>   114 clocks
// V-Blank (10 lines)   =>  1140 clocks
// Frame  (154 lines)   => 17556 clocks
//
// 1048576 / 17556 = 59.7 Hz refresh rate
static const f64 slowdown_factor = 1.0;
static const f64 frame_time = slowdown_factor * (1.0 / VSYNC);
static const f64 scanline_time = frame_time / 154;
static const f64 dot_time = scanline_time / 456;

void ppu_init(PPU *ppu)
{
    ppu->frame_timer = frame_time;
    ppu->scanline_timer = scanline_time;
    ppu->dot_timer = dot_time;
}

void ppu_update(GameBoy *gb)
{
    if ((gb->memory[rLCDC] & LCDCF_ON) == 0) return;

    const Timer *timer = &gb->timer;
    f64 elapsed = timer->elapsed_ticks*0.000001;
    (void)elapsed;
    f64 dt = timer->dt_ticks*0.000001;

    PPU *ppu = &gb->ppu;
    ppu->frame_timer -= dt;
    ppu->scanline_timer -= dt;
    ppu->dot_timer -= dt;

    if (!ppu->frame_started) {
        ppu->frame_started = true;
        //printf("%10lf: Frame %ld begin\n", elapsed, ppu->frame);
    }

    if (!ppu->scanline_started) {
        ppu->scanline_started = true;
        ppu->scanline_mode_count = 0;
        ppu->mode = PM_HBLANK;
        //printf("%10lf:   Scanline %d begin\n", elapsed, ppu->scanline);
    }

    while (ppu->dot_timer <= 0.0) {
        ppu->dot_timer += dot_time;
        ppu->dot = (ppu->dot + 1) % 456;
        PPU_Mode new_mode = ppu->mode;
        if (ppu->scanline >= 144)     new_mode = PM_VBLANK;
        else if (ppu->dot < 80)       new_mode = PM_OAM;
        else if (ppu->dot < (80+172)) new_mode = PM_DRAWING;
        else new_mode = PM_HBLANK;

        if (new_mode != ppu->mode) {
            ppu->mode = new_mode;
            ppu->scanline_mode_count += 1;
            if (ppu->scanline_mode_count <= 3) {
                //static const char *modes[] = {"HBlank", "VBlank", "OAM   ", "Draw  "};
                //printf("%10lf:     Mode %s (%d) begin |dots: %d\n",
                //    elapsed, modes[ppu->mode], ppu->mode, ppu->dot);
            }
        }
    }

    while (ppu->scanline_timer <= 0.0) {
        ppu->scanline_started = false;
        ppu->scanline_timer += scanline_time;
        //printf("%10lf:   Scanline %d end\n\n", elapsed, ppu->scanline);
        ppu->scanline = (ppu->scanline + 1) % 154;
    }

    while (ppu->frame_timer <= 0.0) {
        ppu->frame_started = false;
        ppu->frame_timer += frame_time;
        //printf("%10lf: Frame %ld end\n", elapsed, ppu->frame);
        //printf("=======================================\n");
        ppu->frame += 1;
    }

    // Update I/O registers
    gb->memory[rLY] = ppu->scanline;

    gb->memory[rSTAT] &= ~0x3;
    gb->memory[rSTAT] |= gb->ppu.mode;

    // Update Interrupt Flags register
    if (gb->memory[rLY] == gb->memory[rLYC]) {
        gb->memory[rSTAT] |= 0x04;
    }
}


///////////////////////////////////////////////////////////////////////////////
//                          Utils/Debug                                      //
///////////////////////////////////////////////////////////////////////////////
void gb_dump(const GameBoy *gb)
{
    gb->printf("$PC: $%04X, A: $%02X, F: %c%c%c%c, "
        "BC: $%04X, DE: $%04X, HL: $%04X, SP: $%04X\n",
        gb->PC, gb->A,
        (gb->F & 0x80) ? 'Z' : '-',
        (gb->F & 0x40) ? 'N' : '-',
        (gb->F & 0x20) ? 'H' : '-',
        (gb->F & 0x10) ? 'C' : '-',
        gb->BC, gb->DE, gb->HL, gb->SP);
    gb->printf("LCDC: $%02X, STAT: $%02X, LY: $%02X\n",
        gb->memory[rLCDC],
        gb->memory[rSTAT],
        gb->memory[rLY]);
    gb->printf("SCX: $%02X, SCY: $%02X, WX: $%02X WY: $02X\n",
        gb->memory[rSCX], gb->memory[rSCY],
        gb->memory[rWX], gb->memory[rWY]);
    gb->printf("Cartridge:\n  Type: %d\n  ROM Bank: $%02X\n  ROM Bank Count: $%02X\n",
        gb->cart_type, gb->rom_bank_num, gb->rom_bank_count);
    gb->printf("%02X %02X %02X\n",
        gb->memory[gb->PC+0],
        gb->memory[gb->PC+1],
        gb->memory[gb->PC+2]);
}

const char* gb_reg8_to_str(Reg8 r8)
{
    assert((r8 <= REG_A) && "Invalid Reg8");
    const char *reg8_names[] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
    return reg8_names[r8];
}

const char* gb_reg16_to_str(Reg16 r16)
{
    assert((r16 <= REG_SP) && "Invalid Reg16");
    const char *reg16_names[] = {"BC", "DE", "HL", "SP"};
    return reg16_names[r16];
}

const char* gb_flag_to_str(Flag f)
{
    assert((f <= Flag_H) && "Invalid Flag");
    const char *flag_names[] = {"NZ", "Z", "NC", "C", "N", "H"};
    return flag_names[f];
}

void gb_log_inst_internal(GameBoy *gb, const char *fmt, ...)
{
// A:00 F:11 B:22 C:33 D:44 E:55 H:66 L:77 SP:8888 PC:9999 PCMEM:AA,BB,CC,DD
#if 0
    // gameboy-doctor
    printf("A:%02X F:%02X B:%02X C:%02X D:%02X E:%02X H:%02X L:%02X "
        "SP:%04X PC:%04X PCMEM:%02X,%02X,%02X,%02X\n",
        gb->A, gb->F, gb->B, gb->C, gb->D, gb->E, gb->H, gb->L, gb->SP, gb->PC,
        gb->memory[gb->PC+0], gb->memory[gb->PC+1], gb->memory[gb->PC+2], gb->memory[gb->PC+3]);
#endif

#if 0
    // Gameboy-logs
    printf("A: %02X F: %02X B: %02X C: %02X D: %02X E: %02X H: %02X L: %02X "
        "SP: %04X PC: 00:%04X (%02X %02X %02X %02X)\n",
        gb->A, gb->F, gb->B, gb->C, gb->D, gb->E, gb->H, gb->L, gb->SP, gb->PC,
        gb->memory[gb->PC+0], gb->memory[gb->PC+1], gb->memory[gb->PC+2], gb->memory[gb->PC+3]);
#endif

    return;
    if (gb->printf == NULL) return;

    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Inst inst = gb_fetch(gb);
    gb->printf("%04X:  ", gb->PC);
    gb->printf(" %15s  | ", buf);
    for (size_t i = 0; i < 3; i++) {
        if (i < inst.size) {
            gb->printf("%02X ", inst.data[i]);
        } else {
            gb->printf("   ");
        }
    }
    gb->printf("| ");
    gb_dump(gb);
}

static void prepare_next_token(Token *token, const char *s, size_t i)
{
    char c = s[i];
    if (isalpha(c)) {
        token->type = TT_IDENT;
        token->start = s + i;
        token->len = 1;
    } else if (isdigit(c)) {
        token->type = TT_DEC_LIT;
        token->start = s + i;
        token->len = 1;
    } else if (c == '%') {
        token->type = TT_BIN_LIT;
        token->start = s + i;
        token->len = 1;
    } else if (c == '$') {
        token->type = TT_HEX_LIT;
        token->start = s + i;
        token->len = 1;
    }
}

static u32 bin_lit_value(Token t)
{
    assert(t.type == TT_BIN_LIT);
    assert(t.len >= 2 && t.len <= 17);
    assert(t.start[0] == '%');
    return (u32)strtol(t.start + 1, NULL, 2);
}

static u32 dec_lit_value(Token t)
{
    assert(t.type == TT_DEC_LIT);
    assert(t.len >= 1 && t.len <= 5);
    return (u32)strtol(t.start, NULL, 10);
}

static u32 hex_lit_value(Token t)
{
    assert(t.type == TT_HEX_LIT);
    assert(t.len >= 2 && t.len <= 5);
    assert(t.start[0] == '$');
    return (u32)strtol(t.start + 1, NULL, 16);
}

static u32 lit_value(Token t) {
    switch (t.type) {
        case TT_BIN_LIT: return bin_lit_value(t);
        case TT_DEC_LIT: return dec_lit_value(t);
        case TT_HEX_LIT: return hex_lit_value(t);
        default: assert(0 && "Invalid literal");
    }
}

static bool token_equals(Token t, const char *s)
{
    return t.type == TT_IDENT && t.len == strlen(s) && strncasecmp(t.start, s, t.len) == 0;
}

static bool is_lit_token(Token t)
{
    return t.type == TT_BIN_LIT || t.type == TT_DEC_LIT || t.type == TT_HEX_LIT;
}

static bool is_cond_token(Token t)
{
    return token_equals(t, "Z")  || token_equals(t, "C") ||
           token_equals(t, "NZ") || token_equals(t, "NC");
}

static bool is_reg16_token(Token t)
{
    return token_equals(t, "BC") || token_equals(t, "DE") ||
           token_equals(t, "HL") || token_equals(t, "SP") ||
           token_equals(t, "AF");
}

static bool is_reg16_hl_token(Token t)
{
    return t.type == TT_IDENT && t.len == 2 && strncmp(t.start, "HL", 2) == 0;
}

static Reg16 reg16_value(Token t);
static bool is_reg16(Token t, Reg16 r)
{
    return is_reg16_token(t) && reg16_value(t) == r;
}

static bool is_reg8_token(Token t)
{
    return token_equals(t, "B") || token_equals(t, "C") ||
           token_equals(t, "D") || token_equals(t, "E") ||
           token_equals(t, "H") || token_equals(t, "L") ||
           token_equals(t, "A");
}


static Reg8 reg8_value(Token t);
static bool is_reg8(Token t, Reg8 r)
{
    return is_reg8_token(t) && reg8_value(t) == r;
}

static bool is_reg8_or_mem_hl(Token *ts)
{
    return is_reg8_token(ts[0]) || (ts[0].type == TT_POPEN && ts[2].type == TT_PCLOSE
        && is_reg16_hl_token(ts[1]));
}

static bool is_bc_de_hli_hld(Token *ts)
{
    return is_reg16(ts[0], REG_BC) || is_reg16(ts[0], REG_DE) ||
        (is_reg16(ts[0], REG_HL) && ts[1].type == TT_PLUS) ||
        (is_reg16(ts[0], REG_HL) && ts[1].type == TT_MINUS);
}

static Opcode get_opcode(Token t)
{
    if (token_equals(t, "ADC"))  return OP_ADC;
    if (token_equals(t, "ADD"))  return OP_ADD;
    if (token_equals(t, "AND"))  return OP_AND;
    if (token_equals(t, "BIT"))  return OP_BIT;
    if (token_equals(t, "CALL")) return OP_CALL;
    if (token_equals(t, "CCF"))  return OP_CCF;
    if (token_equals(t, "CP"))   return OP_CP;
    if (token_equals(t, "CPL"))  return OP_CPL;
    if (token_equals(t, "DAA"))  return OP_DAA;
    if (token_equals(t, "DEC"))  return OP_DEC;
    if (token_equals(t, "DI"))   return OP_DI;
    if (token_equals(t, "EI"))   return OP_EI;
    if (token_equals(t, "HALT")) return OP_HALT;
    if (token_equals(t, "INC"))  return OP_INC;
    if (token_equals(t, "JP"))   return OP_JP;
    if (token_equals(t, "JR"))   return OP_JR;
    if (token_equals(t, "LD"))   return OP_LD;
    if (token_equals(t, "LDH"))  return OP_LDH;
    if (token_equals(t, "NOP"))  return OP_NOP;
    if (token_equals(t, "OR"))   return OP_OR;
    if (token_equals(t, "POP"))  return OP_POP;
    if (token_equals(t, "PUSH")) return OP_PUSH;
    if (token_equals(t, "RES"))  return OP_RES;
    if (token_equals(t, "RET"))  return OP_RET;
    if (token_equals(t, "RETI")) return OP_RETI;
    if (token_equals(t, "RL"))   return OP_RL;
    if (token_equals(t, "RLA"))  return OP_RLA;
    if (token_equals(t, "RLC"))  return OP_RLC;
    if (token_equals(t, "RLCA")) return OP_RLCA;
    if (token_equals(t, "RR"))   return OP_RR;
    if (token_equals(t, "RRA"))  return OP_RRA;
    if (token_equals(t, "RRC"))  return OP_RRC;
    if (token_equals(t, "RRCA")) return OP_RRCA;
    if (token_equals(t, "RST"))  return OP_RST;
    if (token_equals(t, "SBC"))  return OP_SBC;
    if (token_equals(t, "SCF"))  return OP_SCF;
    if (token_equals(t, "SET"))  return OP_SET;
    if (token_equals(t, "SLA"))  return OP_SLA;
    if (token_equals(t, "SRA"))  return OP_SRA;
    if (token_equals(t, "SRL"))  return OP_SRL;
    if (token_equals(t, "STOP")) return OP_STOP;
    if (token_equals(t, "SUB"))  return OP_SUB;
    if (token_equals(t, "SWAP")) return OP_SWAP;
    if (token_equals(t, "XOR"))  return OP_XOR;

    return OP_INVALID;
}

static bool is_opcode_token(Token t)
{
    return get_opcode(t) != OP_INVALID;
}

static bool is_flag_token(Token t)
{
    return token_equals(t, "NZ") || token_equals(t, "Z") ||
           token_equals(t, "NC") || token_equals(t, "C");
}

static Flag flag_value(Token t)
{
    if (token_equals(t, "NZ")) return Flag_NZ;
    if (token_equals(t, "Z"))  return Flag_Z;
    if (token_equals(t, "NC")) return Flag_NC;
    if (token_equals(t, "C"))  return Flag_C;
    assert(0 && "Invalid flag");
    return -1;
}

static Reg16 reg16_value(Token t)
{
    assert(is_reg16_token(t));
    if (token_equals(t, "BC")) return REG_BC;
    if (token_equals(t, "DE")) return REG_DE;
    if (token_equals(t, "HL")) return REG_HL;
    if (token_equals(t, "SP")) return REG_SP;
    if (token_equals(t, "AF")) return 3; //REG_AF
    assert(0 && "Unreachable");
}

static Reg8 reg8_value(Token t)
{
    assert(is_reg8_token(t));
    if (token_equals(t, "B")) return REG_B;
    if (token_equals(t, "C")) return REG_C;
    if (token_equals(t, "D")) return REG_D;
    if (token_equals(t, "E")) return REG_E;
    if (token_equals(t, "H")) return REG_H;
    if (token_equals(t, "L")) return REG_L;
    if (token_equals(t, "A")) return REG_A;
    assert(0 && "Unreachable");
}

static Reg8 reg8_or_mem_hl(Token *ts)
{
    assert(is_reg8_or_mem_hl(ts));
    if (is_reg8_token(ts[0])) return reg8_value(ts[0]);
    else return REG_HL_IND;
}

void gb_disassemble(const void *rom, size_t size)
{
    u16 addr = 0x0000;
    const u8 *pc = (const u8*)rom;
    while (size > 0) {
        Inst inst = gb_fetch_internal(pc, 0, false);
        printf("%04x: ", addr);
        if (inst.size == 1) printf("%02x           ", inst.data[0]);
        if (inst.size == 2) printf("%02x %02x        ", inst.data[0], inst.data[1]);
        if (inst.size == 3) printf("%02x %02x %02x     ", inst.data[0], inst.data[1], inst.data[2]);
        char buf[32] = {0};
        printf("%s\n", gb_decode(inst, buf, sizeof(buf)));

        size -= inst.size;
        pc += inst.size;
        addr += inst.size;

        if (addr == 0x0040) printf("_VBLANK_HANDLER:\n");
        if (addr == 0x0048) printf("_STAT___HANDLER:\n");
        if (addr == 0x0050) printf("_TIMER__HANDLER:\n");
        if (addr == 0x0058) printf("_SERIAL_HANDLER:\n");
        if (addr == 0x0060) printf("_JOYPAD_HANDLER:\n");
        if (addr == 0x0104) {
            printf("_CARTRIDGE_HEADER:\n");
            printf("  ; Nintendo Logo\n");
            printf("  ; Title\n");
            printf("  ; Manufacturer Code\n");
            printf("  ; CGB Flag\n");
            printf("  ; New Licensee Code\n");
            printf("  ; SGB Flag\n");
            printf("  ; Cartridge Type\n");
            printf("  ; ROM Size\n");
            printf("  ; RAM Size\n");
            printf("  ; Desination Code\n");
            printf("  ; Old Licensee Code\n");
            printf("  ; Mask ROM Version\n");
            printf("  ; Header Checksum\n");
            printf("  ; Global Checksum\n");
            printf("_LABEL_150:\n");
            size -= 72;
            pc += 72;
            addr += 72;
        }
    }
}

void gb_assemble_prog_to_buf(void *buf, size_t size, const char *program)
{
    (void)buf;
    (void)size;
    (void)program;
    // TODO
}

void* gb_assemble_inst_to_buf(void *buf, size_t *size, const char *src)
{
    assert(buf && size);
    Inst inst = gb_assemble_inst(src);
    assert(inst.size < *size);
    memcpy(buf, inst.data, inst.size);
    *size -= inst.size;
    return (u8*)buf + inst.size;
}

TokenArray gb_tokenize(const char *s)
{
    TokenArray arr = {0};

    size_t len = strlen(s);
    size_t i = 0;

    Token token = (Token){
        .type = TT_INVALID,
        .start = s,
        .len = 0,
    };

    while (i < len) {
        bool is_single_char_token = true;
        Token single_char_token = {0};
        switch (s[i]) {
            case ',': single_char_token = (Token){TT_COMMA,  s + i, 1}; break;
            case ':': single_char_token = (Token){TT_COLON,  s + i, 1}; break;
            case '.': single_char_token = (Token){TT_DOT,    s + i, 1}; break;
            case '-': single_char_token = (Token){TT_MINUS,  s + i, 1}; break;
            case '+': single_char_token = (Token){TT_PLUS,   s + i, 1}; break;
            case '(': single_char_token = (Token){TT_POPEN,  s + i, 1}; break;
            case ')': single_char_token = (Token){TT_PCLOSE, s + i, 1}; break;

            default: is_single_char_token = false; break;
        }

        if (is_single_char_token) {
            if (token.type != TT_INVALID) {
                arr.tokens[arr.count++] = token;
            }
            token = single_char_token;
        } else {
            if (token.type != TT_INVALID && (token.type <= TT_PCLOSE || isspace(s[i]))) {
                arr.tokens[arr.count++] = token;
                token.type = TT_INVALID;
            }

            if (token.type <= TT_PCLOSE) {
                prepare_next_token(&token, s, i);
            } else if (!isspace(s[i])) {
                token.len += 1;
            }
        }

        i += 1;
    }

    if (token.type != TT_INVALID) {
        arr.tokens[arr.count++] = token;
    }

    return arr;
}

Inst gb_assemble_inst(const char *s)
{
    printf("%s\n", s);
    TokenArray arr = gb_tokenize(s);
    Token *tokens = arr.tokens;
    size_t token_count = arr.count;

    Inst inst = {0};

    assert(token_count > 0);
    if (is_opcode_token(tokens[0])) {
        switch (get_opcode(tokens[0])) {
        case OP_ADC: {
            assert(is_reg8(tokens[1], REG_A));
            assert(tokens[2].type == TT_COMMA);
            if (is_lit_token(tokens[3])) return make_inst2(0xce, (u8)lit_value(tokens[3])); // ADC A, $12
            else return make_inst1(0x88 | reg8_or_mem_hl(tokens + 3)); // ADC A, B
        } break;
        case OP_ADD: {
            assert(tokens[2].type == TT_COMMA);
            if (is_reg8(tokens[1], REG_A)) {
                if (is_lit_token(tokens[3])) return make_inst2(0xc6, (u8)lit_value(tokens[3])); // ADD A, $12
                else return make_inst1(0x80 | reg8_or_mem_hl(tokens + 3)); // ADD A, B
            } else if (is_reg16(tokens[1], REG_HL)) {
                return make_inst1(0x09 | (reg16_value(tokens[3]) << 4)); // ADD HL, BC
            } else if (is_reg16(tokens[1], REG_SP)) {
                return make_inst2(0xe8, (u8)lit_value(tokens[3])); // ADD SP, $12
            }
        } break;
        case OP_AND: {
            assert(token_count == 2 || token_count == 4);
            if (is_lit_token(tokens[1])) return make_inst2(0xe6, (u8)lit_value(tokens[1])); // AND $12
            else return make_inst1(0xa0 | reg8_or_mem_hl(tokens + 1)); // AND B
        } break;
        case OP_BIT: {
            assert(token_count == 4 || token_count == 6);
            assert(tokens[2].type == TT_COMMA);
            u32 bit = lit_value(tokens[1]) & 7;
            return make_inst2(0xcb, 0x40 | (bit << 3) | reg8_or_mem_hl(tokens + 3));
        } break;
        case OP_CALL: {
            u32 addr = lit_value(tokens[token_count - 1]);
            u8 addr_lo = addr & 0xff;
            u8 addr_hi = (addr >> 8) & 0xff;
            if (token_count == 2) {
                return make_inst3(0xcd, addr_lo, addr_hi);
            } else if (token_count == 4) {
                return make_inst3(0xc4 | (flag_value(tokens[1])) << 3, addr_lo, addr_hi);
            }
        } break;
        case OP_CCF: return make_inst1(0x3f); break;
        case OP_CPL: return make_inst1(0x2f); break;
        case OP_CP: {
            assert(token_count == 2 || token_count == 4);
            if (is_lit_token(tokens[1])) return make_inst2(0xfe, (u8)lit_value(tokens[1])); // CP $12
            else return make_inst1(0xb8 | reg8_or_mem_hl(tokens + 1)); // CP B
        } break;
        case OP_DAA: return make_inst1(0x27); break;
        case OP_DEC: {
            assert(token_count == 2 || token_count == 4);
            if (is_reg16_token(tokens[1])) return make_inst1(0x0b | (reg16_value(tokens[1]) << 4));
            else return make_inst1(0x05 | (reg8_or_mem_hl(tokens + 1)) << 3);
        } break;
        case OP_DI:   return make_inst1(0xf3); break;
        case OP_EI:   return make_inst1(0xfb); break;
        case OP_HALT: return make_inst1(0x76); break;
        case OP_INC: {
            assert(token_count == 2 || token_count == 4);
            if (is_reg16_token(tokens[1])) return make_inst1(0x03 | reg16_value(tokens[1]) << 4);
            else return make_inst1(0x04 | reg8_or_mem_hl(tokens + 1) << 3);
        } break;
        case OP_JP: {
            if (is_reg16_hl_token(tokens[2])) return make_inst1(0xe9);

            u32 addr = lit_value(tokens[token_count - 1]);
            u8 addr_lo = addr & 0xff;
            u8 addr_hi = (addr >> 8) & 0xff;
            if (token_count == 2) {
                return make_inst3(0xc3, addr_lo, addr_hi);
            } else if (token_count == 4) {
                return make_inst3(0xc2 | (flag_value(tokens[1])) << 3, addr_lo, addr_hi);
            }
        } break;
        case OP_JR: {
            u32 rel = lit_value(tokens[token_count - 1]);
            if (token_count == 2) {
                return make_inst2(0x18, rel);
            } else if (token_count == 4) {
                return make_inst2(0x20 | (flag_value(tokens[1])) << 3, rel);
            }
        } break;
        case OP_LDH: {
            assert(token_count == 6);
            if (is_reg8(tokens[1], REG_A)) return make_inst2(0xf0, lit_value(tokens[4]));
            else return make_inst2(0xe0, lit_value(tokens[2]));
        } break;
        case OP_LD: {
            assert(token_count == 4 || token_count == 6 || token_count == 7);
            if (is_reg8_or_mem_hl(tokens + 1) &&
                (is_reg8_or_mem_hl(tokens + 3) || is_reg8_or_mem_hl(tokens + 5))
            ) {
                // LD B,B ... LD B,(HL) ... LD (HL),B ... LD A,A
                Reg8 dst = reg8_or_mem_hl(tokens + 1);
                Reg8 src = reg8_or_mem_hl(tokens + ((dst != REG_HL_IND) ? 3 : 5));
                return make_inst1(0x40 | (dst << 3) | src);
            } else if (is_reg16_token(tokens[1]) && is_lit_token(tokens[3])) {
                // LD BC, d16 ... LD SP, d16
                u32 value = lit_value(tokens[3]);
                u8 value_lo = value & 0xff;
                u8 value_hi = (value >> 8) & 0xff;
                return make_inst3(0x01 | reg16_value(tokens[1]) << 4, value_lo, value_hi);
            } else if (is_lit_token(tokens[token_count - 1]) && tokens[token_count - 2].type == TT_PLUS) {
                // LD HL,SP+r8
                return make_inst2(0xf8, lit_value(tokens[token_count - 1]));
            } else if (is_lit_token(tokens[token_count - 1]) && tokens[token_count - 2].type != TT_PLUS) {
                // LD B,d8 ... LD A,d8
                u8 value = lit_value(tokens[token_count - 1]);
                return make_inst2(0x06 | (reg8_or_mem_hl(tokens + 1)) << 3, value);
            }
            // LD (BC),A | LD (DE),A | LD (HL+),A | LD (HL-),A
            // LD A,(BC) | LD A,(DE) | LD A,(HL+) | LD A,(HL-)

            if (is_reg16_token(tokens[1])) {
                Reg16 r16 = reg16_value(tokens[1]);
                assert(tokens[2].type == TT_COMMA);
                if (is_lit_token(tokens[3])) {
                    // LD BC, d16
                    u32 value = lit_value(tokens[3]);
                    assert(value <= 0xffff);
                    inst.size = 3;
                    inst.data[0] = (r16 << 4) | 0x01;
                    inst.data[1] = (value >> 0) & 0xff;
                    inst.data[2] = (value >> 8) & 0xff;
                } else if (is_reg16_token(tokens[3]) && reg16_value(tokens[3]) == REG_SP) {
                    // LD HL, SP+r8
                    assert(r16 == REG_HL);
                    assert(token_count == 6);
                    assert(tokens[4].type == TT_PLUS);
                    assert(is_lit_token(tokens[5]));
                    u32 value = lit_value(tokens[5]);
                    assert(value <= 0xff); // TODO: negative
                    inst.size = 2;
                    inst.data[0] = 0xf8;
                    inst.data[1] = (u8)value;
                } else if (is_reg16_token(tokens[3]) && reg16_value(tokens[3]) == REG_HL) {
                    // LD SP, HL
                    assert(r16 == REG_SP);
                    assert(token_count == 4);
                    return make_inst1(0xf9);
                }
            } else if (is_reg8_token(tokens[1])) {
                if (is_reg8_token(tokens[3])) {
                    // LD B, C
                    assert(token_count == 4);
                    Reg8 dst = reg8_value(tokens[1]);
                    Reg8 src = reg8_value(tokens[3]);
                    return make_inst1(0x40 | (dst << 3) | src);
                } else if (token_count == 4 && is_lit_token(tokens[3])) {
                    // LD B, $12
                    u32 value = lit_value(tokens[3]);
                    assert(value <= 0xff);
                    Reg8 dst = reg8_value(tokens[1]);
                    return make_inst2(0x06 | (dst << 3), (u8)value);
                } else if (token_count == 6 && is_reg16_token(tokens[4])) {
                    Reg16 r16 = reg16_value(tokens[4]);
                    Reg8 dst = reg8_value(tokens[1]);
                    if (r16 == REG_HL) {
                        // LD B, (HL)
                        assert(tokens[2].type == TT_COMMA);
                        assert(tokens[3].type == TT_POPEN);
                        assert(tokens[5].type == TT_PCLOSE);
                        Reg8 src = REG_HL_IND;
                        return make_inst1(0x40 | (dst << 3) | src);
                    } else if (r16 == REG_BC && dst == REG_A) { // LD A, (BC)
                        return make_inst1(0x0a);
                    } else if (r16 == REG_DE && dst == REG_A) { // LD A, (DE)
                        return make_inst1(0x1a);
                    }
                } else if (token_count == 6 && is_lit_token(tokens[4])) {
                    // LD A, ($1234)
                    assert(reg8_value(tokens[1]) == REG_A);
                    assert(tokens[3].type == TT_POPEN);
                    assert(tokens[5].type == TT_PCLOSE);
                    u32 value = lit_value(tokens[4]);
                    assert(value <= 0xffff);
                    inst.size = 3;
                    inst.data[0] = 0xfa;
                    inst.data[1] = (value >> 0) & 0xff;
                    inst.data[2] = (value >> 8) & 0xff;
                } else if (token_count == 6 && is_reg8_token(tokens[4])) {
                    assert(tokens[3].type == TT_POPEN);
                    assert(reg8_value(tokens[4]) == REG_C);
                    assert(tokens[5].type == TT_PCLOSE);
                    assert(reg8_value(tokens[1]) == REG_A);
                    return make_inst1(0xf2);
                } else if (token_count == 7 && tokens[5].type == TT_PLUS) { // LD A, (HL+)
                    return make_inst1(0x2a);
                } else if (token_count == 7 && tokens[5].type == TT_MINUS) { // LD A, (HL-)
                    return make_inst1(0x3a);
                }
            } else if (tokens[1].type == TT_POPEN) {
                if (token_count == 6 && is_reg16_token(tokens[2]) && is_reg8_token(tokens[5])) {
                    Reg16 r16 = reg16_value(tokens[2]);
                    Reg8 src = reg8_value(tokens[5]);
                    if (r16 == REG_HL) { // LD (HL), B
                        assert(reg16_value(tokens[2]) == REG_HL);
                        return make_inst1(0x70 | src);
                    } else if (r16 == REG_BC && src == REG_A) { // LD (BC), A
                        return make_inst1(0x02);
                    } else if (r16 == REG_DE && src == REG_A) { // LD (DE), A
                        return make_inst1(0x12);
                    }
                } else if (token_count == 6 && is_lit_token(tokens[5])) {
                    // LD (HL), $12
                    assert(tokens[1].type == TT_POPEN);
                    assert(reg16_value(tokens[2]) == REG_HL);
                    assert(tokens[3].type == TT_PCLOSE);
                    u32 value = lit_value(tokens[5]);
                    assert(value <= 0xff);
                    return make_inst2(0x36, (u8)value);
                } else if (token_count == 6 && is_lit_token(tokens[2])) {
                    u32 value  = lit_value(tokens[2]);
                    assert(value <= 0xffff);
                    assert(tokens[3].type == TT_PCLOSE);
                    if (is_reg16_token(tokens[5])) {
                        // LD ($1234), SP
                        assert(reg16_value(tokens[5]) == REG_SP);
                        inst.data[0] = 0x08;
                    } else if (is_reg8_token(tokens[5])) {
                        // LD ($1234), A
                        assert(reg8_value(tokens[5]) == REG_A);
                        inst.data[0] = 0xea;
                    }
                    inst.size = 3;
                    inst.data[1] = (value >> 0) & 0xff;
                    inst.data[2] = (value >> 8) & 0xff;
                } else if (token_count == 6 && is_reg8_token(tokens[2])) {
                    assert(tokens[1].type == TT_POPEN);
                    assert(reg8_value(tokens[2]) == REG_C);
                    assert(tokens[3].type == TT_PCLOSE);
                    assert(reg8_value(tokens[5]) == REG_A);
                    return make_inst1(0xe2);
                } else if (token_count == 7 && tokens[3].type == TT_PLUS) { // LD (HL+), A
                    return make_inst1(0x22);
                } else if (token_count == 7 && tokens[3].type == TT_MINUS) { // LD (HL-), A
                    return make_inst1(0x32);
                }
            } else if (is_lit_token(tokens[1])) {

            }
        } break;
        case OP_NOP: return make_inst1(0x00); break;
        case OP_OR: {
            assert(token_count == 2 || token_count == 4);
            if (is_lit_token(tokens[1])) {
                return make_inst2(0xf6, (u8)lit_value(tokens[1])); // OR $12
            } else {
                return make_inst1(0xb0 | reg8_or_mem_hl(tokens + 1)); // OR B
            }
        } break;
        case OP_POP: {
            assert(token_count == 2);
            return make_inst1(0xc1 | (reg16_value(tokens[1]) << 4));
        } break;
        case OP_PUSH: {
            assert(token_count == 2);
            return make_inst1(0xc5 | (reg16_value(tokens[1]) << 4));
        } break;
        case OP_RES: {
            assert(token_count == 4 || token_count == 6);
            assert(tokens[2].type == TT_COMMA);
            u32 bit = lit_value(tokens[1]) & 7;
            return make_inst2(0xcb, 0x80 | (bit << 3) | reg8_or_mem_hl(tokens + 3));
        } break;
        case OP_RETI: return make_inst1(0xd9); break;
        case OP_RET: {
            if (token_count == 1) return make_inst1(0xc9);
            else return make_inst1(0xc0 | (flag_value(tokens[1])) << 3);
        } break;
        case OP_RLA:  return make_inst1(0x17); break;
        case OP_RLCA: return make_inst1(0x07); break;
        case OP_RLC: {
            assert(token_count == 2 || token_count == 4);
            return make_inst2(0xcb, 0x00 | reg8_or_mem_hl(tokens + 1));
        } break;
        case OP_RL: {
            assert(token_count == 2 || token_count == 4);
            return make_inst2(0xcb, 0x10 | reg8_or_mem_hl(tokens + 1));
        } break;
        case OP_RRCA: return make_inst1(0x0f); break;
        case OP_RRA:  return make_inst1(0x1f); break;
        case OP_RRC: {
            assert(token_count == 2 || token_count == 4);
            return make_inst2(0xcb, 0x08 | reg8_or_mem_hl(tokens + 1));
        } break;
        case OP_RR: {
            assert(token_count == 2 || token_count == 4);
            return make_inst2(0xcb, 0x18 | reg8_or_mem_hl(tokens + 1));
        } break;
        case OP_RST: {
            assert(token_count == 2);
            u32 rst = lit_value(tokens[1]);
            assert(rst >= 0 && rst <= 0x38 && (rst % 8) == 0);
            return make_inst1(0xc7 | (rst / 8) << 3);
        } break;
        case OP_SBC: {
            assert(token_count == 4 || token_count == 6);
            if (is_lit_token(tokens[3])) {
                return make_inst2(0xde, (u8)lit_value(tokens[3])); // SBC A, $12
            } else {
                return make_inst1(0x98 | reg8_or_mem_hl(tokens + 3)); // SBC A, B
            }
        } break;
        case OP_SCF: return make_inst1(0x37); break;
        case OP_SET: {
            assert(token_count == 4 || token_count == 6);
            assert(tokens[2].type == TT_COMMA);
            u32 bit = lit_value(tokens[1]) & 7;
            return make_inst2(0xcb, 0xc0 | (bit << 3) | reg8_or_mem_hl(tokens + 3));
        } break;
        case OP_SLA: {
            assert(token_count == 2 || token_count == 4);
            return make_inst2(0xcb, 0x20 | reg8_or_mem_hl(tokens + 1));
        } break;
        case OP_SRA: {
            assert(token_count == 2 || token_count == 4);
            return make_inst2(0xcb, 0x28 | reg8_or_mem_hl(tokens + 1));
        } break;
        case OP_SRL: {
            assert(token_count == 2 || token_count == 4);
            return make_inst2(0xcb, 0x38 | reg8_or_mem_hl(tokens + 1));
        } break;
        case OP_STOP: return make_inst2(0x10, 0x00); break;
        case OP_SUB: {
            assert(token_count == 2 || token_count == 4);
            if (is_lit_token(tokens[1])) {
                return make_inst2(0xd6, (u8)lit_value(tokens[1])); // SUB $12
            } else {
                return make_inst1(0x90 | reg8_or_mem_hl(tokens + 1)); // SUB B
            }
        } break;
        case OP_SWAP: {
            assert(token_count == 2 || token_count == 4);
            return make_inst2(0xcb, 0x30 | reg8_or_mem_hl(tokens + 1));
        } break;
        case OP_XOR: {
            assert(token_count == 2 || token_count == 4);
            if (is_lit_token(tokens[1])) return make_inst2(0xee, (u8)lit_value(tokens[1])); // XOR $12
            else return make_inst1(0xa8 | reg8_or_mem_hl(tokens + 1)); // XOR B
        } break;
        default: assert(0 && "Unreachable");
        }
    } else {
        assert(0 && "Invalid Opcode");
    }

    return inst;
}
