#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gb.h"

static const uint8_t NINTENDO_LOGO[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

// Debug Status
static size_t bp_count = 0;
#define MAX_BREAKPOINTS 16
static uint16_t bp[MAX_BREAKPOINTS];

typedef struct RomHeader {
    uint8_t entry[4];       // 0100-0103 (4)
    uint8_t logo[48];       // 0104-0133 (48)
    char title[16];         // 0134-0143 (16)
                            // 013F-0142 (4)    Manufacturer code in new cartridges
                            // 0143      (1)    CGB flag ($80 CGB compat, $C0 CGB only)
    uint8_t new_licensee[2];// 0144-0145 (2)    Nintendo, EA, Bandai, ...
    uint8_t sgb;            // 0146      (1)
    uint8_t cart_type;      // 0147      (1)    ROM Only, MBC1, MBC1+RAM, ...
    uint8_t rom_size;       // 0148      (1)    32KiB, 64KiB, 128KiB, ...
    uint8_t ram_size;       // 0149      (1)    0, 8KiB, 32KiB, ...
    uint8_t dest_code;      // 014A      (1)    Japan / Overseas
    uint8_t old_licensee;   // 014B      (1)
    uint8_t mask_version;   // 014C      (1)    Usually 0
    uint8_t header_check;   // 014D      (1)    Check of bytes 0134-014C (Boot ROM)
    uint8_t global_check[2];// 014E-014F (2)    Not verified
} RomHeader;

uint8_t *read_entire_file(const char *path, size_t *size);
bool get_command(GameBoy *gb);

void gb_dump(GameBoy *gb)
{
    uint8_t flags = gb->AF & 0xff;
    gb->printf("$PC: $%04X, A: $%02X, F: %c%c%c%c, BC: $%04X, DE: $%04X, HL: $%04X, SP: $%04X\n",
        gb->PC,
        gb_get_reg(gb, REG_A),
        (flags & 0x80) ? 'Z' : '-',
        (flags & 0x40) ? 'N' : '-',
        (flags & 0x20) ? 'H' : '-',
        (flags & 0x10) ? 'C' : '-',
        gb->BC, gb->DE, gb->HL, gb->SP);
}

void gb_trigger_interrupt(GameBoy *gb)
{
    gb->memory[rIF/*$FF0F*/] |= (1 << 2);
    // TODO: This is probably not right
    gb->SP -= 2;
    uint16_t ret_addr = gb->PC;
    gb_write_memory(gb, gb->SP+0, ret_addr & 0xff);
    gb_write_memory(gb, gb->SP+1, ret_addr >> 8);
    gb->PC = 0x0050;
}

uint8_t gb_read_memory(GameBoy *gb, uint16_t addr)
{
    return gb->memory[addr];
}

void gb_write_joypad_input(GameBoy *gb, uint8_t value)
{
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
        // TODO
    }
}

void gb_write_timer(GameBoy *gb, uint16_t addr, uint8_t value)
{
    switch (addr) {
        case rDIV:
            printf("write timer: [rDIV %04X] = %02X\n", addr, value);
            gb->memory[rDIV] = 0; // Reset
            break;
        case rTIMA:
            printf("write timer: [rTIMA %04X] = %02X\n", addr, value);
            //assert(0);
            break;
        case rTMA: {
            // 00 -> Interrupt every 256 increments of TAC
            // 01 -> Interrupt every 255 increments of TAC
            // ...
            // BF -> Interrupt every ? increments of TAC
            // ...
            // FE -> Interrupt every 2 increments of TAC
            // FF -> Interrupt every increment of TAC
            int modulo = 0x100 - value;
            printf("write timer: [rTMA %04X] = %02X\n", addr, value);
            printf("Interrupt every %d increments of TAC\n", modulo);
            gb->memory[rTMA] = value;
        } break;
        case rTAC:
            if (value & 0x04) {
                int clock_select;
                switch (value & 0x03) {
                    case 0:
                        clock_select = 1024;
                        break;
                    case 1:
                        clock_select = 16;
                        break;
                    case 2:
                        clock_select = 64;
                        break;
                    case 3:
                        clock_select = 256;
                        break;
                }
                printf("write timer: [rTAC %04X] = %02X\n", addr, value);
                printf("Enabling Timer at CPU Clock / %d = %d Hz\n",
                    clock_select, (int)(CPU_FREQ / clock_select));
                //assert(0);
            }
            gb->memory[rTAC] = value;
            break;
        default: assert(0 && "Unreachable");
    }
}

void gb_write_memory(GameBoy *gb, uint16_t addr, uint8_t value)
{
    if (addr == rLCDC && value != 0) {
        printf("[rLCDC] = %02X\n", value);
        //assert(0);
    }
    gb->memory[addr] = value;
    if (addr == 0xFF00) {
        gb_write_joypad_input(gb, value);
    }
    return;

    if (addr == 0xFF00) {
        gb->memory[addr] = value;
        gb_write_joypad_input(gb, value);
    } else if (addr >= 0xFF04 && addr <= 0xFF07) {
        gb_write_timer(gb, addr, value);
    } else if (addr == 0xFF46) {
        assert(0);
    } else if (addr == rIE/*0xFFFF*/) {
        // Bit 0: VBlank   Interrupt Enable  (INT $40)  (1=Enable)
        // Bit 1: LCD STAT Interrupt Enable  (INT $48)  (1=Enable)
    } else if (addr == rIF/*0xFF0F*/) {
        // Bit 0: VBlank   Interrupt Request (INT $40)  (1=Request)
    } else {
        gb->memory[addr] = value;
    }
}

uint8_t gb_get_flag(GameBoy *gb, Flag flag)
{
    switch (flag) {
        case Flag_Z: return (gb->AF >> 7) & 1;
        case Flag_NZ:return ((gb->AF >> 7) & 1) == 0;
        case Flag_N: return (gb->AF >> 6) & 1;
        case Flag_H: return (gb->AF >> 5) & 1;
        case Flag_C: return (gb->AF >> 4) & 1;
        case Flag_NC:return ((gb->AF >> 4) & 1) == 0;
        default: assert(0 && "Invalid flag");
    }
}

