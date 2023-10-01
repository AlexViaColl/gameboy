#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gb.h"

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

void gb_dump(const GameBoy *gb)
{
    uint8_t flags = gb->AF & 0xff;
    gb->printf("$PC: $%04X, A: $%02X, F: %c%c%c%c, "
        "BC: $%04X, DE: $%04X, HL: $%04X, SP: $%04X\n",
        gb->PC,
        gb_get_reg(gb, REG_A),
        (flags & 0x80) ? 'Z' : '-',
        (flags & 0x40) ? 'N' : '-',
        (flags & 0x20) ? 'H' : '-',
        (flags & 0x10) ? 'C' : '-',
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

uint8_t gb_read_memory(const GameBoy *gb, uint16_t addr)
{
#if 0
    if (addr == 0xFF0F) {
        fprintf(stderr, "Reading [IF/*$FF0F*/] = %02X\n", gb->memory[addr]);
    } else if (addr == 0xFFFF) {
        fprintf(stderr, "Reading [IE/*$FFFF*/] = %02X\n", gb->memory[addr]);
    }
#endif
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
        // TODO
    }
}

void gb_write_timer(GameBoy *gb, uint16_t addr, uint8_t value)
{
    switch (addr) {
        case rDIV:
            //printf("write timer: [rDIV %04X] = %02X\n", addr, value);
            gb->memory[rDIV] = 0; // Reset
            break;
        case rTIMA:
            //printf("write timer: [rTIMA %04X] = %02X\n", addr, value);
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
            //int modulo = 0x100 - value;
            //printf("write timer: [rTMA %04X] = %02X\n", addr, value);
            //printf("Interrupt every %d increments of TAC\n", modulo);
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
                (void)clock_select;
                //printf("write timer: [rTAC %04X] = %02X\n", addr, value);
                //printf("Enabling Timer at CPU Clock / %d = %d Hz\n",
                //    clock_select, (int)(CPU_FREQ / clock_select));
                //assert(0);
            }
            gb->memory[rTAC] = value;
            break;
        default: assert(0 && "Unreachable");
    }
}

static int gb_clock_freq(int clock)
{
    switch (clock & 3) {
        case 0: return 4096;
        case 1: return 262144;
        case 2: return 65536;
        case 3: return 16384;
    }
    assert(0 && "Unreachable");
}

void gb_write_memory(GameBoy *gb, uint16_t addr, uint8_t value)
{
    if (addr == 0xFFA4 && value != 0) {
        printf("$%04X: [$FFA4] = $%02X\n", gb->PC, value);
        //assert(0);
    }

    // No MBC (32 KiB ROM only)
    if (gb->cart_type == 0) {
        if (addr <= 0x7FFF) return;
        assert(addr >= 0x8000);
    }

    // MBC1
    else if (gb->cart_type == 1) {
        if (addr <= 0x1FFF) {
            if ((value & 0xF) == 0xA) {
                fprintf(stderr, "RAM Enable\n");
                gb->ram_enabled = true;
                assert(0);
            } else {
                fprintf(stderr, "RAM Disable\n");
                gb->ram_enabled = false;
            }
        } else if (addr <= 0x3FFF) {
            value = value & 0x1F; // Consider only lower 5-bits
            if (value == 0) value = 1;
            gb->rom_bank_num = value % gb->rom_bank_count;
            //fprintf(stderr, "ROM Bank Number: %02X\n", gb->rom_bank_num);
            memcpy(gb->memory+0x4000, gb->rom + gb->rom_bank_num*0x4000, 0x4000);
        } else if (addr <= 0x5FFF) {
            //fprintf(stderr, "RAM Bank Number\n");
        } else if (addr <= 0x7FFF) {
            fprintf(stderr, "Banking Mode Select\n");
        }

        if (addr <= 0x7FFF) return;
    }

    else {
        assert(0 && "MBC X is not supported yet!");
    }

    if (addr == rP1/*0xFF00*/) {
        //gb->memory[addr] = value;
        gb_write_joypad_input(gb, value);
    } else if (addr == rDIV/*0xFF04*/) {
        gb->memory[addr] = 0;
        fprintf(stderr, "$%04X: [DIV ] = %3d (%02X)\n", gb->PC, value, value);
    } else if (addr == rTIMA/*0xFF05*/) {
        gb->memory[addr] = value;
        fprintf(stderr, "$%04X: [TIMA] = %3d (%02X)\n", gb->PC, value, value);
    } else if (addr == rTMA/*0xFF06*/) {
        gb->memory[addr] = value;
        fprintf(stderr, "$%04X: [TMA ] = %3d (%02X)\n", gb->PC, value, value);
    } else if (addr == rTAC/*0xFF07*/) {
        gb->memory[addr] = value;
        fprintf(stderr, "$%04X: [TAC ] = %3d (%02X)", gb->PC, value, value);
        if (value & 0x04) {
            uint8_t clock = value & 3;
            fprintf(stderr, " Enable %02X %d Hz\n", clock, gb_clock_freq(clock));
        } else {
            fprintf(stderr, " Disable\n");
        }
    } else if (addr == rIF/*0xFF0F*/) {
        gb->memory[addr] = value;
        fprintf(stderr, "$%04X: [IF  ] = %3d (%02X)", gb->PC, value, value);
        if (value & IEF_HILO)   fprintf(stderr, " | IEF_HILO");
        if (value & IEF_SERIAL) fprintf(stderr, " | IEF_SERIAL");
        if (value & IEF_TIMER)  fprintf(stderr, " | IEF_TIMER");
        if (value & IEF_STAT)   fprintf(stderr, " | IEF_STAT");
        if (value & IEF_VBLANK) fprintf(stderr, " | IEF_VBLANK");
        fprintf(stderr, "\n");
    } else if (addr == rIE/*0xFFFF*/) {
        gb->memory[addr] = value;
        fprintf(stderr, "$%04X: [IE  ] = %3d (%02X)", gb->PC, value, value);
        if (value & IEF_HILO)   fprintf(stderr, " | IEF_HILO");
        if (value & IEF_SERIAL) fprintf(stderr, " | IEF_SERIAL");
        if (value & IEF_TIMER)  fprintf(stderr, " | IEF_TIMER");
        if (value & IEF_STAT)   fprintf(stderr, " | IEF_STAT");
        if (value & IEF_VBLANK) fprintf(stderr, " | IEF_VBLANK");
        fprintf(stderr, "\n");
    } else if (addr == rLCDC/*0xFF40*/) {
        gb->memory[addr] = value;
        fprintf(stderr, "$%04X: [LCDC] = %3d (%02X)", gb->PC, value, value);
        if (value & LCDCF_ON)       fprintf(stderr, " | LCDCF_ON");
        if (value & LCDCF_WIN9800)  fprintf(stderr, " | LCDCF_WIN9800");
        if (value & LCDCF_WINON)    fprintf(stderr, " | LCDCF_WINON");
        if (value & LCDCF_BG8000)   fprintf(stderr, " | LCDCF_BG8000");
        if (value & LCDCF_BG9C00)   fprintf(stderr, " | LCDCF_BG9C00");
        if (value & LCDCF_OBJ16)    fprintf(stderr, " | LCDCF_OBJ16");
        if (value & LCDCF_OBJON)    fprintf(stderr, " | LCDCF_OBJON");
        if (value & LCDCF_BGON)     fprintf(stderr, " | LCDCF_BGON");
        fprintf(stderr, "\n");
    } else if (addr == rSTAT/*0xFF41*/) {
        gb->memory[addr] = value;
        fprintf(stderr, "$%04X: [STAT] = %3d (%02X)", gb->PC, value, value);
        if (value & 0x08)       fprintf(stderr, " | HBlank STAT interrupt enabled");
        if (value & 0x10)       fprintf(stderr, " | VBlank STAT interrupt enabled");
        if (value & 0x20)       fprintf(stderr, " | OAM STAT interrupt enabled");
        if (value & 0x40)       fprintf(stderr, " | LYC=LY STAT interrupt enabled (LYC=%02X)", gb->memory[rLYC]);
        fprintf(stderr, "\n");
        //if (value != 0) assert(0);
    } else if (addr == rSCY/*0xFF42*/ || addr == rSCX/*0xFF43*/) {
        if (gb->memory[addr] == value) return;
        // TODO: This is a hack to prevent a glitch where the frame is renderer twice
        // once with SCX = 0 and another time with SCX != 0
        if (gb->memory[rLY] < 0xF) return;
        fprintf(stderr, "$%04X: [%s ] = %3d (%02X)\n", gb->PC, addr == rSCY ? "SCY" : "SCX", value, value);
        fprintf(stderr, "LY=%02X\n", gb->memory[rLY]);
        gb->memory[addr] = value;
    } else if (addr == rLYC/*0xFF45*/) {
        fprintf(stderr, "$%04X: [LCY ] = %3d (%02X)\n", gb->PC, value, value);
        gb->memory[addr] = value;
    } else if (addr == rDMA/*0xFF46*/) {
        assert(value <= 0xDF);
        uint16_t src = value << 8;
        memcpy(gb->memory + 0xFE00, gb->memory + src, 0x9F);
        gb->memory[addr] = value;
        //fprintf(stderr, "$%04X: [DMA ] = %3d (%02X)\n", gb->PC, value, value);
    } else {
        gb->memory[addr] = value;
    }
}

