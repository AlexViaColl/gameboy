#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gb.h"

#define SCALE  5

#define MAX_TILE_IDS (128*3)
#define TILEMAP_ROWS 32
#define TILEMAP_COLS 32
#define VIEWPORT_COLS 20
#define VIEWPORT_ROWS 18
#define TILE_PIXELS 8
#define OAM_COUNT 40

#define CPU_FREQ    4194304.0 // 4.19 MHz
#define VSYNC       59.73
#define HSYNC       9198.0    // 9.198 KHz

#define CLOCK_MS    (1000.0 / CPU_FREQ)
#define VSYNC_MS    (1000.0 / VSYNC)
#define HSYNC_MS    (1000.0 / HSYNC)

#define VRAM_TILES      0x8000
//#define VRAM_TILES      0x9000
#define VRAM_TILEMAP    0x9800

#define rP1     0xFF00
#define rLCDC   0xFF40
#define rLY     0xFF44
#define rBGP    0xFF47
#define rWY     0xFF4A
#define rWX     0xFF4B

#define LCDCF_ON        0x80
#define LCDCF_WIN9C00   0x40
#define LCDCF_WINON     0x20
#define LCDCF_BG8000    0x10
#define LCDCF_OBJON     0x02
#define LCDCF_BGON      0x01

#define P1F_GET_BTN  0x10
#define P1F_GET_DPAD 0x20
#define P1F_GET_NONE (P1F_GET_BTN | P1F_GET_DPAD)

