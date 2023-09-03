#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WIDTH  160  // 20 tiles
#define HEIGHT 144  // 18 tiles

const uint8_t NINTENDO_LOGO[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

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
    printf("SP = 0x%04X, PC = 0x%04X\n", gb->SP, gb->PC);
}

void gb_set_zero_flag(GameBoy *gb)
{
    // A           F
    //             Z NH C
    // 0000 0000   0000 0000
    gb->AF |= 0x0080;
}

void gb_set_reg(GameBoy *gb, uint8_t reg, uint8_t value)
{
    // reg (0-7): B, C, D, E, H, L, (HL), A
    switch (reg) {
        case 0:
            gb->BC |= (value << 8);
            break;
        case 1:
            gb->BC |= (value << 0);
            break;
        case 2:
            gb->DE |= (value << 8);
            break;
        case 3:
            gb->DE |= (value << 0);
            break;
        case 4:
            gb->HL |= (value << 8);
            break;
        case 5:
            gb->HL |= (value << 0);
            break;
        case 6:
            assert(0 && "(HL) not implemented");
        case 7:
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

void gb_exec(GameBoy *gb, uint8_t *inst, size_t inst_size)
{
    if (inst_size == 1) {
        if (inst[0] == 0x00) {
            // NOP
        } else if ((inst[0] >> 4) == 0xA) {
            // A = A xor r
            int reg = inst[0] & 0xf;
            assert(reg >= 8 && reg <= 0xf);

            uint8_t A = (gb->AF >> 8);
            uint8_t r = gb_get_reg(gb, reg-8);
            gb_set_reg(gb, reg-8, A ^ r);
            if ((A ^ r) == 0) {
                gb_set_zero_flag(gb);
            }
        } else {
            assert(0 && "Instruction not implemented");
        }
    } else if (inst_size == 3) {
        uint16_t n = inst[1] | (inst[2] << 8);
        if (inst[0] == 0xC3) { // JP a16
            gb->PC = n;
        } else {
            assert((inst[0] & 0xf) == 1);
            int reg = inst[0] >> 4;
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
        }
    }
}

void gb_load_rom(GameBoy *gb, uint8_t *raw, size_t size)
{
    assert(size > 0x14F && size <= 0xFFFF);
    RomHeader *header = (RomHeader*)(raw + 0x100);
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
    printf("\n\n");

    memcpy(gb->memory, raw, size);

    gb->PC = 0x100;
}


void gb_load_rom_file(GameBoy *gb, const char *path)
{
    printf("Loading ROM \"%s\"...\n", path);
    size_t size;
    uint8_t *raw = read_entire_file(path, &size);
    gb_load_rom(gb, raw, size);
    free(raw);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path to ROM>\n", argv[0]);
        exit(1);
    }

    GameBoy gb = {0};
    gb_load_rom_file(&gb, argv[1]);

    {
    uint8_t inst[] = {0x00}; // NOP
    gb_exec(&gb, inst, 1);
    }

    {
    uint8_t inst[] = {0xC3, 0x50, 0x01}; // JP $0150
    gb_exec(&gb, inst, 3);
    }

    {
    gb.BC = 0xFFFF;
    uint8_t inst[] = {0xA8}; // XOR B
    gb_exec(&gb, inst, 1);
    }

    {
    //uint8_t inst[] = {0x0A}; // LD A,(BC)
    //gb_exec(&gb, inst, 1);
    }

    {
    uint8_t inst[] = {0x21, 0x69, 0x00}; // LD HL, 0x69
    gb_exec(&gb, inst, 3);
    }

    gb_dump(&gb);

    return 0;
}
