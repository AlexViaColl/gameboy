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

const uint8_t NINTENDO_LOGO[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

typedef struct Inst {
    uint8_t *data;
    size_t size;
} Inst;

typedef struct GameBoy {
    // CPU freq:        4.194304 MHz
    // Horizontal sync: 9.198 KHz
    // Vertical sync:   59.73 Hz
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
    Flag_Z = 0,
    Flag_N = 1,
    Flag_H = 2,
    Flag_C = 3,
    //FLAG_COUNT,
} Flag;

uint8_t *read_entire_file(const char *path, size_t *size);

void gb_dump(GameBoy *gb)
{
    uint8_t flags = gb->AF & 0xff;
    printf("AF = 0x%04X (A = 0x%02X, Flags: Z: %d, C: %d, H: %d, N: %d)\n",
        gb->AF, gb->AF >> 8,
        (flags & 0x80) > 0,
        (flags & 0x10) > 0,
        (flags & 0x20) > 0,
        (flags & 0x40) > 0);
    printf("BC = 0x%04X (B = 0x%02X, C = 0x%02X)\n", gb->BC, gb->BC >> 8, gb->BC & 0xff);
    printf("DE = 0x%04X (D = 0x%02X, E = 0x%02X)\n", gb->DE, gb->DE >> 8, gb->DE & 0xff);
    printf("HL = 0x%04X (H = 0x%02X, L = 0x%02X)\n", gb->HL, gb->HL >> 8, gb->HL & 0xff);
    printf("SP = 0x%04X, PC = 0x%04X, IME = %d\n", gb->SP, gb->PC, gb->IME);
}

void gb_set_flag(GameBoy *gb, Flag flag, uint8_t value)
{
    // A           F
    //             Z NH C
    // 0000 0000   0000 0000
    switch (flag) {
        case Flag_Z:
            if (value) {
                gb->AF |= 0x0080;
            } else {
                gb->AF &= ~0x0080;
            }
            break;
        case Flag_N:
            if (value) {
                gb->AF |= 0x0020;
            } else {
                gb->AF &= ~0x0020;
            }
            break;
        case Flag_H:
            if (value) {
                gb->AF |= 0x0010;
            } else {
                gb->AF &= ~0x0010;
            }
            break;
        case Flag_C:
            if (value) {
                gb->AF |= 0x0008;
            } else {
                gb->AF &= ~0x0008;
            }
            break;
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
            assert(0 && "(HL) not implemented");
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
    uint8_t first = gb->memory[gb->PC];
    uint8_t *data = &gb->memory[gb->PC];

    // TODO: Use a Table-driven approach to determine instruction size!!!
    // 1-byte instructions
    if (first == 0x00) {
        return (Inst){.data = data, .size = 1};
    } else if ( // DEC reg8
        first == 0x05 || first == 0x15 || first == 0x25 || first == 0x35 ||
        first == 0x0D || first == 0x1D || first == 0x2D || first == 0x3D
    ) {
        return (Inst){.data = data, .size = 1};
    } else if ( // INC reg8
        first == 0x04 || first == 0x14 || first == 0x24 || first == 0x34 ||
        first == 0x0C || first == 0x1C || first == 0x2C || first == 0x3C
    ) {
        return (Inst){.data = data, .size = 1};
    } else if (first == 0x0B || first == 0x1B || first == 0x2B || first == 0x3B) {
        return (Inst){.data = data, .size = 1};
    } else if (first == 0x22 || first == 0x32 || first == 0x2A || first == 0x3A) {
        return (Inst){.data = data, .size = 1};
    } else if (first == 0x32) {
        return (Inst){.data = data, .size = 1};
    } else if (first >= 0x40 && first <= 0x7F) {
        return (Inst){.data = data, .size = 1};
    } else if (first >= 0x80 && first <= 0xBF) {
        return (Inst){.data = data, .size = 1};
    } else if (first == 0xAF) {
        return (Inst){.data = data, .size = 1};
    } else if (first == 0xE2) {
        return (Inst){.data = data, .size = 1};
    } else if (first == 0xF3) {
        return (Inst){.data = data, .size = 1};
    }

    // 2-byte instructions
    else if ( // LD reg,d8
        first == 0x06 || first == 0x16 || first == 0x26 || first == 0x36 ||
        first == 0x0E || first == 0x1E || first == 0x2E || first == 0x3E
    ) {
        return (Inst){.data = data, .size = 2};
    } else if (first == 0x0E) {
        return (Inst){.data = data, .size = 2};
    } else if (first == 0x20 || first == 0x30 || first == 0x28 || first == 0x38) {
        // 0x20 (NZ), 0x30 (NC), 0x28 (Z), 0x38 (C)
        return (Inst){.data = data, .size = 2};
    } else if (first == 0xE0 || first == 0xF0) {
        return (Inst){.data = data, .size = 2};
    } else if (first == 0xFE) {
        return (Inst){.data = data, .size = 2};
    }

    // 3-byte instructions
    else if (first == 0x01 || first == 0x11 || first == 0x21 || first == 0x31) {
        return (Inst){.data = data, .size = 3};
    } else if (first == 0xC3) {
        return (Inst){.data = data, .size = 3};
    } else if (first == 0xCD) {
        return (Inst){.data = data, .size = 3};
    } else if (first == 0xEA || first == 0xFA) {
        return (Inst){.data = data, .size = 3};
    }

    printf("%02X\n", first);
    assert(0 && "Not implemented");
}

void gb_exec(GameBoy *gb, Inst inst)
{
    uint8_t first = inst.data[0];
    // 1-byte instructions
    if (inst.size == 1) {
        if (inst.data[0] == 0x00) {
            // NOP
            printf("NOP\n");
            gb->PC += inst.size;
        } else if ( // INC reg
            first == 0x04 || first == 0x14 || first == 0x24 || first == 0x34 ||
            first == 0x0C || first == 0x1C || first == 0x2C || first == 0x3C
        ) {
            Reg8 reg = (first >> 3) & 0x7;
            printf("INC %s\n", gb_reg_to_str(reg));
            gb_set_reg(gb, reg, gb_get_reg(gb, reg) + 1);
            gb->PC += inst.size;
        } else if ( // DEC reg
            first == 0x05 || first == 0x15 || first == 0x25 || first == 0x35 ||
            first == 0x0D || first == 0x1D || first == 0x2D || first == 0x3D
        ) {
            Reg8 reg = (first >> 3) & 0x7;
            printf("DEC %s\n", gb_reg_to_str(reg));
            gb_set_reg(gb, reg, gb_get_reg(gb, reg) - 1);
            gb->PC += inst.size;
        } else if (first == 0x0B || first == 0x1B || first == 0x2B || first == 0x3B) {
            Reg16 reg = (first >> 4) & 0x3;
            printf("DEC %s\n", gb_reg16_to_str(reg));
            gb_set_reg16(gb, reg, gb_get_reg16(gb, reg) - 1);
            gb->PC += inst.size;
        } else if (
            first == 0x02 || first == 0x12 || first == 0x0A || first == 0x1A ||
            first == 0x22 || first == 0x32 || first == 0x2A || first == 0x3A
        ) {
            uint8_t reg_idx = (first >> 4) & 0x3;
            uint8_t read_write = (first >> 3) & 0x1;
            switch (reg_idx) {
                case 0:
                case 1:
                    if (read_write == 0) {
                        printf("LD (%s),A\n", gb_reg16_to_str(reg_idx));
                        gb->memory[gb_get_reg16(gb, reg_idx)] = gb_get_reg(gb, REG_A);
                    } else {
                        printf("LD A,(%s)\n", gb_reg16_to_str(reg_idx));
                        gb_set_reg(gb, REG_A, gb->memory[gb_get_reg16(gb, reg_idx)]);
                    }
                    break;
                case 2:
                    if (read_write == 0) {
                        printf("LD (HL+),A\n");
                        gb->memory[gb_get_reg16(gb, REG_HL)] = gb_get_reg(gb, REG_A);
                    } else {
                        printf("LD A,(HL+)\n");
                        gb_set_reg(gb, REG_A, gb->memory[gb_get_reg16(gb, REG_HL)]);
                    }
                    gb_set_reg16(gb, REG_HL, gb_get_reg16(gb, REG_HL) + 1);
                    break;
                case 3:
                    if (read_write == 0) {
                        printf("LD (HL-),A\n");
                        gb->memory[gb_get_reg16(gb, REG_HL)] = gb_get_reg(gb, REG_A);
                    } else {
                        printf("LD A,(HL-)\n");
                        gb_set_reg(gb, REG_A, gb->memory[gb_get_reg16(gb, REG_HL)]);
                    }
                    gb_set_reg16(gb, REG_HL, gb_get_reg16(gb, REG_HL) - 1);
                    break;
            }
            gb->PC += inst.size;
        } else if (inst.data[0] == 0x32) {
            printf("LD (HL-),A\n");
            gb->memory[gb->HL--] = gb_get_reg(gb, REG_A);
            gb->PC += inst.size;
        } else if (first >= 0x40 && first <= 0x7F) {
            if (first == 0x76) {
                printf("HALT\n");
                exit(0);
            }
            Reg8 src = first & 0x7;
            Reg8 dst = (first >> 3) & 0x7;
            printf("LD %s,%s\n", gb_reg_to_str(dst), gb_reg_to_str(src));
            uint8_t value = gb_get_reg(gb, src);
            gb_set_reg(gb, dst, value);
            gb->PC += inst.size;
        } else if (first >= 0xB0 && first <= 0xB7) {
            Reg8 reg = first & 0x7;
            printf("OR %s\n", gb_reg_to_str(reg));
            gb_set_reg(gb, REG_A, gb_get_reg(gb, reg) | gb_get_reg(gb, REG_A));
            if (gb_get_reg(gb, REG_A) == 0) {
                gb_set_flag(gb, Flag_Z, 1);
            } else {
                gb_set_flag(gb, Flag_Z, 0);
            }
            gb_set_flag(gb, Flag_N, 0);
            gb_set_flag(gb, Flag_H, 0);
            gb_set_flag(gb, Flag_C, 0);
            gb->PC += inst.size;
        } else if (inst.data[0] == 0x77) {
            printf("LD (HL),A\n");
            assert(0);
            gb->memory[gb->HL] = gb_get_reg(gb, REG_A);
            gb->PC += inst.size;
        } else if ((inst.data[0] >> 4) == 0xA) {
            // A = A xor r
            printf("XOR ??\n");
            int reg = inst.data[0] & 0xf;
            assert(reg >= 8 && reg <= 0xf);

            uint8_t A = (gb->AF >> 8);
            uint8_t r = gb_get_reg(gb, reg-8);
            gb_set_reg(gb, reg-8, A ^ r);
            if ((A ^ r) == 0) {
                gb_set_zero_flag(gb);
            }
            gb->PC += inst.size;
        } else if (first == 0xE2) {
            printf("LD (C),A\n");
            gb->memory[gb_get_reg(gb, REG_C)] = gb_get_reg(gb, REG_A);
            gb->PC += inst.size;
        } else if (first == 0xF3) {
            printf("DI\n");
            gb->IME = 0;
            gb->PC += inst.size;
        } else {
            printf("%02X\n", inst.data[0]);
            assert(0 && "Instruction not implemented");
        }
    }
    // 2-byte instructions
    else if (inst.size == 2) {
        if (inst.data[0] == 0x06) {
            printf("LD B,0x%02X\n", inst.data[1]);
            gb_set_reg(gb, REG_B, inst.data[1]);
            gb->PC += inst.size;
        } else if ( // LD reg,d8
            first == 0x06 || first == 0x16 || first == 0x26 || first == 0x36 ||
            first == 0x0E || first == 0x1E || first == 0x2E || first == 0x3E
        ) {
            Reg8 reg = (first >> 3) & 0x7;
            printf("LD %s,0x%02X\n", gb_reg_to_str(reg), inst.data[1]);
            gb_set_reg(gb, reg, inst.data[1]);
            gb->PC += inst.size;
        } else if (inst.data[0] == 0x20) {
            printf("JR NZ,0x%02X\n", inst.data[1]);
            if (!gb_get_zero_flag(gb)) {
                printf("Jump taken\n");
                gb->PC += inst.data[1]; // TODO: Handle negative values!!!
            } else {
                printf("Jump NOT taken\n");
                gb->PC += inst.size;
            }
        } else if (inst.data[0] == 0xE0) {
            printf("LDH (FF00+%02X),A\n", inst.data[1]);
            gb->memory[0xFF00 + inst.data[1]] = gb_get_reg(gb, REG_A);
            gb->PC += inst.size;
        } else if (inst.data[0] == 0xF0) {
            printf("LDH A,(FF00+%02X)\n", inst.data[1]);
            gb_set_reg(gb, REG_A, gb->memory[0xFF00 + inst.data[1]]);
            gb->PC += inst.size;
        } else if (inst.data[0] == 0xFE) {
            printf("CP 0x%02X\n", inst.data[1]);
            // Z 1 H C
            int res = (int)gb_get_reg(gb, REG_A) - (int)inst.data[1];
            if (res == 0) gb_set_flag(gb, Flag_Z, 1);
            gb_set_flag(gb, Flag_N, 1);
            if (res < 0) gb_set_flag(gb, Flag_C, 1);
            gb->PC += inst.size;
        } else {
            assert(0 && "Instruction not implemented");
        }
    }
    // 3-byte instructions
    else if (inst.size == 3) {
        uint16_t n = inst.data[1] | (inst.data[2] << 8);
        if (inst.data[0] == 0xC3) { // JP a16
            printf("JP 0x%04X\n", n);
            gb->PC = n;
        } else if (first == 0x01 || first == 0x11 || first == 0x21 || first == 0x31) {
            Reg16 reg = first >> 4;
            printf("LD %s, 0x%04X\n", gb_reg16_to_str(reg), n);
            gb_set_reg16(gb, reg, n);
            gb->PC += inst.size;
        } else if (first == 0xCD) {
            printf("CALL 0x%04X\n", n);
            gb->SP -= 2;
            gb->memory[gb->SP] = gb->PC;
            gb->PC = n;
        } else if (first == 0xEA) {
            printf("LD (0x%04X),A\n", n);
            gb->memory[n] = gb_get_reg(gb, REG_A);
            gb->PC += inst.size;
        } else {
            // Some kind of load??
            assert((inst.data[0] & 0xf) == 1);
            int reg = inst.data[0] >> 4;
            switch (reg) {
                case 0:
                    gb->BC = n;
                    break;
                case 1:
                    gb->DE = n;
                    break;
                case 2:
                    gb->HL = n;
                    break;
                case 3:
                    gb->SP = n;
                    break;
                default:
                    assert(0 && "Unreachable");
            }
            gb->PC += inst.size;
        }
    }
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
}

void gb_load_rom_file(GameBoy *gb, const char *path)
{
    printf("Loading ROM \"%s\"...\n", path);
    size_t size;
    uint8_t *raw = read_entire_file(path, &size);
    gb_load_rom(gb, raw, size);
    free(raw);
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

    GameBoy gb = {0};
    gb_load_rom_file(&gb, argv[1]);

#if 0
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "Failed to initialize SDL\n");
        exit(1);
    }

    int width = SCALE*WIDTH;
    int height = SCALE*HEIGHT;
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
    while (running) {
        SDL_PollEvent(&e);
        if (e.type == SDL_QUIT) {
            running = false;
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        gb_render_logo(renderer, width, height);

        SDL_RenderPresent(renderer);
    }
#endif

    while (true) {
        printf("%04X: ", gb.PC);
        Inst inst = gb_fetch_inst(&gb);
        printf("raw: %02X", inst.data[0]);
        for (size_t i = 1; i < inst.size; i++) {
            printf(" %02X", inst.data[i]);
        }
        printf("\n");
        gb_exec(&gb, inst);
        gb_dump(&gb);
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
