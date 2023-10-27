#ifndef GB_H
#define GB_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float  f32;
typedef double f64;

typedef u32 Color;
extern const Color PALETTE[4];

#define BIT_CLR(x, index) (x) &= ~(1 << (index))
#define BIT_SET(x, index) (x) |=  (1 << (index))
#define BIT_ASSIGN(x, index, value) do {                  \
    if (value) BIT_SET(x, index); else BIT_CLR(x, index); \
} while (false)

#define WIDTH  160 // 20 tiles
#define HEIGHT 144 // 18 tiles

#define CPU_FREQ    4194304.0 // 4.19 MHz
#define MCYCLE_MS   (1000.0 / 1048576.0)
#define VSYNC       59.73
#define HSYNC       9198.0  // 9.198 KHz

#define DOTS_PER_FRAME      70224
#define DOTS_PER_SCANLINE   456     // 144 frame scanlines + 10 vblank scanlines = 153
#define DOTS_TO_MS(dots) (f64)((1000.0 / (VSYNC*DOTS_PER_FRAME))*(dots))
#define MS_TO_DOTS(ms)   (u64)(((VSYNC*DOTS_PER_FRAME) / 1000.0)*(ms))

// 154 scanlines (144 lines displayed and 10 lines of VBlank)
// 4 dots per cycle (~4.194 MHz, 1 frame takes 70224 dots)
// PPU modes:
// Mode 2 - OAM Scan        - 80 dots
// Mode 3 - Drawing Pixels  - 172-289 dots
// Mode 0 - HBlank          -  87-204 dots
// Mode 1 - VBlank          - 4560 dots (10 lines)

#define SCRN_X      160 // Width of screen in pixels
#define SCRN_Y      144 // Height of screen in pixels
#define SCRN_X_B    20  // Width of screen in bytes
#define SCRN_Y_B    18  // Height of screen in bytes

#define SCRN_VX     256 // Virtual width of screen in pixels
#define SCRN_VY     256 // Virtual height of screen in pixels
#define SCRN_VX_B   32  // Virtual width of screen in bytes
#define SCRN_VY_B   32  // Virtual height of screen in bytes

#define TILE_SIZE 16
#define MAX_TILE_IDS (128*3)
#define TILEMAP_ROWS 32
#define TILEMAP_COLS 32
#define VIEWPORT_COLS 20
#define VIEWPORT_ROWS 18
#define TILE_PIXELS 8
#define OAM_COUNT 40

#define rP1     0xFF00 // Joypad
#define rSB     0xFF01 // Serial transfer data
#define rSC     0xFF02 // Serial transfer control
#define rDIV    0xFF04 // Divider register
#define rTIMA   0xFF05 // Timer counter
#define rTMA    0xFF06 // Timer modulo
#define rTAC    0xFF07 // Timer control
#define rIF     0xFF0F // Interrupt flag

#define rNR10   0xFF10 // Sound channel 1 sweep
#define rNR11   0xFF11 // Sound channel 1 length timer & duty cycle
#define rNR12   0xFF12 // Sound channel 1 volume & envelope
#define rNR13   0xFF13 // Sound channel 1 period low
#define rNR14   0xFF14 // Sound channel 1 period high & control

//      rNR20   0xFF16 // Sound channel 2 unused
#define rNR21   0xFF16 // Sound channel 2 length timer & duty cycle
#define rNR22   0xFF17 // Sound channel 2 volume & envelope
#define rNR23   0xFF18 // Sound channel 2 period low
#define rNR24   0xFF19 // Sound channel 2 period high & control

#define rNR30   0xFF1A // Sound channel 3 DAC enable
#define rNR31   0xFF1B // Sound channel 3 length timer
#define rNR32   0xFF1C // Sound channel 3 output level
#define rNR33   0xFF1D // Sound channel 3 period low
#define rNR34   0xFF1E // Sound channel 3 period high & control

