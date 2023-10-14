#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "gb.h"

//////////////////////////////////////////////////////////////////////

// printf("\x1B[1m\x1B[92m%s\x1B[0m\n", "PASS");
// printf("\x1B[1m\x1B[91m%s\x1B[0m\n", "FAIL");

#define test_begin \
    bool pass = true;               \
    (void)pass;                     \
    printf("%-50s", __func__);

#define test_begin2 \
    bool pass = true;               \
    (void)pass;                     \
    printf("%s", __func__);

#define test_end \
    if (pass) printf("\x1B[1m\x1B[92m%s\x1B[0m\n", "PASS");     \
    else exit(1);

#undef assert
#define assert(b) do {                                          \
    if (!(b)) {                                                 \
        printf("\x1B[1m\x1B[91m%s\x1B[0m\n", "FAIL");           \
        printf("  %s:%d: %s\n", __FILE__, __LINE__, __func__);  \
        printf("  Expected: %s\n", #b);                         \
        pass = false;                                           \
        exit(1); \
    }                                                           \
} while (0)

//////////////////////////////////////////////////////////////////////

void test_fetch(void)
{
    test_begin
    GameBoy gb = {0};
    for (int b = 0x00; b <= 0xFF; b++) {
        if (b == 0xD3 || b == 0xDB || b == 0xDD || b == 0xE3 || b == 0xE4 ||
            b == 0xEB || b == 0xEC || b == 0xED || b == 0xF4 || b == 0xFC || b == 0xFD)
        {
            continue;
        }

        gb.memory[0] = (u8)b;
        gb_fetch(&gb);
    }
    test_end
}

void test_inst_nop(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (u8*)"\x00", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    test_end
}

void test_inst_stop(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (u8*)"\x10", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    test_end
}

void test_inst_ld_reg16_n(Reg16 reg)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    u8 data[] = {0x01, 0x12, 0x34};
    data[0] |= (reg << 4);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 3);
    assert(gb_get_reg16(&gb, reg) == 0x3412);
    test_end
}

void test_inst_ld_reg16_mem_a(Reg16 reg)
{
    test_begin
    u16 start_pc = 0x0032;
    u16 addr = 0xC234;
    u8 a = 0x99;
    GameBoy gb = {0};
    gb_set_reg16(&gb, reg, addr);
    gb_set_reg(&gb, REG_A, a);
    u8 data[] = {0x02};
    data[0] |= (reg << 4);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb.memory[addr] == a);
    test_end
}

void test_inst_ld_a_reg16_mem(Reg16 reg)
{
    test_begin
    u16 start_pc = 0x0032;
    u16 addr = 0x1234;
    u8 a = 0x33;
    GameBoy gb = {0};
    gb_set_reg16(&gb, reg, addr);
    gb.memory[addr] = a;

    u8 data[] = {0x0A};
    data[0] |= (reg << 4);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg8(&gb, REG_A) == a);
    test_end
}

void test_inst_ldi_hl_mem_a(void)
{
    test_begin
    u16 start_pc = 0x0032;
    u16 value = 0xC234;
    u8 a = 0x99;
    GameBoy gb = {0};
    gb.HL = value;
    gb_set_reg(&gb, REG_A, a);
    u8 data[] = {0x22};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb.memory[value] == a);
    assert(gb.HL == value + 1);
    test_end
}

void test_inst_ldi_a_hl_mem(void)
{
    test_begin
    u16 start_pc = 0x0032;
    u16 value = 0x1234;
    u8 a = 0x99;
    GameBoy gb = {0};
    gb.HL = value;
    gb.memory[gb.HL] = a;
    u8 data[] = {0x2A};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg8(&gb, REG_A) == gb.memory[value]);
    assert(gb.HL == value + 1);
    test_end
}

void test_inst_ldd_hl_mem_a(void)
{
    test_begin
    u16 start_pc = 0x0032;
    u16 value = 0xC234;
    u8 a = 0x99;
    GameBoy gb = {0};
    gb.HL = value;
    gb_set_reg(&gb, REG_A, a);
    u8 data[] = {0x32};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb.memory[value] == a);
    assert(gb.HL == value - 1);
    test_end
}

void test_inst_ldd_a_hl_mem(void)
{
    test_begin
    u16 start_pc = 0x0032;
    u16 value = 0x1234;
    u8 a = 0x99;
    GameBoy gb = {0};
    gb.HL = value;
    gb.memory[gb.HL] = a;
    u8 data[] = {0x3A};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg8(&gb, REG_A) == gb.memory[value]);
    assert(gb.HL == value - 1);
    test_end
}

void test_inst_inc_reg16(Reg16 reg)
{
    test_begin
    //printf("(%-50s)", gb_reg16_to_str(reg));
    u16 start_pc = 0x0032;
    u16 value = 0x1234;
    GameBoy gb = {0};
    gb_set_reg16(&gb, reg, value);
    u8 data[] = {0x03};
    data[0] |= (reg << 4);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg16(&gb, reg) == value + 1);
    test_end
}

void test_inst_inc_reg8(Reg8 reg)
{
    test_begin
    u16 start_pc = 0x0032;
    u8 value = 0xFF;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    gb_set_reg(&gb, reg, value);
    gb_set_flag(&gb, Flag_N, 1);
    u8 data[] = {0x04};
    data[0] |= (reg << 3);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg8(&gb, reg) == (u8)(value + 1));
    // assert_flags(gb, "Z0H-");
    assert(gb_get_flag(&gb, Flag_Z) == 1);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 1);
    test_end
}

void test_inst_dec_reg8(Reg8 reg)
{
    test_begin
    u16 start_pc = 0x0032;
    u8 value = 0xFF;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    gb_set_reg(&gb, reg, value); // REG = $FF
    u8 data[] = {0x05};
    data[0] |= (reg << 3);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg8(&gb, reg) == (u8)(value - 1));
    // assert_flags(gb, "Z0H-");
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 1);
    assert(gb_get_flag(&gb, Flag_H) == 0);
    test_end
}

void test_inst_ld_reg8_n(Reg8 reg)
{
    test_begin
    //printf("(%-50s)", gb_reg_to_str(reg));
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    u8 data[] = {0x06, 0x78};
    data[0] |= (reg << 3);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 2);
    assert(gb_get_reg8(&gb, reg) == 0x78);
    test_end
}