static const uint8_t NINTENDO_LOGO[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

#define TILE_SIZE 16

// Debug Status
static bool step_debug;
static size_t bp_count = 0;
#define MAX_BREAKPOINTS 16
static uint16_t bp[MAX_BREAKPOINTS];

typedef struct RomHeader {
    uint8_t entry[4];
    uint8_t logo[48];
    char title[16];
} RomHeader;

uint8_t *read_entire_file(const char *path, size_t *size);
bool get_command(GameBoy *gb);

void gb_dump(GameBoy *gb)
{
    uint8_t flags = gb->AF & 0xff;
    printf("$PC: $%04X, A: $%02X (%d), F: %c%c%c%c, BC: $%04X, DE: $%04X, HL: $%04X, SP: $%04X\n",
        gb->PC,
        gb_get_reg(gb, REG_A), gb_get_reg(gb, REG_A),
        (flags & 0x80) ? 'Z' : '-',
        (flags & 0x40) ? 'N' : '-',
        (flags & 0x20) ? 'H' : '-',
        (flags & 0x10) ? 'C' : '-',
        gb->BC, gb->DE, gb->HL, gb->SP);
    //printf("AF = 0x%04X (A = 0x%02X (%d), Flags: Z: %d, C: %d, H: %d, N: %d)\n",
    //    gb->AF, gb->AF >> 8, gb->AF >> 8,
    //    (flags & 0x80) > 0,
    //    (flags & 0x10) > 0,
    //    (flags & 0x20) > 0,
    //    (flags & 0x40) > 0);
    //printf("BC = 0x%04X (B = 0x%02X, C = 0x%02X)\n", gb->BC, gb->BC >> 8, gb->BC & 0xff);
    //printf("DE = 0x%04X (D = 0x%02X, E = 0x%02X)\n", gb->DE, gb->DE >> 8, gb->DE & 0xff);
    //printf("HL = 0x%04X (H = 0x%02X, L = 0x%02X)\n", gb->HL, gb->HL >> 8, gb->HL & 0xff);
    //printf("SP = 0x%04X, PC = 0x%04X, IME = %d\n", gb->SP, gb->PC, gb->IME);
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
            gb->memory[gb->HL] = value;
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
            return gb->memory[gb->HL];
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
    uint8_t b = gb->memory[gb->PC];
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
    } else if (b == 0xDE || b == 0xFE) {
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
    for (size_t i = 0; i < 3; i++) {
        if (i < inst.size) {
            gb->printf("%02X ", inst.data[i]);
        } else {
            gb->printf("   ");
        }
    }
    gb->printf(" | %s\n", buf);
}

void gb_exec(GameBoy *gb, Inst inst)
{
    uint8_t b = inst.data[0];
    // 1-byte instructions
    if (inst.size == 1) {
        if (b == 0x00) {
            gb_log_inst("NOP");
            gb->PC += inst.size;
        } else if (b == 0x09 || b == 0x19 || b == 0x29 || b == 0x39) {
            Reg16 src = (b >> 4) & 0x3;
            gb_log_inst("ADD HL,%s", gb_reg16_to_str(src));
            uint16_t hl_prev = gb_get_reg16(gb, REG_HL);
            uint16_t res = hl_prev + gb_get_reg16(gb, src);
            gb_set_reg16(gb, REG_HL, res);
            gb_set_flag(gb, Flag_N, 0);
            //gb_set_flag(gb, Flag_H, ???); // TODO
            gb_set_flag(gb, Flag_C, res < hl_prev ? 1 : 0);
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
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, (res & 0xF) < (prev & 0xF) ? 1 : 0);
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
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 1);
            gb_set_flag(gb, Flag_H, (res & 0xF) < (prev & 0xF) ? 1 : 0);
            gb->PC += inst.size;
        } else if (b == 0x07) {
            gb_log_inst("RLCA");
            uint8_t prev = gb_get_reg(gb, REG_A);
            uint8_t res = (prev << 1) | (prev >> 7);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, prev >> 7);
            gb->PC += inst.size;
        } else if (b == 0x0F) {
            gb_log_inst("RRCA");
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t c = a & 1;
            uint8_t res = (a >> 1) | (c << 7);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, c);
            gb->PC += inst.size;
        } else if (b == 0x17) {
            gb_log_inst("RLA");
            uint8_t prev = gb_get_reg(gb, REG_A);
            uint8_t res = (prev << 1) | gb_get_flag(gb, Flag_C);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, prev >> 7);
            gb->PC += inst.size;
        } else if (b == 0x1F) {
            gb_log_inst("RRA");
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t c = a & 1;
            uint8_t res = (a >> 1) | (gb_get_flag(gb, Flag_C) << 7);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, c);
            gb->PC += inst.size;
        } else if (b == 0x2F) {
            gb_log_inst("CPL");
            uint8_t a = gb_get_reg(gb, REG_A);
            gb_set_reg(gb, REG_A, ~a);
            gb_set_flag(gb, Flag_N, 1);
            gb_set_flag(gb, Flag_H, 1);
            gb->PC += inst.size;
        } else if (b == 0x37) {
            gb_log_inst("SCF");
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, 1);
            gb->PC += inst.size;
        } else if (b == 0x3F) {
            gb_log_inst("CCF");
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, !gb_get_flag(gb, Flag_C));
            gb->PC += inst.size;
        } else if (b == 0x0A || b == 0x1A) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("LD A,(%s)", gb_reg16_to_str(reg));
            uint8_t value = gb->memory[gb_get_reg16(gb, reg)];
            gb_set_reg(gb, REG_A, value);
            gb->PC += inst.size;
        } else if (b == 0x02 || b == 0x12) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("LD (%s),A", gb_reg16_to_str(reg));
            gb->memory[gb_get_reg16(gb, reg)] = gb_get_reg(gb, REG_A);
            gb->PC += inst.size;
        } else if (b == 0x22 || b == 0x32) {
            gb_log_inst("LD (HL%c),A", b == 0x22 ? '+' : '-');
            gb->memory[gb->HL] = gb_get_reg(gb, REG_A);
            if (b == 0x22) gb->HL += 1;
            else gb->HL -= 1;
            gb->PC += inst.size;
        } else if (b == 0x0A || b == 0x1A) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("LD A,(%s)", gb_reg16_to_str(reg));
            gb_set_reg(gb, REG_A, gb->memory[gb_get_reg16(gb, reg)]);
            gb->PC += inst.size;
        } else if (b == 0x2A || b == 0x3A) {
            gb_log_inst("LD A,(HL%c)", b == 0x22 ? '+' : '-');
            gb_set_reg(gb, REG_A, gb->memory[gb->HL]);
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

            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, h);
            gb_set_flag(gb, Flag_C, (res < a) ? 1 : 0);

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

            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, h);
            gb_set_flag(gb, Flag_C, (res < a) ? 1 : 0);

            gb->PC += inst.size;
        } else if (b >= 0x90 && b <= 0x97) {
            Reg8 reg = b & 0x7;
            gb_log_inst("SUB A,%s", gb_reg_to_str(reg));
            int res = gb_get_reg(gb, REG_A) - gb_get_reg(gb, reg);
            uint8_t c = gb_get_reg(gb, REG_A) >= gb_get_reg(gb, reg) ? 0 : 1;
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 1);
            gb_set_flag(gb, Flag_H, 1); // TODO
            gb_set_flag(gb, Flag_C, c);
            gb->PC += inst.size;
        } else if (b >= 0x98 && b <= 0x9F) {
            Reg8 reg = b & 0x7;
            gb_log_inst("SBC A,%s", gb_reg_to_str(reg));
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t res = (uint8_t)(a - gb_get_flag(gb, Flag_C) - gb_get_reg(gb, reg));
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 1);
            gb_set_flag(gb, Flag_H, 1); // TODO
            //gb_set_flag(gb, Flag_C, 1); // TODO
            gb->PC += inst.size;
        } else if (b >= 0xB0 && b <= 0xB7) {
            Reg8 reg = b & 0x7;
            gb_log_inst("OR %s", gb_reg_to_str(reg));
            uint8_t res = gb_get_reg(gb, REG_A) | gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, 0);
            gb->PC += inst.size;
        } else if (b >= 0xB8 && b <= 0xBF) {
            Reg8 reg = b & 0x7;
            gb_log_inst("CP %s", gb_reg_to_str(reg));
            int a = (int)gb_get_reg(gb, REG_A);
            int n = (int)gb_get_reg(gb, reg);
            int res = a - n;
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 1);
            //TODO: gb_set_flag(gb, Flag_H, ???);
            gb_set_flag(gb, Flag_C, a < n ? 1 : 0);
            gb->PC += inst.size;
        } else if (b >= 0xA0 && b <= 0xA7) {
            Reg8 reg = b & 0x7;
            gb_log_inst("AND %s", gb_reg_to_str(reg));
            uint8_t res = gb_get_reg(gb, REG_A) & gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 1);
            gb_set_flag(gb, Flag_C, 0);
            gb->PC += inst.size;
        } else if (b >= 0xA8 && b <= 0xAF) {
            Reg8 reg = b & 0x7;
            gb_log_inst("XOR %s", gb_reg_to_str(reg));
            uint8_t res = gb_get_reg(gb, REG_A) ^ gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, 0);
            gb->PC += inst.size;
        } else if (b == 0xE2) {
            gb_log_inst("LD (C),A -- LD(0xFF00+%02X),%02X", gb_get_reg(gb, REG_C), gb_get_reg(gb, REG_A));
            gb->memory[0xFF00 + gb_get_reg(gb, REG_C)] = gb_get_reg(gb, REG_A);
            gb->PC += inst.size;
        } else if (b == 0xF2) {
            gb_log_inst("LD A,(C)");
            gb_set_reg(gb, REG_A, gb->memory[0xFF00 + gb_get_reg(gb, REG_C)]);
            gb->PC += inst.size;
        } else if (b == 0xF3 || b == 0xFB) {
            gb_log_inst(b == 0xF3 ? "DI" : "EI");
            gb->IME = b == 0xF3 ? 0 : 1;
            gb->PC += inst.size;
        } else if (b == 0xC0 || b == 0xD0 || b == 0xC8 || b == 0xD8) {
            Flag f = (b >> 3) & 0x3;
            uint8_t low = gb->memory[gb->SP+0];
            uint8_t high = gb->memory[gb->SP+1];
            uint16_t addr = (high << 8) | low;
            gb_log_inst("RET %s (address: 0x%04X)", gb_flag_to_str(f), addr);
            if (gb_get_flag(gb, f)) {
                gb->SP += 2;
                gb->PC = addr;
            } else {
                gb->PC += inst.size;
            }
        } else if (b == 0xC9) {
            uint8_t low = gb->memory[gb->SP+0];
            uint8_t high = gb->memory[gb->SP+1];
            gb->SP += 2;
            uint16_t addr = (high << 8) | low;
            gb_log_inst("RET (address: 0x%04X)", addr);
            gb->PC = addr;
        } else if (b == 0xD9) {
            uint8_t low = gb->memory[gb->SP+0];
            uint8_t high = gb->memory[gb->SP+1];
            gb->SP += 2;
            uint16_t addr = (high << 8) | low;
            gb_log_inst("RETI (address: 0x%04X)", addr);
            gb->IME = 1;
            gb->PC = addr;
        } else if (b == 0xC1 || b == 0xD1 || b == 0xE1 || b == 0xF1) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("POP %s", gb_reg16_to_str(reg));
            uint8_t low = gb->memory[gb->SP+0];
            uint8_t high = gb->memory[gb->SP+1];
            gb_set_reg16(gb, reg, (high << 8) | low);
            gb->SP += 2;
            gb->PC += inst.size;
        } else if (b == 0xC5 || b == 0xD5 || b == 0xE5 || b == 0xF5) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("PUSH %s", gb_reg16_to_str(reg));
            uint16_t value = gb_get_reg16(gb, reg);
            gb->SP -= 2;
            gb->memory[gb->SP+0] = (value & 0xff);
            gb->memory[gb->SP+1] = (value >> 8);
            gb->PC += inst.size;
        } else if (
            b == 0xC7 || b == 0xD7 || b == 0xE7 || b == 0xF7 ||
            b == 0xCF || b == 0xDF || b == 0xEF || b == 0xFF
        ) {
            uint8_t n = ((b >> 3) & 0x7)*8;
            gb_log_inst("RST %02XH", n);
            gb->SP -= 2;
            uint16_t ret_addr = gb->PC + inst.size;
            //uint16_t ret_addr = gb->PC;
            gb->memory[gb->SP+0] = (ret_addr & 0xff);
            gb->memory[gb->SP+1] = (ret_addr >> 8);
            gb->PC = n;
        } else if (b == 0xE9) {
            gb_log_inst("JP (HL)");
            gb->PC = gb->memory[gb->HL];
        } else {
            gb_log_inst("%02X", inst.data[0]);
            assert(0 && "Instruction not implemented");
        }
    }
    // 2-byte instructions
    else if (inst.size == 2 && b != 0xCB) {
        if (b == 0x10) {
            gb_log_inst("STOP");
            exit(1);
        } else if (b == 0x18) {
            int r8 = inst.data[1] >= 0x80 ? (int8_t)inst.data[1] : inst.data[1];
            gb_log_inst("JR %d", r8);
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
        } else if (b == 0x2F) {
            gb_log_inst("CPL");
            gb_set_reg(gb, REG_A, ~gb_get_reg(gb, REG_A));
            gb_set_flag(gb, Flag_N, 1);
            gb_set_flag(gb, Flag_H, 1);
            gb->PC += inst.size;
        } else if (b == 0xE0) {
            gb_log_inst("LDH (FF00+%02X),A", inst.data[1]);
            uint8_t value = gb_get_reg(gb, REG_A);
            gb->memory[0xFF00 + inst.data[1]] = value;
            if (inst.data[1] == 0) {
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
            gb->PC += inst.size;
        } else if (b == 0xC6) {
            gb_log_inst("ADD A,0x%02X", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) + inst.data[1];
            uint8_t c = res < gb_get_reg(gb, REG_A) ? 1 : 0;
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0); // TODO: compute H flag
            gb_set_flag(gb, Flag_C, c);
            gb->PC += inst.size;
        } else if (b == 0xD6) {
            gb_log_inst("SUB A,0x%02X", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) - inst.data[1];
            uint8_t c = gb_get_reg(gb, REG_A) < inst.data[1] ? 1 : 0;
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 1);
            gb_set_flag(gb, Flag_H, 0); // TODO: compute H flag
            gb_set_flag(gb, Flag_C, c);
            gb->PC += inst.size;
        } else if (b == 0xDE) {
            gb_log_inst("SBC A,0x%02X", inst.data[1]);
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t res = (uint8_t)(a - gb_get_flag(gb, Flag_C) - inst.data[1]);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 1);
            //gb_set_flag(gb, Flag_H, 1); // TODO
            //gb_set_flag(gb, Flag_C, 1); // TODO
            gb->PC += inst.size;
        } else if (b == 0xE6) {
            gb_log_inst("AND A,0x%02X", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) & inst.data[1];
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 1);
            gb_set_flag(gb, Flag_C, 0);
            gb->PC += inst.size;
        } else if (b == 0xE8) {
            gb_log_inst("ADD SP,0x%02X", inst.data[1]);
            gb->SP += (int)inst.data[1];
            gb->PC += inst.size;
        } else if (b == 0xF0) {
            gb_log_inst("LDH A,(FF00+%02X)", inst.data[1]);
            gb_set_reg(gb, REG_A, gb->memory[0xFF00 + inst.data[1]]);
            gb->PC += inst.size;
        } else if (b == 0xF6) {
            gb_log_inst("OR A,0x%02X", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) | inst.data[1];
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, 0);
            gb->PC += inst.size;
        } else if (b == 0xF8) {
            gb_log_inst("LD HL,SP+%d", (int8_t)inst.data[1]);

            gb->HL = gb->SP + (int8_t)inst.data[1];

            gb_set_flag(gb, Flag_Z, 0);
            gb_set_flag(gb, Flag_N, 0);
            //TODO:gb_set_flag(gb, Flag_H, 0);
            //TODO:gb_set_flag(gb, Flag_C, 0);

            gb->PC += inst.size;
        } else if (b == 0xFE) {
            gb_log_inst("CP 0x%02X", inst.data[1]);
            int a = (int)gb_get_reg(gb, REG_A);
            int n = (int)inst.data[1];
            uint8_t res = (uint8_t)(a - n);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 1);
            // H
            gb_set_flag(gb, Flag_C, a < n ? 1 : 0);
            gb->PC += inst.size;
        } else {
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
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, value >> 7);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x18 && inst.data[1] <= 0x1F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RR %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t c = gb_get_flag(gb, Flag_C);
            uint8_t res = (value >> 1) | (c << 7);
            gb_set_reg(gb, reg, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, value & 1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x20 && inst.data[1] <= 0x27) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SLA %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t res = value << 1;
            gb_set_reg(gb, reg, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, value >> 7);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x30 && inst.data[1] <= 0x37) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SWAP %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            gb_set_reg(gb, reg, ((value & 0xF) << 4) | ((value & 0xF0) >> 4));
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x38 && inst.data[1] <= 0x3F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SRL %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t res = value >> 1;
            gb_set_reg(gb, reg, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, value & 0x1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x40 && inst.data[1] <= 0x7F) {
            uint8_t b = (inst.data[1] >> 3) & 0x7;
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("BIT %d,%s", b, gb_reg_to_str(reg));
            uint8_t value = (gb_get_reg(gb, reg) >> b) & 1;
            gb_set_flag(gb, Flag_Z, value == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 1);
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
            gb_log_inst("JP 0x%04X", n);
            gb->PC = n;
        } else if (b == 0x01 || b == 0x11 || b == 0x21 || b == 0x31) {
            Reg16 reg = b >> 4;
            gb_log_inst("LD %s,0x%04X", gb_reg16_to_str(reg), n);
            gb_set_reg16(gb, reg, n);
            gb->PC += inst.size;
        } else if (b == 0x08) {
            gb_log_inst("LD (0x%04X),SP", n);
            gb->memory[n+0] = (gb->SP & 0xff);
            gb->memory[n+1] = (gb->SP >> 8);
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
                gb->memory[gb->SP+0] = (ret_addr & 0xff);
                gb->memory[gb->SP+1] = (ret_addr >> 8);
                gb->PC = n;
            } else {
                gb->PC += inst.size;
            }
        } else if (b == 0xCD) {
            gb_log_inst("CALL 0x%04X", n);
            gb->SP -= 2;
            uint16_t ret_addr = gb->PC + inst.size;
            gb->memory[gb->SP+0] = (ret_addr & 0xff);
            gb->memory[gb->SP+1] = (ret_addr >> 8);
            gb->PC = n;
        } else if (b == 0xEA) {
            gb_log_inst("LD (0x%04X),A", n);
            gb->memory[n] = gb_get_reg(gb, REG_A);
            gb->PC += inst.size;
        } else if (b == 0xFA) {
            gb_log_inst("LD A,(0x%04X)", n);
            gb_set_reg(gb, REG_A, gb->memory[n]);
            gb->PC += inst.size;
        } else {
            assert(0 && "Not implemented");
        }
    }

    assert(gb->PC <= 0xFFFF);
}