void gb_set_flag(GameBoy *gb, Flag flag, uint8_t value)
{
    switch (flag) {
        case Flag_Z:
            gb->AF &= ~0x0080;
            gb->AF |= (value) ? 0x0080 : 0;
            break;
        case Flag_N:
            gb->AF &= ~0x0040;
            gb->AF |= (value) ? 0x0040 : 0;
            break;
        case Flag_H:
            gb->AF &= ~0x0020;
            gb->AF |= (value) ? 0x0020 : 0;
            break;
        case Flag_C:
            gb->AF &= ~0x0010;
            gb->AF |= (value) ? 0x0010 : 0;
            break;
        default: assert(0 && "Invalid flag");
    }
}

#define UNCHANGED (-1)
void gb_set_flags(GameBoy *gb, int z, int n, int h, int c)
{
    if (z >= 0) gb_set_flag(gb, Flag_Z, z);
    if (n >= 0) gb_set_flag(gb, Flag_N, n);
    if (h >= 0) gb_set_flag(gb, Flag_H, h);
    if (c >= 0) gb_set_flag(gb, Flag_C, c);
}

void gb_set_zero_flag(GameBoy *gb)
{
    gb_set_flag(gb, Flag_Z, 1);
}

uint8_t gb_get_zero_flag(GameBoy *gb)
{
    return (gb->AF & 0x0080) == 0 ? 0 : 1;
}

const char* gb_reg_to_str(Reg8 r)
{
    switch (r) {
        case REG_B: return "B";
        case REG_C: return "C";
        case REG_D: return "D";
        case REG_E: return "E";
        case REG_H: return "H";
        case REG_L: return "L";
        case REG_HL_MEM: return "(HL)";
        case REG_A: return "A";
        default: assert(0 && "Invalid register index");
    }
}

const char* gb_reg16_to_str(Reg16 r)
{
    switch (r) {
        case REG_BC: return "BC";
        case REG_DE: return "DE";
        case REG_HL: return "HL";
        case REG_SP: return "SP";
        default: assert(0 && "Invalid register index");
    }
}

const char* gb_flag_to_str(Flag f)
{
    switch (f) {
        case Flag_NZ: return "NZ";
        case Flag_Z:  return "Z";
        case Flag_NC: return "NC";
        case Flag_C:  return "C";
        case Flag_H:  return "H";
        case Flag_N:  return "N";
        default: assert(0 && "Invalid flag");
    }
}

void gb_set_reg(GameBoy *gb, Reg8 reg, uint8_t value)
{
    // reg (0-7): B, C, D, E, H, L, (HL), A
    switch (reg) {
        case REG_B/*0*/:
            gb->BC &= 0x00ff;
            gb->BC |= (value << 8);
            break;
        case REG_C/*1*/:
            gb->BC &= 0xff00;
            gb->BC |= (value << 0);
            break;
        case REG_D/*2*/:
            gb->DE &= 0x00ff;
            gb->DE |= (value << 8);
            break;
        case REG_E/*3*/:
            gb->DE &= 0xff00;
            gb->DE |= (value << 0);
            break;
        case REG_H/*4*/:
            gb->HL &= 0x00ff;
            gb->HL |= (value << 8);
            break;
        case REG_L/*5*/:
            gb->HL &= 0xff00;
            gb->HL |= (value << 0);
            break;
        case REG_HL_MEM/*6*/:
            gb_write_memory(gb, gb->HL, value);
            break;
        case REG_A/*7*/:
            gb->AF &= 0x00ff;
            gb->AF |= (value << 8);
            break;
        default:
            assert(0 && "Invalid register");
    }
}

uint8_t gb_get_reg(GameBoy *gb, Reg8 reg)
{
    // reg (0-7): B, C, D, E, H, L, (HL), A
    switch (reg) {
        case REG_B/*0*/:
            return (gb->BC >> 8);
        case REG_C/*1*/:
            return (gb->BC & 0xff);
        case REG_D/*2*/:
            return (gb->DE >> 8);
        case REG_E/*3*/:
            return (gb->DE & 0xff);
        case REG_H/*4*/:
            return (gb->HL >> 8);
        case REG_L/*5*/:
            return (gb->HL & 0xff);
        case REG_HL_MEM/*6*/:
            return gb_read_memory(gb, gb->HL);
        case REG_A/*7*/:
            return (gb->AF >> 8);
        default:
            assert(0 && "Invalid register");
    }
}

void gb_set_reg16(GameBoy *gb, Reg16 reg, uint16_t value)
{
    switch (reg) {
        case REG_BC:
            gb->BC = value;
            break;
        case REG_DE:
            gb->DE = value;
            break;
        case REG_HL:
            gb->HL = value;
            break;
        case REG_SP:
            gb->SP = value;
            break;
        default:
            assert(0 && "Invalid register provided");
    }
}

uint16_t gb_get_reg16(GameBoy *gb, Reg16 reg)
{
    switch (reg) {
        case REG_BC: return gb->BC;
        case REG_DE: return gb->DE;
        case REG_HL: return gb->HL;
        case REG_SP: return gb->SP;
        default:
            assert(0 && "Invalid register provided");
    }
}