//      rNR40   0xFF1F // Sound channel 4 unused
#define rNR41   0xFF20 // Sound channel 4 length timer
#define rNR42   0xFF21 // Sound channel 4 volume & envelope
#define rNR43   0xFF22 // Sound channel 4 frequency & randomness
#define rNR44   0xFF23 // Sound channel 4 control

#define rNR50   0xFF24 // Master volume & VIN panning
#define rNR51   0xFF25 // Sound panning
#define rNR52   0xFF26 // Sound on/off

#define rLCDC   0xFF40 // LCD control
#define rSTAT   0xFF41 // LCD status
#define rSCY    0xFF42 // Viewport Y position
#define rSCX    0xFF43 // Viewport X position
#define rLY     0xFF44 // LCD Y coordinate
#define rLYC    0xFF45 // LY compare
#define rDMA    0xFF46 // OAM DMA source address & start
#define rBGP    0xFF47 // BG palette data
#define rOBP0   0xFF48 // OBJ palette 0 data
#define rOBP1   0xFF49 // OBJ palette 1 data
#define rWY     0xFF4A // Window Y position
#define rWX     0xFF4B // Window X position plus 7
#define rIE     0xFFFF // Interrupt enable

#define _VRAM       0x8000
#define _VRAM8000   0x8000  // Tile Data (3 x 128 16-byte tiles => 3 x 2KiB)
#define _VRAM8800   0x8800
#define _VRAM9000   0x9000
#define _SCRN0      0x9800  // Tilemap Offset (32x32 1-byte tile ids => 1KiB)
#define _SCRN1      0x9C00
#define _SRAM       0xA000
#define _RAM        0xC000
#define _OAMRAM     0xFE00
#define _IO         0xFF00
#define _HRAM       0xFF80

#define LCDCF_OFF       0x00  // LCDC.7: LCD Control
#define LCDCF_ON        0x80
#define LCDCF_WIN9800   0x00  // LCDC.6: Window Tile Map Select
#define LCDCF_WIN9C00   0x40
#define LCDCF_WINOFF    0x00  // LCDC.5: Window Display
#define LCDCF_WINON     0x20
#define LCDCF_BG8800    0x00  // LCDC.4: BG & Window Tile Data Select
#define LCDCF_BG8000    0x10
#define LCDCF_BG9800    0x00  // LCDC.3: BG Tile Map Select
#define LCDCF_BG9C00    0x08
#define LCDCF_OBJ8      0x00  // LCDC.2: OBJ Size
#define LCDCF_OBJ16     0x04
#define LCDCF_OBJOFF    0x00  // LCDC.1: OBJ Display
#define LCDCF_OBJON     0x02
#define LCDCF_BGOFF     0x00  // LCDC.0: BG Display
#define LCDCF_BGON      0x01

#define STATF_LYC       0x40  // %01000000 ; LYC=LY Coincidence (Selectable)
#define STATF_MODE10    0x20  // %00100000 ; Mode 10
#define STATF_MODE01    0x10  // %00010000 ; Mode 01 (V-Blank)
#define STATF_MODE00    0x08  // %00001000 ; Mode 00 (H-Blank)
#define STATF_LYCF      0x04  // %00000100 ; Coincidence Flag
#define STATF_HBL       0x00  // %00000000 ; H-Blank
#define STATF_VBL       0x01  // %00000001 ; V-Blank
#define STATF_OAM       0x02  // %00000010 ; OAM-RAM is used by system
#define STATF_LCD       0x03  // %00000011 ; Both OAM and VRAM used by system
#define STATF_BUSY      0x02  // %00000010 ; When set, VRAM access is unsafe

#define IEF_HILO        0x10  // %00010000 ; Transition from High to Low of Pin number P10-P13
#define IEF_SERIAL      0x08  // %00001000 ; Serial I/O transfer end
#define IEF_TIMER       0x04  // %00000100 ; Timer Overflow
#define IEF_STAT        0x02  // %00000010 ; STAT
#define IEF_VBLANK      0x01  // %00000001 ; V-Blank