void test_inst_rot(GameBoy *gb, u8 opcode, u8 value)
{
    u16 start_pc = 0x0032;
    u8 data[] = {opcode};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb->PC = start_pc;
    gb_set_reg(gb, REG_A, value);

    gb_exec(gb, inst);

    bool pass = true;
    (void)pass;
    assert(gb->PC == start_pc + 1);
}

void test_inst_rlca(void)
{
    test_begin
    GameBoy gb = {0};
    test_inst_rot(&gb, 0x07, 0x81);
    assert(gb_get_reg8(&gb, REG_A) == 0x03);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);

    test_inst_rot(&gb, 0x0F, 0x00);
    assert(gb_get_flag(&gb, Flag_C) == 0);
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);
    test_end
}

void test_inst_rrca(void)
{
    test_begin
    GameBoy gb = {0};
    test_inst_rot(&gb, 0x0F, 0x81);
    assert(gb_get_reg8(&gb, REG_A) == 0xC0);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);

    test_inst_rot(&gb, 0x0F, 0x00);
    assert(gb_get_flag(&gb, Flag_C) == 0);
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);
    test_end
}

void test_inst_rla(void)
{
    test_begin
    GameBoy gb = {0};
    test_inst_rot(&gb, 0x17, 0x81);
    assert(gb_get_reg8(&gb, REG_A) == 0x02);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);

    test_inst_rot(&gb, 0x0F, 0x00);
    assert(gb_get_flag(&gb, Flag_C) == 0);
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);
    test_end
}

void test_inst_rra(void)
{
    test_begin
    GameBoy gb = {0};
    test_inst_rot(&gb, 0x1F, 0x81);
    assert(gb_get_reg8(&gb, REG_A) == 0x40);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);

    test_inst_rot(&gb, 0x0F, 0x00);
    assert(gb_get_flag(&gb, Flag_C) == 0);
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);
    test_end
}

void test_inst_cpl(void)
{
    test_begin
    u16 start_pc = 0x0032;
    u8 value = 0xFE;
    GameBoy gb = {0};
    Inst inst = {.data = (u8*)"\x2F", .size = 1};
    gb.PC = start_pc;
    gb_set_reg(&gb, REG_A, value);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg8(&gb, REG_A) == (u8)~value);
    assert(gb_get_flag(&gb, Flag_N) == 1);
    assert(gb_get_flag(&gb, Flag_H) == 1);
    test_end
}

void test_inst_scf(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (u8*)"\x37", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    test_end
}

void test_inst_ccf(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (u8*)"\x3F", .size = 1};

    for (int i = 0; i <= 1; i++) {
        gb.PC = start_pc;
        gb_set_flag(&gb, Flag_C, i);

        gb_exec(&gb, inst);

        assert(gb.PC == start_pc + 1);
        assert(gb_get_flag(&gb, Flag_N) == 0);
        assert(gb_get_flag(&gb, Flag_H) == 0);
        assert(gb_get_flag(&gb, Flag_C) == !i);
    }
    test_end
}

void test_inst_ld_mem16_sp(void)
{
    test_begin
    u16 start_pc = 0x0032;
    u16 sp = 0x8754;
    GameBoy gb = {0};
    Inst inst = {.data = (u8*)"\x08\x34\xC2", .size = 3};
    gb.PC = start_pc;
    gb_set_reg16(&gb, REG_SP, sp);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 3);
    assert(gb.memory[0xC234] == (sp & 0xff));
    assert(gb.memory[0xC235] == (sp >> 8));
    test_end
}

void test_inst_jr_n(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    u8 n = 0x05;
    u8 data[] = {0x18, n};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + inst.size + 5);
    test_end
}

void test_inst_jr_nz_n_taken(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    u8 n = 0xFE;
    u8 data[] = {0x20, n};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_flag(&gb, Flag_Z, 0);

    gb_exec(&gb, inst);

    assert(gb.PC == (start_pc + inst.size) + (int8_t)n);
    test_end
}

void test_inst_jr_nz_n_not_taken(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    u8 n = 0xFE;
    u8 data[] = {0x20, n};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_flag(&gb, Flag_Z, 1);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + inst.size);
    test_end
}

void test_inst_jr_z_n_taken(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    u8 n = 0xFE;
    u8 data[] = {0x28, n};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_flag(&gb, Flag_Z, 1);

    gb_exec(&gb, inst);

    assert(gb.PC == (start_pc + inst.size) + (int8_t)n);
    test_end
}

void test_inst_jr_z_n_not_taken(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    u8 n = 0xFE;
    u8 data[] = {0x28, n};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_flag(&gb, Flag_Z, 0);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + inst.size);
    test_end
}

void test_inst_jr_nc_n_taken(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    u8 n = 0xFE;
    u8 data[] = {0x30, n};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_flag(&gb, Flag_C, 0);

    gb_exec(&gb, inst);

    assert(gb.PC == (start_pc + inst.size) + (int8_t)n);
    test_end
}

void test_inst_jr_nc_n_not_taken(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    u8 n = 0xFE;
    u8 data[] = {0x30, n};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_flag(&gb, Flag_C, 1);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + inst.size);
    test_end
}

void test_inst_jr_c_n_taken(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    u8 n = 0xFE;
    u8 data[] = {0x38, n};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_flag(&gb, Flag_C, 1);

    gb_exec(&gb, inst);

    assert(gb.PC == (start_pc + inst.size) + (int8_t)n);
    test_end
}

void test_inst_jr_c_n_not_taken(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    u8 n = 0xFE;
    u8 data[] = {0x38, n};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_flag(&gb, Flag_C, 0);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + inst.size);
    test_end
}


void test_inst_add_hl_reg16(Reg16 reg)
{
    test_begin
    //printf("(%-50s)", gb_reg16_to_str(reg));
    u16 start_pc = 0x0032;
    u16 hl = 0x9A34;
    u16 value = 0xDE78;
    //  0x9A34
    // +0xDE78
    // -------
    //  0x78AC
    GameBoy gb = {0};
    u8 data[] = {0x09};
    data[0] |= (reg << 4);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg16(&gb, reg, value);
    gb_set_reg16(&gb, REG_HL, hl);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_HL) {
        assert(gb_get_reg16(&gb, REG_HL) == (u16)(hl + hl));
    } else {
        assert(gb_get_reg16(&gb, REG_HL) == (u16)(hl + value));
    }
    // -0HC
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 1);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    test_end
}


void test_inst_dec_reg16(Reg16 reg)
{
    test_begin
    //printf("(%-50s)", gb_reg16_to_str(reg));
    u16 start_pc = 0x0032;
    u16 value = 0xDE78;
    GameBoy gb = {0};
    u8 data[] = {0x0B};
    data[0] |= (reg << 4);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg16(&gb, reg, value);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg16(&gb, reg) == (u16)(value - 1));
    test_end
}

