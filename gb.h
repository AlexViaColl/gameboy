#ifndef GB_H
#define GB_H

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
} GameBoy;

typedef struct Inst {
    uint8_t *data;
    size_t size;
} Inst;

void gb_load_rom_file(GameBoy *gb, const char *path);
void gb_load_rom(GameBoy *gb, uint8_t *raw, size_t size);
Inst gb_fetch_inst(GameBoy *gb);
void gb_exec(GameBoy *gb, Inst inst);
void gb_tick(GameBoy *gb, double dt_ms);
void gb_dump(GameBoy *gb);

#endif // GB_H