#define P1F_GET_BTN  0x10
#define P1F_GET_DPAD 0x20
#define P1F_GET_NONE (P1F_GET_BTN | P1F_GET_DPAD)

typedef struct GameBoy {
    union { u16 AF; struct { u8 F; u8 A; }; };
    union { u16 BC; struct { u8 C; u8 B; }; };
    union { u16 DE; struct { u8 E; u8 D; }; };
    union { u16 HL; struct { u8 L; u8 H; }; };
    u16 SP;
    u16 PC;

    u8 IME; // Interrupt master enable flag (Instructions EI, DI, RETI, ISR)

    // Tiles, Palettes and Layers
    // Tiles are 8x8 pixels (also called patterns or characters)
    // Stored with color ID's from 0 to 3 or 2 bits per pixel
    // 8x8x2 = 128 bits = 16 bytes/tile
    Color display[SCRN_VX*SCRN_VY]; // 256x256 pixels

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
    u8 memory[0x10000];

    u8 cart_type;
    bool boot_mode;
    u8 boot_rom[256];
    u8 *rom; // from 32 KiB (2 banks) to 8 MiB (512 banks)
    u32 rom_bank_count;

    // MBC1-specific
    bool ram_enabled;
    u8 rom_bank_num;

    // Input
    u8 button_a;
    u8 button_b;
    u8 button_start;
    u8 button_select;
    u8 dpad_up;
    u8 dpad_down;
    u8 dpad_left;
    u8 dpad_right;

    // Timers
    u64 elapsed_us;// Microseconds elapsed since the start
    f64 elapsed_ms;  // Milliseconds elapsed since the start
    f64 timer_mcycle;// Timer for counting Mcycles (~1 MHz => 1048576 Hz)
    f64 timer_clock;
    f64 timer_div;   // Ticks at 16384Hz (in ms)
    f64 timer_tima;  // Ticks at 4096/262144/65536/16384 Hz (depending on TAC)
    f64 timer_ly;    // Ticks at ~9180 Hz (every 0.1089 ms)

    u64 dots;      // 0-70223

    int (*printf)(const char *fmt, ...);

    u64 inst_executed;
    bool halted;
    bool stopped;
    bool running;
    bool paused;
} GameBoy;

typedef struct Inst {
    const u8 *data;
    u8 size;
    u8 min_cycles;
    u8 max_cycles;
} Inst;

typedef enum Reg8 {
    REG_B = 0,
    REG_C = 1,
    REG_D = 2,
    REG_E = 3,
    REG_H = 4,
    REG_L = 5,
    REG_HL_IND = 6,
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
void gb_load_rom(GameBoy *gb, u8 *raw, size_t size);
void gb_tick_us(GameBoy *gb, u64 dt_us);
void gb_tick(GameBoy *gb, f64 dt_ms);

Inst gb_fetch_inst(const GameBoy *gb);
const char *gb_decode(Inst inst, char *buf, size_t size);
bool gb_exec(GameBoy *gb, Inst inst);
void gb_render(GameBoy *gb);

void gb_dump(const GameBoy *gb);

u8 gb_mem_read(const GameBoy *gb, u16 addr);
void gb_mem_write(GameBoy *gb, u16 addr, u8 value);

// CPU
#define UNCHANGED (-1)
u8 gb_get_flag(const GameBoy *gb, Flag flag);
void gb_set_flag(GameBoy *gb, Flag flag, u8 value);
void gb_set_flags(GameBoy *gb, int z, int n, int h, int c);

u8 gb_get_reg8(const GameBoy *gb, Reg8 r8);
u16 gb_get_reg16(const GameBoy *gb, Reg16 r16);
void gb_set_reg8(GameBoy *gb, Reg8 r8, u8 value);
void gb_set_reg16(GameBoy *gb, Reg16 r16, u16 value);

const char* gb_reg8_to_str(Reg8 r8);
const char* gb_reg16_to_str(Reg16 r16);

u8 *read_entire_file(const char *path, size_t *size);

static const u8 NINTENDO_LOGO[] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E,
};

