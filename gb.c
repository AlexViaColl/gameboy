#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gb.h"

#include "platform.c"

/*
BACKLOG:
[x] - Fix palette in Dr. Mario
[ ] - LY and STAT should not automatically update when the LCD/PPU is OFF LCDC.7 = 0
[ ] - Crash due to Illegal Instruction (0xFD) - Super Mario Land
        It seems that the incorrect Bank (2) is loaded into $4000-$8000 and we
        try to execute an invalid instruction.
        Most probably, the reason is we have not implemented timing/interrupts correctly
[ ] - STAT register does not reflect the mode (0, 1, 2, 3)
[ ] - Implement Window rendering
    [ ] - Alleyway
    [ ] - Super Mario Land => Game Over screen
    [ ] - Ms. Pacman
[ ] - Render by lines instead of the entire frame at once (SCX might change between lines!!!)
[ ] - Other MBC's
    [ ] - MBC1+RAM+BATTERY ($03)
    [ ] - MBC3+RAM+BATTERY ($13)
[ ] - Music
*/

const Color PALETTE[] = {0xE0F8D0FF, 0x88C070FF, 0x346856FF, 0x081820FF};
//const Color PALETTE[] = {0xFFFFFFFF, 0xC0C0C0FF, 0x404040FF, 0x000000FF};

typedef struct RomHeader {
    u8 entry[4];       // 0100-0103 (4)
    u8 logo[48];       // 0104-0133 (48)
    char title[16];    // 0134-0143 (16)
                       // 013F-0142 (4)    Manufacturer code in new cartridges
                       // 0143      (1)    CGB flag ($80 CGB compat, $C0 CGB only)
    u8 new_licensee[2];// 0144-0145 (2)    Nintendo, EA, Bandai, ...
    u8 sgb;            // 0146      (1)
    u8 cart_type;      // 0147      (1)    ROM Only, MBC1, MBC1+RAM, ...
    u8 rom_size;       // 0148      (1)    32KiB, 64KiB, 128KiB, ...
    u8 ram_size;       // 0149      (1)    0, 8KiB, 32KiB, ...
    u8 dest_code;      // 014A      (1)    Japan / Overseas
    u8 old_licensee;   // 014B      (1)
    u8 mask_version;   // 014C      (1)    Usually 0
    u8 header_check;   // 014D      (1)    Check of bytes 0134-014C (Boot ROM)
    u8 global_check[2];// 014E-014F (2)    Not verified
} RomHeader;

u8 *read_entire_file(const char *path, size_t *size);

void gb_dump(const GameBoy *gb)
{
    u8 flags = gb->AF & 0xff;
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

u8 gb_mem_read(const GameBoy *gb, u16 addr)
{
    return gb->memory[addr];
}

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
    else if (addr == rSC) gb->memory[addr] = 0xff;
}

void gb_timer_write(GameBoy *gb, u16 addr, u8 value)
{
    assert(addr == rDIV || addr == rTIMA || addr == rTMA || addr == rTAC);
    if (0) {}
    else if (addr == rDIV)  gb->memory[addr] = 0;
    else if (addr == rTIMA) gb->memory[addr] = value;
    else if (addr == rTMA)  gb->memory[addr] = value;
    else if (addr == rTAC)  gb->memory[addr] = value;
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

static int gb_clock_freq(int clock)
{
    int freq[] = {4096, 262144, 65536, 16384};
    return freq[clock & 3];
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
        case rIE:   gb->memory[addr] = value; break;

        default: break; // assert(0 && "Unreachable");
    }
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
                //fprintf(stderr, "RAM Enable\n");
                gb->ram_enabled = true;
                //assert(0);
            } else {
                //fprintf(stderr, "RAM Disable\n");
                gb->ram_enabled = false;
            }
        } else if (addr <= 0x3FFF) {
            value = value & 0x1F; // Consider only lower 5-bits
            if (value == 0) value = 1;
            gb->rom_bank_num = value % gb->rom_bank_count;
            //fprintf(stderr, "$%04X: ROM Bank Number: %02X\n", gb->PC, gb->rom_bank_num);
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

u8 gb_get_flag(const GameBoy *gb, Flag flag)
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

void gb_set_flag(GameBoy *gb, Flag flag, u8 value)
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

void gb_set_reg(GameBoy *gb, Reg8 r8, u8 value)
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
            gb_mem_write(gb, gb->HL, value);
            break;
        case REG_A/*7*/:
            gb->AF &= 0x00ff;
            gb->AF |= (value << 8);
            break;
        default:
            assert(0 && "Invalid register");
    }
}