uint8_t gb_get_flag(const GameBoy *gb, Flag flag)
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

const char* gb_reg_to_str(Reg8 r8)
{
    switch (r8) {
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

const char* gb_reg16_to_str(Reg16 r16)
{
    switch (r16) {
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

void gb_set_reg(GameBoy *gb, Reg8 r8, uint8_t value)
{
    // r8 (0-7): B, C, D, E, H, L, (HL), A
    switch (r8) {
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

uint8_t gb_get_reg(const GameBoy *gb, Reg8 r8)
{
    // r8 (0-7): B, C, D, E, H, L, (HL), A
    switch (r8) {
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

void gb_set_reg16(GameBoy *gb, Reg16 r16, uint16_t value)
{
    switch (r16) {
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

uint16_t gb_get_reg16(const GameBoy *gb, Reg16 r16)
{
    switch (r16) {
        case REG_BC: return gb->BC;
        case REG_DE: return gb->DE;
        case REG_HL: return gb->HL;
        case REG_SP: return gb->SP;
        default:
            assert(0 && "Invalid register provided");
    }
}

#define gb_log_inst(...) gb_log_inst_internal(gb, __VA_ARGS__)
void gb_log_inst_internal(GameBoy *gb, const char *fmt, ...)
{
    return;
    if (gb->printf == NULL) return;

    va_list args;
    va_start(args, fmt);
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    Inst inst = gb_fetch_inst(gb);
#if 1
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
#else
    printf("A: %02X F: %02X B: %02X C: %02X D: %02X E: %02X H: %02X L: %02X",
        gb_get_reg(gb, REG_A), gb->AF & 0xff,
        gb_get_reg(gb, REG_B), gb_get_reg(gb, REG_C),
        gb_get_reg(gb, REG_D), gb_get_reg(gb, REG_E),
        gb_get_reg(gb, REG_H), gb_get_reg(gb, REG_L));

    printf(" SP: %04X PC: 00:%04X (%02X %02X %02X %02X)\n",
        gb->SP, gb->PC,
        gb->memory[gb->PC+0],
        gb->memory[gb->PC+1],
        gb->memory[gb->PC+2],
        gb->memory[gb->PC+3]);
#endif
}

Inst gb_fetch_inst(const GameBoy *gb)
{
    uint8_t b = gb_read_memory(gb, gb->PC);
    const uint8_t *data = &gb->memory[gb->PC];

    if (b == 0xD3 || b == 0xDB || b == 0xDD || b == 0xE3 || b == 0xE4 ||
        b == 0xEB || b == 0xEC || b == 0xED || b == 0xF4 || b == 0xFC || b == 0xFD)
    {
        fprintf(stderr, "Illegal Instruction 0x%02X\n", b);
        exit(1);
    }

    // 1-byte instructions
    if (b == 0x00 || b == 0x10 || b == 0x76 || b == 0xF3 || b == 0xFB) { // NOP, STOP, HALT, DI, EI
        return (Inst){.data = data, .size = 1, .min_cycles = 4, .max_cycles = 4};
    } else if (b == 0x02 || b == 0x12 || b == 0x22 || b == 0x32) { // LD (BC),A | LD (DE),A | LD (HL-),A
        return (Inst){.data = data, .size = 1, .min_cycles = 8, .max_cycles = 8};
    } else if (b == 0x09 || b == 0x19 || b == 0x29 || b == 0x39) { // ADD HL,n (n = BC,DE,HL,SP)
        return (Inst){.data = data, .size = 1, .min_cycles = 8, .max_cycles = 8};
    } else if ((b >> 6) == 0 && (b & 7) == 4) { // INC reg8: 00|xxx|100
        return (Inst){.data = data, .size = 1, .min_cycles = 4, .max_cycles = 4};
    } else if ((b >> 6) == 0 && (b & 7) == 5) { // DEC reg8: 00|xxx|101
        return (Inst){.data = data, .size = 1, .min_cycles = 4, .max_cycles = 4};
    } else if ((b >> 6) == 0 && (b & 7) == 7) { // RLCA|RRCA|RLA|RRA|DAA|CPL|SCF|CCF: 00|xxx|111
        return (Inst){.data = data, .size = 1, .min_cycles = 4, .max_cycles = 4};
    } else if ((b >> 6) == 0 && (b & 7) == 3) { // INC reg16|DEC reg16: 00|xxx|011
        return (Inst){.data = data, .size = 1, .min_cycles = 8, .max_cycles = 8};
    } else if ((b >> 6) == 0 && (b & 7) == 2) { // LD (reg16),A|LD A,(reg16): 00|xxx|010
        return (Inst){.data = data, .size = 1, .min_cycles = 8, .max_cycles = 8};
    } else if (b >= 0x40 && b <= 0x7F) {
        bool is_ld_r8_hl = (b >> 6) == 1 && (b & 7) == 6;
        bool is_ld_hl_r8 = b >= 0x70 && b <= 0x77;
        bool is_ld_hl = is_ld_r8_hl || is_ld_hl_r8;
        uint8_t cycles = is_ld_hl ? 8 : 4;
        return (Inst){.data = data, .size = 1, .min_cycles = cycles, .max_cycles = cycles};
    } else if (b >= 0x80 && b <= 0xBF) {
        bool reads_hl = (b >> 6) == 2 && (b & 7) == 6;
        uint8_t cycles = reads_hl ? 8 : 4;
        return (Inst){.data = data, .size = 1, .min_cycles = cycles, .max_cycles = cycles};
    } else if (b == 0xC0 || b == 0xD0 || b == 0xC8 || b == 0xD8) {
        return (Inst){.data = data, .size = 1, .min_cycles = 8, .max_cycles = 20};
    } else if (b == 0xC1 || b == 0xD1 || b == 0xE1 || b == 0xF1) { // POP reg16: 11|xx|0001
        return (Inst){.data = data, .size = 1, .min_cycles = 12, .max_cycles = 12};
    } else if (b == 0xC5 || b == 0xD5 || b == 0xE5 || b == 0xF5) { // PUSH reg16: 11|xx|0101
        return (Inst){.data = data, .size = 1, .min_cycles = 16, .max_cycles = 16};
    } else if ((b >> 6) == 3 && (b & 7) == 7) { // RST xx: 11|xxx|111
        return (Inst){.data = data, .size = 1, .min_cycles = 16, .max_cycles = 16};
    } else if (b == 0xC9 || b == 0xD9) { // RET|RETI
        return (Inst){.data = data, .size = 1, .min_cycles = 16, .max_cycles = 16};
    } else if (b == 0xE2 || b == 0xF2) { // LD (C),A|LD A,(C)
        return (Inst){.data = data, .size = 1, .min_cycles = 8, .max_cycles = 8};
    } else if (b == 0xE9) { // JP (HL)
        return (Inst){.data = data, .size = 1, .min_cycles = 4, .max_cycles = 4};
    } else if (b == 0xF9) { // LD SP,HL
        return (Inst){.data = data, .size = 1, .min_cycles = 8, .max_cycles = 8};
    }

    // 2-byte instructions
    else if ((b >> 6) == 0 && (b & 7) == 6) { // LD reg8,d8|LD (HL),d8
        uint8_t cycles = b == 0x36 ? 12 : 8;
        return (Inst){.data = data, .size = 2, .min_cycles = cycles, .max_cycles = cycles};
    } else if (b == 0x18) { // JR r8
        return (Inst){.data = data, .size = 2, .min_cycles = 12, .max_cycles = 12};
    } else if (b == 0x20 || b == 0x30 || b == 0x28 || b == 0x38) { // JR NZ|NC|Z|C,r8
        return (Inst){.data = data, .size = 2, .min_cycles = 8, .max_cycles = 12};
    } else if ((b >> 6) == 3 && (b & 7) == 6) { // ADD|ADC|SUB|SBC|AND|XOR|OR|CP d8: 11|xxx|110
        return (Inst){.data = data, .size = 2, .min_cycles = 8, .max_cycles = 8};
    } else if (b == 0xE0 || b == 0xF0) { // LDH (a8),A|LDH A,(a8)
        return (Inst){.data = data, .size = 2, .min_cycles = 12, .max_cycles = 12};
    } else if (b == 0xE8) { // ADD SP,r8
        return (Inst){.data = data, .size = 2, .min_cycles = 16, .max_cycles = 16};
    } else if (b == 0xF8) { // LD HL,SP+r8
        return (Inst){.data = data, .size = 2, .min_cycles = 12, .max_cycles = 12};
    }

    // Prefix CB
    else if (b == 0xCB) {
        uint8_t b2 = gb_read_memory(gb, gb->PC+1);
        uint8_t cycles = (b2 & 7) == 6 ? 16 : 8;
        return (Inst){.data = data, .size = 2, .min_cycles = cycles, .max_cycles = cycles};
    }

    // 3-byte instructions
    else if (b == 0x01 || b == 0x11 || b == 0x21 || b == 0x31) { // LD r16,d16
        return (Inst){.data = data, .size = 3, .min_cycles = 12, .max_cycles = 12};
    } else if (b == 0x08) { // LD (a16),SP
        return (Inst){.data = data, .size = 3, .min_cycles = 20, .max_cycles = 20};
    } else if (b == 0xC3) { // JP a16
        return (Inst){.data = data, .size = 3, .min_cycles = 16, .max_cycles = 16};
    } else if (b == 0xC4 || b == 0xD4 || b == 0xCC || b == 0xDC) {
        return (Inst){.data = data, .size = 3, .min_cycles = 12, .max_cycles = 24};
    } else if (b == 0xC2 || b == 0xCA || b == 0xD2 || b == 0xDA) {
        return (Inst){.data = data, .size = 3, .min_cycles = 12, .max_cycles = 16};
    } else if (b == 0xCD) { // CALL a16
        return (Inst){.data = data, .size = 3, .min_cycles = 24, .max_cycles = 24};
    } else if (b == 0xEA || b == 0xFA) { // LD (a16),A|LD A,(a16)
        return (Inst){.data = data, .size = 3, .min_cycles = 16, .max_cycles = 16};
    }

    printf("%02X\n", b);
    assert(0 && "Not implemented");
}

const char *gb_decode(Inst inst, char *buf, size_t size)
{
    uint8_t b = inst.data[0];
    if (inst.size == 1) {
        if (b == 0x00) {
            snprintf(buf, size, "NOP");
        } else if (b == 0x10) {
            snprintf(buf, size, "STOP");
        } else if (b == 0x09 || b == 0x19 || b == 0x29 || b == 0x39) {
            Reg16 r16 = (b >> 4) & 0x3;
            snprintf(buf, size, "ADD HL,%s", gb_reg16_to_str(r16));
        } else if (
            b == 0x04 || b == 0x14 || b == 0x24 || b == 0x34 ||
            b == 0x0C || b == 0x1C || b == 0x2C || b == 0x3C
        ) {
            Reg8 r8 = (b >> 3) & 7;
            snprintf(buf, size, "INC %s", gb_reg_to_str(r8));
        } else if (
            b == 0x05 || b == 0x15 || b == 0x25 || b == 0x35 ||
            b == 0x0D || b == 0x1D || b == 0x2D || b == 0x3D
        ) {
            Reg8 r8 = (b >> 3) & 7;
            snprintf(buf, size, "DEC %s", gb_reg_to_str(r8));
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
            snprintf(buf, size, "LD %s,%s", gb_reg_to_str(dst), gb_reg_to_str(src));
        } else if (b >= 0x80 && b <= 0x87) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "ADD A,%s", gb_reg_to_str(r8));
        } else if (b >= 0x88 && b <= 0x8F) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "ADC A,%s", gb_reg_to_str(r8));
        } else if (b >= 0x90 && b <= 0x97) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "SUB A,%s", gb_reg_to_str(r8));
        } else if (b >= 0x98 && b <= 0x9F) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "SBC A,%s", gb_reg_to_str(r8));
        } else if (b >= 0xB0 && b <= 0xB7) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "OR %s", gb_reg_to_str(r8));
        } else if (b >= 0xB8 && b <= 0xBF) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "CP %s", gb_reg_to_str(r8));
        } else if (b >= 0xA0 && b <= 0xA7) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "AND %s", gb_reg_to_str(r8));
        } else if (b >= 0xA8 && b <= 0xAF) {
            Reg8 r8 = b & 7;
            snprintf(buf, size, "XOR %s", gb_reg_to_str(r8));
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
            uint8_t n = ((b >> 3) & 0x7)*8;
            snprintf(buf, size, "RST %02XH", n);
        } else if (b == 0xE9) {
            snprintf(buf, size, "JP HL");
        } else if (b == 0xF9) {
            snprintf(buf, size, "LD SP,HL");
        } else {
            assert(0 && "Instruction not implemented");
        }
    }
    // 2-byte instructions
    else if (inst.size == 2 && b != 0xCB) {
        uint8_t b2 = inst.data[1];
        if (b == 0x18) {
            int r8 = b2 >= 0x80 ? (int8_t)b2 : b2;
            snprintf(buf, size, "JR %d", r8);
        } else if ( // LD reg,d8
            b == 0x06 || b == 0x16 || b == 0x26 || b == 0x36 ||
            b == 0x0E || b == 0x1E || b == 0x2E || b == 0x3E
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            snprintf(buf, size, "LD %s,0x%02X", gb_reg_to_str(reg), b2);
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
        uint8_t b2 = inst.data[1];
        Reg8 reg = b2 & 7;
        uint8_t bit = (b2 >> 3) & 7;
        if (b2 <= 0x07) {
            snprintf(buf, size, "RLC %s", gb_reg_to_str(reg));
        } else if (b2 >= 0x08 && b2 <= 0x0F) {
            snprintf(buf, size, "RRC %s", gb_reg_to_str(reg));
        } else if (b2 >= 0x10 && b2 <= 0x17) {
            snprintf(buf, size, "RL %s", gb_reg_to_str(reg));
        } else if (b2 >= 0x18 && b2 <= 0x1F) {
            snprintf(buf, size, "RR %s", gb_reg_to_str(reg));
        } else if (b2 >= 0x20 && b2 <= 0x27) {
            snprintf(buf, size, "SLA %s", gb_reg_to_str(reg));
        } else if (b2 >= 0x28 && b2 <= 0x2F) {
            snprintf(buf, size, "SRA %s", gb_reg_to_str(reg));
        } else if (b2 >= 0x30 && b2 <= 0x37) {
            snprintf(buf, size, "SWAP %s", gb_reg_to_str(reg));
        } else if (b2 >= 0x38 && b2 <= 0x3F) {
            snprintf(buf, size, "SRL %s", gb_reg_to_str(reg));
        } else if (b2 >= 0x40 && b2 <= 0x7F) {
            snprintf(buf, size, "BIT %d,%s", bit, gb_reg_to_str(reg));
        } else if (b2 >= 0x80 && b2 <= 0xBF) {
            snprintf(buf, size, "RES %d,%s", bit, gb_reg_to_str(reg));
        } else if (b2 >= 0xC0) {
            snprintf(buf, size, "SET %d,%s", bit, gb_reg_to_str(reg));
        } else {
            assert(0 && "Instruction not implemented");
        }
    }

    // 3-byte instructions
    else if (inst.size == 3) {
        uint16_t n = inst.data[1] | (inst.data[2] << 8);
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

void gb_exec(GameBoy *gb, Inst inst)
{
    // Break at 
    // $069F - ret z
    // $0296 - halt
    // $0050 - timer int. handler
    //
    // $1DFD - update $FFA4 (variable holding scroll X ???)
    // $00A5 - copy from $FFA4 to SCX ???
    // $008B - 
#if 0
    if (gb->PC == 0x0048) {
        gb_dump(gb);
        assert(0);
    }
#endif
    assert(gb->PC <= 0x7FFF || gb->PC >= 0xFF80 || (gb->PC >= 0xA000 && gb->PC <= 0xDFFF));

    uint8_t IE = gb->memory[rIE];
    uint8_t IF = gb->memory[rIF];
    if (gb->halted) {
        if ((IF & IE) == 0) return;
        gb->halted = false;
    }

    if (gb->IME) {
        // VBlank Interrupt
        if (((IF & 0x01) == 0x01) && ((IE & 0x01) == 0x01)) {
            gb->SP -= 2;
            gb_write_memory(gb, gb->SP+0, gb->PC & 0xff);
            gb_write_memory(gb, gb->SP+1, gb->PC >> 8);
            gb->PC = 0x0040;

            // Clear IME and corresponding bit of IF
            if ((IF & 0x01) == 0x01) {
                gb->IME = 0;
                gb->memory[rIF] &= ~0x01;
            }
            return;
        }

        // STAT Interrupt
        if (((IF & 0x02) == 0x02) && ((IE & 0x02) == 0x02)) {
            gb->SP -= 2;
            gb_write_memory(gb, gb->SP+0, gb->PC & 0xff);
            gb_write_memory(gb, gb->SP+1, gb->PC >> 8);
            gb->PC = 0x0048;

            // Clear IME and corresponding bit of IF
            if ((IF & 0x02) == 0x02) {
                gb->IME = 0;
                gb->memory[rIF] &= ~0x02;
            }
            return;
        }

        // Timer Interrupt
        if (((IF & 0x04) == 0x04) && ((IE & 0x04) == 0x04)) {
            if (gb->halted) gb->halted = false;

            gb->SP -= 2;
            gb_write_memory(gb, gb->SP+0, gb->PC & 0xff);
            gb_write_memory(gb, gb->SP+1, gb->PC >> 8);
            gb->PC = 0x0050;

            // Clear IME and corresponding bit of IF
            if ((IF & 0x04) == 0x04) {
                gb->IME = 0;
                gb->memory[rIF] &= ~0x04;
            }
            return;
        }

        // Joypad Interrupt
        if (((IF & 0x1F) == 0x1F) && ((IE & 0x1F) == 0x1F)) {
            gb->SP -= 2;
            gb_write_memory(gb, gb->SP+0, gb->PC & 0xff);
            gb_write_memory(gb, gb->SP+1, gb->PC >> 8);
            gb->PC = 0x0060;

            // Clear IME and corresponding bit of IF
            if ((IF & 0x1F) == 0x1F) {
                gb->IME = 0;
                gb->memory[rIF] &= ~0x1F;
            }
            return;
        }

        //if (gb->ime_cycles == 1) {
        //    gb->ime_cycles -= 1;
        //} else if (gb->ime_cycles == 0 && gb->memory[rLY] == 0) {
        //    // Handle interrupt
        //    gb->SP -= 2;
        //    uint16_t ret_addr = gb->PC;
        //    gb_write_memory(gb, gb->SP+0, ret_addr & 0xff);
        //    gb_write_memory(gb, gb->SP+1, ret_addr >> 8);
        //    gb->PC = 0x0040; // VBlank

        //    gb->IME = 0;
        //    return;
        //}
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
            gb->PC += inst.size;
        } else if (b == 0x09 || b == 0x19 || b == 0x29 || b == 0x39) {
            Reg16 src = (b >> 4) & 0x3;
            gb_log_inst("ADD HL,%s", gb_reg16_to_str(src));
            uint16_t hl_prev = gb_get_reg16(gb, REG_HL);
            uint16_t reg_prev = gb_get_reg16(gb, src);
            uint16_t res = hl_prev + reg_prev;
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
            gb_set_flags(gb, res == 0, 1, (res & 0xF) > (prev & 0xF), UNCHANGED);
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
        } else if (b == 0x27) {
            // TODO: Improve/Fix this implementation
            gb_log_inst("DAA");
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t n = gb_get_flag(gb, Flag_N);
            uint8_t prev_h = gb_get_flag(gb, Flag_H);
            uint8_t prev_c = gb_get_flag(gb, Flag_C);
            uint8_t res = a;
            uint8_t c = 0;
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
                if (((a & 0xF) > 0x09) || prev_h) {
                    res += 0x06;
                }
                if ((a > 0x99) | prev_c) {
                    res += 0x60;
                    c = 1;
                }
            }
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, UNCHANGED, 0, c);
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
            gb_log_inst("LD A,(HL%c)", b == 0x2A ? '+' : '-');
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
                gb->halted = true;
                gb->PC += inst.size;
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
            int h = ((a & 0xF) + (r & 0xF)) > 0xF;
            int c = ((a & 0xFF) + (r & 0xFF)) > 0xFF;
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b >= 0x88 && b <= 0x8F) {
            Reg8 reg = b & 0x7;
            gb_log_inst("ADC A,%s", gb_reg_to_str(reg));
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t r = gb_get_reg(gb, reg);
            uint8_t prev_c = gb_get_flag(gb, Flag_C);
            uint8_t res = prev_c + a + r;
            int h = ((a & 0xF) + (r & 0xF) + prev_c) > 0xF;
            int c = ((a & 0xFF) + (r & 0xFF) + prev_c) > 0xFF;
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b >= 0x90 && b <= 0x97) {
            Reg8 reg = b & 0x7;
            gb_log_inst("SUB A,%s", gb_reg_to_str(reg));
            int prev = gb_get_reg(gb, REG_A);
            int res = prev - gb_get_reg(gb, reg);
            uint8_t c = prev >= gb_get_reg(gb, reg) ? 0 : 1;
            gb_set_reg(gb, REG_A, res);
            uint8_t h = (prev & 0xF) < (res & 0xF);
            gb_set_flags(gb, res == 0, 1, h, c);
            gb_set_flag(gb, Flag_C, c);
            gb->PC += inst.size;
        } else if (b >= 0x98 && b <= 0x9F) {
            Reg8 reg = b & 0x7;
            gb_log_inst("SBC A,%s", gb_reg_to_str(reg));
            uint8_t a = gb_get_reg(gb, REG_A);
            uint8_t r = gb_get_reg(gb, reg);
            uint8_t prev_c = gb_get_flag(gb, Flag_C);
            uint8_t res = (uint8_t)(a - prev_c - r);
            int h = ((a & 0xF) - (r & 0xF) - prev_c) < 0;
            int c = ((a & 0xFF) - (r & 0xFF) - prev_c) < 0;
            gb_set_reg(gb, REG_A, res);
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
            uint8_t h = (a & 0xF) < (res & 0xF);
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
            //fprintf(stderr, "%04X: %s\n", gb->PC, b == 0xF3 ? "DI" : "EI");
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
                if (gb->timer_mcycle >= (inst.max_cycles/4)*MCYCLE_MS) {
                    gb->timer_mcycle -= (inst.max_cycles/4)*MCYCLE_MS;
                    gb->SP += 2;
                    gb->PC = addr;
                }
            } else {
                gb->PC += inst.size;
                gb->timer_mcycle -= (inst.min_cycles/4)*MCYCLE_MS;
            }
        } else if (b == 0xC9) {
            uint8_t low = gb_read_memory(gb, gb->SP + 0);
            uint8_t high = gb_read_memory(gb, gb->SP + 1);
            uint16_t addr = (high << 8) | low;
            gb_log_inst("RET");
            gb->SP += 2;
            gb->PC = addr;
        } else if (b == 0xD9) {
            uint8_t low = gb_read_memory(gb, gb->SP + 0);
            uint8_t high = gb_read_memory(gb, gb->SP + 1);
            uint16_t addr = (high << 8) | low;
            gb_log_inst("RETI");
            gb->SP += 2;
            gb->IME = 1;
            gb->PC = addr;
        } else if (b == 0xC1 || b == 0xD1 || b == 0xE1) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("POP %s", gb_reg16_to_str(reg));
            uint8_t low = gb_read_memory(gb, gb->SP + 0);
            uint8_t high = gb_read_memory(gb, gb->SP + 1);
            gb_set_reg16(gb, reg, (high << 8) | low);
            gb->SP += 2;
            gb->PC += inst.size;
        } else if (b == 0xF1) {
            gb_log_inst("POP AF");
            uint8_t low = gb_read_memory(gb, gb->SP + 0) & 0xF0; // Clear the lower 4-bits
            uint8_t high = gb_read_memory(gb, gb->SP + 1);
            gb->AF = (high << 8) | low;
            gb->SP += 2;
            gb->PC += inst.size;
        } else if (b == 0xC5 || b == 0xD5 || b == 0xE5) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("PUSH %s", gb_reg16_to_str(reg));
            uint16_t value = gb_get_reg16(gb, reg);
            gb->SP -= 2;
            gb_write_memory(gb, gb->SP + 0, value & 0xff);
            gb_write_memory(gb, gb->SP + 1, value >> 8);
            gb->PC += inst.size;
        } else if (b == 0xF5) {
            gb_log_inst("PUSH AF");
            uint16_t value = gb->AF;
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
            gb_log_inst("JP HL");
            uint16_t pc = gb->HL;
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
        if (b == 0x18) {
            static bool infinite_loop = false;
            int r8 = inst.data[1] >= 0x80 ? (int8_t)inst.data[1] : inst.data[1];
            if (!infinite_loop) {
                gb_log_inst("JR %d", r8);
            }
            if (r8 == -2 && !infinite_loop) {
                fprintf(stderr, "Detected infinite loop...\n");
                fprintf(stderr, "SCX: %d, SCY: %d\n", gb->memory[rSCX], gb->memory[rSCY]);
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
            static bool infinite_loop = false;
            if (!infinite_loop) {
                gb_log_inst("JR %s,0x%02X", gb_flag_to_str(f), inst.data[1]);
            }
            if (gb_get_flag(gb, f)) {
                if (gb->timer_mcycle >= (inst.max_cycles/4)*MCYCLE_MS) {
                    gb->timer_mcycle -= (inst.max_cycles/4)*MCYCLE_MS;
                    int offset = inst.data[1] >= 0x80 ? (int8_t)inst.data[1] : inst.data[1];
                    if (offset == -2 && !infinite_loop) {
                        infinite_loop = true;
                        printf("Detected infinite loop...\n");
                    }
                    gb->PC = (gb->PC + inst.size) + offset;
                }
            } else {
                gb->PC += inst.size;
                gb->timer_mcycle -= (inst.min_cycles/4)*MCYCLE_MS;
            }
        } else if (b == 0x20) {
            gb_log_inst("JR NZ,0x%02X", inst.data[1]);
            if (!gb_get_flag(gb, Flag_Z)) {
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
            uint8_t prev = gb_get_reg(gb, REG_A);
            uint8_t res = prev + inst.data[1];
            uint8_t c = res < prev ? 1 : 0;
            gb_set_reg(gb, REG_A, res);
            uint8_t h = (prev & 0xF) > (res & 0xF);
            gb_set_flags(gb, res == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b == 0xCE) {
            gb_log_inst("ADC A,0x%02X", inst.data[1]);
            uint8_t prev = gb_get_reg(gb, REG_A);
            uint8_t prev_c = gb_get_flag(gb, Flag_C);
            uint8_t res = prev + inst.data[1] + prev_c;
            int h = ((prev & 0xF) + (inst.data[1] & 0xF) + prev_c) > 0xF;
            int c = ((prev & 0xFF) + (inst.data[1] & 0xFF) + prev_c) > 0xFF;
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b == 0xD6) {
            gb_log_inst("SUB A,0x%02X", inst.data[1]);
            uint8_t prev = gb_get_reg(gb, REG_A);
            uint8_t res = prev - inst.data[1];
            uint8_t c = prev < inst.data[1] ? 1 : 0;
            gb_set_reg(gb, REG_A, res);
            uint8_t h = (prev & 0xF) < (res & 0xF);
            gb_set_flags(gb, res == 0, 1, h, c);
            gb->PC += inst.size;
        } else if (b == 0xDE) {
            gb_log_inst("SBC A,0x%02X", inst.data[1]);
            int a = gb_get_reg(gb, REG_A);
            int n = inst.data[1];
            int prev_c = gb_get_flag(gb, Flag_C);
            uint8_t res = (uint8_t)(a - prev_c - n);
            int h = ((a & 0xF) - (n & 0xF) - prev_c) < 0;
            int c = ((a & 0xFF) - (n & 0xFF) - prev_c) < 0;
            gb_set_reg(gb, REG_A, res);
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
            int res = gb->SP + (int8_t)inst.data[1];
            int h = ((gb->SP & 0xF) + (inst.data[1] & 0xF)) > 0xF;
            int c = ((gb->SP & 0xFF) + (inst.data[1] & 0xFF)) > 0xFF;
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
            int h = ((gb->SP & 0xF) + (inst.data[1] & 0xF)) > 0xF;
            int c = ((gb->SP & 0xFF) + (inst.data[1] & 0xFF)) > 0xFF;
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
            uint8_t h = (a & 0xF) < (res & 0xF);
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
            // CB 08 => RRC B (B = 01, F = 00)
            // B = 80, F = 10
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RRC %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t res = (value >> 1) | (value << 7);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x10 && inst.data[1] <= 0x17) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RL %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t c = gb_get_flag(gb, Flag_C);
            uint8_t res = (value << 1) | (c & 1);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value >> 7);
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
        } else if (inst.data[1] >= 0x28 && inst.data[1] <= 0x2F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SRA %s", gb_reg_to_str(reg));
            uint8_t value = gb_get_reg(gb, reg);
            uint8_t res = (value & 0x80) | (value >> 1);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 1);
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
                if (gb->timer_mcycle >= (inst.max_cycles/4)*MCYCLE_MS) {
                    gb->timer_mcycle -= (inst.max_cycles/4)*MCYCLE_MS;
                    gb->PC = n;
                }
            } else {
                gb->PC += inst.size;
                gb->timer_mcycle -= (inst.min_cycles/4)*MCYCLE_MS;
            }
        } else if (b == 0xC4 || b == 0xD4 || b == 0xCC || b == 0xDC) {
            Flag f = (b >> 3) & 0x3;
            gb_log_inst("CALL %s,0x%04X", gb_flag_to_str(f), n);
            if (gb_get_flag(gb, f)) {
                if (gb->timer_mcycle >= (inst.max_cycles/4)*MCYCLE_MS) {
                    gb->timer_mcycle -= (inst.max_cycles/4)*MCYCLE_MS;
                    gb->SP -= 2;
                    uint16_t ret_addr = gb->PC + inst.size;
                    gb_write_memory(gb, gb->SP+0, ret_addr & 0xff);
                    gb_write_memory(gb, gb->SP+1, ret_addr >> 8);
                    gb->PC = n;
                }
            } else {
                gb->timer_mcycle -= (inst.min_cycles/4)*MCYCLE_MS;
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
    gb->inst_executed += 1;
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
// x - pixel coordinate
// y - pixel coordinate
static void fill_tile(GameBoy *gb, int x, int y, uint8_t *tile, bool transparency, uint8_t plt)
{
    // tile is 16 bytes
    //uint8_t bgp = gb->memory[rBGP];
    uint8_t bgp = plt;
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
                int r = (row+y) % 256;
                int c = (col+x) % 256;
                gb->display[r*256 + c] = color;
            }
        }
    }
}