Inst gb_fetch_inst(GameBoy *gb)
{
    uint8_t b = gb_read_memory(gb, gb->PC);
    uint8_t *data = &gb->memory[gb->PC];

    if (b == 0xD3 || b == 0xDB || b == 0xDD || b == 0xE3 || b == 0xE4 ||
        b == 0xEB || b == 0xEC || b == 0xED || b == 0xF4 || b == 0xFC || b == 0xFD)
    {
        fprintf(stderr, "Illegal Instruction 0x%02X\n", b);
        exit(1);
    }

    // TODO: Use a Table-driven approach to determine instruction size!!!
    // 1-byte instructions
    if (b == 0x00 || b == 0x76 || b == 0xF3 || b == 0xFB) { // NOP, HALT, DI, EI
        return (Inst){.data = data, .size = 1};
    } else if (b == 0x10) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0x02 || b == 0x12 || b == 0x22 || b == 0x32) { // LD (BC),A | LD (DE),A | LD (HL-),A
        return (Inst){.data = data, .size = 1};
    } else if (b == 0x09 || b == 0x19 || b == 0x29 || b == 0x39) { // ADD HL,n (n = BC,DE,HL,SP)
        return (Inst){.data = data, .size = 1};
    } else if ( // INC reg8
        b == 0x04 || b == 0x14 || b == 0x24 || b == 0x34 ||
        b == 0x0C || b == 0x1C || b == 0x2C || b == 0x3C
    ) {
        return (Inst){.data = data, .size = 1};
    } else if ( // DEC reg8
        b == 0x05 || b == 0x15 || b == 0x25 || b == 0x35 ||
        b == 0x0D || b == 0x1D || b == 0x2D || b == 0x3D
    ) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0x07 || b == 0x17) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0x03 || b == 0x13 || b == 0x23 || b == 0x33) { // INC reg16
        return (Inst){.data = data, .size = 1};
    } else if (b == 0x0B || b == 0x1B || b == 0x2B || b == 0x3B) { // DEC reg16
        return (Inst){.data = data, .size = 1};
    } else if (b == 0x0A || b == 0x1A) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0x22 || b == 0x32 || b == 0x2A || b == 0x3A) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0x0F || b == 0x1F) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0x2F) {
        return (Inst){.data = data, .size = 1};
    } else if (b >= 0x40 && b <= 0x7F) {
        return (Inst){.data = data, .size = 1};
    } else if (b >= 0x80 && b <= 0xBF) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0xAF) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0xC0 || b == 0xD0 || b == 0xC8 || b == 0xD8) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0xC1 || b == 0xD1 || b == 0xE1 || b == 0xF1) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0xC5 || b == 0xD5 || b == 0xE5 || b == 0xF5) {
        return (Inst){.data = data, .size = 1};
    } else if (((b & 0xC0) == 0xC0) && ((b & 0x7) == 0x7)) { // RST
        return (Inst){.data = data, .size = 1};
    } else if (b == 0xC9 || b == 0xD9) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0xE2 || b == 0xF2) {
        return (Inst){.data = data, .size = 1};
    } else if (b == 0xE9) {
        return (Inst){.data = data, .size = 1};
    }

    // 2-byte instructions
    else if ( // 8-bit Loads (LD B,n | LD C,n | LD D,n | LD E,n | LD H,n | LH L,n  **LD (HL),n** | **LD A,n**)
        b == 0x06 || b == 0x0E || b == 0x16 || b == 0x1E ||
        b == 0x26 || b == 0x2E || b == 0x36 || b == 0x3E
    ) {
        return (Inst){.data = data, .size = 2};
    } else if (b == 0x18) {
        return (Inst){.data = data, .size = 2};
    } else if (b == 0x20 || b == 0x30 || b == 0x28 || b == 0x38) {
        return (Inst){.data = data, .size = 2};
    } else if (b == 0x2F) {
        return (Inst){.data = data, .size = 2};
    } else if (b == 0xC6 || b == 0xD6 || b == 0xE6 || b == 0xF6) {
        return (Inst){.data = data, .size = 2};
    } else if (b == 0xE0 || b == 0xF0) {
        return (Inst){.data = data, .size = 2};
    } else if (b == 0xE8) {
        return (Inst){.data = data, .size = 2};
    } else if (b == 0xF8) {
        return (Inst){.data = data, .size = 2};
    } else if (b == 0xCE || b == 0xDE || b == 0xEE || b == 0xFE) {
        return (Inst){.data = data, .size = 2};
    }

    // Prefix CB
    else if (b == 0xCB) {
        return (Inst){.data = data, .size = 2};
    }

    // 3-byte instructions
    else if (b == 0x01 || b == 0x11 || b == 0x21 || b == 0x31) {
        return (Inst){.data = data, .size = 3};
    } else if (b == 0x08) {
        return (Inst){.data = data, .size = 3};
    } else if (b == 0xC3) {
        return (Inst){.data = data, .size = 3};
    } else if (b == 0xC4 || b == 0xD4 || b == 0xCC || b == 0xDC) {
        return (Inst){.data = data, .size = 3};
    } else if (b == 0xC2 || b == 0xCA || b == 0xD2 || b == 0xDA) {
        return (Inst){.data = data, .size = 3};
    } else if (b == 0xCD) {
        return (Inst){.data = data, .size = 3};
    } else if (b == 0xEA || b == 0xFA) {
        return (Inst){.data = data, .size = 3};
    }

    printf("%02X\n", b);
    assert(0 && "Not implemented");
}