static const u8 BOOT_ROM[] = {
    0x31, 0xfe, 0xff, 0xaf, 0x21, 0xff, 0x9f, 0x32, 0xcb, 0x7c, 0x20, 0xfb, 0x21, 0x26, 0xff, 0x0e,
    0x11, 0x3e, 0x80, 0x32, 0xe2, 0x0c, 0x3e, 0xf3, 0xe2, 0x32, 0x3e, 0x77, 0x77, 0x3e, 0xfc, 0xe0,
    0x47, 0x11, 0x04, 0x01, 0x21, 0x10, 0x80, 0x1a, 0xcd, 0x95, 0x00, 0xcd, 0x96, 0x00, 0x13, 0x7b,
    0xfe, 0x34, 0x20, 0xf3, 0x11, 0xd8, 0x00, 0x06, 0x08, 0x1a, 0x13, 0x22, 0x23, 0x05, 0x20, 0xf9,
    0x3e, 0x19, 0xea, 0x10, 0x99, 0x21, 0x2f, 0x99, 0x0e, 0x0c, 0x3d, 0x28, 0x08, 0x32, 0x0d, 0x20,
    0xf9, 0x2e, 0x0f, 0x18, 0xf3, 0x67, 0x3e, 0x64, 0x57, 0xe0, 0x42, 0x3e, 0x91, 0xe0, 0x40, 0x04,
    0x1e, 0x02, 0x0e, 0x0c, 0xf0, 0x44, 0xfe, 0x90, 0x20, 0xfa, 0x0d, 0x20, 0xf7, 0x1d, 0x20, 0xf2,
    0x0e, 0x13, 0x24, 0x7c, 0x1e, 0x83, 0xfe, 0x62, 0x28, 0x06, 0x1e, 0xc1, 0xfe, 0x64, 0x20, 0x06,
    0x7b, 0xe2, 0x0c, 0x3e, 0x87, 0xe2, 0xf0, 0x42, 0x90, 0xe0, 0x42, 0x15, 0x20, 0xd2, 0x05, 0x20,
    0x4f, 0x16, 0x20, 0x18, 0xcb, 0x4f, 0x06, 0x04, 0xc5, 0xcb, 0x11, 0x17, 0xc1, 0xcb, 0x11, 0x17,
    0x05, 0x20, 0xf5, 0x22, 0x23, 0x22, 0x23, 0xc9, 0xce, 0xed, 0x66, 0x66, 0xcc, 0x0d, 0x00, 0x0b,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0c, 0x00, 0x0d, 0x00, 0x08, 0x11, 0x1f, 0x88, 0x89, 0x00, 0x0e,
    0xdc, 0xcc, 0x6e, 0xe6, 0xdd, 0xdd, 0xd9, 0x99, 0xbb, 0xbb, 0x67, 0x63, 0x6e, 0x0e, 0xec, 0xcc,
    0xdd, 0xdc, 0x99, 0x9f, 0xbb, 0xb9, 0x33, 0x3e, 0x3c, 0x42, 0xb9, 0xa5, 0xb9, 0xa5, 0x42, 0x3c,
    0x21, 0x04, 0x01, 0x11, 0xa8, 0x00, 0x1a, 0x13, 0xbe, 0x20, 0xfe, 0x23, 0x7d, 0xfe, 0x34, 0x20,
    0xf5, 0x06, 0x19, 0x78, 0x86, 0x23, 0x05, 0x20, 0xfb, 0x86, 0x20, 0xfe, 0x3e, 0x01, 0xe0, 0x50,
};

#endif // GB_H