void test_inst_ld_reg8_reg8(Reg8 dst, Reg8 src)
{
    if (dst == REG_HL_MEM && src == REG_HL_MEM) {
        return;
    }
    test_begin
    //printf("(%s <- %s)", gb_reg_to_str(dst), gb_reg_to_str(src));
    u16 start_pc = 0x0032;
    u8 value = 0xC9;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    u8 data[] = {0x40};
    data[0] |= (dst << 3) | src;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, src, value);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg8(&gb, dst) == value);
    test_end
}

void test_inst_halt(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (u8*)"\x76", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    test_end
}

void test_inst_add_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    u16 start_pc = 0x0032;
    u8 a = 0x19;
    u8 value = 0xC3;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    u8 data[] = {0x80};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg8(&gb, REG_A) == (u8)(a + a));
    } else {
        assert(gb_get_reg8(&gb, REG_A) == (u8)(a + value));
    }
    assert(gb_get_flag(&gb, Flag_N) == 0);
    test_end
}

void test_inst_add_reg8_zero_and_carry_flags(void)
{
    test_begin
    GameBoy gb = {0};
    gb_set_reg(&gb, REG_A, 0xFF);
    gb_set_reg(&gb, REG_B, 0x01);
    Inst inst = {.data = (u8*)"\x80", .size = 1};

    gb_exec(&gb, inst);

    assert(gb_get_flag(&gb, Flag_Z) == 1);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    test_end
}

void test_inst_add_reg8_half_carry_flag(void)
{
    test_begin
    GameBoy gb = {0};
    Inst inst = {.data = (u8*)"\x80", .size = 1};

    // Case 1
    //  0000 1000
    // +0000 1000
    // -----------------
    //  0001 0000   H=1
    gb_set_reg(&gb, REG_A, 0x08);
    gb_set_reg(&gb, REG_B, 0x08);

    gb_exec(&gb, inst);

    assert(gb_get_flag(&gb, Flag_H) == 1);

    // Case 2
    //  0000 0111
    // +0000 0111
    // ----------
    //  0000 1110   H=0
    gb_set_reg(&gb, REG_A, 0x07);
    gb_set_reg(&gb, REG_B, 0x07);

    gb_exec(&gb, inst);

    assert(gb_get_flag(&gb, Flag_H) == 0);

    // Case 3
    //  0000 0111
    // +0000 1111
    // ----------
    //  0001 0110   H=1
    gb_set_reg(&gb, REG_A, 0x07);
    gb_set_reg(&gb, REG_B, 0x0F);

    gb_exec(&gb, inst);

    assert(gb_get_flag(&gb, Flag_H) == 1);
    test_end
}

void test_inst_adc_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    u8 c = 1;
    u8 a = 0x19;
    u8 value = 0xC3;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    u8 data[] = {0x88};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);
    gb_set_flag(&gb, Flag_C, c);

    gb_exec(&gb, inst);

    if (reg == REG_A) {
        assert(gb_get_reg8(&gb, REG_A) == (u8)(a + a + c));
    } else {
        assert(gb_get_reg8(&gb, REG_A) == (u8)(a + value + c));
    }
    assert(gb_get_flag(&gb, Flag_N) == 0);
    test_end
}

void test_inst_sub_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    u16 start_pc = 0x0032;
    u8 a = 0xDD;
    u8 value = 0xC3;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    u8 data[] = {0x90};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg8(&gb, REG_A) == 0);
    } else {
        assert(gb_get_reg8(&gb, REG_A) == (u8)(a - value));
    }
    assert(gb_get_flag(&gb, Flag_N) == 1);
    test_end
}

void test_inst_sbc_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    u16 start_pc = 0x0032;
    u8 c = 1;
    u8 a = 0xDD;
    u8 value = 0xC3;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    u8 data[] = {0x98};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);
    gb_set_flag(&gb, Flag_C, c);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg8(&gb, REG_A) == (u8)-1);
    } else {
        assert(gb_get_reg8(&gb, REG_A) == (u8)(a - value - c));
    }
    assert(gb_get_flag(&gb, Flag_N) == 1);
    test_end
}

void test_inst_and_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    u16 start_pc = 0x0032;
    u8 a = 0xDD;
    u8 value = 0xC3;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    u8 data[] = {0xA0};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg8(&gb, REG_A) == a);
    } else {
        assert(gb_get_reg8(&gb, REG_A) == (u8)(a & value));
    }
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 1);
    assert(gb_get_flag(&gb, Flag_C) == 0);
    test_end
}

void test_inst_xor_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    u16 start_pc = 0x0032;
    u8 a = 0xDD;
    u8 value = 0xC3;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    u8 data[] = {0xA8};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg8(&gb, REG_A) == 0);
    } else {
        assert(gb_get_reg8(&gb, REG_A) == (u8)(a ^ value));
    }
    assert(gb_get_flag(&gb, Flag_Z) == (reg == REG_A) ? 1 : 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);
    assert(gb_get_flag(&gb, Flag_C) == 0);
    test_end
}

void test_inst_or_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    u16 start_pc = 0x0032;
    u8 a = 0xDD;
    u8 value = 0xC3;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    u8 data[] = {0xB0};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg8(&gb, REG_A) == a);
    } else {
        assert(gb_get_reg8(&gb, REG_A) == (u8)(a | value));
    }
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);
    assert(gb_get_flag(&gb, Flag_C) == 0);
    test_end
}

void test_inst_cp_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    u8 a = 0xDD;
    u8 value = 0xC3;
    GameBoy gb = {0};
    u8 data[] = {0xB8};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    if (reg == REG_A) {
        assert(gb_get_flag(&gb, Flag_Z) == 1);
    } else {
        assert(gb_get_flag(&gb, Flag_Z) == 0);
    }
    assert(gb_get_flag(&gb, Flag_N) == 1);
    test_end
}

void test_inst_jp_mem_hl(void)
{
    test_begin
    GameBoy gb = {0};
    u8 data[] = {0xE9};
    Inst inst = {.data = data, .size = sizeof(data)};
    u16 addr = 0x1234;
    gb.HL = addr;

    gb_exec(&gb, inst);

    assert(gb.PC == addr);
    test_end
}