#define gb_log_inst(...) gb_log_inst_internal(gb, __VA_ARGS__)
void gb_log_inst_internal(GameBoy *gb, const char *fmt, ...)
{
    if (gb->printf == NULL) return;

    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Inst inst = gb_fetch_inst(gb);

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

void gb_exec(GameBoy *gb, Inst inst)
{
    if (gb->IME) {
        if (gb->ime_cycles == 1) {
            gb->ime_cycles -= 1;
        } else if (gb->ime_cycles == 0 && gb->memory[rLY] == 0) {
            // Handle interrupt
            gb->SP -= 2;
            uint16_t ret_addr = gb->PC;
            gb_write_memory(gb, gb->SP+0, ret_addr & 0xff);
            gb_write_memory(gb, gb->SP+1, ret_addr >> 8);
            gb->PC = 0x0040; // VBlank

            gb->IME = 0;
            return;
        }
    }

    uint8_t b = inst.data[0];
    // 1-byte instructions
    if (inst.size == 1) {
        if (b == 0x00) {
            gb_log_inst("NOP");
            gb->PC += inst.size;
        } else if (b == 0x10) {
            gb_log_inst("STOP");
            gb_write_timer(gb, rDIV, 0);
            //assert(0);
        } else if (b == 0x09 || b == 0x19 || b == 0x29 || b == 0x39) {
            Reg16 src = (b >> 4) & 0x3;
            gb_log_inst("ADD HL,%s", gb_reg16_to_str(src));
            uint16_t hl_prev = gb_get_reg16(gb, REG_HL);
            uint16_t res = hl_prev + gb_get_reg16(gb, src);
            gb_set_reg16(gb, REG_HL, res);
            int h = (res & 0xF0) < (hl_prev & 0xF0);
            int c = res < hl_prev;
            gb_set_flags(gb, UNCHANGED, 0, h, c);
            gb->PC += inst.size;
        } else if ( // INC reg
            b == 0x04 || b == 0x14 || b == 0x24 || b == 0x34 ||
            b == 0x0C || b == 0x1C || b == 0x2C || b == 0x3C
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            gb_log_inst("INC %s", gb_reg_to_str(reg));
            uint8_t prev = gb_get_reg(gb, reg);
            uint8_t res = prev + 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, (res & 0xF) < (prev & 0xF), UNCHANGED);
            gb->PC += inst.size;
        } else if ( // DEC reg
            b == 0x05 || b == 0x15 || b == 0x25 || b == 0x35 ||
            b == 0x0D || b == 0x1D || b == 0x2D || b == 0x3D
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            gb_log_inst("DEC %s", gb_reg_to_str(reg));
            uint8_t prev = gb_get_reg(gb, reg);
            int res = prev - 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 1, (res & 0xF) < (prev & 0xF), UNCHANGED);
            gb->PC += inst.size;
        } else if (b == 0x07) {
            gb_log_inst("RLCA");
            uint8_t prev = gb_get_reg(gb, REG_A);
            uint8_t res = (prev << 1) | (prev >> 7);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, 0, 0, 0, prev >> 7);
            gb->PC += inst.size;
        } else if (b == 0x0F) {
            gb_log_inst("RRCA");
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t c = a & 1;
            uint8_t res = (a >> 1) | (c << 7);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, 0, 0, 0, c);
            gb->PC += inst.size;
        } else if (b == 0x17) {
            gb_log_inst("RLA");
            uint8_t prev = gb_get_reg(gb, REG_A);
            uint8_t res = (prev << 1) | gb_get_flag(gb, Flag_C);
            uint8_t c = prev >> 7;
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, 0, 0, 0, c);
            gb->PC += inst.size;
        } else if (b == 0x1F) {
            gb_log_inst("RRA");
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t c = a & 1;
            uint8_t res = (a >> 1) | (gb_get_flag(gb, Flag_C) << 7);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, 0, 0, 0, c);
            gb->PC += inst.size;
        } else if (b == 0x2F) {
            gb_log_inst("CPL");
            uint8_t a = gb_get_reg(gb, REG_A);
            gb_set_reg(gb, REG_A, ~a);
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
            uint8_t value = gb_read_memory(gb, gb_get_reg16(gb, reg));
            gb_set_reg(gb, REG_A, value);
            gb->PC += inst.size;
        } else if (b == 0x02 || b == 0x12) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("LD (%s),A", gb_reg16_to_str(reg));
            uint16_t addr = gb_get_reg16(gb, reg);
            uint8_t value = gb_get_reg(gb, REG_A);
            gb_write_memory(gb, addr, value);
            gb->PC += inst.size;
        } else if (b == 0x22 || b == 0x32) {
            gb_log_inst("LD (HL%c),A", b == 0x22 ? '+' : '-');
            uint8_t a = gb_get_reg(gb, REG_A);
            gb_write_memory(gb, gb->HL, a);
            if (b == 0x22) gb->HL += 1;
            else gb->HL -= 1;
            gb->PC += inst.size;
        } else if (b == 0x0A || b == 0x1A) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("LD A,(%s)", gb_reg16_to_str(reg));
            uint16_t addr = gb_get_reg16(gb, reg);
            uint8_t value = gb_read_memory(gb, addr);
            gb_set_reg(gb, REG_A, value);
            gb->PC += inst.size;
        } else if (b == 0x2A || b == 0x3A) {
            gb_log_inst("LD A,(HL%c)", b == 0x22 ? '+' : '-');
            uint8_t value = gb_read_memory(gb, gb->HL);
            gb_set_reg(gb, REG_A, value);
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
                return;
            }
            Reg8 src = b & 0x7;
            Reg8 dst = (b >> 3) & 0x7;
            gb_log_inst("LD %s,%s", gb_reg_to_str(dst), gb_reg_to_str(src));
            uint8_t value = gb_get_reg(gb, src);
            gb_set_reg(gb, dst, value);
            gb->PC += inst.size;
        } else if (b >= 0x80 && b <= 0x87) {
            Reg8 reg = b & 0x7;
            gb_log_inst("ADD A,%s", gb_reg_to_str(reg));
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t r = gb_get_reg(gb, reg);
            uint8_t res = a + r;
            gb_set_reg(gb, REG_A, res);
            #define BIT3(x) (((x) >> 3) & 1)
            uint8_t h = ((BIT3(a) == 1 || BIT3(r) == 1) && BIT3(res) == 0);
            gb_set_flags(gb, res == 0, 0, h, (res < a));
            gb->PC += inst.size;
        } else if (b >= 0x88 && b <= 0x8F) {
            Reg8 reg = b & 0x7;
            gb_log_inst("ADC A,%s", gb_reg_to_str(reg));
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t r = gb_get_reg(gb, reg);
            uint8_t res = gb_get_flag(gb, Flag_C) + a + r;
            gb_set_reg(gb, REG_A, res);
            #define BIT3(x) (((x) >> 3) & 1)
            uint8_t h = ((BIT3(a) == 1 || BIT3(r) == 1) && BIT3(res) == 0);
            gb_set_flags(gb, res == 0, 0, h, res < a);
            gb->PC += inst.size;
        } else if (b >= 0x90 && b <= 0x97) {
            Reg8 reg = b & 0x7;
            gb_log_inst("SUB A,%s", gb_reg_to_str(reg));
            int res = gb_get_reg(gb, REG_A) - gb_get_reg(gb, reg);
            uint8_t c = gb_get_reg(gb, REG_A) >= gb_get_reg(gb, reg) ? 0 : 1;
            gb_set_reg(gb, REG_A, res);
            uint8_t h = 1; // TODO
            gb_set_flags(gb, res == 0, 1, h, c);
            gb_set_flag(gb, Flag_C, c);
            gb->PC += inst.size;
        } else if (b >= 0x98 && b <= 0x9F) {
            Reg8 reg = b & 0x7;
            gb_log_inst("SBC A,%s", gb_reg_to_str(reg));
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t res = (uint8_t)(a - gb_get_flag(gb, Flag_C) - gb_get_reg(gb, reg));
            gb_set_reg(gb, REG_A, res);
            uint8_t h = 1; // TODO
            uint8_t c = 1; // TODO
            gb_set_flags(gb, res == 0, 1, h, c);
            gb->PC += inst.size;
        } else if (b >= 0xB0 && b <= 0xB7) {
            Reg8 reg = b & 0x7;
            gb_log_inst("OR %s", gb_reg_to_str(reg));
            uint8_t res = gb_get_reg(gb, REG_A) | gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (b >= 0xB8 && b <= 0xBF) {
            Reg8 reg = b & 0x7;
            gb_log_inst("CP %s", gb_reg_to_str(reg));
            int a = (int)gb_get_reg(gb, REG_A);
            int n = (int)gb_get_reg(gb, reg);
            int res = a - n;
            uint8_t h = 0; // TODO
            gb_set_flags(gb, res == 0, 1, h, a < n);
            gb->PC += inst.size;
        } else if (b >= 0xA0 && b <= 0xA7) {
            Reg8 reg = b & 0x7;
            gb_log_inst("AND %s", gb_reg_to_str(reg));
            uint8_t res = gb_get_reg(gb, REG_A) & gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, 1, 0);
            gb->PC += inst.size;
        } else if (b >= 0xA8 && b <= 0xAF) {
            Reg8 reg = b & 0x7;
            gb_log_inst("XOR %s", gb_reg_to_str(reg));
            uint8_t res = gb_get_reg(gb, REG_A) ^ gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (b == 0xE2) {
            gb_log_inst("LD(0xFF00+%02X),%02X", gb_get_reg(gb, REG_C), gb_get_reg(gb, REG_A));
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t c = gb_get_reg(gb, REG_C);
            gb_write_memory(gb, 0xFF00 + c, a);
            gb->PC += inst.size;
        } else if (b == 0xF2) {
            gb_log_inst("LD A,(C)");
            uint8_t c = gb_get_reg(gb, REG_C);
            uint8_t value = gb_read_memory(gb, 0xFF00 + c);
            gb_set_reg(gb, REG_A, value);
            gb->PC += inst.size;
        } else if (b == 0xF3 || b == 0xFB) {
            gb_log_inst(b == 0xF3 ? "DI" : "EI");
            gb->IME = b == 0xF3 ? 0 : 1;
            if (gb->IME) gb->ime_cycles = 1;
            gb->PC += inst.size;
        } else if (b == 0xC0 || b == 0xD0 || b == 0xC8 || b == 0xD8) {
            Flag f = (b >> 3) & 0x3;
            uint8_t low = gb_read_memory(gb, gb->SP + 0);
            uint8_t high = gb_read_memory(gb, gb->SP + 1);
            uint16_t addr = (high << 8) | low;
            gb_log_inst("RET %s", gb_flag_to_str(f));
            if (gb_get_flag(gb, f)) {
                gb->SP += 2;
                gb->PC = addr;
            } else {
                gb->PC += inst.size;
            }
        } else if (b == 0xC9) {
            uint8_t low = gb_read_memory(gb, gb->SP + 0);
            uint8_t high = gb_read_memory(gb, gb->SP + 1);
            uint16_t addr = (high << 8) | low;
            gb->SP += 2;
            gb_log_inst("RET");
            gb->PC = addr;
        } else if (b == 0xD9) {
            uint8_t low = gb_read_memory(gb, gb->SP + 0);
            uint8_t high = gb_read_memory(gb, gb->SP + 1);
            uint16_t addr = (high << 8) | low;
            gb->SP += 2;
            gb_log_inst("RETI");
            gb->IME = 1;
            gb->PC = addr;
        } else if (b == 0xC1 || b == 0xD1 || b == 0xE1 || b == 0xF1) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("POP %s", gb_reg16_to_str(reg));
            uint8_t low = gb_read_memory(gb, gb->SP + 0);
            uint8_t high = gb_read_memory(gb, gb->SP + 1);
            gb_set_reg16(gb, reg, (high << 8) | low);
            gb->SP += 2;
            gb->PC += inst.size;
        } else if (b == 0xC5 || b == 0xD5 || b == 0xE5 || b == 0xF5) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("PUSH %s", gb_reg16_to_str(reg));
            uint16_t value = gb_get_reg16(gb, reg);
            gb->SP -= 2;
            gb_write_memory(gb, gb->SP + 0, value & 0xff);
            gb_write_memory(gb, gb->SP + 1, value >> 8);
            gb->PC += inst.size;
        } else if (
            b == 0xC7 || b == 0xD7 || b == 0xE7 || b == 0xF7 ||
            b == 0xCF || b == 0xDF || b == 0xEF || b == 0xFF
        ) {
            uint8_t n = ((b >> 3) & 0x7)*8;
            gb_log_inst("RST %02XH", n);
            gb->SP -= 2;
            uint16_t ret_addr = gb->PC + inst.size;
            gb_write_memory(gb, gb->SP + 0, ret_addr & 0xff);
            gb_write_memory(gb, gb->SP + 1, ret_addr >> 8);
            gb->PC = n;
        } else if (b == 0xE9) {
            gb_log_inst("JP (HL)");
            uint8_t low = gb_read_memory(gb, gb->HL + 0);
            uint8_t high = gb_read_memory(gb, gb->HL + 1);
            uint16_t pc = (high << 8) | low;
            gb->PC = pc;
        } else {
            gb_log_inst("%02X", inst.data[0]);
            assert(0 && "Instruction not implemented");
        }
    }
    // 2-byte instructions
    else if (inst.size == 2 && b != 0xCB) {
        if (b == 0x18) {
            static bool infinite_loop = false;
            int r8 = inst.data[1] >= 0x80 ? (int8_t)inst.data[1] : inst.data[1];
            if (!infinite_loop) {
                gb_log_inst("JR %d", r8);
            }
            if (r8 == -2 && !infinite_loop) {
                printf("Detected infinite loop...\n");
                infinite_loop = true;
            }
            gb->PC = (gb->PC + inst.size) + r8;
        } else if ( // LD reg,d8
            b == 0x06 || b == 0x16 || b == 0x26 || b == 0x36 ||
            b == 0x0E || b == 0x1E || b == 0x2E || b == 0x3E
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            gb_log_inst("LD %s,0x%02X", gb_reg_to_str(reg), inst.data[1]);
            gb_set_reg(gb, reg, inst.data[1]);
            gb->PC += inst.size;
        } else if (b == 0x20 || b == 0x30 || b == 0x28 || b == 0x38) {
            Flag f = (b >> 3) & 0x3;
            gb_log_inst("JR %s,0x%02X", gb_flag_to_str(f), inst.data[1]);
            if (gb_get_flag(gb, f)) {
                int offset = inst.data[1] >= 0x80 ? (int8_t)inst.data[1] : inst.data[1];
                gb->PC = (gb->PC + inst.size) + offset;
            } else {
                gb->PC += inst.size;
            }
        } else if (b == 0x20) {
            gb_log_inst("JR NZ,0x%02X", inst.data[1]);
            if (!gb_get_zero_flag(gb)) {
                int offset = inst.data[1] >= 0x80 ? (int8_t)inst.data[1] : inst.data[1];
                gb->PC = (gb->PC + inst.size) + offset;
            } else {
                gb->PC += inst.size;
            }
        } else if (b == 0xE0) {
            gb_log_inst("LDH (FF00+%02X),A", inst.data[1]);
            uint8_t value = gb_get_reg(gb, REG_A);
            gb_write_memory(gb, 0xFF00 + inst.data[1], value);
            gb->PC += inst.size;
        } else if (b == 0xC6) {
            gb_log_inst("ADD A,0x%02X", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) + inst.data[1];
            uint8_t c = res < gb_get_reg(gb, REG_A) ? 1 : 0;
            gb_set_reg(gb, REG_A, res);
            uint8_t h = 0; // TODO
            gb_set_flags(gb, res == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b == 0xCE) {
            gb_log_inst("ADC A,0x%02X", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) + inst.data[1] + gb_get_flag(gb, Flag_C);
            uint8_t c = res < gb_get_reg(gb, REG_A) ? 1 : 0;
            gb_set_reg(gb, REG_A, res);
            uint8_t h = 0; // TODO
            gb_set_flags(gb, res == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b == 0xD6) {
            gb_log_inst("SUB A,0x%02X", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) - inst.data[1];
            uint8_t c = gb_get_reg(gb, REG_A) < inst.data[1] ? 1 : 0;
            gb_set_reg(gb, REG_A, res);
            uint8_t h = 0;
            gb_set_flags(gb, res == 0, 1, h, c);
            gb->PC += inst.size;
        } else if (b == 0xDE) {
            gb_log_inst("SBC A,0x%02X", inst.data[1]);
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t res = (uint8_t)(a - gb_get_flag(gb, Flag_C) - inst.data[1]);
            gb_set_reg(gb, REG_A, res);
            uint8_t h = 0; // TODO
            uint8_t c = 0; // TODO
            gb_set_flags(gb, res == 0, 1, h, c);
            gb->PC += inst.size;
        } else if (b == 0xE6) {
            gb_log_inst("AND A,0x%02X", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) & inst.data[1];
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, 1, 0);
            gb->PC += inst.size;
        } else if (b == 0xE8) {
            gb_log_inst("ADD SP,0x%02X", inst.data[1]);
            uint16_t res = gb->SP + (int)inst.data[1];
            uint8_t h = 0; // TODO
            uint8_t c = 0; // TODO
            gb_set_flags(gb, 0, 0, h, c);
            gb->SP = res;
            gb->PC += inst.size;
        } else if (b == 0xF0) {
            gb_log_inst("LDH A,(FF00+%02X)", inst.data[1]);
            uint8_t value = gb_read_memory(gb, 0xFF00 + inst.data[1]);
            gb_set_reg(gb, REG_A, value);
            gb->PC += inst.size;
        } else if (b == 0xF6) {
            gb_log_inst("OR A,0x%02X", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) | inst.data[1];
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (b == 0xF8) {
            gb_log_inst("LD HL,SP+%d", (int8_t)inst.data[1]);
            gb->HL = gb->SP + (int8_t)inst.data[1];
            uint8_t h = 0; // TODO
            uint8_t c = 0; // TODO
            gb_set_flags(gb, 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b == 0xEE) {
            gb_log_inst("XOR 0x%02X", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) ^ inst.data[1];
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (b == 0xFE) {
            gb_log_inst("CP 0x%02X", inst.data[1]);
            int a = (int)gb_get_reg(gb, REG_A);
            int n = (int)inst.data[1];
            uint8_t res = (uint8_t)(a - n);
            uint8_t h = 0; // TODO
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
            gb_log_inst("RLC %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t res = (value << 1) | (value >> 7);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value >> 7);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x08 && inst.data[1] <= 0x0F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RRC %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t res = (value >> 1) | (value >> 7);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x18 && inst.data[1] <= 0x1F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RR %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t c = gb_get_flag(gb, Flag_C);
            uint8_t res = (value >> 1) | (c << 7);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x20 && inst.data[1] <= 0x27) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SLA %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t res = value << 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value >> 7);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x30 && inst.data[1] <= 0x37) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SWAP %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t res = ((value & 0xF) << 4) | ((value & 0xF0) >> 4);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x38 && inst.data[1] <= 0x3F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SRL %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t res = value >> 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 0x1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x40 && inst.data[1] <= 0x7F) {
            uint8_t b = (inst.data[1] >> 3) & 0x7;
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("BIT %d,%s", b, gb_reg_to_str(reg));
            uint8_t value = (gb_get_reg(gb, reg) >> b) & 1;
            gb_set_flags(gb, value == 0, 0, 1, UNCHANGED);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x80 && inst.data[1] <= 0xBF) {
            uint8_t b = (inst.data[1] >> 3) & 0x7;
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RES %d,%s", b, gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg) & ~(1 << b);
            gb_set_reg(gb, reg, value);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0xC0) {
            uint8_t b = (inst.data[1] >> 3) & 0x7;
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SET %d,%s", b, gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg) | (1 << b);
            gb_set_reg(gb, reg, value);
            gb->PC += inst.size;
        } else {
            printf("%02X %02X\n", inst.data[0], inst.data[1]);
            assert(0 && "Instruction not implemented");
        }
    }

    // 3-byte instructions
    else if (inst.size == 3) {
        uint16_t n = inst.data[1] | (inst.data[2] << 8);
        if (b == 0xC3) {
            static bool infinite_loop = false;
            if (!infinite_loop) {
                gb_log_inst("JP 0x%04X", n);
            }
            if (n == gb->PC && !infinite_loop) {
                printf("Detected infinite loop...\n");
                infinite_loop = true;
            }
            gb->PC = n;
        } else if (b == 0x01 || b == 0x11 || b == 0x21 || b == 0x31) {
            Reg16 reg = b >> 4;
            gb_log_inst("LD %s,0x%04X", gb_reg16_to_str(reg), n);
            gb_set_reg16(gb, reg, n);
            gb->PC += inst.size;
        } else if (b == 0x08) {
            gb_log_inst("LD (0x%04X),SP", n);
            gb_write_memory(gb, n+0, gb->SP & 0xff);
            gb_write_memory(gb, n+1, gb->SP >> 8);
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
                uint16_t ret_addr = gb->PC + inst.size;
                gb_write_memory(gb, gb->SP+0, ret_addr & 0xff);
                gb_write_memory(gb, gb->SP+1, ret_addr >> 8);
                gb->PC = n;
            } else {
                gb->PC += inst.size;
            }
        } else if (b == 0xCD) {
            gb_log_inst("CALL 0x%04X", n);
            gb->SP -= 2;
            uint16_t ret_addr = gb->PC + inst.size;
            gb_write_memory(gb, gb->SP+0, ret_addr & 0xff);
            gb_write_memory(gb, gb->SP+1, ret_addr >> 8);
            gb->PC = n;
        } else if (b == 0xEA) {
            gb_log_inst("LD (0x%04X),A", n);
            uint8_t a = gb_get_reg(gb, REG_A);
            gb_write_memory(gb, n, a);
            gb->PC += inst.size;
        } else if (b == 0xFA) {
            gb_log_inst("LD A,(0x%04X)", n);
            uint8_t value = gb_read_memory(gb, n);
            gb_set_reg(gb, REG_A, value);
            gb->PC += inst.size;
        } else {
            assert(0 && "Not implemented");
        }
    }

    assert(gb->PC <= 0xFFFF);
}