u8 gb_get_reg(const GameBoy *gb, Reg8 r8)
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
            return gb_mem_read(gb, gb->HL);
        case REG_A/*7*/:
            return (gb->AF >> 8);
        default:
            assert(0 && "Invalid register");
    }
}

void gb_set_reg16(GameBoy *gb, Reg16 r16, u16 value)
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

u16 gb_get_reg16(const GameBoy *gb, Reg16 r16)
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

Inst gb_fetch_inst(const GameBoy *gb)
{
    u8 b = gb_mem_read(gb, gb->PC);
    const u8 *data = &gb->memory[gb->PC];

    if (b == 0xD3 || b == 0xDB || b == 0xDD || b == 0xE3 || b == 0xE4 ||
        b == 0xEB || b == 0xEC || b == 0xED || b == 0xF4 || b == 0xFC || b == 0xFD)
    {
        gb_dump(gb);
        fprintf(stderr, "Illegal Instruction 0x%02X\n", b);
        exit(1);
    }

    // 1-byte instructions
    if (b == 0x00 || b == 0x76 || b == 0xF3 || b == 0xFB) { // NOP, HALT, DI, EI
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
        u8 cycles = is_ld_hl ? 8 : 4;
        return (Inst){.data = data, .size = 1, .min_cycles = cycles, .max_cycles = cycles};
    } else if (b >= 0x80 && b <= 0xBF) {
        bool reads_hl = (b >> 6) == 2 && (b & 7) == 6;
        u8 cycles = reads_hl ? 8 : 4;
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
    else if (b == 0x10) { // STOP
        return (Inst){.data = data, .size = 2, .min_cycles = 4, .max_cycles = 4};
    } else if ((b >> 6) == 0 && (b & 7) == 6) { // LD reg8,d8|LD (HL),d8
        u8 cycles = b == 0x36 ? 12 : 8;
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
        u8 b2 = gb_mem_read(gb, gb->PC+1);
        u8 cycles = (b2 & 7) == 6 ? 16 : 8;
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
            u8 n = ((b >> 3) & 0x7)*8;
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
        u8 b2 = inst.data[1];
        Reg8 reg = b2 & 7;
        u8 bit = (b2 >> 3) & 7;
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

bool gb_button_down(GameBoy *gb)
{
    return gb->button_a || gb->button_b || gb->button_start || gb->button_select ||
        gb->dpad_up || gb->dpad_down || gb->dpad_left || gb->dpad_right;
}

bool gb_exec(GameBoy *gb, Inst inst)
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
            return true;
        }
    }

    u8 IE = gb->memory[rIE];
    u8 IF = gb->memory[rIF];
    if (gb->halted) {
        if ((IF & IE) == 0) return false;
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
            return true;
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
            return true;
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
            return true;
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
            return true;
        }

        //if (gb->ime_cycles == 1) {
        //    gb->ime_cycles -= 1;
        //} else if (gb->ime_cycles == 0 && gb->memory[rLY] == 0) {
        //    // Handle interrupt
        //    gb->SP -= 2;
        //    u16 ret_addr = gb->PC;
        //    gb_mem_write(gb, gb->SP+0, ret_addr & 0xff);
        //    gb_mem_write(gb, gb->SP+1, ret_addr >> 8);
        //    gb->PC = 0x0040; // VBlank

        //    gb->IME = 0;
        //    return;
        //}
    }

    if (gb->timer_mcycle < (inst.min_cycles/4)*MCYCLE_MS) {
        return false;
    }

    if (inst.min_cycles == inst.max_cycles) {
        gb->timer_mcycle -= (inst.min_cycles/4)*MCYCLE_MS;
    }

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
            gb_log_inst("INC %s", gb_reg_to_str(reg));
            u8 prev = gb_get_reg(gb, reg);
            u8 res = prev + 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, (res & 0xF) < (prev & 0xF), UNCHANGED);
            gb->PC += inst.size;
        } else if ( // DEC reg
            b == 0x05 || b == 0x15 || b == 0x25 || b == 0x35 ||
            b == 0x0D || b == 0x1D || b == 0x2D || b == 0x3D
        ) {
            Reg8 reg = (b >> 3) & 0x7;
            gb_log_inst("DEC %s", gb_reg_to_str(reg));
            u8 prev = gb_get_reg(gb, reg);
            int res = prev - 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 1, (res & 0xF) > (prev & 0xF), UNCHANGED);
            gb->PC += inst.size;
        } else if (b == 0x07) {
            gb_log_inst("RLCA");
            u8 prev = gb_get_reg(gb, REG_A);
            u8 res = (prev << 1) | (prev >> 7);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, 0, 0, 0, prev >> 7);
            gb->PC += inst.size;
        } else if (b == 0x0F) {
            gb_log_inst("RRCA");
            u8 a = gb_get_reg(gb, REG_A);
            u8 c = a & 1;
            u8 res = (a >> 1) | (c << 7);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, 0, 0, 0, c);
            gb->PC += inst.size;
        } else if (b == 0x17) {
            gb_log_inst("RLA");
            u8 prev = gb_get_reg(gb, REG_A);
            u8 res = (prev << 1) | gb_get_flag(gb, Flag_C);
            u8 c = prev >> 7;
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, 0, 0, 0, c);
            gb->PC += inst.size;
        } else if (b == 0x1F) {
            gb_log_inst("RRA");
            u8 a = gb_get_reg(gb, REG_A);
            u8 c = a & 1;
            u8 res = (a >> 1) | (gb_get_flag(gb, Flag_C) << 7);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, 0, 0, 0, c);
            gb->PC += inst.size;
        } else if (b == 0x27) {
            // TODO: Improve/Fix this implementation
            gb_log_inst("DAA");
            u8 a = gb_get_reg(gb, REG_A);
            u8 n = gb_get_flag(gb, Flag_N);
            u8 prev_h = gb_get_flag(gb, Flag_H);
            u8 prev_c = gb_get_flag(gb, Flag_C);
            u8 res = a;
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
            u8 a = gb_get_reg(gb, REG_A);
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
            u8 value = gb_mem_read(gb, gb_get_reg16(gb, reg));
            gb_set_reg(gb, REG_A, value);
            gb->PC += inst.size;
        } else if (b == 0x02 || b == 0x12) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("LD (%s),A", gb_reg16_to_str(reg));
            u16 addr = gb_get_reg16(gb, reg);
            u8 value = gb_get_reg(gb, REG_A);
            gb_mem_write(gb, addr, value);
            gb->PC += inst.size;
        } else if (b == 0x22 || b == 0x32) {
            gb_log_inst("LD (HL%c),A", b == 0x22 ? '+' : '-');
            u8 a = gb_get_reg(gb, REG_A);
            gb_mem_write(gb, gb->HL, a);
            if (b == 0x22) gb->HL += 1;
            else gb->HL -= 1;
            gb->PC += inst.size;
        } else if (b == 0x0A || b == 0x1A) {
            Reg16 reg = (b >> 4) & 0x3;
            gb_log_inst("LD A,(%s)", gb_reg16_to_str(reg));
            u16 addr = gb_get_reg16(gb, reg);
            u8 value = gb_mem_read(gb, addr);
            gb_set_reg(gb, REG_A, value);
            gb->PC += inst.size;
        } else if (b == 0x2A || b == 0x3A) {
            gb_log_inst("LD A,(HL%c)", b == 0x2A ? '+' : '-');
            u8 value = gb_mem_read(gb, gb->HL);
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
                return true;
            }
            Reg8 src = b & 0x7;
            Reg8 dst = (b >> 3) & 0x7;
            gb_log_inst("LD %s,%s", gb_reg_to_str(dst), gb_reg_to_str(src));
            u8 value = gb_get_reg(gb, src);
            gb_set_reg(gb, dst, value);
            gb->PC += inst.size;
        } else if (b >= 0x80 && b <= 0x87) {
            Reg8 reg = b & 0x7;
            gb_log_inst("ADD A,%s", gb_reg_to_str(reg));
            u8 a = gb_get_reg(gb, REG_A);
            u8 r = gb_get_reg(gb, reg);
            u8 res = a + r;
            int h = ((a & 0xF) + (r & 0xF)) > 0xF;
            int c = ((a & 0xFF) + (r & 0xFF)) > 0xFF;
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b >= 0x88 && b <= 0x8F) {
            Reg8 reg = b & 0x7;
            gb_log_inst("ADC A,%s", gb_reg_to_str(reg));
            u8 a = gb_get_reg(gb, REG_A);
            u8 r = gb_get_reg(gb, reg);
            u8 prev_c = gb_get_flag(gb, Flag_C);
            u8 res = prev_c + a + r;
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
            u8 c = prev >= gb_get_reg(gb, reg) ? 0 : 1;
            gb_set_reg(gb, REG_A, res);
            u8 h = (prev & 0xF) < (res & 0xF);
            gb_set_flags(gb, res == 0, 1, h, c);
            gb_set_flag(gb, Flag_C, c);
            gb->PC += inst.size;
        } else if (b >= 0x98 && b <= 0x9F) {
            Reg8 reg = b & 0x7;
            gb_log_inst("SBC A,%s", gb_reg_to_str(reg));
            u8 a = gb_get_reg(gb, REG_A);
            u8 r = gb_get_reg(gb, reg);
            u8 prev_c = gb_get_flag(gb, Flag_C);
            u8 res = (u8)(a - prev_c - r);
            int h = ((a & 0xF) - (r & 0xF) - prev_c) < 0;
            int c = ((a & 0xFF) - (r & 0xFF) - prev_c) < 0;
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 1, h, c);
            gb->PC += inst.size;
        } else if (b >= 0xB0 && b <= 0xB7) {
            Reg8 reg = b & 0x7;
            gb_log_inst("OR %s", gb_reg_to_str(reg));
            u8 res = gb_get_reg(gb, REG_A) | gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (b >= 0xB8 && b <= 0xBF) {
            Reg8 reg = b & 0x7;
            gb_log_inst("CP %s", gb_reg_to_str(reg));
            int a = (int)gb_get_reg(gb, REG_A);
            int n = (int)gb_get_reg(gb, reg);
            int res = a - n;
            u8 h = (a & 0xF) < (res & 0xF);
            gb_set_flags(gb, res == 0, 1, h, a < n);
            gb->PC += inst.size;
        } else if (b >= 0xA0 && b <= 0xA7) {
            Reg8 reg = b & 0x7;
            gb_log_inst("AND %s", gb_reg_to_str(reg));
            u8 res = gb_get_reg(gb, REG_A) & gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, 1, 0);
            gb->PC += inst.size;
        } else if (b >= 0xA8 && b <= 0xAF) {
            Reg8 reg = b & 0x7;
            gb_log_inst("XOR %s", gb_reg_to_str(reg));
            u8 res = gb_get_reg(gb, REG_A) ^ gb_get_reg(gb, reg);
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (b == 0xE2) {
            gb_log_inst("LD(0xFF00+%02X),%02X", gb_get_reg(gb, REG_C), gb_get_reg(gb, REG_A));
            u8 a = gb_get_reg(gb, REG_A);
            u8 c = gb_get_reg(gb, REG_C);
            gb_mem_write(gb, 0xFF00 + c, a);
            gb->PC += inst.size;
        } else if (b == 0xF2) {
            gb_log_inst("LD A,(C)");
            u8 c = gb_get_reg(gb, REG_C);
            u8 value = gb_mem_read(gb, 0xFF00 + c);
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
            u8 low = gb_mem_read(gb, gb->SP + 0);
            u8 high = gb_mem_read(gb, gb->SP + 1);
            u16 addr = (high << 8) | low;
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
            u8 low = gb_mem_read(gb, gb->SP + 0) & 0xF0; // Clear the lower 4-bits
            u8 high = gb_mem_read(gb, gb->SP + 1);
            gb->AF = (high << 8) | low;
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
            u16 value = gb->AF;
            gb->SP -= 2;
            gb_mem_write(gb, gb->SP + 0, value & 0xff);
            gb_mem_write(gb, gb->SP + 1, value >> 8);
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
            u8 value = gb_get_reg(gb, REG_A);
            gb_mem_write(gb, 0xFF00 + inst.data[1], value);
            gb->PC += inst.size;
        } else if (b == 0xC6) {
            gb_log_inst("ADD A,0x%02X", inst.data[1]);
            u8 prev = gb_get_reg(gb, REG_A);
            u8 res = prev + inst.data[1];
            u8 c = res < prev ? 1 : 0;
            gb_set_reg(gb, REG_A, res);
            u8 h = (prev & 0xF) > (res & 0xF);
            gb_set_flags(gb, res == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b == 0xCE) {
            gb_log_inst("ADC A,0x%02X", inst.data[1]);
            u8 prev = gb_get_reg(gb, REG_A);
            u8 prev_c = gb_get_flag(gb, Flag_C);
            u8 res = prev + inst.data[1] + prev_c;
            int h = ((prev & 0xF) + (inst.data[1] & 0xF) + prev_c) > 0xF;
            int c = ((prev & 0xFF) + (inst.data[1] & 0xFF) + prev_c) > 0xFF;
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, h, c);
            gb->PC += inst.size;
        } else if (b == 0xD6) {
            gb_log_inst("SUB A,0x%02X", inst.data[1]);
            u8 prev = gb_get_reg(gb, REG_A);
            u8 res = prev - inst.data[1];
            u8 c = prev < inst.data[1] ? 1 : 0;
            gb_set_reg(gb, REG_A, res);
            u8 h = (prev & 0xF) < (res & 0xF);
            gb_set_flags(gb, res == 0, 1, h, c);
            gb->PC += inst.size;
        } else if (b == 0xDE) {
            gb_log_inst("SBC A,0x%02X", inst.data[1]);
            int a = gb_get_reg(gb, REG_A);
            int n = inst.data[1];
            int prev_c = gb_get_flag(gb, Flag_C);
            u8 res = (u8)(a - prev_c - n);
            int h = ((a & 0xF) - (n & 0xF) - prev_c) < 0;
            int c = ((a & 0xFF) - (n & 0xFF) - prev_c) < 0;
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 1, h, c);
            gb->PC += inst.size;
        } else if (b == 0xE6) {
            gb_log_inst("AND A,0x%02X", inst.data[1]);
            u8 res = gb_get_reg(gb, REG_A) & inst.data[1];
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
            u8 value = gb_mem_read(gb, 0xFF00 + inst.data[1]);
            gb_set_reg(gb, REG_A, value);
            gb->PC += inst.size;
        } else if (b == 0xF6) {
            gb_log_inst("OR A,0x%02X", inst.data[1]);
            u8 res = gb_get_reg(gb, REG_A) | inst.data[1];
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
            u8 res = gb_get_reg(gb, REG_A) ^ inst.data[1];
            gb_set_reg(gb, REG_A, res);
            gb_set_flags(gb, res == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (b == 0xFE) {
            gb_log_inst("CP 0x%02X", inst.data[1]);
            int a = (int)gb_get_reg(gb, REG_A);
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
            gb_log_inst("RLC %s", gb_reg_to_str(reg));
            u8 value = gb_get_reg(gb, reg);
            u8 res = (value << 1) | (value >> 7);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value >> 7);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x08 && inst.data[1] <= 0x0F) {
            // CB 08 => RRC B (B = 01, F = 00)
            // B = 80, F = 10
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RRC %s", gb_reg_to_str(reg));
            u8 value = gb_get_reg(gb, reg);
            u8 res = (value >> 1) | (value << 7);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x10 && inst.data[1] <= 0x17) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RL %s", gb_reg_to_str(reg));
            u8 value = gb_get_reg(gb, reg);
            u8 c = gb_get_flag(gb, Flag_C);
            u8 res = (value << 1) | (c & 1);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value >> 7);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x18 && inst.data[1] <= 0x1F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RR %s", gb_reg_to_str(reg));
            u8 value = gb_get_reg(gb, reg);
            u8 c = gb_get_flag(gb, Flag_C);
            u8 res = (value >> 1) | (c << 7);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x20 && inst.data[1] <= 0x27) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SLA %s", gb_reg_to_str(reg));
            u8 value = gb_get_reg(gb, reg);
            u8 res = value << 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value >> 7);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x28 && inst.data[1] <= 0x2F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SRA %s", gb_reg_to_str(reg));
            u8 value = gb_get_reg(gb, reg);
            u8 res = (value & 0x80) | (value >> 1);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x30 && inst.data[1] <= 0x37) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SWAP %s", gb_reg_to_str(reg));
            u8 value = gb_get_reg(gb, reg);
            u8 res = ((value & 0xF) << 4) | ((value & 0xF0) >> 4);
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, 0);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x38 && inst.data[1] <= 0x3F) {
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SRL %s", gb_reg_to_str(reg));
            u8 value = gb_get_reg(gb, reg);
            u8 res = value >> 1;
            gb_set_reg(gb, reg, res);
            gb_set_flags(gb, res == 0, 0, 0, value & 0x1);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x40 && inst.data[1] <= 0x7F) {
            u8 b = (inst.data[1] >> 3) & 0x7;
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("BIT %d,%s", b, gb_reg_to_str(reg));
            u8 value = (gb_get_reg(gb, reg) >> b) & 1;
            gb_set_flags(gb, value == 0, 0, 1, UNCHANGED);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0x80 && inst.data[1] <= 0xBF) {
            u8 b = (inst.data[1] >> 3) & 0x7;
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("RES %d,%s", b, gb_reg_to_str(reg));
            u8 value = gb_get_reg(gb, reg) & ~(1 << b);
            gb_set_reg(gb, reg, value);
            gb->PC += inst.size;
        } else if (inst.data[1] >= 0xC0) {
            u8 b = (inst.data[1] >> 3) & 0x7;
            Reg8 reg = inst.data[1] & 0x7;
            gb_log_inst("SET %d,%s", b, gb_reg_to_str(reg));
            u8 value = gb_get_reg(gb, reg) | (1 << b);
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
            gb_mem_write(gb, n+0, gb->SP & 0xff);
            gb_mem_write(gb, n+1, gb->SP >> 8);
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
                    u16 ret_addr = gb->PC + inst.size;
                    gb_mem_write(gb, gb->SP+0, ret_addr & 0xff);
                    gb_mem_write(gb, gb->SP+1, ret_addr >> 8);
                    gb->PC = n;
                }
            } else {
                gb->timer_mcycle -= (inst.min_cycles/4)*MCYCLE_MS;
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
            u8 a = gb_get_reg(gb, REG_A);
            gb_mem_write(gb, n, a);
            gb->PC += inst.size;
        } else if (b == 0xFA) {
            gb_log_inst("LD A,(0x%04X)", n);
            u8 value = gb_mem_read(gb, n);
            gb_set_reg(gb, REG_A, value);
            gb->PC += inst.size;
        } else {
            assert(0 && "Not implemented");
        }
    }

    assert(gb->PC <= 0xFFFF);
    gb->inst_executed += 1;

    return true;
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
    RomHeader *header = (RomHeader*)(raw + 0x100);
    printf("  ROM Size:          $%lx (%ld KiB, %ld bytes)\n", size, size / 1024, size);
    printf("  ROM Banks:         #%zu\n", size / 0x4000);
    if (strlen(header->title)) {
        printf("  Title:             %s\n", header->title);
    } else {
        printf("  Title:");
        for (int i = 0; i < 16; i++) printf(" %02X", header->title[i]);
        printf("\n");
    }
    if (memcmp(header->logo, NINTENDO_LOGO, sizeof(NINTENDO_LOGO)) != 0) {
        fprintf(stderr, "Nintendo Logo does NOT match\n");
        exit(1);
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
    u8 checksum = 0;
    for (u16 addr = 0x0134; addr <= 0x014C; addr++) {
        checksum = checksum - raw[addr] - 1;
    }
    if (header->header_check != checksum) {
        fprintf(stderr, "    Checksum does NOT match: %02X vs. %02X\n", header->header_check, checksum);
        exit(1);
    }

    printf("  Global checksum:   $%02X $%02X\n", header->global_check[0], header->global_check[1]);
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
    } else if (gb->cart_type == 1 || gb->cart_type == 3 || gb->cart_type == 0x13) {
        memcpy(gb->rom, raw, size);
        gb->rom_bank_count = size / (16*1024);
        printf("Size: %ld, ROM Bank Count: %d\n", size, gb->rom_bank_count);

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

    //gb->memory[rLY] = 0x90;
    //gb->memory[rP1] = 0x00;
    gb->memory[rLCDC] = 0x91;
    gb->memory[rSTAT] = 0x85;
#endif

    gb->timer_div = (1000.0 / 16384.0);
}