void test_inst_rrc(void)
{
    test_begin
    GameBoy gb = {0};
    u8 data[] = {0xCB, 0x08};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb_set_reg(&gb, REG_B, 0x01);

    gb_exec(&gb, inst);

    assert(gb_get_reg8(&gb, REG_B) == 0x80);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    test_end
}

void test_inst_sra(void)
{
    test_begin
    GameBoy gb = {0};
    u8 data[] = {0xCB, 0x28};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb_set_reg(&gb, REG_B, 0x80);

    gb_exec(&gb, inst);

    assert(gb_get_reg8(&gb, REG_B) == 0xC0);
    test_end
}

void test_inst_add_a_hl_mem(void)
{
    test_begin
    GameBoy gb = {0};
    u8 data[] = {0x86};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb_set_reg(&gb, REG_A, 0xFF);
    gb.HL = 0xC000;
    gb_mem_write(&gb, gb.HL, 0x01);
    // A = FF
    // +   01
    // ------
    //   1 00 => Z, C and H are set

    gb_exec(&gb, inst);

    assert(gb_get_reg8(&gb, REG_A) == 0x00);
    assert(gb_get_flag(&gb, Flag_Z) == 1);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 1);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    test_end
}

// https://forums.nesdev.org/viewtopic.php?t=15944
// https://ehaskins.com/2018-01-30%20Z80%20DAA/
void test_inst_daa(void)
{
    test_begin

    {
        GameBoy gb = {0};
        u8 data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00);

        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x00);
        assert(gb_get_flag(&gb, Flag_Z) == 1);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        u8 data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x0F); // BCD: we can only represent 0-9

        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x15);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        u8 data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0xF0); // BCD: we can only represent 0-9

        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x50);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        u8 data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x10); // BCD: we can only represent 0-9

        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x10);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        u8 data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 0, 0, 1);
        // A = 80
        //   + 80
        // ----------
        //   1 00 C=1
        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x60);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        u8 data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0xF0); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 0, 0, 1);
        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x50);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        u8 data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 1, 1, 1, 0); // F = ZNH- (After subtraction)
        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0xFA);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_N) == 1);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        u8 data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 1, 1, 1, 1); // F = ZNHC (After subtraction)
        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x9A);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_N) == 1);
        assert(gb_get_flag(&gb, Flag_H) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        u8 data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x9A); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 0, 0, 0); // F = ---- (After addition)
        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x00);
        assert(gb_get_flag(&gb, Flag_Z) == 1);
        assert(gb_get_flag(&gb, Flag_N) == 0);
        assert(gb_get_flag(&gb, Flag_H) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        u8 data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 0, 1, 0); // F = ---- (After addition)
        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x06);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_N) == 0);
        assert(gb_get_flag(&gb, Flag_H) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        u8 data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 1, 0, 0); // F = -N-- (After subtraction)
        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x00);
        assert(gb_get_flag(&gb, Flag_Z) == 1);
        assert(gb_get_flag(&gb, Flag_N) == 1);
        assert(gb_get_flag(&gb, Flag_H) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

#if 0
    u16 start_pc = 0x0032;
    u8 value = 0x33;
    GameBoy gb = {0};
    u8 data[] = {0x27};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, REG_A, value);

    // Addition     (N flag == 0)
    // Subtraction  (N flag == 1)
    //
    // Case 1:
    //   0x15   0001 0101
    // + 0x12   0001 0010
    // ------------------
    //   0x27   0010 0111 => C=0,N=0,H=0 Valid BCD no adjustment required
    //
    // Case 2:
    //   0x05   0000 0101
    // + 0x05   0000 0101
    // ------------------
    //   0x0A   0000 1010 => C=0,N=0,H=0 Invalid BCD, Adjust to 0x10 (add 6 to low nibble)
    //
    // Case 3
    //   0x50   0101 0000
    // + 0x50   0101 0000
    // ------------------
    //   0xA0   1010 0000 => C=0,N=0,H=0 Invalid BCD, Adjust to 0x00 (add 6 to high nibble)
    //
    // Case 4
    //   0x55   0101 0101
    // + 0x55   0101 0101
    // ------------------
    //   0xAA   1010 1010 => C=0,N=0,H=0 Invalid BCD, Adjust to 0x10 (add 6 to both)
    //
    // Case 5
    //   0x??   ???? 0111
    // + 0x??   ???? 0111
    // ------------------
    //   0x??   ????

    // The DAA instruction looks at the carry flag C and half-carry flag H
    // and the value in register A, and determines if it must
    // add 0x00, 0x06, 0x60, or 0x66, and then sets the output flags C,N,Z
    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
#endif
    test_end
}

void test_inst_cp_n(void)
{
    test_begin
    GameBoy gb = {0};
    u8 data[] = {0xFE, 0x90};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb_set_reg(&gb, REG_A, 0x90);

    gb_exec(&gb, inst);

    assert(gb_get_reg8(&gb, REG_A) == 0x90);
    assert(gb_get_flag(&gb, Flag_Z) == 1);
    assert(gb_get_flag(&gb, Flag_N) == 1);
    assert(gb_get_flag(&gb, Flag_H) == 0);
    assert(gb_get_flag(&gb, Flag_C) == 0);
    test_end
}

void test_time_nop(void)
{
    test_begin
    GameBoy gb = {0};

    gb_tick_ms(&gb, 0);
    assert(gb.PC == 0);

    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 1);
    test_end
}

void test_time_inc_reg16(void)
{
    test_begin
    GameBoy gb = {0};
    gb.memory[0] = 0x03;

    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 0);
    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 1);
    test_end
}

void test_time_add_a_mem_hl(void)
{
    test_begin
    GameBoy gb = {0};
    gb.memory[0] = 0x86;

    Inst inst = gb_fetch(&gb);
    assert(inst.min_cycles == 8);
    assert(inst.max_cycles == 8);

    gb_tick_ms(&gb, 0);
    assert(gb.PC == 0);
    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 0);
    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 1);

    test_end
}

void test_time_jr_z(void)
{
    test_begin
    GameBoy gb = {0};
    gb.memory[0] = 0x28;
    gb.memory[1] = 0x05;

    // JP taken
    gb_set_flag(&gb, Flag_Z, 1);
    gb_tick_ms(&gb, 2*MCYCLE_MS);
    assert(gb.PC == 0);
    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 7);
    
    // JP not taken
    gb.PC = 0;
    gb_set_flag(&gb, Flag_Z, 0);
    gb_tick_ms(&gb, 2*MCYCLE_MS);
    assert(gb.PC == 2);

    test_end
}