static size_t gb_tile_coord_to_pixel(int row, int col)
{
    assert(row >= 0 && row < 32);
    assert(col >= 0 && col < 32);
    int row_pixel = row*TILE_PIXELS; // (0, 8, 16, ..., 248)
    int col_pixel = col*TILE_PIXELS; // (0, 8, 16, ..., 248)
    return row_pixel*SCRN_VX + col_pixel;
}

static void fill_solid_tile(GameBoy *gb, int x, int y, uint8_t color)
{
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            gb->display[(row+y)*256 + (col+x)] = color;
        }
    }
}

// Render Heart tile at 0,0
//uint8_t tile[] = {
//    0x00, 0x00, 0x6C, 0x6C, 0xFE, 0xFA, 0xFE, 0xFE,
//    0xFE, 0xFE, 0x7C, 0x7C, 0x38, 0x38, 0x10, 0x10,
//};
//fill_tile(gb, 0, 0, tile);
static void fill_tile(GameBoy *gb, int x, int y, uint8_t *tile, bool transparency)
{
    // tile is 16 bytes
    uint8_t bgp = gb->memory[rBGP];
    uint8_t PALETTE[] = {0xFF, 0x80, 0x40, 0x00};
    uint8_t bgp_tbl[] = {(bgp >> 0) & 3, (bgp >> 2) & 3, (bgp >> 4) & 3, (bgp >> 6) & 3};
    for (int row = 0; row < 8; row++) {
        // 64 pixels, but only 16 bytes (2 bytes per row)
        uint8_t low_bitplane = tile[row*2+0];
        uint8_t high_bitplane = tile[row*2+1];
        for (int col = 0; col < 8; col++) {
            uint8_t bit0 = (low_bitplane & 0x80) >> 7;
            uint8_t bit1 = (high_bitplane & 0x80) >> 7;
            low_bitplane <<= 1;
            high_bitplane <<= 1;
            uint8_t color_idx = (bit1 << 1) | bit0; // 0-3
            uint8_t palette_idx = bgp_tbl[color_idx];
            uint8_t color = PALETTE[palette_idx];
            if (!transparency || palette_idx != 0) {
                gb->display[(row+y)*256 + (col+x)] = color;
            }
        }
    }
}

