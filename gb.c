#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#define SCALE  5
#define WIDTH  160  // 20 tiles
#define HEIGHT 144  // 18 tiles

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

const uint8_t NINTENDO_LOGO[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

// Tile: 8x8 pixels, 2 bpp (index into Palette)
const uint8_t PALETTE[4] = {0x00, 0x40, 0x80, 0xFF};
//const uint8_t PALETTE[4] = {0xff, 0x80, 0x40, 0x00};

#define TILE_SIZE 16
const uint8_t TILE[TILE_SIZE] = {
    0x00, 0xff, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80,
    0x00, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0x80
};

// Debug Status
static bool step_debug;
static size_t bp_count = 0;
#define MAX_BREAKPOINTS 16
static uint16_t bp[MAX_BREAKPOINTS];

typedef struct Inst {
    uint8_t *data;
    size_t size;
} Inst;

typedef struct GameBoy {
    // CPU freq:        4.194304 MHz    (~4194304 cycles/s)
    // Horizontal sync: 9.198 KHz       ( 0.10871929 ms/line)
    // Vertical sync:   59.73 Hz        (16.74200569 ms/frame)
    uint16_t AF; // Accumulator & Flags
    uint16_t BC;
    uint16_t DE;
    uint16_t HL;
    uint16_t SP; // Stack Pointer
    uint16_t PC; // Program Counter

    uint8_t IME; // Interrupt master enable flag (Instructions EI, DI, RETI, ISR)

    // Tiles, Palettes and Layers
    // Tiles are 8x8 pixels (also called patterns or characters)
    // Stored with color ID's from 0 to 3 or 2 bits per pixel
    // 8x8x2 = 128 bits = 16 bytes/tile
    uint8_t display[WIDTH*HEIGHT];

    // 0000-3FFF    16 KiB ROM bank 00
    // 4000-7FFF    16 KiB ROM bank 01~NN
    // 8000-9FFF     8 KiB Video RAM (VRAM)
    // A000-BFFF     8 KiB External RAM
    // C000-CFFF     4 KiB Work RAM (WRAM)
    // D000-DFFF     4 KiB Work RAM (WRAM)
    // E000-FDFF     Mirror of C000~DDFF (ECHO RAM)
    // FE00-FE9F     Object Attribute Memory (OAM)
    // FEA0-FEFF     Not Usable
    // FF00-FF7F     I/O Registers
    // FF80-FFFE     High RAM (HRAM)
    // FFFF-FFFF     Interrup Enable Register (IE)
    uint8_t memory[0xFFFF];

    uint8_t button_a;
    uint8_t button_b;
    uint8_t dpad_up;
    uint8_t dpad_down;
    uint8_t dpad_left;
    uint8_t dpad_right;

    double elapsed_ms;  // Milliseconds elapsed since the start
    double timer_sec;   // Timer for counting seconds
    double timer_clock;
} GameBoy;

typedef struct RomHeader {
    uint8_t entry[4];
    uint8_t logo[48];
    char title[16];
} RomHeader;

typedef enum Reg8 {
    REG_B = 0,
    REG_C = 1,
    REG_D = 2,
    REG_E = 3,
    REG_H = 4,
    REG_L = 5,
    REG_HL_MEM = 6,
    REG_A = 7,
    REG_COUNT,
} Reg8;

typedef enum Reg16 {
    REG_BC = 0,
    REG_DE = 1,
    REG_HL = 2,
    REG_SP = 3,
} Reg16;

typedef enum Flag {
    Flag_NZ = 0,
    Flag_Z  = 1,
    Flag_NC = 2,
    Flag_C  = 3,
    Flag_N  = 4,
    Flag_H  = 5,
    //FLAG_COUNT,
} Flag;

uint8_t *read_entire_file(const char *path, size_t *size);
bool get_command(GameBoy *gb);

void gb_dump(GameBoy *gb)
{
    uint8_t flags = gb->AF & 0xff;
    printf("AF = 0x%04X (A = 0x%02X (%d), Flags: Z: %d, C: %d, H: %d, N: %d)\n",
        gb->AF, gb->AF >> 8, gb->AF >> 8,
        (flags & 0x80) > 0,
        (flags & 0x10) > 0,
        (flags & 0x20) > 0,
        (flags & 0x40) > 0);
    printf("BC = 0x%04X (B = 0x%02X, C = 0x%02X)\n", gb->BC, gb->BC >> 8, gb->BC & 0xff);
    printf("DE = 0x%04X (D = 0x%02X, E = 0x%02X)\n", gb->DE, gb->DE >> 8, gb->DE & 0xff);
    printf("HL = 0x%04X (H = 0x%02X, L = 0x%02X)\n", gb->HL, gb->HL >> 8, gb->HL & 0xff);
    printf("SP = 0x%04X, PC = 0x%04X, IME = %d\n", gb->SP, gb->PC, gb->IME);
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

void gb_set_reg(GameBoy *gb, uint8_t reg, uint8_t value)
{
    // reg (0-7): B, C, D, E, H, L, (HL), A
    switch (reg) {
        case 0:
            gb->BC &= 0x00ff;
            gb->BC |= (value << 8);
            break;
        case 1:
            gb->BC &= 0xff00;
            gb->BC |= (value << 0);
            break;
        case 2:
            gb->DE &= 0x00ff;
            gb->DE |= (value << 8);
            break;
        case 3:
            gb->DE &= 0xff00;
            gb->DE |= (value << 0);
            break;
        case 4:
            gb->HL &= 0x00ff;
            gb->HL |= (value << 8);
            break;
        case 5:
            gb->HL &= 0xff00;
            gb->HL |= (value << 0);
            break;
        case 6:
            gb->memory[gb->HL] = value;
            break;
        case 7:
            gb->AF &= 0x00ff;
            gb->AF |= (value << 8);
            break;
        default:
            assert(0 && "Invalid register");
    }
}

uint8_t gb_get_reg(GameBoy *gb, uint8_t reg)
{
    // reg (0-7): B, C, D, E, H, L, (HL), A
    switch (reg) {
        case 0:
            return (gb->BC >> 8);
        case 1:
            return (gb->BC & 0xff);
        case 2:
            return (gb->DE >> 8);
        case 3:
            return (gb->DE & 0xff);
        case 4:
            return (gb->HL >> 8);
        case 5:
            return (gb->HL & 0xff);
        case 6:
            return gb->memory[gb->HL];
        case 7:
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
    } else if (b == 0xC9) {
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
    } else if (b == 0xFE) {
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

void gb_exec(GameBoy *gb, Inst inst)
{
    printf("%04X:  ", gb->PC);
    uint8_t b = inst.data[0];
    // 1-byte instructions
    if (inst.size == 1) {
        if (b == 0x00) {
            printf("NOP\n");
            gb->PC += inst.size;
        } else if (b == 0x09 || b == 0x19 || b == 0x29 || b == 0x39) {
            Reg16 src = (b >> 4) & 0x3;
            printf("ADD HL,%s\n", gb_reg16_to_str(src));
            int res = gb_get_reg16(gb, REG_HL) + gb_get_reg16(gb, src);
            gb_set_reg16(gb, REG_HL, res);
            // TODO: flags
            gb->PC += inst.size;
        } else if ( // INC reg
            b == 0x04 || b == 0x14 || b == 0x24 || b == 0x34 ||
            b == 0x0C || b == 0x1C || b == 0x2C || b == 0x3C
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            printf("INC %s\n", gb_reg_to_str(reg));
            gb_set_reg(gb, reg, gb_get_reg(gb, reg) + 1);
            gb->PC += inst.size;
        } else if ( // DEC reg
            b == 0x05 || b == 0x15 || b == 0x25 || b == 0x35 ||
            b == 0x0D || b == 0x1D || b == 0x2D || b == 0x3D
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            printf("%04X: DEC %s\n", gb->PC, gb_reg_to_str(reg));
            int res = gb_get_reg(gb, reg) - 1;
            gb_set_reg(gb, reg, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 1);
            //gb_set_flag(gb, Flag_H, ???);
            gb->PC += inst.size;
        } else if (b == 0x0A || b == 0x1A) {
            Reg16 reg = (b >> 4) & 0x3;
            printf("LD A,(%s)\n", gb_reg16_to_str(reg));
            uint8_t value = gb->memory[gb_get_reg16(gb, reg)];
            gb_set_reg(gb, REG_A, value);
            gb->PC += inst.size;
        } else if (b == 0x02 || b == 0x12) {
            Reg16 reg = (b >> 4) & 0x3;
            printf("LD (%s),A\n", gb_reg16_to_str(reg));
            gb->memory[gb_get_reg16(gb, reg)] = gb_get_reg(gb, REG_A);
            gb->PC += inst.size;
        } else if (b == 0x22 || b == 0x32) {
            printf("LD (HL%c),A\n", b == 0x22 ? '+' : '-');
            gb->memory[gb->HL] = gb_get_reg(gb, REG_A);
            if (b == 0x22) gb->HL += 1;
            else gb->HL -= 1;
            gb->PC += inst.size;
        } else if (b == 0x0A || b == 0x1A) {
            Reg16 reg = (b >> 4) & 0x3;
            printf("LD A,(%s)\n", gb_reg16_to_str(reg));
            gb_set_reg(gb, REG_A, gb->memory[gb_get_reg16(gb, reg)]);
            gb->PC += inst.size;
        } else if (b == 0x2A || b == 0x3A) {
            printf("LD A,(HL%c)\n", b == 0x22 ? '+' : '-');
            gb_set_reg(gb, REG_A, gb->memory[gb->HL]);
            if (b == 0x22) gb->HL += 1;
            else gb->HL -= 1;
            gb->PC += inst.size;
        } else if (b == 0x03 || b == 0x13 || b == 0x23 || b == 0x33) { // INC reg16
            Reg16 reg = (b >> 4) & 0x3;
            printf("INC %s\n", gb_reg16_to_str(reg));
            gb_set_reg16(gb, reg, gb_get_reg16(gb, reg) + 1);
            gb->PC += inst.size;
        } else if (b == 0x0B || b == 0x1B || b == 0x2B || b == 0x3B) {
            Reg16 reg = (b >> 4) & 0x3;
            printf("DEC %s\n", gb_reg16_to_str(reg));
            gb_set_reg16(gb, reg, gb_get_reg16(gb, reg) - 1);
            gb->PC += inst.size;
        } else if (b == 0x0F) {
            printf("RRCA\n");
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t c = a & 1;
            uint8_t res = (a >> 1) | (c << 7);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, c);
            gb->PC += inst.size;
        } else if (b >= 0x40 && b <= 0x7F) {
            if (b == 0x76) {
                printf("HALT\n");
                return;
            }
            Reg8 src = b & 0x7;
            Reg8 dst = (b >> 3) & 0x7;
            printf("LD %s,%s\n", gb_reg_to_str(dst), gb_reg_to_str(src));
            uint8_t value = gb_get_reg(gb, src);
            gb_set_reg(gb, dst, value);
            gb->PC += inst.size;
        } else if (b >= 0x80 && b <= 0x87) {
            Reg8 reg = b & 0x7;
            printf("ADD A,%s\n", gb_reg_to_str(reg));
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t res = gb_get_reg(gb, reg) + a;
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0); // TODO
            gb_set_flag(gb, Flag_C, (res < a) ? 1 : 0);
            gb->PC += inst.size;
        } else if (b >= 0x88 && b <= 0x8F) {
            Reg8 reg = b & 0x7;
            printf("ADC A,%s\n", gb_reg_to_str(reg));
            uint8_t res = gb_get_flag(gb, Flag_C) + gb_get_reg(gb, REG_A) +
                gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            // TODO: Flags
            gb->PC += inst.size;
        } else if (b >= 0x90 && b <= 0x97) {
            Reg8 reg = b & 0x7;
            printf("SUB A,%s\n", gb_reg_to_str(reg));
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
            printf("SBC A,%s\n", gb_reg_to_str(reg));
            exit(1);
            int res = gb_get_flag(gb, Flag_C) + gb_get_reg(gb, REG_A) +
                gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            // TODO: Flags
            gb->PC += inst.size;
        } else if (b >= 0xB0 && b <= 0xB7) {
            Reg8 reg = b & 0x7;
            printf("OR %s\n", gb_reg_to_str(reg));
            uint8_t res = gb_get_reg(gb, REG_A) | gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, 0);
            gb->PC += inst.size;
        } else if (b >= 0xB8 && b <= 0xBF) {
            Reg8 reg = b & 0x7;
            printf("CP %s\n", gb_reg_to_str(reg));
            int a = (int)gb_get_reg(gb, REG_A);
            int n = (int)gb_get_reg(gb, reg);
            int res = a - n;
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 1);
            //gb_set_flag(gb, Flag_H, ???);
            gb_set_flag(gb, Flag_C, a < n ? 1 : 0);
            gb->PC += inst.size;
        } else if (b >= 0xA0 && b <= 0xA7) {
            Reg8 reg = b & 0x7;
            printf("AND %s\n", gb_reg_to_str(reg));
            uint8_t res = gb_get_reg(gb, REG_A) & gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 1);
            gb_set_flag(gb, Flag_C, 0);
            gb->PC += inst.size;
        } else if (b >= 0xA8 && b <= 0xAF) {
            Reg8 reg = b & 0x7;
            printf("XOR %s\n", gb_reg_to_str(reg));
            uint8_t res = gb_get_reg(gb, REG_A) ^ gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, 0);
            gb->PC += inst.size;
        } else if (b == 0xE2) {
            printf("LD (C),A -- LD(0xFF00+%02X),%02X\n", gb_get_reg(gb, REG_C), gb_get_reg(gb, REG_A));
            gb->memory[0xFF00 + gb_get_reg(gb, REG_C)] = gb_get_reg(gb, REG_A);
            gb->PC += inst.size;
        } else if (b == 0xF2) {
            printf("LD A,(C)\n");
            gb_set_reg(gb, REG_A, gb->memory[0xFF00 + gb_get_reg(gb, REG_C)]);
            gb->PC += inst.size;
        } else if (b == 0xF3 || b == 0xFB) {
            printf(b == 0xF3 ? "DI\n" : "EI\n");
            gb->IME = b == 0xF3 ? 0 : 1;
            gb->PC += inst.size;
        } else if (b == 0xC0 || b == 0xD0 || b == 0xC8 || b == 0xD8) {
            Flag f = (b >> 3) & 0x3;
            uint8_t low = gb->memory[gb->SP+0];
            uint8_t high = gb->memory[gb->SP+1];
            uint16_t addr = (high << 8) | low;
            printf("RET %s (address: 0x%04X)", gb_flag_to_str(f), addr);
            if (gb_get_flag(gb, f)) {
                printf("  Taken\n");
                gb->SP += 2;
                gb->PC = addr;
            } else {
                printf("  NOT Taken\n");
                gb->PC += inst.size;
            }
        } else if (b == 0xC9) {
            uint8_t low = gb->memory[gb->SP+0];
            uint8_t high = gb->memory[gb->SP+1];
            gb->SP += 2;
            uint16_t addr = (high << 8) | low;
            printf("RET (address: 0x%04X)\n", addr);
            gb->PC = addr;
        } else if (b == 0xC1 || b == 0xD1 || b == 0xE1 || b == 0xF1) {
            Reg16 reg = (b >> 4) & 0x3;
            printf("POP %s\n", gb_reg16_to_str(reg));
            uint8_t low = gb->memory[gb->SP+0];
            uint8_t high = gb->memory[gb->SP+1];
            gb_set_reg16(gb, reg, (high << 8) | low);
            gb->SP += 2;
            gb->PC += inst.size;
        } else if (b == 0xC5 || b == 0xD5 || b == 0xE5 || b == 0xF5) {
            Reg16 reg = (b >> 4) & 0x3;
            printf("PUSH %s\n", gb_reg16_to_str(reg));
            uint16_t value = gb_get_reg16(gb, reg);
            gb->SP -= 2;
            gb->memory[gb->SP+0] = (value & 0xff);
            gb->memory[gb->SP+1] = (value >> 8);
            gb->PC += inst.size;
        } else if (((b & 0xC0) == 0xC0) && ((b & 0x7) == 0x7)) { // RST
            assert(b != 0xC3);
            uint8_t n = ((b >> 3) & 0x7)*8;
            printf("RST %02XH\n", n);
            gb->SP -= 2;
            gb->memory[gb->SP+0] = (gb->PC & 0xff);
            gb->memory[gb->SP+1] = (gb->PC >> 8);
            gb->PC = n;
        } else if (b == 0xE9) {
            printf("JP (HL)\n");
            gb->PC = gb->memory[gb->HL];
        } else {
            printf("%02X\n", inst.data[0]);
            assert(0 && "Instruction not implemented");
        }
    }
    // 2-byte instructions
    else if (inst.size == 2 && b != 0xCB) {
        if (b == 0x06) {
            printf("LD B,0x%02X\n", inst.data[1]);
            gb_set_reg(gb, REG_B, inst.data[1]);
            gb->PC += inst.size;
        } else if (b == 0x10) {
            printf("STOP\n");
            exit(1);
        } else if (b == 0x18) {
            int r8 = inst.data[1] >= 0x80 ? (int8_t)inst.data[1] : inst.data[1];
            printf("JR %d\n", r8);
            gb->PC = (gb->PC + inst.size) + r8;
        } else if ( // LD reg,d8
            b == 0x06 || b == 0x16 || b == 0x26 || b == 0x36 ||
            b == 0x0E || b == 0x1E || b == 0x2E || b == 0x3E
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            printf("LD %s,0x%02X\n", gb_reg_to_str(reg), inst.data[1]);
            gb_set_reg(gb, reg, inst.data[1]);
            gb->PC += inst.size;
        } else if (b == 0x20 || b == 0x30 || b == 0x28 || b == 0x38) {
            Flag f = (b >> 3) & 0x3;
            printf("JR %s,0x%02X\n", gb_flag_to_str(f), inst.data[1]);
            if (gb_get_flag(gb, f)) {
                printf("Jump taken\n");
                int offset = inst.data[1] >= 0x80 ? (int8_t)inst.data[1] : inst.data[1];
                printf("Offset: %d\n", offset);
                gb->PC = (gb->PC + inst.size) + offset;
            } else {
                printf("Jump NOT taken\n");
                gb->PC += inst.size;
            }
        } else if (b == 0x20) {
            printf("JR NZ,0x%02X\n", inst.data[1]);
            if (!gb_get_zero_flag(gb)) {
                printf("Jump taken\n");
                int offset = inst.data[1] >= 0x80 ? (int8_t)inst.data[1] : inst.data[1];
                printf("Offset: %d\n", offset);
                gb->PC = (gb->PC + inst.size) + offset;
            } else {
                printf("Jump NOT taken\n");
                gb->PC += inst.size;
            }
        } else if (b == 0x2F) {
            printf("CPL\n");
            gb_set_reg(gb, REG_A, ~gb_get_reg(gb, REG_A));
            gb_set_flag(gb, Flag_N, 1);
            gb_set_flag(gb, Flag_H, 1);
            gb->PC += inst.size;
        } else if (b == 0xE0) {
            printf("LDH (FF00+%02X),A\n", inst.data[1]);
            uint8_t value = gb_get_reg(gb, REG_A);
            gb->memory[0xFF00 + inst.data[1]] = value;
            if (inst.data[1] == 0) {
                if (value == P1F_GET_BTN) {
                    printf("Requesting BTN state\n");
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
                    printf("Requesting DPAD state\n");
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
            printf("ADD A, 0x%02X\n", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) + inst.data[1];
            uint8_t c = res < gb_get_reg(gb, REG_A) ? 1 : 0;
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0); // TODO: compute H flag
            gb_set_flag(gb, Flag_C, c);
            gb->PC += inst.size;
        } else if (b == 0xD6) {
            printf("SUB A, 0x%02X\n", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) - inst.data[1];
            uint8_t c = gb_get_reg(gb, REG_A) < inst.data[1] ? 1 : 0;
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 1);
            gb_set_flag(gb, Flag_H, 0); // TODO: compute H flag
            gb_set_flag(gb, Flag_C, c);
            gb->PC += inst.size;
        } else if (b == 0xE6) {
            printf("AND A, 0x%02X\n", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) & inst.data[1];
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 1);
            gb_set_flag(gb, Flag_C, 0);
            gb->PC += inst.size;
        } else if (b == 0xE8) {
            printf("ADD SP,0x%02X\n", inst.data[1]);
            gb->SP += (int)inst.data[1];
            gb->PC += inst.size;
        } else if (b == 0xF0) {
            printf("LDH A,(FF00+%02X)\n", inst.data[1]);
            gb_set_reg(gb, REG_A, gb->memory[0xFF00 + inst.data[1]]);
            gb->PC += inst.size;
        } else if (b == 0xF6) {
            printf("OR A, 0x%02X\n", inst.data[1]);
            uint8_t res = gb_get_reg(gb, REG_A) | inst.data[1];
            gb_set_reg(gb, REG_A, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, 0);
            gb->PC += inst.size;
        } else if (b == 0xFE) {
            printf("CP 0x%02X\n", inst.data[1]);
            int a = (int)gb_get_reg(gb, REG_A);
            int n = (int)inst.data[1];
            int res = a - n;
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 1);
            gb_set_flag(gb, Flag_C, a < n ? 1 : 0);
            gb->PC += inst.size;
        } else {
            assert(0 && "Instruction not implemented");
        }
    }

    // Prefix CB
    else if (inst.size == 2 && inst.data[0] == 0xCB) {
        if (inst.data[1] >= 0x30 && inst.data[1] <= 0x37) {
            Reg8 reg = inst.data[1] & 0x7;
            printf("SWAP %s\n", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            gb_set_reg(gb, reg, ((value & 0xF) << 4) | ((value & 0xF0) >> 4));
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x38 && inst.data[1] <= 0x3F) {
            Reg8 reg = inst.data[1] & 0x7;
            printf("SRL %s\n", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t res = value >> 1;
            gb_set_reg(gb, reg, res);
            gb_set_flag(gb, Flag_Z, res == 0 ? 1 : 0);
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, value & 0x1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x80 && inst.data[1] <= 0xBF) {
            uint8_t b = (inst.data[1] >> 3) & 0x7;
            Reg8 reg = inst.data[1] & 0x7;
            printf("RES %d,%s\n", b, gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg) & ~(1 << b);
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
            printf("JP 0x%04X\n", n);
            gb->PC = n;
        } else if (b == 0x01 || b == 0x11 || b == 0x21 || b == 0x31) {
            Reg16 reg = b >> 4;
            printf("LD %s, 0x%04X\n", gb_reg16_to_str(reg), n);
            gb_set_reg16(gb, reg, n);
            gb->PC += inst.size;
        } else if (b == 0x08) {
            printf("LD (0x%04X),SP\n", n);
            gb->memory[n+0] = (gb->SP & 0xff);
            gb->memory[n+1] = (gb->SP >> 8);
            gb->PC += inst.size;
        } else if (b == 0xC2 || b == 0xCA || b == 0xD2 || b == 0xDA) {
            Flag f = (b >> 3) & 0x3;
            printf("JP %s,0x%04X", gb_flag_to_str(f), n);
            if (gb_get_flag(gb, f)) {
                printf("  Taken\n");
                gb->PC = n;
            } else {
                printf("  NOT Taken\n");
                gb->PC += inst.size;
            }
        } else if (b == 0xC4 || b == 0xD4 || b == 0xCC || b == 0xDC) {
            Flag f = (b >> 3) & 0x3;
            printf("CALL %s,0x%04X\n", gb_flag_to_str(f), n);
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
            printf("CALL 0x%04X\n", n);
            gb->SP -= 2;
            uint16_t ret_addr = gb->PC + inst.size;
            gb->memory[gb->SP+0] = (ret_addr & 0xff);
            gb->memory[gb->SP+1] = (ret_addr >> 8);
            gb->PC = n;
        } else if (b == 0xEA) {
            printf("LD (0x%04X),A\n", n);
            gb->memory[n] = gb_get_reg(gb, REG_A);
            gb->PC += inst.size;
        } else if (b == 0xFA) {
            printf("LD A,(0x%04X)\n", n);
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

void gb_render_tile(GameBoy *gb, SDL_Renderer *renderer, const uint8_t *tile, int xoffset, int yoffset, bool transparency)
{
    uint8_t bgp = gb->memory[rBGP]; // E4 - 11|10|01|00
    uint8_t bgp_tbl[] = {bgp >> 6, (bgp >> 4) & 3, (bgp >> 2) & 3, bgp & 3};

    for (int tile_row = 0; tile_row < TILE_PIXELS; tile_row++) {
        uint8_t low_bitplane = tile[tile_row*2+0];
        uint8_t high_bitplane = tile[tile_row*2+1];
        for (int tile_col = 0; tile_col < TILE_PIXELS; tile_col++) {
            uint8_t bit0 = (low_bitplane & 0x80) >> 7;
            uint8_t bit1 = (high_bitplane & 0x80) >> 7;
            low_bitplane <<= 1;
            high_bitplane <<= 1;

            uint8_t color_idx = (bit1 << 1) | bit0;
            uint8_t color = PALETTE[bgp_tbl[color_idx]];

            if (!transparency || color_idx != 0) {
                SDL_SetRenderDrawColor(renderer, color, color, color, 255);
                SDL_Rect rect = {.x = xoffset + tile_col*SCALE, .y = yoffset + tile_row*SCALE, .w = SCALE, .h = SCALE};
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }
}

bool gb_render(GameBoy *gb, SDL_Renderer *renderer)
{
    if ((gb->memory[rLCDC] & LCDCF_ON  /*0x80*/) == 0) return false;
    if ((gb->memory[rLCDC] & LCDCF_BGON/*0x01*/) == 0) return false;

    // Render the BG
    bool bg8000_mode = (gb->memory[rLCDC] & LCDCF_BG8000/*0x10*/);
    uint16_t bg_win_tile_data_offset = bg8000_mode ? 0x8000 : 0x9000;

    int yoffset = 0;
    int xoffset = 0;
    for (int row = 0; row < TILEMAP_ROWS/*VIEWPORT_ROWS*/; row++) {
        for (int col = 0; col < TILEMAP_COLS/*VIEWPORT_COLS*/; col++) {
            int tile_idx = bg8000_mode ?
                gb->memory[VRAM_TILEMAP + row*(TILEMAP_COLS) + col] :
                (int8_t)gb->memory[VRAM_TILEMAP + row*(TILEMAP_COLS) + col];

            const uint8_t *tile = gb->memory + bg_win_tile_data_offset + tile_idx*TILE_SIZE;
            gb_render_tile(gb, renderer, tile, xoffset, yoffset, false);
            xoffset += TILE_PIXELS*SCALE;
        }
        yoffset += TILE_PIXELS*SCALE;
        xoffset = 0;
    }

    // Render Window
    if ((gb->memory[rLCDC] & LCDCF_WINON/*0x20*/) != 0) {
        uint16_t win_tile_data_offset = (gb->memory[rLCDC] & LCDCF_WIN9C00/*0x40*/) ? 0x9C00 : 0x9800;
        int win_x = gb->memory[rWX] - 7;
        int win_y = gb->memory[rWY];
        yoffset = win_y*SCALE;
        xoffset = win_x*SCALE;

        for (int row = 0; row < VIEWPORT_ROWS; row++) {
            for (int col = 0; col < VIEWPORT_COLS; col++) {
                int tile_idx = gb->memory[win_tile_data_offset + row*(TILEMAP_COLS) + col];
                const uint8_t *tile = gb->memory + bg_win_tile_data_offset + tile_idx*TILE_SIZE;
                gb_render_tile(gb, renderer, tile, xoffset, yoffset, false);
                xoffset += TILE_PIXELS*SCALE;
            }
            yoffset += TILE_PIXELS*SCALE;
            xoffset = win_x*SCALE;
        }
    }

    // Render OBJ
    if ((gb->memory[rLCDC] & LCDCF_OBJON/*0x02*/) != 0) {
        for (int i = 0; i < OAM_COUNT; i++) {
            uint8_t *obj = gb->memory + 0xFE00 + i*4;
            int y = *(obj+0) - 16;
            int x = *(obj+1) - 8;
            int tile_id = *(obj+2);
            const uint8_t *tile = gb->memory + 0x8000 + tile_id*TILE_SIZE;
            gb_render_tile(gb, renderer, tile, x*SCALE, y*SCALE, true);
            //printf("OBJ[%2d] x: %d, y: %d, tileID: %d, attributes: 0x%02X\n",
            //    i, x, y, tileID, *(obj+3));
        }
    }

    return true;
}

void gb_render_logo(SDL_Renderer *renderer, int width, int height)
{
    // The Logo is 48x8 pixels
    int pixel_width = (width/2) / (12*4);
    int pixel_height = (height/2) / 8;
    int xstart = (width/2) - (24*pixel_width);
    int ystart = (height/2) - (4*pixel_height);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    int xoffset = xstart;
    int yoffset = ystart;
    for (int i = 0; i < 24; i++) {
        uint8_t b = NINTENDO_LOGO[i];
        uint8_t first_row = b >> 4;
        uint8_t second_row = b & 0xf;

        for (int j = 0; j < 4; j++) {
            if ((first_row & 0x8) != 0) {
                SDL_Rect rect = {.x = xoffset + j*pixel_width, .y = yoffset, .w = pixel_width, .h = pixel_height};
                SDL_RenderFillRect(renderer, &rect);
            }
            first_row <<= 1;
        }
        for (int j = 0; j < 4; j++) {
            if ((second_row & 0x8) != 0) {
                SDL_Rect rect = {.x = xoffset + j*pixel_width, .y = yoffset + pixel_height, .w = pixel_width, .h = pixel_height};
                SDL_RenderFillRect(renderer, &rect);
            }
            second_row <<= 1;
        }

        yoffset = yoffset + 2*pixel_height;
        if (i & 0x1) {
            yoffset = ystart;
            xoffset = xoffset + 4*pixel_width;
        }
    }

    // Second half
    xoffset = xstart;
    yoffset = ystart + pixel_height*4;
    for (int i = 24; i < 48; i++) {
        uint8_t b = NINTENDO_LOGO[i];
        uint8_t first_row = b >> 4;
        uint8_t second_row = b & 0xf;

        for (int j = 0; j < 4; j++) {
            if ((first_row & 0x8) != 0) {
                SDL_Rect rect = {.x = xoffset + j*pixel_width, .y = yoffset, .w = pixel_width, .h = pixel_height};
                SDL_RenderFillRect(renderer, &rect);
            }
            first_row <<= 1;
        }
        for (int j = 0; j < 4; j++) {
            if ((second_row & 0x8) != 0) {
                SDL_Rect rect = {.x = xoffset + j*pixel_width, .y = yoffset + pixel_height, .w = pixel_width, .h = pixel_height};
                SDL_RenderFillRect(renderer, &rect);
            }
            second_row <<= 1;
        }

        yoffset = yoffset + 2*pixel_height;
        if (i & 0x1) {
            yoffset = ystart + pixel_height*4;
            xoffset = xoffset + 4*pixel_width;
        }
    }
}


int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path to ROM>\n", argv[0]);
        exit(1);
    }

    bool show_tile_grid = false;

    step_debug = true;
    GameBoy gb = {0};
    gb_load_rom_file(&gb, argv[1]);

    //uint8_t buf[] = {0xEA, 0x94, 0xFF};
    //Inst inst = {.data = buf, .size = 3};
    //gb_exec(&gb, inst);
    //exit(1);

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Failed to initialize SDL\n");
        exit(1);
    }

    int width = SCALE*WIDTH; // + SCALE*WIDTH;
    int height = SCALE*HEIGHT; // + SCALE*HEIGHT;
    SDL_Window *window = SDL_CreateWindow("GameBoy Emulator", 0, 0, width, height, 0);
    if (!window) {
        fprintf(stderr, "Failed to create window\n");
        exit(1);
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if (!renderer) {
        fprintf(stderr, "Failed to create renderer\n");
        exit(1);
    }

    SDL_Event e;
    bool running = true;
    Uint64 counter_freq = SDL_GetPerformanceFrequency();
    printf("Performance Frequency: %ld\n", counter_freq);
    Uint64 start_counter = SDL_GetPerformanceCounter();
    Uint64 prev_counter = start_counter;
    while (running) {
        Uint64 curr_counter = SDL_GetPerformanceCounter();
        Uint64 delta_counter = curr_counter - prev_counter;
        prev_counter = curr_counter;
        double dt_ms = (double)delta_counter / (counter_freq / 1000.0);
        //printf("dt: %f ms\n", dt_ms);

        // Input
        // TODO: fix unresponsiveness
        SDL_PollEvent(&e);
        if (e.type == SDL_QUIT) {
            running = false;
        } else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
            switch (e.key.keysym.sym) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_g:
                    if (e.key.type == SDL_KEYDOWN) {
                        show_tile_grid = !show_tile_grid;
                    }
                    break;
                case SDLK_SPACE:
                    if (e.key.type == SDL_KEYDOWN) gb_dump(&gb);
                    break;
                case SDLK_s:
                    gb.button_a = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                case SDLK_a:
                    gb.button_b = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                case SDLK_UP:
                    gb.dpad_up = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                case SDLK_DOWN:
                    gb.dpad_down = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                case SDLK_RIGHT:
                    gb.dpad_right = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                case SDLK_LEFT:
                    gb.dpad_left = e.key.type == SDL_KEYDOWN ? 1 : 0;
                    break;
                default:
                    break;
            }
        }

        // Update
        for (int i = 0; i < 20; i++) {
            gb_tick(&gb, dt_ms);
        }

        // Render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (gb_render(&gb, renderer)) {
            //gb_render_logo(renderer, width, height);

            if (show_tile_grid) {
                SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
                for (int i = 0; i <= 32; i++) {
                    int y = i*TILE_PIXELS*SCALE;
                    SDL_RenderDrawLine(renderer, 0, y, 32*TILE_PIXELS*SCALE, y);
                }
                for (int i = 0; i <= 32; i++) {
                    int x = i*TILE_PIXELS*SCALE;
                    SDL_RenderDrawLine(renderer, x, 0, x, 32*TILE_PIXELS*SCALE);
                }
            }

            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            int thickness = 5;
            SDL_Rect r = {0, HEIGHT*SCALE, WIDTH*SCALE, thickness};
            SDL_RenderFillRect(renderer, &r);
            r = (SDL_Rect){WIDTH*SCALE, 0, thickness, HEIGHT*SCALE};
            SDL_RenderFillRect(renderer, &r);

            SDL_RenderPresent(renderer);
        }
    }

    return 0;
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