void test_time_ret_z(void)
{
    test_begin
    GameBoy gb = {0};
    gb.memory[0] = 0xC8;
    gb.SP = 0xD000;
    gb.memory[gb.SP+0] = 0x34;
    gb.memory[gb.SP+1] = 0x12;

    // RET taken
    gb_set_flag(&gb, Flag_Z, 1);
    gb_tick_ms(&gb, 4*MCYCLE_MS);
    assert(gb.PC == 0);
    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 0x1234);

    // RET not taken
    gb.PC = 0;
    gb.SP = 0xD000;
    gb_set_flag(&gb, Flag_Z, 0);
    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 0);
    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 1);

    test_end
}

void test_time_jp_z(void)
{
    test_begin
    GameBoy gb = {0};
    gb.memory[0] = 0xCA;
    gb.memory[1] = 0x34;
    gb.memory[2] = 0x12;

    // JP taken
    gb_set_flag(&gb, Flag_Z, 1);
    gb_tick_ms(&gb, 3*MCYCLE_MS);
    assert(gb.PC == 0);
    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 0x1234);
    
    // JP not taken
    gb.PC = 0;
    gb_set_flag(&gb, Flag_Z, 0);
    gb_tick_ms(&gb, 3*MCYCLE_MS);
    assert(gb.PC == 3);

    test_end
}

void test_time_call_z(void)
{
    test_begin
    GameBoy gb = {0};
    gb.memory[0] = 0xCC;
    gb.memory[1] = 0x34;
    gb.memory[2] = 0x12;

    // CALL taken
    gb_set_flag(&gb, Flag_Z, 1);
    gb_tick_ms(&gb, 5*MCYCLE_MS);
    assert(gb.PC == 0);
    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 0x1234);

    // CALL not taken
    gb.PC = 0;
    gb_set_flag(&gb, Flag_Z, 0);
    gb_tick_ms(&gb, 2*MCYCLE_MS);
    assert(gb.PC == 0);
    gb_tick_ms(&gb, MCYCLE_MS);
    assert(gb.PC == 3);

    test_end
}

void test_render_lcd_off(void)
{
    test_begin
    GameBoy gb = {0};
    gb_render(&gb);
    for (size_t i = 0; i < SCRN_VX*SCRN_VY; i++) {
        assert(gb.display[i] == 0x00);
    }
    test_end
}

void test_render_lcd_on_bg_on(void)
{
    test_begin
    GameBoy gb = {0};
    gb.memory[rLCDC] = 0x81; // LCDCF_ON | LCDCF_BGON
    // Tilemap
    //for (size_t i = 0; i < 32*32; i++) {
    //    gb.memory[0x9800+i] = 0x01;
    //}
    // Tiles
    //for (size_t i = 0; i < 8*8; i++) {
    //    gb.memory[0x8800+i] = 0xFF;
    //}
    gb.memory[0x9800] = 0x80;
    for (size_t i = 0; i < 8*8; i++) gb.memory[0x8800+i] = 0xFF;

    gb_render(&gb);
    for (size_t i = 0; i < SCRN_VX*SCRN_VY; i++) {
        assert(gb.display[i] == PALETTE[0]);
    }
    test_end
}

void test_cpu_instructions(void)
{
    test_inst_nop();
    //test_inst_stop(); printf("\n");

    test_inst_ld_reg16_n(REG_BC);
    test_inst_ld_reg16_n(REG_DE);
    test_inst_ld_reg16_n(REG_HL);
    test_inst_ld_reg16_n(REG_SP);
    printf("\n");

    test_inst_ld_reg16_mem_a(REG_BC);
    test_inst_ld_reg16_mem_a(REG_DE);
    test_inst_ldi_hl_mem_a();
    test_inst_ldd_hl_mem_a();

    test_inst_ld_a_reg16_mem(REG_BC);
    test_inst_ld_a_reg16_mem(REG_DE);
    test_inst_ldi_a_hl_mem();
    test_inst_ldd_a_hl_mem();
    printf("\n");

    test_inst_inc_reg16(REG_BC);
    test_inst_inc_reg16(REG_DE);
    test_inst_inc_reg16(REG_HL);
    test_inst_inc_reg16(REG_SP);
    printf("\n");

    test_inst_inc_reg8(REG_B);
    test_inst_inc_reg8(REG_C);
    test_inst_inc_reg8(REG_D);
    test_inst_inc_reg8(REG_E);
    test_inst_inc_reg8(REG_H);
    test_inst_inc_reg8(REG_L);
    test_inst_inc_reg8(REG_HL_MEM);
    test_inst_inc_reg8(REG_A);
    printf("\n");

    test_inst_dec_reg8(REG_B);
    test_inst_dec_reg8(REG_C);
    test_inst_dec_reg8(REG_D);
    test_inst_dec_reg8(REG_E);
    test_inst_dec_reg8(REG_H);
    test_inst_dec_reg8(REG_L);
    test_inst_dec_reg8(REG_HL_MEM);
    test_inst_dec_reg8(REG_A);
    printf("\n");

    test_inst_ld_reg8_n(REG_B);
    test_inst_ld_reg8_n(REG_C);
    test_inst_ld_reg8_n(REG_D);
    test_inst_ld_reg8_n(REG_E);
    test_inst_ld_reg8_n(REG_H);
    test_inst_ld_reg8_n(REG_L);
    test_inst_ld_reg8_n(REG_HL_MEM);
    test_inst_ld_reg8_n(REG_A);
    printf("\n");

    test_inst_rlca();
    test_inst_rrca();
    test_inst_rla();
    test_inst_rra();
    printf("\n");

    test_inst_cpl();
    test_inst_scf();
    test_inst_ccf();
    printf("\n");

    test_inst_ld_mem16_sp();
    test_inst_jr_n();
    test_inst_jr_nz_n_taken();
    test_inst_jr_nz_n_not_taken();
    test_inst_jr_z_n_taken();
    test_inst_jr_z_n_not_taken();
    test_inst_jr_nc_n_taken();
    test_inst_jr_nc_n_not_taken();
    test_inst_jr_c_n_taken();
    test_inst_jr_c_n_not_taken();
    printf("\n");

    test_inst_add_hl_reg16(REG_BC);
    test_inst_add_hl_reg16(REG_DE);
    test_inst_add_hl_reg16(REG_HL);
    test_inst_add_hl_reg16(REG_SP);
    printf("\n");

    test_inst_dec_reg16(REG_BC);
    test_inst_dec_reg16(REG_DE);
    test_inst_dec_reg16(REG_HL);
    test_inst_dec_reg16(REG_SP);
    printf("\n");

    for (int dst = 0; dst < REG_COUNT; dst++) {
        for (int src = 0; src < REG_COUNT; src++) {
            test_inst_ld_reg8_reg8(dst, src);
        }
    }
    printf("\n");

    test_inst_halt();
    printf("\n");

    for (int src = 0; src < REG_COUNT; src++) {
        test_inst_add_reg8(src);
    }
    test_inst_add_reg8_zero_and_carry_flags();
    test_inst_add_reg8_half_carry_flag();
    printf("\n");

    for (int src = 0; src < REG_COUNT; src++) {
        test_inst_adc_reg8(src);
    }
    printf("\n");

    for (int src = 0; src < REG_COUNT; src++) {
        test_inst_sub_reg8(src);
    }
    printf("\n");

    for (int src = 0; src < REG_COUNT; src++) {
        test_inst_sbc_reg8(src);
    }
    printf("\n");

    for (int src = 0; src < REG_COUNT; src++) {
        test_inst_and_reg8(src);
    }
    printf("\n");

    for (int src = 0; src < REG_COUNT; src++) {
        test_inst_xor_reg8(src);
    }
    printf("\n");

    for (int src = 0; src < REG_COUNT; src++) {
        test_inst_or_reg8(src);
    }
    printf("\n");

    for (int src = 0; src < REG_COUNT; src++) {
        test_inst_cp_reg8(src);
    }
    printf("\n");

    test_inst_jp_mem_hl();

    test_inst_rrc();
    test_inst_sra();
    test_inst_add_a_hl_mem();
    test_inst_daa();
    test_inst_cp_n();
}