void gb_render(GameBoy *gb)
{
    uint8_t lcdc = gb->memory[rLCDC];
    if ((lcdc & LCDCF_ON) != LCDCF_ON) return;

    uint16_t bg_win_td_off = (lcdc & LCDCF_BG8000) == LCDCF_BG8000 ?
        _VRAM8000 : _VRAM9000;

    // Render the Background
    if ((lcdc & LCDCF_BGON) == LCDCF_BGON) {
        uint16_t bg_tm_off = (lcdc & LCDCF_BG9C00) == LCDCF_BG9C00 ? _SCRN1 : _SCRN0;

        for (int row = 0; row < SCRN_VY_B; row++) {
            for (int col = 0; col < SCRN_VX_B; col++) {
                int tile_idx = gb->memory[bg_tm_off + row*32 + col];
                //fill_solid_tile(gb, col*8, row*8, 0xff);
                uint8_t *tile = gb->memory + bg_win_td_off + tile_idx*16;
                fill_tile(gb, col*8, row*8, tile, false);
            }
        }
    }

    // Render the Window
    if ((lcdc & LCDCF_WINON) == LCDCF_WINON) {
        //uint8_t win_tm_off = (lcdc & LCDCF_WIN9C00) == LCDCF_WIN9C00 ? _SCRN1 : _SCRN0;
        assert(0 && "Window rendering is not implemented");
    }

    // Render the Sprites (OBJ)
    if ((lcdc & LCDCF_OBJON) == LCDCF_OBJON) {
        assert((lcdc & LCDCF_OBJ16) == 0 && "Only 8x8 sprites supported");
        for (int i = 0; i < OAM_COUNT; i++) {
            uint8_t y = gb->memory[_OAMRAM + i*4 + 0] - 16;
            uint8_t x = gb->memory[_OAMRAM + i*4 + 1] - 8;
            uint8_t tile_idx = gb->memory[_OAMRAM + i*4 + 2];
            //uint8_t attribs = gb->memory[_OAMRAM + i*4 + 3];

            uint8_t *tile = gb->memory + _VRAM8000 + tile_idx*16;
            fill_tile(gb, x, y, tile, true);
        }
    }
}