void gb_load_rom(GameBoy *gb, uint8_t *raw, size_t size)
{
    printf("Size: %lx\n", size);
    assert(size > 0x14F);
    RomHeader *header = (RomHeader*)(raw + 0x100);
    uint8_t cartridge_type = *(raw + 0x147);
    printf("Title: %s\n", header->title);
    printf("Logo: ");
    if (memcmp(header->logo, NINTENDO_LOGO, sizeof(NINTENDO_LOGO)) != 0) {
        printf("NOT Present\n");
    } else {
        printf("Present\n");
    }
    printf("Entry: ");
    for (int i = 0; i < 4; i++) {
        printf("0x%02X ", header->entry[i]);
    }
    printf("\nCartridge Type: 0x%02X\n", cartridge_type);
    printf("\n\n");
    assert(cartridge_type == 0);

    memcpy(gb->memory, raw, size);

    gb->PC = 0x100;
    // Should we set SP = $FFFE as specified in GBCPUman.pdf ???

    gb->timer_sec = 1000.0;
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
    //if (gb->timer_sec <= 0.0) {
        gb->timer_sec += 1000.0;

        if (get_command(gb)) {
            Inst inst = gb_fetch_inst(gb);
            gb_exec(gb, inst);
        }
    //}

    // HACK: increase the LY register without taking VSync/HSync into consideration!!!
    gb->memory[rLY] += 1;
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
            step_debug = true;
            printf("Hit Breakpoint at: %04X\n", gb->PC);
        }
    }

    if (!step_debug) return true;

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
        step_debug = false;
        return true;
    } else if (strncmp(cmd, "c", 1) == 0 || strncmp(cmd, "continue", 8) == 0) {
        step_debug = false;
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