void test_cpu_timing(void)
{
    //test_time_nop();
    //test_time_inc_reg16();
    //test_time_add_a_mem_hl();

    // Instructions with different timings
    //test_time_jr_z();   // 20, 28, 30, 38
    //test_time_ret_z();  // C0, C8, D0, D8
    //test_time_jp_z();   // C2, CA, D2, DA
    //test_time_call_z(); // C4, CC, D4, DC
}

void test_vblank_interrupt_with_ime_not_set(void)
{
    test_begin
    GameBoy gb = {0};
    gb.IME = 0;
    gb.memory[rIE] |= IEF_VBLANK;

    Inst inst = {.data = (u8*)"\x00", .size = 1};
    gb_exec(&gb, inst);

    assert(gb.PC == 1);
    test_end
}

void test_vblank_interrupt_with_ime_set(void)
{
    test_begin
    GameBoy gb = {0};
    gb.IME = 1;
    gb.memory[rIE] |= IEF_VBLANK;
    gb.memory[rIF] |= IEF_VBLANK;
    
    Inst inst = {.data = (u8*)"\x00", .size = 1};
    gb_exec(&gb, inst);

    assert(gb.PC == 0x0040);
    test_end
}

void test_interrupts(void)
{
    test_vblank_interrupt_with_ime_not_set();
    test_vblank_interrupt_with_ime_set();
}

void test_p1_joypad_register(void)
{
    test_begin
    // No buttons pressed
    {
        GameBoy gb = {0};
        gb_mem_write(&gb, rP1, P1F_GET_BTN);
        assert(gb.memory[rP1] == 0xCF);
    }
    {
        GameBoy gb = {0};
        gb_mem_write(&gb, rP1, P1F_GET_DPAD);
        assert(gb.memory[rP1] == 0xCF);
    }
    {
        GameBoy gb = {0};
        gb_mem_write(&gb, rP1, P1F_GET_NONE);
        assert(gb.memory[rP1] == 0xCF);
    }
    // Action buttons (single)
    {
        GameBoy gb = {0};
        gb.button_a = 1;
        gb_mem_write(&gb, rP1, P1F_GET_BTN);
        assert(gb.memory[rP1] == 0xCE/*11|00|1110*/);
    }
    {
        GameBoy gb = {0};
        gb.button_b = 1;
        gb_mem_write(&gb, rP1, P1F_GET_BTN);
        assert(gb.memory[rP1] == 0xCD/*11|00|1101*/);
    }
    {
        GameBoy gb = {0};
        gb.button_select = 1;
        gb_mem_write(&gb, rP1, P1F_GET_BTN);
        assert(gb.memory[rP1] == 0xCB/*11|00|1011*/);
    }
    {
        GameBoy gb = {0};
        gb.button_start = 1;
        gb_mem_write(&gb, rP1, P1F_GET_BTN);
        assert(gb.memory[rP1] == 0xC7/*11|00|0111*/);
    }
    // Action buttons (multi)
    {
        GameBoy gb = {0};
        gb.button_a = 1;
        gb.button_b = 1;
        gb_mem_write(&gb, rP1, P1F_GET_BTN);
        assert(gb.memory[rP1] == 0xCC/*11|00|1100*/);
    }
    // Direction buttons (single)
    {
        GameBoy gb = {0};
        gb.dpad_right = 1;
        gb_mem_write(&gb, rP1, P1F_GET_DPAD);
        assert(gb.memory[rP1] == 0xCE/*11|00|1110*/);
    }
    {
        GameBoy gb = {0};
        gb.dpad_left = 1;
        gb_mem_write(&gb, rP1, P1F_GET_DPAD);
        assert(gb.memory[rP1] == 0xCD/*11|00|1101*/);
    }
    {
        GameBoy gb = {0};
        gb.dpad_up = 1;
        gb_mem_write(&gb, rP1, P1F_GET_DPAD);
        assert(gb.memory[rP1] == 0xCB/*11|00|1011*/);
    }
    {
        GameBoy gb = {0};
        gb.dpad_down = 1;
        gb_mem_write(&gb, rP1, P1F_GET_DPAD);
        assert(gb.memory[rP1] == 0xC7/*11|00|0111*/);
    }
    test_end
}

void test_serial_transfer_data_and_control_register(void)
{
    test_begin
    GameBoy gb = {0};
    gb.memory[rSB] = 0x69;
    gb_mem_write(&gb, rSC, 0x81); // Transfer Start | Internal Clock
    assert(gb.memory[rSC] == 0xFF);
    test_end
}

