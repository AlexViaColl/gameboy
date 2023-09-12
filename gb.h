#ifndef GB_H
#define GB_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#define WIDTH  160 // 20 tiles
#define HEIGHT 144 // 18 tiles

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

    int (*printf)(const char *fmt, ...);
} GameBoy;

typedef struct Inst {
    uint8_t *data;
    size_t size;
} Inst;

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


void gb_load_rom_file(GameBoy *gb, const char *path);
void gb_load_rom(GameBoy *gb, uint8_t *raw, size_t size);
Inst gb_fetch_inst(GameBoy *gb);
void gb_exec(GameBoy *gb, Inst inst);
void gb_tick(GameBoy *gb, double dt_ms);
void gb_dump(GameBoy *gb);

uint8_t gb_get_flag(GameBoy *gb, Flag flag);
void gb_set_flag(GameBoy *gb, Flag flag, uint8_t value);

uint8_t gb_get_reg(GameBoy *gb, Reg8 reg);
uint16_t gb_get_reg16(GameBoy *gb, Reg16 reg);
void gb_set_reg(GameBoy *gb, Reg8 reg, uint8_t value);
void gb_set_reg16(GameBoy *gb, Reg16 reg, uint16_t value);

#endif // GB_H