void gb_load_rom_file(GameBoy *gb, const char *path)
{
    printf("Loading ROM \"%s\"...\n", path);
    size_t size;
    u8 *raw = read_entire_file(path, &size);
    gb_load_rom(gb, raw, size);
    free(raw);
}

void gb_tick(GameBoy *gb, f64 dt_ms)
{
    if (gb->paused) return;
    gb->elapsed_ms += dt_ms;
    gb->timer_div  -= dt_ms;
    gb->timer_tima -= dt_ms;
    gb->timer_ly   += dt_ms;

    gb->timer_mcycle += dt_ms;

    gb->dots = (gb->dots + MS_TO_DOTS(dt_ms)) % DOTS_PER_FRAME;
    u64 scanline_dots = (gb->dots % DOTS_PER_SCANLINE);
    //u8 ly = gb->memory[rLY];
    int ly = gb->dots / DOTS_PER_SCANLINE;
    if (ly > 143) {
        gb->memory[rSTAT] |= 0x1;   // Mode 1 (VBlank)
    } else if (scanline_dots < 80) {
        gb->memory[rSTAT] |= 0x2;   // Mode 2 (OAM scan)
    } else if (scanline_dots < (80+172)) {
        gb->memory[rSTAT] |= 0x3;   // Mode 3 (LCD transfer)
    } else {
        gb->memory[rSTAT] &= ~0x3;  // Mode 0 (HBlank)
    }

    for (int i = 0; i < 50; i++) {
        Inst inst = gb_fetch_inst(gb);
        if (!gb_exec(gb, inst)) break;
    }

    if (gb->timer_ly > 0.1089) {
        gb->timer_ly -= 0.1089;
        // VSync ~60Hz
        // 60*153 ~9180 times/s (run every 0.1089 ms)
        if (gb->memory[rLY] == gb->memory[rLYC]) {
            gb->memory[rSTAT] |= 0x04;
            if (gb->memory[rIE] & 0x02) {
                gb->memory[rIF] |= 0x02;
            }
        } else {
            gb->memory[rSTAT] &= ~0x04;
        }
        if ((gb->memory[rLCDC] & LCDCF_ON) == 0) {
            gb->memory[rLY] = 0;
            gb->memory[rSTAT] &= 0xF8; // Clear low 3-bits
        } else {
            gb->memory[rLY] += 1;
        }
        if (gb->memory[rLY] > 153) {
            gb->memory[rIF] |= 0x01;
            // Run this line 60 times/s (60Hz)
            gb->memory[rLY] = 0;
        } else if (gb->memory[rLY] == 144) {
            // VBlank start
        }
    }

    // Copy tiles to display
    //gb_render(gb);

    assert(gb->memory[rLY] <= 153);
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
            //fprintf(stderr, "Increasing TIMA %02X -> %02X\n", (u8)(gb->memory[rTIMA] - 1), gb->memory[rTIMA]);
            if (gb->memory[rTIMA] == 0) {
                //fprintf(stderr, "Reseting TIMA to %02X (TMA)\n", gb->memory[rTMA]);
                gb->memory[rTIMA] = gb->memory[rTMA];
                // Trigger interrupt
                gb->memory[rIF] |= 0x04;
            }
        }
    }
}