void test_divider_register(void)
{
    test_begin

    // Writing any value to DIV resets it to $00
    {
        GameBoy gb = {0};
        gb.memory[rDIV] = 0x69;
        gb_mem_write(&gb, rDIV, 1);
        assert(gb.memory[rDIV] == 0);
    }

    // DIV increments at a rate of 16384 Hz
    {
        GameBoy gb = {0};
        gb.timer_div = (1000.0 / 16384.0);
        gb_tick_ms(&gb, 0);
        assert(gb.memory[rDIV] == 0);

        gb_tick_ms(&gb, 1000.0 / 16384);
        assert(gb.memory[rDIV] == 1);

        gb_tick_ms(&gb, 10 * (1000.0 / 16384));
        assert(gb.memory[rDIV] == 11);
    }

    // DIV is reset after a STOP instruction
    {
        GameBoy gb = {0};
        gb.memory[rDIV] = 0x69;
        Inst inst = {.data = (u8*)"\x10\x00", .size = 2};
        gb_exec(&gb, inst);
        assert(gb.stopped == true);

        gb.button_a = 1;
        gb_exec(&gb, inst);
        assert(gb.stopped == false);
        assert(gb.PC == 2);
        assert(gb.memory[rDIV] == 0);
    }

    test_end
}

void test_timer_counter_register(void)
{
    test_begin
    // TIMA is incremented at the clock frequency specified by TAC
    {
        GameBoy gb = {0};
        gb_mem_write(&gb, rTAC, 0x04); // Timer Enable

        gb_mem_write(&gb, rTIMA, 0);
        gb_mem_write(&gb, rTAC, 0x04); // Timer Enable at 4096 Hz
        gb_tick_ms(&gb, 1000.0 / 4096);
        assert(gb.memory[rTIMA] == 1);

        gb_mem_write(&gb, rTIMA, 0);
        gb_mem_write(&gb, rTAC, 0x05); // Timer Enable at 262144 Hz
        gb_tick_ms(&gb, 1000.0 / 262144);
        assert(gb.memory[rTIMA] == 1);

        gb_mem_write(&gb, rTIMA, 0);
        gb_mem_write(&gb, rTAC, 0x06); // Timer Enable at 65536 Hz
        gb_tick_ms(&gb, 1000.0 / 65536);
        assert(gb.memory[rTIMA] == 1);

        gb_mem_write(&gb, rTIMA, 0);
        gb_mem_write(&gb, rTAC, 0x07); // Timer Enable at 16384 Hz
        gb_tick_ms(&gb, 1000.0 / 16384);
        assert(gb.memory[rTIMA] == 1);
    }

    // TIME is reset to the value specified in TMA on overflow
    {
        GameBoy gb = {0};
        gb_mem_write(&gb, rTAC, 0x04); // Timer Enable at 4096 Hz

        gb_mem_write(&gb, rTIMA, 0xff);
        gb_mem_write(&gb, rTAC, 0x04); // Timer Enable at 4096 Hz
        gb_tick_ms(&gb, 1000.0 / 4096);
        assert(gb.memory[rTIMA] == 0);
        assert(gb.memory[rIF] & 0x04);

    }
    test_end
}

void test_lcd_control_register(void)
{
    test_begin
    // LCD/PPU Off
    {
        GameBoy gb = {0};
        gb.memory[0] = 0x18; // JR -2
        gb.memory[1] = 0xfe;
        gb_mem_write(&gb, rLCDC, LCDCF_OFF); // $00
        f64 dt = 0.0066;
        for (int i = 0; i < 5000; i++) {
            gb_tick_ms(&gb, dt);
        }
        assert(gb.memory[rLY] == 0);
        assert((gb.memory[rSTAT] & 7) == 0);
        assert(gb.memory[rLCDC] == 0);

        for (size_t i = 0; i < 256*256; i++) {
            //assert(gb.display[i] == 0xFFFFFFFF);
        }
    }

    // LCD/PPU On + BG Off
    {
        GameBoy gb = {0};
        gb_init(&gb);
        gb.memory[0] = 0x18; // JR -2
        gb.memory[1] = 0xfe;
        gb_mem_write(&gb, rLCDC, LCDCF_ON|LCDCF_BGOFF); // $80
        bool stat_00 = false;
        bool stat_01 = false;
        bool stat_10 = false;
        bool stat_11 = false;
        int min_ly = 1000000;
        int max_ly = -1;

        Timer timer = {0};
        timer_init(&timer);

        while (timer.elapsed_ticks < 1*1000*1000) {
            timer_update(&timer);

            gb_update(&gb);
            u8 stat = gb.memory[rSTAT] & 3;
            u8 ly = gb.memory[rLY];
            if (stat == 0) stat_00 = true;
            if (stat == 1) stat_01 = true;
            if (stat == 2) stat_10 = true;
            if (stat == 3) stat_11 = true;
            if (ly < min_ly) min_ly = ly;
            if (ly > max_ly) max_ly = ly;
        }
        if(!(stat_00 && stat_01 && stat_10 && stat_11)) {
            //printf("stat_00: %d, stat_01: %d, stat_10: %d, stat_11: %d\n",
            //    stat_00, stat_01, stat_10, stat_11);
        }
        //assert(stat_00 && stat_01 && stat_10 && stat_11);
        assert(min_ly == 0);
        //assert(max_ly == 153);
        assert(gb.memory[rLCDC] == LCDCF_ON);

        for (size_t i = 0; i < 256*256; i++) {
            //assert(gb.display[i] == 0xFFFFFFFF);
        }
    }

    // LCD/PPU On + BG Tilemap $9800 + Tile data "$8800" addressing ($9000 base + signed addressing)
    {
        GameBoy gb = {0};
        gb.memory[0] = 0x18; // JR -2
        gb.memory[1] = 0xfe;
        gb_mem_write(&gb, rLCDC, LCDCF_ON|LCDCF_BG9800|LCDCF_BG8800|LCDCF_BGON); // $81

        gb.memory[rBGP] = 0xE4; // 11|10|01|00

        // Fill 2 8x8 tiles (16-bytes each)
        int tile_idx1 = 1;
        int tile_idx2 = -1;
        for (int i = 0; i < 16; i++) gb.memory[0x9000 + 16*tile_idx1 + i] = 0xFF;
        for (int i = 0; i < 16; i++) gb.memory[0x9000 + 16*tile_idx2 + i] = 0xFF;

        // Set the first tilemap to the new tile
        gb.memory[0x9800] = tile_idx1;
        gb.memory[0x9801] = tile_idx2;

        for (int i = 0; i < 5000; i++) gb_tick_ms(&gb, 0.0066);

        for (size_t row = 0; row < 256; row++) {
            for (size_t col = 0; col < 256; col++) {
                //if (row < 8 && col < 16) assert(gb.display[row*256 + col] == PALETTE[3]);
                //else assert(gb.display[row*256 + col] == PALETTE[0]);
            }
        }
    }

    // LCD/PPU On + BG Tilemap $9800 + Tile data "$8000" addressing
    {
        GameBoy gb = {0};
        gb.memory[0] = 0x18; // JR -2
        gb.memory[1] = 0xfe;
        gb_mem_write(&gb, rLCDC, LCDCF_ON|LCDCF_BG9800|LCDCF_BG8000|LCDCF_BGON); // $91

        gb.memory[rBGP] = 0xE4; // 11|10|01|00

        // Fill 2 8x8 tiles (16-bytes each)
        int tile_idx1 = 1;
        int tile_idx2 = 2;
        for (int i = 0; i < 16; i++) gb.memory[0x8000 + 16*tile_idx1 + i] = 0xFF;
        for (int i = 0; i < 16; i++) gb.memory[0x8000 + 16*tile_idx2 + i] = 0xFF;

        // Set the first tilemap to the new tile
        gb.memory[0x9800] = tile_idx1;
        gb.memory[0x9801] = tile_idx2;

        for (int i = 0; i < 5000; i++) gb_tick_ms(&gb, 0.0066);

        for (size_t row = 0; row < 256; row++) {
            for (size_t col = 0; col < 256; col++) {
                //if (row < 8 && col < 16) assert(gb.display[row*256 + col] == PALETTE[3]);
                //else assert(gb.display[row*256 + col] == PALETTE[0]);
            }
        }
    }

    // LCD/PPU On + BG Tilemap $9C00 + Tile data "$8000" addressing
    {
        GameBoy gb = {0};
        gb.memory[0] = 0x18; // JR -2
        gb.memory[1] = 0xfe;
        gb_mem_write(&gb, rLCDC, LCDCF_ON|LCDCF_BG9C00|LCDCF_BG8000|LCDCF_BGON); // $99

        gb.memory[rBGP] = 0xE4; // 11|10|01|00

        // Fill 2 8x8 tiles (16-bytes each)
        int tile_idx1 = 1;
        int tile_idx2 = 2;
        for (int i = 0; i < 16; i++) gb.memory[0x8000 + 16*tile_idx1 + i] = 0xFF;
        for (int i = 0; i < 16; i++) gb.memory[0x8000 + 16*tile_idx2 + i] = 0xFF;

        // Set the first tilemap to the new tile
        gb.memory[0x9C00] = tile_idx1;
        gb.memory[0x9C01] = tile_idx2;

        for (int i = 0; i < 5000; i++) gb_tick_ms(&gb, 0.0066);

        for (size_t row = 0; row < 256; row++) {
            for (size_t col = 0; col < 256; col++) {
                //if (row < 8 && col < 16) assert(gb.display[row*256 + col] == PALETTE[3]);
                //else assert(gb.display[row*256 + col] == PALETTE[0]);
            }
        }
    }

    // LCD/PPU On + OBJ On
    // OBJ 8x16

    test_end
}