void gb_load_rom(GameBoy *gb, uint8_t *raw, size_t size)
{
    printf("  ROM Size: %lx\n", size);
    assert(size > 0x14F);
    RomHeader *header = (RomHeader*)(raw + 0x100);
    if (strlen(header->title)) {
        printf("  Title: %s\n", header->title);
    } else {
        printf("  Title:");
        for (int i = 0; i < 16; i++) printf(" %02X", header->title[i]);
        printf("\n");
    }
    if (memcmp(header->logo, NINTENDO_LOGO, sizeof(NINTENDO_LOGO)) != 0) {
        fprintf(stderr, "Nintendo Logo does NOT match\n");
        exit(1);
    }
    printf("  CGB: %02X\n", (uint8_t)header->title[15]);
    printf("  New licensee code: %02X %02X\n", header->new_licensee[0], header->new_licensee[1]);
    printf("  SGB: %02X\n", header->sgb);
    printf("  Cartridge Type: %02X\n", header->cart_type);
    printf("  ROM size: %d KiB\n", 32*(1 << header->rom_size));
    printf("  RAM size: %02X\n", header->ram_size);
    printf("  Destination code: %02X\n", header->dest_code);
    printf("  Old licensee code: %02X\n", header->old_licensee);
    printf("  Mask ROM version number: %02X\n", header->mask_version);
    printf("  Header checksum: %02X\n", header->header_check);
    uint8_t checksum = 0;
    for (uint16_t addr = 0x0134; addr <= 0x014C; addr++) {
        checksum = checksum - raw[addr] - 1;
    }
    if (header->header_check != checksum) {
        fprintf(stderr, "    Checksum does NOT match: %02X vs. %02X\n", header->header_check, checksum);
        exit(1);
    }

    printf("  Global checksum: %02X %02X\n", header->global_check[0], header->global_check[1]);
    printf("\n");
    //assert(header->cart_type == 0);

    memcpy(gb->memory, raw, size > 0xFFFF ? 0xFFFF : size);

    printf("Executing...\n");
    gb->PC = 0x100;
    // Should we set SP = $FFFE as specified in GBCPUman.pdf ???

    gb->timer_sec = 1000.0;
    gb->timer_div = (1000.0 / 16384.0);
}