static uint8_t flip_horz(uint8_t b)
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

static void flip_vert(uint8_t *tile)
{
    for (int i = 0; i < 4; i++) {
        uint8_t t = tile[2*i+0];
        tile[2*i+0] = tile[14-2*i];
        tile[14-2*i] = t;

        t = tile[2*i+1];
        tile[2*i+1] = tile[14-(2*i)+1];
        tile[14-(2*i)+1] = t;
    }
}

static void dump_tile(uint8_t *tile)
{
    for (int i = 0; i < 8; i++) {
        printf("%02X %02X\n", tile[i*2+0], tile[i*2+1]);
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
                if (bg_win_td_off == _VRAM9000) tile_idx = (int8_t)tile_idx;
                //fill_solid_tile(gb, col*8, row*8, 0xff);
                uint8_t *tile = gb->memory + bg_win_td_off + tile_idx*16;
                fill_tile(gb, col*8, row*8, tile, false, gb->memory[rBGP]);
            }
        }
    }

    // Render the Window
    if ((lcdc & LCDCF_WINON) == LCDCF_WINON) {
        //uint8_t wx = gb->memory[rWX] - 7;
        //uint8_t wy = gb->memory[rWY];
        //printf("WX: %3d, WY: %3d\n", wx, wy);
        //assert(0 && "Window rendering is not implemented");

#if 0
        uint16_t bg_tm_off = (lcdc & LCDCF_BG9C00) == LCDCF_BG9C00 ? _SCRN1 : _SCRN0;

        for (int row = wy; row < SCRN_VY_B; row++) {
            for (int col = wx; col < SCRN_VX_B; col++) {
                int tile_idx = gb->memory[bg_tm_off + row*32 + col];
                if (bg_win_td_off == _VRAM9000) tile_idx = (int8_t)tile_idx;
                fill_solid_tile(gb, col*8, row*8, 0xff);
                uint8_t *tile = gb->memory + bg_win_td_off + tile_idx*16;
                fill_tile(gb, col*8, row*8, tile, false, gb->memory[rBGP]);
            }
        }
#endif
    }

    // Render the Sprites (OBJ)
    if ((lcdc & LCDCF_OBJON) == LCDCF_OBJON) {
        uint16_t scx = gb->memory[rSCX];
        uint16_t scy = gb->memory[rSCY];
        for (int i = 0; i < OAM_COUNT; i++) {
            uint8_t y = gb->memory[_OAMRAM + i*4 + 0] - 16;
            uint8_t x = gb->memory[_OAMRAM + i*4 + 1] - 8;
            x = (x+scx) % 256;
            y = (y+scy) % 256;

            uint8_t tile_idx = gb->memory[_OAMRAM + i*4 + 2];
            uint8_t attribs = gb->memory[_OAMRAM + i*4 + 3];
            uint8_t bg_win_over = (attribs >> 7) & 1;
            uint8_t yflip = (attribs >> 6) & 1;
            uint8_t xflip = (attribs >> 5) & 1;
            uint8_t plt_idx = (attribs >> 4) & 1;

            // TODO: Support BG/Win over OBJ
            //assert(bg_win_over == 0);
            (void)bg_win_over;

            uint8_t *tile_start = gb->memory + _VRAM8000 + tile_idx*16;
            uint8_t tile[16];
            memcpy(tile, tile_start, 16);
            if (xflip) {
                for (int i = 0; i < 16; i++) {
                    uint8_t *b = tile + i;
                    *b = flip_horz(*b);
                }
            }
            if (yflip) {
                flip_vert(tile);
            }

            fill_tile(gb, x, yflip ? (y + 8) : y, tile, true, gb->memory[rOBP0+plt_idx]);

            if(lcdc & LCDCF_OBJ16) {
                uint8_t *tile_start = gb->memory + _VRAM8000 + (tile_idx+1)*16;
                uint8_t tile[16];
                memcpy(tile, tile_start, 16);

                if (xflip) {
                    for (int i = 0; i < 16; i++) {
                        uint8_t *b = tile + i;
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
}

void gb_load_boot_rom(GameBoy *gb)
{
    memcpy(gb->memory, BOOT_ROM, sizeof(BOOT_ROM));
    memcpy(gb->memory+0x104, NINTENDO_LOGO, sizeof(NINTENDO_LOGO));
    //gb->memory[0x134] = 0xe7; // $19 + $e7 = $00 => Don't lock up

    gb->PC = 0;
}

void gb_load_rom(GameBoy *gb, uint8_t *raw, size_t size)
{
    printf("  ROM Size: $%lx (%ld KiB, %ld bytes)\n", size, size / 1024, size);
    printf("  ROM Banks: #%zu\n", size / 0x4000);
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
    printf("  ROM size: $%02X %d KiB\n", header->rom_size, 32*(1 << header->rom_size));
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

    gb->cart_type = header->cart_type;
    printf("Executing...\n");

    // TODO: Handle MBC1
    gb->rom = malloc(size);
    assert(gb->rom);
    if (gb->cart_type == 0) {
        assert(size == 32*1024);
        memcpy(gb->rom, raw, size);
        gb->rom_bank_count = 2;

        memcpy(gb->memory, raw, size);
    } else if (gb->cart_type == 1) {
        memcpy(gb->rom, raw, size);
        gb->rom_bank_count = size / (16*1024);
        printf("Size: %ld, ROM Bank Count: %d\n", size, gb->rom_bank_count);

        memcpy(gb->memory, raw, 32*1024); // Copy only the first 2 banks
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

    //gb->memory[rLY] = 0x90;
    //gb->memory[rP1] = 0x00;
#endif

    gb->timer_div = (1000.0 / 16384.0);
}

void gb_load_rom_file(GameBoy *gb, const char *path)
{
    printf("Loading ROM \"%s\"...\n", path);
    size_t size;
    uint8_t *raw = read_entire_file(path, &size);
    gb_load_rom(gb, raw, size);
    //gb->step_debug = true;
    free(raw);
}

void gb_tick(GameBoy *gb, double dt_ms)
{
    if (gb->paused) return;
    gb->elapsed_ms += dt_ms;
    gb->timer_sec -= dt_ms;
    gb->timer_div -= dt_ms;
    gb->timer_tima -= dt_ms;
    gb->timer_ly += dt_ms;

    gb->timer_mcycle += dt_ms;
    Inst inst = gb_fetch_inst(gb);

    if (gb->timer_mcycle < (inst.min_cycles/4)*MCYCLE_MS) {
        return;
    }

    if (inst.min_cycles == inst.max_cycles) {
        gb->timer_mcycle -= (inst.min_cycles/4)*MCYCLE_MS;
    }

    if (!gb->step_debug || gb->next_inst) {
        if (gb->next_inst) gb->next_inst = false;

        gb_exec(gb, inst);

        if (gb->timer_ly > 0.1089) {
            gb->timer_ly -= 0.1089;
            // VSync ~60Hz
            // 60*153 ~9180 times/s (run every 0.1089 ms)
            gb->memory[rLY] += 1;
            if (gb->memory[rLY] == gb->memory[rLYC]) {
                gb->memory[rSTAT] |= 0x04; 
                if (gb->memory[rIE] & 0x02) {
                    gb->memory[rIF] |= 0x02;
                }
            } else {
                gb->memory[rSTAT] &= ~0x04;
            }
            if (gb->memory[rLY] > 153) {
                gb->memory[rIF] |= 0x01;
                // Run this line 60 times/s (60Hz)
                gb->memory[rLY] = 0;
            }
        }
    }

    // Copy tiles to display
    if (gb->memory[rLY] == 144) {
        gb_render(gb);
    }

    // Increase rDIV at a rate of 16384Hz (every 0.06103515625 ms)
    while (gb->timer_div < 0) {
        gb->memory[rDIV] += 1;
        gb->timer_div += 1000.0 / 16384;
    }

    // Timer Enabled in TAC
    if (gb->memory[rTAC] & 0x04) {
        // Update TIMA (timer counter)
        int freq = gb_clock_freq(gb->memory[rTAC] & 3);
        if (gb->timer_tima <= 0) {
            gb->memory[rTIMA] += 1;
            gb->timer_tima += 1000.0 / freq; 
            //fprintf(stderr, "Increasing TIMA %02X -> %02X\n", (uint8_t)(gb->memory[rTIMA] - 1), gb->memory[rTIMA]);
            if (gb->memory[rTIMA] == 0) {
                //fprintf(stderr, "Reseting TIMA to %02X (TMA)\n", gb->memory[rTMA]);
                gb->memory[rTIMA] = gb->memory[rTMA];
                // Trigger interrupt
                gb->memory[rIF] |= 0x04;
                //gb_trigger_interrupt(gb);
            }
        }
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