void test_lcd_status_register(void)
{
    test_begin

    assert(fabs(DOTS_TO_MS(MS_TO_DOTS(1.0)) - 1.0) < 0.001);

    for (size_t dots = 0; dots < 100000; dots++) {
        assert(dots == MS_TO_DOTS(DOTS_TO_MS(dots)));
        if (dots != MS_TO_DOTS(DOTS_TO_MS(dots))) {
            printf("%5ld dots = %10lf ms = %5ld dots\n", dots, DOTS_TO_MS(dots), MS_TO_DOTS(DOTS_TO_MS(dots)));
        }
    }

    // Mode 2 (OAM scan)
    {
        GameBoy gb = {0};
        gb_tick_ms(&gb, DOTS_TO_MS(0));
        //assert((gb.memory[rSTAT] & 3) == PM_OAM);
    }
    {
        GameBoy gb = {0};
        gb_tick_ms(&gb, DOTS_TO_MS(79));
        //assert(gb.ppu.dot == 79);
        //assert((gb.memory[rSTAT] & 3) == 2);
    }

    // Mode 3 (Transfer to LCD)
    {
        GameBoy gb = {0};
        gb_tick_ms(&gb, DOTS_TO_MS(80));
        //assert(gb.ppu.dot == 80);
        //assert((gb.memory[rSTAT] & 3) == 3);
    }
    {
        GameBoy gb = {0};
        gb_tick_ms(&gb, DOTS_TO_MS(251));
        //assert(gb.ppu.dot == 251);
        //assert((gb.memory[rSTAT] & 3) == 3);
    }

    // Mode 0 (HBlank)
    {
        GameBoy gb = {0};
        gb_tick_ms(&gb, DOTS_TO_MS(252));
        //assert(gb.ppu.dot == 252);
        //assert((gb.memory[rSTAT] & 3) == 0);
    }
    {
        GameBoy gb = {0};
        gb_tick_ms(&gb, DOTS_TO_MS(DOTS_PER_SCANLINE - 1));
        //assert(gb.ppu.dot == DOTS_PER_SCANLINE - 1);
        //assert((gb.memory[rSTAT] & 3) == 0);
    }

    // Mode 1 (VBlank)
    {
        GameBoy gb = {0};
        gb.memory[rLCDC] = LCDCF_ON;
        gb_tick_ms(&gb, DOTS_TO_MS(144*DOTS_PER_SCANLINE));
        //assert(gb.ppu.dot == 144*DOTS_PER_SCANLINE);
        //assert((gb.memory[rSTAT] & 3) == 1);
    }

    test_end
}

int main(void)
{
    test_fetch();
    test_cpu_instructions();
    test_cpu_timing();

    test_render_lcd_off();
    test_render_lcd_on_bg_on();

    test_interrupts();

    // I/O registers
    test_p1_joypad_register();
    test_serial_transfer_data_and_control_register();
    test_divider_register();
    test_timer_counter_register();
    //test_timer_modulo_register();
    //test_timer_control_register();
    //test_interrupt_flag_register();
    //test_audio_registers(); // NR10 - NR52, Wave RAM
    test_lcd_control_register();
    test_lcd_status_register();
    //test_viewport_y_register();
    //test_viewport_x_register();
    //test_lcd_y_coordinate_register();
    //test_ly_compare_register();
    //test_dma_register();
    //test_bgp_register();
    //test_obp0_register();
    //test_obp1_register();
    //test_window_y_register();
    //test_window_x_register();
    //test_interrupt_enable_register();

    return 0;
}