void gb_load_rom_file(GameBoy *gb, const char *path)
{
    printf("Loading ROM \"%s\"...\n", path);
    size_t size;
    uint8_t *raw = read_entire_file(path, &size);
    gb_load_rom(gb, raw, size);
    free(raw);
}

void gb_tick(GameBoy *gb, double dt_ms)
{
    gb->elapsed_ms += dt_ms;
    gb->timer_sec -= dt_ms;
    gb->timer_div -= dt_ms;
    gb->timer_tima -= dt_ms;
    //if (gb->timer_sec <= 0.0) {
        gb->timer_sec += 1000.0;

        if (get_command(gb)) {
            Inst inst = gb_fetch_inst(gb);
            gb_exec(gb, inst);
        }
    //}

    // Copy tiles to display
    gb_render(gb);

    // Increase rDIV at a rate of 16384Hz (every 0.06103515625 ms)
    if (gb->timer_div <= 0) {
        gb->memory[rDIV] += 1;
        gb->timer_div += 1000.0 / 16384;
    }

    // Timer Enabled in TAC
    if (gb->memory[rTAC] & 0x04) {
        // Update TIMA (timer counter)
        int freq;
        switch (gb->memory[rTAC] & 0x03) {
            case 0:
                freq = 4096;
                break;
            case 1:
                freq = 262144;
                break;
            case 2:
                freq = 65536;
                break;
            case 3:
                freq = 16384;
                break;
        }
        if (gb->timer_tima <= 0) {
            gb->memory[rTIMA] += 1;
            gb->timer_tima += 1000.0 / freq; 
            printf("Increasing TIMA %02X -> %02X\n", (uint8_t)(gb->memory[rTIMA] - 1), gb->memory[rTIMA]);
            if (gb->memory[rTIMA] == 0) {
                printf("Reseting TIMA to %02X (TMA)\n", gb->memory[rTMA]);
                gb->memory[rTIMA] = gb->memory[rTMA];
                // TODO: Trigger interrupt
                gb_trigger_interrupt(gb);
            }
        }
    }

    // HACK: increase the LY register without taking VSync/HSync into consideration!!!
    gb->memory[rLY] += 1;
    if (gb->memory[rLY] == 0) {
        // VBlank interrupt maybe?
    }
}

uint8_t *read_entire_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "Failed to open %s\n", path);
        exit(1);
    }
    if (fseek(f, 0, SEEK_END) < 0) {
        fprintf(stderr, "Failed to read file %s\n", path);
        exit(1);
    }

    long file_size = ftell(f);
    assert(file_size > 0);

    rewind(f);

    uint8_t *file_data = malloc(file_size);
    assert(file_data);

    size_t bytes_read = fread(file_data, 1, file_size, f);
    if (bytes_read != (size_t)file_size) {
        fprintf(stderr, "Could not read entire file\n");
        exit(1);
    }

    if (size != NULL) {
        *size = bytes_read;
    }
    return file_data;
}

bool get_command(GameBoy *gb)
{
    for (size_t i = 0; i < bp_count; i++) {
        if (bp[i] == gb->PC) {
            gb->step_debug = true;
            printf("Hit Breakpoint at: %04X\n", gb->PC);
        }
    }

    if (!gb->step_debug) return true;

    char buf[256];
    char *cmd = buf;
    fgets(cmd, sizeof(buf), stdin);

    if (strncmp(cmd, "q", 1) == 0 || strncmp(cmd, "quit", 4) == 0) {
        printf("Quitting...\n");
        exit(0);
    } else if (strncmp(cmd, "s", 1) == 0 || strncmp(cmd, "step", 4) == 0) {
        return true;
    } else if (strncmp(cmd, "n", 1) == 0 || strncmp(cmd, "next", 4) == 0) {
        Inst inst = gb_fetch_inst(gb);
        bp[bp_count++] = gb->PC + inst.size;
        gb->step_debug = false;
        gb->paused = false;
        return true;
    } else if (strncmp(cmd, "c", 1) == 0 || strncmp(cmd, "continue", 8) == 0) {
        gb->step_debug = false;
        gb->paused = false;
        return true;
    } else if (strncmp(cmd, "d", 1) == 0 || strncmp(cmd, "dump", 4) == 0) {
        gb_dump(gb);
    } else if (strncmp(cmd, "b", 1) == 0 || strncmp(cmd, "break", 5) == 0) {
        while (*cmd != ' ') cmd += 1;
        cmd += 1;

        unsigned long addr;
        if (cmd[0] == '0' && cmd[1] == 'x') {
            // Parse hex
            addr = strtoul(cmd + 2, NULL, 16);
        } else if (cmd[0] >= '0' && cmd[0] <= '9') {
            addr = strtoul(cmd, NULL, 10);
        } else {
            assert(0 && "TODO: Invalid break address");
        }

        printf("Setting breakpoint at 0x%04lX\n", addr);

        bp[bp_count] = addr;
        bp_count += 1;
    } else if (strncmp(cmd, "x", 1) == 0) {
        // x/nfu addr
        // x/5 addr
        // x/5i addr
        assert(cmd[1] == '/');
        char *end;
        unsigned long n = strtoul(cmd + 2, &end, 10);
        char f = *end;
        while (!isdigit(*end)) {
            end++;
        }

        unsigned long addr = strtoul(end, NULL, 16);
        assert(n > 0);
        assert(addr <= 0xFFFF);
        if (f == 'i') {
            printf("NOT IMPLEMENTED\n");
            printf("%04lX: ", addr);
            printf("\n");
        } else {
            printf("%04lX: ", addr);
            for (size_t i = 0; i < n && addr + i <= 0xFFFF; i++) {
                printf("%02X ", gb->memory[addr + i]);
            }
            printf("\n");
        }
    }

    return false;
}
