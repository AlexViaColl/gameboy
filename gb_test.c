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
    gb.PC = start_pc;

    gb_exec(&gb, gb_assemble_inst("nop"));

    assert(gb.PC == start_pc + 1);
    test_end
}

void test_inst_stop(void)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    gb.PC = start_pc;

    gb_exec(&gb, gb_assemble_inst("stop"));

    assert(gb.PC == start_pc + 1);
    test_end
}

void test_inst_ld_reg16_n(Reg16 reg)
{
    test_begin
    u16 start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = {0x01 | (reg << 4), 0x12, 0x34}, .size = 3};
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
    u16 addr = 0xc234;
    u8 a = 0x99;
    GameBoy gb = {0};
    gb_set_reg16(&gb, reg, addr);
    gb_set_reg(&gb, REG_A, a);
    Inst inst = {.data = {0x02 | (reg << 4)}, .size = 1};
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

    Inst inst = {.data = {0x0a | (reg << 4)}, .size = 1};
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
    u16 value = 0xc234;
    u8 a = 0x99;
    GameBoy gb = {0};
    gb.HL = value;
    gb_set_reg(&gb, REG_A, a);
    Inst inst = {.data = {0x22}, .size = 1};
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
    Inst inst = {.data = {0x2a}, .size = 1};
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
    u16 value = 0xc234;
    u8 a = 0x99;
    GameBoy gb = {0};
    gb.HL = value;
    gb_set_reg(&gb, REG_A, a);
    Inst inst = {.data = {0x32}, .size = 1};
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
    Inst inst = {.data = {0x3a}, .size = 1};
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
    Inst inst = {.data = {0x03 | (reg << 4)}, .size = 1};
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
    u8 value = 0xff;
    GameBoy gb = {0};
    gb.HL = 0xc234;
    gb_set_reg(&gb, reg, value);
    gb_set_flag(&gb, Flag_N, 1);
    Inst inst = {.data = {0x04 | (reg << 3)}, .size = 1};
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
    u8 value = 0xff;
    GameBoy gb = {0};
    gb.HL = 0xc234;
    gb_set_reg(&gb, reg, value); // REG = $FF
    Inst inst = {.data = {0x05 | (reg << 3)}, .size = 1};
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
    gb.HL = 0xc234;
    Inst inst = {.data = {0x06 | (reg << 3), 0x78}, .size = 2};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 2);
    assert(gb_get_reg8(&gb, reg) == 0x78);
    test_end
}

void test_inst_rot(GameBoy *gb, u8 opcode, u8 value)
{
    u16 start_pc = 0x0032;
    Inst inst = {.data = {opcode}, .size = 1};
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

    test_inst_rot(&gb, 0x0f, 0x00);
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
    test_inst_rot(&gb, 0x0f, 0x81);
    assert(gb_get_reg8(&gb, REG_A) == 0xc0);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);

    test_inst_rot(&gb, 0x0f, 0x00);
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

    test_inst_rot(&gb, 0x0f, 0x00);
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
    test_inst_rot(&gb, 0x1f, 0x81);
    assert(gb_get_reg8(&gb, REG_A) == 0x40);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);

    test_inst_rot(&gb, 0x0f, 0x00);
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
    u8 value = 0xfe;
    GameBoy gb = {0};
    Inst inst = {.data = {0x2f}, .size = 1};
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
    Inst inst = {.data = {0x37}, .size = 1};
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
    Inst inst = {.data = {0x3f}, .size = 1};

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
    Inst inst = {.data = {0x08, 0x34, 0xc2}, .size = 3};
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
    Inst inst = {.data = {0x18, n}, .size = 2};
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
    u8 n = 0xfe;
    Inst inst = {.data = {0x20, n}, .size = 2};
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
    Inst inst = {.data = {0x20, 0xfe}, .size = 2};
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
    u8 n = 0xfe;
    Inst inst = {.data = {0x28, n}, .size = 2};
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
    u8 n = 0xfe;
    Inst inst = {.data = {0x28, n}, .size = 2};
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
    u8 n = 0xfe;
    Inst inst = {.data = {0x30, n}, .size = 2};
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
    u8 n = 0xfe;
    Inst inst = {.data = {0x30, n}, .size = 2};
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
    u8 n = 0xfe;
    Inst inst = {.data = {0x38, n}, .size = 2};
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
    u8 n = 0xfe;
    Inst inst = {.data = {0x38, n}, .size = 2};
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
    u16 hl = 0x9a34;
    u16 value = 0xde78;
    //  0x9A34
    // +0xDE78
    // -------
    //  0x78AC
    GameBoy gb = {0};
    Inst inst = {.data = {0x09 | reg << 4}, .size = 1};
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
    Inst inst = {.data = {0x0b | reg << 4}, .size = 1};
    gb.PC = start_pc;
    gb_set_reg16(&gb, reg, value);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg16(&gb, reg) == (u16)(value - 1));
    test_end
}

void test_inst_ld_reg8_reg8(Reg8 dst, Reg8 src)
{
    if (dst == REG_HL_IND && src == REG_HL_IND) {
        return;
    }
    test_begin
    //printf("(%s <- %s)", gb_reg_to_str(dst), gb_reg_to_str(src));
    u16 start_pc = 0x0032;
    u8 value = 0xC9;
    GameBoy gb = {0};
    gb.HL = 0xC234;
    Inst inst = {.data = {0x40 | (dst << 3) | src}, .size = 1};
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
    Inst inst = {.data = {0x76}, .size = 1};
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
    u8 value = 0xc3;
    GameBoy gb = {0};
    gb.HL = 0xc234;
    Inst inst = {.data = {0x80 | reg}, .size = 1};
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
    Inst inst = {.data = {0x80}, .size = 1};

    gb_exec(&gb, inst);

    assert(gb_get_flag(&gb, Flag_Z) == 1);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    test_end
}

void test_inst_add_reg8_half_carry_flag(void)
{
    test_begin
    GameBoy gb = {0};
    Inst inst = {.data = {0x80}, .size = 1};

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
    Inst inst = {.data = {0x88 | reg}, .size = 1};
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
    Inst inst = {.data = {0x90 | reg}, .size = 1};
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
    Inst inst = {.data = {0x98 | reg}, .size = 1};
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
    Inst inst = {.data = {0xa0 | reg}, .size = 1};
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
    Inst inst = {.data = {0xa8 | reg}, .size = 1};
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
    Inst inst = {.data = {0xb0 | reg}, .size = 1};
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
    Inst inst = {.data = {0xb8 | reg}, .size = 1};
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
    Inst inst = {.data = {0xe9}, .size = 1};
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
    Inst inst = {.data = {0xcb, 0x08}, .size = 2};
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
    Inst inst = {.data = {0xcb, 0x28}, .size = 2};
    gb_set_reg(&gb, REG_B, 0x80);

    gb_exec(&gb, inst);

    assert(gb_get_reg8(&gb, REG_B) == 0xC0);
    test_end
}

void test_inst_add_a_hl_mem(void)
{
    test_begin
    GameBoy gb = {0};
    Inst inst = {.data = {0x86}, .size = 1};
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
        Inst inst = {.data = {0x27}, .size = 1};
        gb_set_reg(&gb, REG_A, 0x00);

        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x00);
        assert(gb_get_flag(&gb, Flag_Z) == 1);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        Inst inst = {.data = {0x27}, .size = 1};
        gb_set_reg(&gb, REG_A, 0x0F); // BCD: we can only represent 0-9

        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x15);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        Inst inst = {.data = {0x27}, .size = 1};
        gb_set_reg(&gb, REG_A, 0xF0); // BCD: we can only represent 0-9

        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x50);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        Inst inst = {.data = {0x27}, .size = 1};
        gb_set_reg(&gb, REG_A, 0x10); // BCD: we can only represent 0-9

        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x10);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        Inst inst = {.data = {0x27}, .size = 1};
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
        Inst inst = {.data = {0x27}, .size = 1};
        gb_set_reg(&gb, REG_A, 0xF0); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 0, 0, 1);
        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x50);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        Inst inst = {.data = {0x27}, .size = 1};
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
        Inst inst = {.data = {0x27}, .size = 1};
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
        Inst inst = {.data = {0x27}, .size = 1};
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
        Inst inst = {.data = {0x27}, .size = 1};
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
        Inst inst = {.data = {0x27}, .size = 1};
        gb_set_reg(&gb, REG_A, 0x00); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 1, 0, 0); // F = -N-- (After subtraction)
        gb_exec(&gb, inst);

        assert(gb_get_reg8(&gb, REG_A) == 0x00);
        assert(gb_get_flag(&gb, Flag_Z) == 1);
        assert(gb_get_flag(&gb, Flag_N) == 1);
        assert(gb_get_flag(&gb, Flag_H) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    test_end
}

void test_inst_cp_n(void)
{
    test_begin
    GameBoy gb = {0};
    Inst inst = {.data = {0xfe, 0x90}, .size = 2};
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
    test_inst_inc_reg8(REG_HL_IND);
    test_inst_inc_reg8(REG_A);
    printf("\n");

    test_inst_dec_reg8(REG_B);
    test_inst_dec_reg8(REG_C);
    test_inst_dec_reg8(REG_D);
    test_inst_dec_reg8(REG_E);
    test_inst_dec_reg8(REG_H);
    test_inst_dec_reg8(REG_L);
    test_inst_dec_reg8(REG_HL_IND);
    test_inst_dec_reg8(REG_A);
    printf("\n");

    test_inst_ld_reg8_n(REG_B);
    test_inst_ld_reg8_n(REG_C);
    test_inst_ld_reg8_n(REG_D);
    test_inst_ld_reg8_n(REG_E);
    test_inst_ld_reg8_n(REG_H);
    test_inst_ld_reg8_n(REG_L);
    test_inst_ld_reg8_n(REG_HL_IND);
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

    Inst inst = {.data = {0x00}, .size = 1};
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
    
    Inst inst = {.data = {0x00}, .size = 1};
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
        Inst inst = {.data = {0x10, 0x00}, .size = 2};
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

void test_clock_step(void)
{
    test_begin
    GameBoy gb = {0};

    // Assemble instructions
    gb.memory[0] = 0x3c; // INC A       (1 cycle fetch op + 1 cycle exec)

    gb.memory[1] = 0xe0; // LDH (n), A  (1 cycle fetch op + 1 cycle fetch n + 1 cycle write mem
    gb.memory[2] = 0x01;

    gb.memory[3] = 0xcf; // RST $08

    gb.memory[4] = 0x00; // NOP

    // PHI: 0 | CLK: 0 (Initial state)
    assert(gb.phi == 0);
    assert(gb.clk == 0);
    assert(gb.mem_rw == RW_NONE);

    gb_clock_step(&gb);
    // PHI: 0 | CLK: 1
    // R/W:   Read Opcode $0000: $3c (INC A)
    // Exec:  -
    // Fetch: INC A
    assert(gb.phi == 0);
    assert(gb.clk == 1);
    assert(gb.mem_rw == RW_R_OPCODE);
    assert(gb.mem_rw_addr  == 0x0000);
    assert(gb.mem_rw_value == 0x003c);
    
    gb_clock_step(&gb);
    assert(gb.phi == 0);
    assert(gb.clk == 2);

    gb_clock_step(&gb);
    assert(gb.phi == 0);
    assert(gb.clk == 3);

    gb_clock_step(&gb);
    // PHI: 1 | CLK: 4
    // R/W:   Read Opcode $0001: $e0 (LDH (n), A)
    // Exec:  INC A (Cycle 1 out of 1)
    // Fetch: LDH (n), A
    assert(gb.phi == 1);
    assert(gb.clk == 4);
    assert(gb.mem_rw == RW_R_OPCODE);
    assert(gb.mem_rw_addr  == 0x0001);
    assert(gb.mem_rw_value == 0x00e0);
    assert(gb.A == 1);

    gb_clock_step(&gb);
    gb_clock_step(&gb);
    gb_clock_step(&gb);

    gb_clock_step(&gb);
    // PHI: 2 | CLK: 8
    // R/W:   Read Addr.  $0002: $01 (n = $01)
    // Exec:  LDH (n), A (Cycle 1 out of 3)
    // Fetch: -
    assert(gb.phi == 2);
    assert(gb.clk == 8);
    //assert(gb.mem_rw == RW_R_MEM);
    //assert(gb.mem_rw_addr  == 0x0002);
    //assert(gb.mem_rw_value == 0x0001);

    gb_clock_step(&gb);
    gb_clock_step(&gb);
    gb_clock_step(&gb);

    gb_clock_step(&gb);
    // PHI: 3 | CLK: 12
    // R/W:   Write Addr. $ff01: $01 (LDH (n), A)
    // Exec:  LDH (n), A (Cycle 2 out of 3)
    // Fetch: -
    assert(gb.phi == 3);
    assert(gb.clk == 12);
    //assert(gb.mem_rw == RW_W_MEM);
    //assert(gb.mem_rw_addr  == 0xff01);
    //assert(gb.mem_rw_value == 0x0001);

    gb_clock_step(&gb);
    gb_clock_step(&gb);
    gb_clock_step(&gb);

    gb_clock_step(&gb);
    // PHI: 4 | CLK: 16
    // R/W:   Read Opcode $0003: $cf (RST $08)
    // Exec:  LDH (n), A (Cycle 2 out of 3)
    // Fetch: -
    assert(gb.phi == 4);
    assert(gb.clk == 16);
    //assert(gb.mem_rw == RW_R_OPCODE);
    //assert(gb.mem_rw_addr  == 0x0003);
    //assert(gb.mem_rw_value == 0x00cf);

    test_end
}

#include <string.h>
#define assert_inst(inst, sz, d)                        \
{                                                       \
    Inst i = (inst);                                    \
    assert((i).size == (sz));                           \
    if (memcmp((i).data, (d), (size_t)(sz)) != 0) {     \
        for (size_t n = 0; n < (sz); n++) {             \
            printf("%02x ", i.data[n]);                 \
        }                                               \
        printf("\n");                                   \
        assert(0);                                      \
    }                                                   \
}

void test_assemble_prog_to_buf(void)
{
    test_begin
    u8 buffer[0x10000] = {0};
    size_t size = sizeof(buffer);
    const char *program = ""
"wait_vblank:\n"
"    LD A, 0\n"
"    LD ($ff26), A\n"
"    LD A, ($ff44)\n"
"    CP 144\n"
"    JP C, wait_vblank\n"
"";
    gb_assemble_prog_to_buf(buffer, size, program);

    test_end
}

void test_assemble_inst_to_buf(void)
{
    test_begin
    u8 buffer[0x10000] = {0};
    size_t size = sizeof(buffer);
    u8 *buf = buffer;
    buf = gb_assemble_inst_to_buf(buf, &size, "LD A, 0");
    buf = gb_assemble_inst_to_buf(buf, &size, "LD ($ff26), A");
    buf = gb_assemble_inst_to_buf(buf, &size, "LD A, ($ff44)");
    buf = gb_assemble_inst_to_buf(buf, &size, "CP 144");

    size_t size_assembled = sizeof(buffer) - size;
    u8 expected[10] = {0x3e, 0x00, 0xea, 0x26, 0xff, 0xfa, 0x44, 0xff, 0xfe, 0x90};
#if 0
    printf("Written %zu bytes\n", size_assembled);
    for (size_t i = 0; i < size_assembled; i++) {
        printf("%02x ", buffer[i]);
    }
    printf("\n");
#endif
    assert(size_assembled == 10);
    assert(memcmp(buffer, expected, 10) == 0);

    test_end
}

void test_disassemble(void)
{
    test_begin
    size_t size;
    //u8 *rom = read_entire_file("./test-roms/unbricked.gb", &size);
    u8 *rom = read_entire_file("./test-roms/blargg/cpu_instrs/cpu_instrs.gb", &size);
    assert(size <= 0x10000);
    printf("\n");
    gb_disassemble(rom, size);
    test_end
}

void test_assemble_inst(void)
{
    test_begin

    //gb_assemble_inst("; comment");

    assert_inst(gb_assemble_inst("NOP"),            1, "\x00");
    assert_inst(gb_assemble_inst("nop"),            1, "\x00");
    assert_inst(gb_assemble_inst("LD BC, $1234"),   3, "\x01\x34\x12");
    assert_inst(gb_assemble_inst("LD (BC), A"),     1, "\x02");
    assert_inst(gb_assemble_inst("INC BC"),         1, "\x03");
    assert_inst(gb_assemble_inst("INC B"),          1, "\x04");
    assert_inst(gb_assemble_inst("DEC B"),          1, "\x05");
    assert_inst(gb_assemble_inst("LD B, $12"),      2, "\x06\x12");
    assert_inst(gb_assemble_inst("RLCA"),           1, "\x07");
    assert_inst(gb_assemble_inst("LD ($1234), SP"), 3, "\x08\x34\x12");
    assert_inst(gb_assemble_inst("ADD HL, BC"),     1, "\x09");
    assert_inst(gb_assemble_inst("LD A, (BC)"),     1, "\x0a");
    assert_inst(gb_assemble_inst("DEC BC"),         1, "\x0b");
    assert_inst(gb_assemble_inst("INC C"),          1, "\x0c");
    assert_inst(gb_assemble_inst("DEC C"),          1, "\x0d");
    assert_inst(gb_assemble_inst("LD C, $12"),      2, "\x0e\x12");
    assert_inst(gb_assemble_inst("RRCA"),           1, "\x0f");

    assert_inst(gb_assemble_inst("STOP"),           2, "\x10\x00");
    assert_inst(gb_assemble_inst("LD DE, $4321"),   3, "\x11\x21\x43");
    assert_inst(gb_assemble_inst("LD (DE), A"),     1, "\x12");
    assert_inst(gb_assemble_inst("INC DE"),         1, "\x13");
    assert_inst(gb_assemble_inst("INC D"),          1, "\x14");
    assert_inst(gb_assemble_inst("DEC D"),          1, "\x15");
    assert_inst(gb_assemble_inst("LD D, $12"),      2, "\x16\x12");
    assert_inst(gb_assemble_inst("RLA"),            1, "\x17");
    assert_inst(gb_assemble_inst("JR 5"),           2, "\x18\x05");
    assert_inst(gb_assemble_inst("ADD HL, DE"),     1, "\x19");
    assert_inst(gb_assemble_inst("LD A, (DE)"),     1, "\x1a");
    assert_inst(gb_assemble_inst("DEC DE"),         1, "\x1b");
    assert_inst(gb_assemble_inst("INC E"),          1, "\x1c");
    assert_inst(gb_assemble_inst("DEC E"),          1, "\x1d");
    assert_inst(gb_assemble_inst("LD E, $12"),      2, "\x1e\x12");
    assert_inst(gb_assemble_inst("RRA"),            1, "\x1f");

    assert_inst(gb_assemble_inst("JR NZ, 5"),       2, "\x20\x05");
    assert_inst(gb_assemble_inst("LD HL, $1234"),   3, "\x21\x34\x12");
    assert_inst(gb_assemble_inst("LD (HL+), A"),    1, "\x22");
    assert_inst(gb_assemble_inst("INC HL"),         1, "\x23");
    assert_inst(gb_assemble_inst("INC H"),          1, "\x24");
    assert_inst(gb_assemble_inst("DEC H"),          1, "\x25");
    assert_inst(gb_assemble_inst("LD H, $12"),      2, "\x26\x12");
    assert_inst(gb_assemble_inst("DAA"),            1, "\x27");
    assert_inst(gb_assemble_inst("JR Z, 5"),        2, "\x28\x05");
    assert_inst(gb_assemble_inst("ADD HL, HL"),     1, "\x29");
    assert_inst(gb_assemble_inst("LD A, (HL+)"),    1, "\x2a");
    assert_inst(gb_assemble_inst("DEC HL"),         1, "\x2b");
    assert_inst(gb_assemble_inst("INC L"),          1, "\x2c");
    assert_inst(gb_assemble_inst("DEC L"),          1, "\x2d");
    assert_inst(gb_assemble_inst("LD L, $12"),      2, "\x2e\x12");
    assert_inst(gb_assemble_inst("CPL"),            1, "\x2f");

    assert_inst(gb_assemble_inst("JR NC, 5"),       2, "\x30\x05");
    assert_inst(gb_assemble_inst("LD SP, $1234"),   3, "\x31\x34\x12");
    assert_inst(gb_assemble_inst("LD (HL-), A"),    1, "\x32");
    assert_inst(gb_assemble_inst("INC SP"),         1, "\x33");
    assert_inst(gb_assemble_inst("INC (HL)"),       1, "\x34");
    assert_inst(gb_assemble_inst("DEC (HL)"),       1, "\x35");
    assert_inst(gb_assemble_inst("LD (HL), $12"),   2, "\x36\x12");
    assert_inst(gb_assemble_inst("SCF"),            1, "\x37");
    assert_inst(gb_assemble_inst("JR C, 5"),        2, "\x38\x05");
    assert_inst(gb_assemble_inst("ADD HL, SP"),     1, "\x39");
    assert_inst(gb_assemble_inst("LD A, (HL-)"),    1, "\x3a");
    assert_inst(gb_assemble_inst("DEC SP"),         1, "\x3b");
    assert_inst(gb_assemble_inst("INC A"),          1, "\x3c");
    assert_inst(gb_assemble_inst("DEC A"),          1, "\x3d");
    assert_inst(gb_assemble_inst("LD A, $12"),      2, "\x3e\x12");
    assert_inst(gb_assemble_inst("CCF"),            1, "\x3f");

    assert_inst(gb_assemble_inst("LD B, B"),        1, "\x40");
    assert_inst(gb_assemble_inst("LD B, C"),        1, "\x41");
    assert_inst(gb_assemble_inst("LD B, D"),        1, "\x42");
    assert_inst(gb_assemble_inst("LD B, E"),        1, "\x43");
    assert_inst(gb_assemble_inst("LD B, H"),        1, "\x44");
    assert_inst(gb_assemble_inst("LD B, L"),        1, "\x45");
    assert_inst(gb_assemble_inst("LD B, (HL)"),     1, "\x46");
    assert_inst(gb_assemble_inst("LD B, A"),        1, "\x47");
    assert_inst(gb_assemble_inst("LD C, B"),        1, "\x48");
    assert_inst(gb_assemble_inst("LD C, C"),        1, "\x49");
    assert_inst(gb_assemble_inst("LD C, D"),        1, "\x4a");
    assert_inst(gb_assemble_inst("LD C, E"),        1, "\x4b");
    assert_inst(gb_assemble_inst("LD C, H"),        1, "\x4c");
    assert_inst(gb_assemble_inst("LD C, L"),        1, "\x4d");
    assert_inst(gb_assemble_inst("LD C, (HL)"),     1, "\x4e");
    assert_inst(gb_assemble_inst("LD C, A"),        1, "\x4f");

    assert_inst(gb_assemble_inst("LD D, B"),        1, "\x50");
    assert_inst(gb_assemble_inst("LD D, C"),        1, "\x51");
    assert_inst(gb_assemble_inst("LD D, D"),        1, "\x52");
    assert_inst(gb_assemble_inst("LD D, E"),        1, "\x53");
    assert_inst(gb_assemble_inst("LD D, H"),        1, "\x54");
    assert_inst(gb_assemble_inst("LD D, L"),        1, "\x55");
    assert_inst(gb_assemble_inst("LD D, (HL)"),     1, "\x56");
    assert_inst(gb_assemble_inst("LD D, A"),        1, "\x57");
    assert_inst(gb_assemble_inst("LD E, B"),        1, "\x58");
    assert_inst(gb_assemble_inst("LD E, C"),        1, "\x59");
    assert_inst(gb_assemble_inst("LD E, D"),        1, "\x5a");
    assert_inst(gb_assemble_inst("LD E, E"),        1, "\x5b");
    assert_inst(gb_assemble_inst("LD E, H"),        1, "\x5c");
    assert_inst(gb_assemble_inst("LD E, L"),        1, "\x5d");
    assert_inst(gb_assemble_inst("LD E, (HL)"),     1, "\x5e");
    assert_inst(gb_assemble_inst("LD E, A"),        1, "\x5f");

    assert_inst(gb_assemble_inst("LD H, B"),        1, "\x60");
    assert_inst(gb_assemble_inst("LD H, C"),        1, "\x61");
    assert_inst(gb_assemble_inst("LD H, D"),        1, "\x62");
    assert_inst(gb_assemble_inst("LD H, E"),        1, "\x63");
    assert_inst(gb_assemble_inst("LD H, H"),        1, "\x64");
    assert_inst(gb_assemble_inst("LD H, L"),        1, "\x65");
    assert_inst(gb_assemble_inst("LD H, (HL)"),     1, "\x66");
    assert_inst(gb_assemble_inst("LD H, A"),        1, "\x67");
    assert_inst(gb_assemble_inst("LD L, B"),        1, "\x68");
    assert_inst(gb_assemble_inst("LD L, C"),        1, "\x69");
    assert_inst(gb_assemble_inst("LD L, D"),        1, "\x6a");
    assert_inst(gb_assemble_inst("LD L, E"),        1, "\x6b");
    assert_inst(gb_assemble_inst("LD L, H"),        1, "\x6c");
    assert_inst(gb_assemble_inst("LD L, L"),        1, "\x6d");
    assert_inst(gb_assemble_inst("LD L, (HL)"),     1, "\x6e");
    assert_inst(gb_assemble_inst("LD L, A"),        1, "\x6f");

    assert_inst(gb_assemble_inst("LD (HL), B"),     1, "\x70");
    assert_inst(gb_assemble_inst("LD (HL), C"),     1, "\x71");
    assert_inst(gb_assemble_inst("LD (HL), D"),     1, "\x72");
    assert_inst(gb_assemble_inst("LD (HL), E"),     1, "\x73");
    assert_inst(gb_assemble_inst("LD (HL), H"),     1, "\x74");
    assert_inst(gb_assemble_inst("LD (HL), L"),     1, "\x75");
    assert_inst(gb_assemble_inst("HALT"),           1, "\x76");
    assert_inst(gb_assemble_inst("LD (HL), A"),     1, "\x77");
    assert_inst(gb_assemble_inst("LD A, B"),        1, "\x78");
    assert_inst(gb_assemble_inst("LD A, C"),        1, "\x79");
    assert_inst(gb_assemble_inst("LD A, D"),        1, "\x7a");
    assert_inst(gb_assemble_inst("LD A, E"),        1, "\x7b");
    assert_inst(gb_assemble_inst("LD A, H"),        1, "\x7c");
    assert_inst(gb_assemble_inst("LD A, L"),        1, "\x7d");
    assert_inst(gb_assemble_inst("LD A, (HL)"),     1, "\x7e");
    assert_inst(gb_assemble_inst("LD A, A"),        1, "\x7f");

    assert_inst(gb_assemble_inst("ADD A, B"),       1, "\x80");
    assert_inst(gb_assemble_inst("ADD A, C"),       1, "\x81");
    assert_inst(gb_assemble_inst("ADD A, D"),       1, "\x82");
    assert_inst(gb_assemble_inst("ADD A, E"),       1, "\x83");
    assert_inst(gb_assemble_inst("ADD A, H"),       1, "\x84");
    assert_inst(gb_assemble_inst("ADD A, L"),       1, "\x85");
    assert_inst(gb_assemble_inst("ADD A, (HL)"),    1, "\x86");
    assert_inst(gb_assemble_inst("ADD A, A"),       1, "\x87");
    assert_inst(gb_assemble_inst("ADC A, B"),       1, "\x88");
    assert_inst(gb_assemble_inst("ADC A, C"),       1, "\x89");
    assert_inst(gb_assemble_inst("ADC A, D"),       1, "\x8a");
    assert_inst(gb_assemble_inst("ADC A, E"),       1, "\x8b");
    assert_inst(gb_assemble_inst("ADC A, H"),       1, "\x8c");
    assert_inst(gb_assemble_inst("ADC A, L"),       1, "\x8d");
    assert_inst(gb_assemble_inst("ADC A, (HL)"),    1, "\x8e");
    assert_inst(gb_assemble_inst("ADC A, A"),       1, "\x8f");

    assert_inst(gb_assemble_inst("SUB B"),          1, "\x90");
    assert_inst(gb_assemble_inst("SUB C"),          1, "\x91");
    assert_inst(gb_assemble_inst("SUB D"),          1, "\x92");
    assert_inst(gb_assemble_inst("SUB E"),          1, "\x93");
    assert_inst(gb_assemble_inst("SUB H"),          1, "\x94");
    assert_inst(gb_assemble_inst("SUB L"),          1, "\x95");
    assert_inst(gb_assemble_inst("SUB (HL)"),       1, "\x96");
    assert_inst(gb_assemble_inst("SUB A"),          1, "\x97");
    assert_inst(gb_assemble_inst("SBC A, B"),       1, "\x98");
    assert_inst(gb_assemble_inst("SBC A, C"),       1, "\x99");
    assert_inst(gb_assemble_inst("SBC A, D"),       1, "\x9a");
    assert_inst(gb_assemble_inst("SBC A, E"),       1, "\x9b");
    assert_inst(gb_assemble_inst("SBC A, H"),       1, "\x9c");
    assert_inst(gb_assemble_inst("SBC A, L"),       1, "\x9d");
    assert_inst(gb_assemble_inst("SBC A, (HL)"),    1, "\x9e");
    assert_inst(gb_assemble_inst("SBC A, A"),       1, "\x9f");

    assert_inst(gb_assemble_inst("AND B"),          1, "\xa0");
    assert_inst(gb_assemble_inst("AND C"),          1, "\xa1");
    assert_inst(gb_assemble_inst("AND D"),          1, "\xa2");
    assert_inst(gb_assemble_inst("AND E"),          1, "\xa3");
    assert_inst(gb_assemble_inst("AND H"),          1, "\xa4");
    assert_inst(gb_assemble_inst("AND L"),          1, "\xa5");
    assert_inst(gb_assemble_inst("AND (HL)"),       1, "\xa6");
    assert_inst(gb_assemble_inst("AND A"),          1, "\xa7");
    assert_inst(gb_assemble_inst("XOR B"),          1, "\xa8");
    assert_inst(gb_assemble_inst("XOR C"),          1, "\xa9");
    assert_inst(gb_assemble_inst("XOR D"),          1, "\xaa");
    assert_inst(gb_assemble_inst("XOR E"),          1, "\xab");
    assert_inst(gb_assemble_inst("XOR H"),          1, "\xac");
    assert_inst(gb_assemble_inst("XOR L"),          1, "\xad");
    assert_inst(gb_assemble_inst("XOR (HL)"),       1, "\xae");
    assert_inst(gb_assemble_inst("XOR A"),          1, "\xaf");

    assert_inst(gb_assemble_inst("OR B"),           1, "\xb0");
    assert_inst(gb_assemble_inst("OR C"),           1, "\xb1");
    assert_inst(gb_assemble_inst("OR D"),           1, "\xb2");
    assert_inst(gb_assemble_inst("OR E"),           1, "\xb3");
    assert_inst(gb_assemble_inst("OR H"),           1, "\xb4");
    assert_inst(gb_assemble_inst("OR L"),           1, "\xb5");
    assert_inst(gb_assemble_inst("OR (HL)"),        1, "\xb6");
    assert_inst(gb_assemble_inst("OR A"),           1, "\xb7");
    assert_inst(gb_assemble_inst("CP B"),           1, "\xb8");
    assert_inst(gb_assemble_inst("CP C"),           1, "\xb9");
    assert_inst(gb_assemble_inst("CP D"),           1, "\xba");
    assert_inst(gb_assemble_inst("CP E"),           1, "\xbb");
    assert_inst(gb_assemble_inst("CP H"),           1, "\xbc");
    assert_inst(gb_assemble_inst("CP L"),           1, "\xbd");
    assert_inst(gb_assemble_inst("CP (HL)"),        1, "\xbe");
    assert_inst(gb_assemble_inst("CP A"),           1, "\xbf");

    assert_inst(gb_assemble_inst("RET NZ"),         1, "\xc0");
    assert_inst(gb_assemble_inst("POP BC"),         1, "\xc1");
    assert_inst(gb_assemble_inst("JP NZ, $1234"),   3, "\xc2\x34\x12");
    assert_inst(gb_assemble_inst("JP $1234"),       3, "\xc3\x34\x12");
    assert_inst(gb_assemble_inst("CALL NZ, $1234"), 3, "\xc4\x34\x12");
    assert_inst(gb_assemble_inst("PUSH BC"),        1, "\xc5");
    assert_inst(gb_assemble_inst("ADD A, $12"),     2, "\xc6\x12");
    assert_inst(gb_assemble_inst("RST $00"),        1, "\xc7");
    assert_inst(gb_assemble_inst("RET Z"),          1, "\xc8");
    assert_inst(gb_assemble_inst("RET"),            1, "\xc9");
    assert_inst(gb_assemble_inst("JP Z, $1234"),    3, "\xca\x34\x12");
    // --- Prefix CB ---                                \xcb
    assert_inst(gb_assemble_inst("CALL Z, $1234"),  3, "\xcc\x34\x12");
    assert_inst(gb_assemble_inst("CALL $1234"),     3, "\xcd\x34\x12");
    assert_inst(gb_assemble_inst("ADC A, $12"),     2, "\xce\x12");
    assert_inst(gb_assemble_inst("RST $08"),        1, "\xcf");

    assert_inst(gb_assemble_inst("RET NC"),         1, "\xd0");
    assert_inst(gb_assemble_inst("POP DE"),         1, "\xd1");
    assert_inst(gb_assemble_inst("JP NC, $1234"),   3, "\xd2\x34\x12");
    // --- Invalid Opcode D3 ---                        \xd3
    assert_inst(gb_assemble_inst("CALL NC, $1234"), 3, "\xd4\x34\x12");
    assert_inst(gb_assemble_inst("PUSH DE"),        1, "\xd5");
    assert_inst(gb_assemble_inst("SUB $12"),        2, "\xd6\x12");
    assert_inst(gb_assemble_inst("RST $10"),        1, "\xd7");
    assert_inst(gb_assemble_inst("RET C"),          1, "\xd8");
    assert_inst(gb_assemble_inst("RETI"),           1, "\xd9");
    assert_inst(gb_assemble_inst("JP C, $1234"),    3, "\xda\x34\x12");
    // --- Invalid Opcode DB ---                        \xdb
    assert_inst(gb_assemble_inst("CALL C, $1234"),  3, "\xdc\x34\x12");
    // --- Invalid Opcode DD ---                        \xdd
    assert_inst(gb_assemble_inst("SBC A, $12"),     2, "\xde\x12");
    assert_inst(gb_assemble_inst("RST $18"),        1, "\xdf");

    assert_inst(gb_assemble_inst("LDH ($01), A"),   2, "\xe0\x01");
    assert_inst(gb_assemble_inst("POP HL"),         1, "\xe1");
    assert_inst(gb_assemble_inst("LD (C), A"),      1, "\xe2");
    // --- Invalid Opcode E3 ---                        \xe3
    // --- Invalid Opcode E4 ---                        \xe4
    assert_inst(gb_assemble_inst("PUSH HL"),        1, "\xe5");
    assert_inst(gb_assemble_inst("AND $05"),        2, "\xe6\x05");
    assert_inst(gb_assemble_inst("RST $20"),        1, "\xe7");
    assert_inst(gb_assemble_inst("ADD SP, $12"),    2, "\xe8\x12");
    assert_inst(gb_assemble_inst("JP (HL)"),        1, "\xe9");
    assert_inst(gb_assemble_inst("LD ($1234), A"),  3, "\xea\x34\x12");
    // --- Invalid Opcode EB ---                        \xeb
    // --- Invalid Opcode EC ---                        \xec
    // --- Invalid Opcode ED ---                        \xed
    assert_inst(gb_assemble_inst("XOR $12"),        2, "\xee\x12");
    assert_inst(gb_assemble_inst("RST $28"),        1, "\xef");

    assert_inst(gb_assemble_inst("LDH A, ($01)"),   2, "\xf0\x01");
    assert_inst(gb_assemble_inst("POP AF"),         1, "\xf1");
    assert_inst(gb_assemble_inst("LD A, (C)"),      1, "\xf2");
    assert_inst(gb_assemble_inst("DI"),             1, "\xf3");
    // --- Invalid Opcode F4 ---                        \xf4
    assert_inst(gb_assemble_inst("PUSH AF"),        1, "\xf5");
    assert_inst(gb_assemble_inst("OR $12"),         2, "\xf6\x12");
    assert_inst(gb_assemble_inst("RST $30"),        1, "\xf7");
    assert_inst(gb_assemble_inst("LD HL, SP+3"),    2, "\xf8\x03"); // TODO: negative
    assert_inst(gb_assemble_inst("LD SP, HL"),      1, "\xf9");
    assert_inst(gb_assemble_inst("LD A, ($1234)"),  3, "\xfa\x34\x12");
    assert_inst(gb_assemble_inst("EI"),             1, "\xfb");
    // --- Invalid Opcode FC ---                        \xfc
    // --- Invalid Opcode FD ---                        \xfd
    assert_inst(gb_assemble_inst("CP $12"),         2, "\xfe\x12");
    assert_inst(gb_assemble_inst("RST $38"),        1, "\xff");

    // PREFIX CB
    assert_inst(gb_assemble_inst("RLC B"),          2, "\xcb\x00");
    assert_inst(gb_assemble_inst("RLC C"),          2, "\xcb\x01");
    assert_inst(gb_assemble_inst("RLC D"),          2, "\xcb\x02");
    assert_inst(gb_assemble_inst("RLC E"),          2, "\xcb\x03");
    assert_inst(gb_assemble_inst("RLC H"),          2, "\xcb\x04");
    assert_inst(gb_assemble_inst("RLC L"),          2, "\xcb\x05");
    assert_inst(gb_assemble_inst("RLC (HL)"),       2, "\xcb\x06");
    assert_inst(gb_assemble_inst("RLC A"),          2, "\xcb\x07");

    assert_inst(gb_assemble_inst("RRC B"),          2, "\xcb\x08");
    assert_inst(gb_assemble_inst("RRC C"),          2, "\xcb\x09");
    assert_inst(gb_assemble_inst("RRC D"),          2, "\xcb\x0a");
    assert_inst(gb_assemble_inst("RRC E"),          2, "\xcb\x0b");
    assert_inst(gb_assemble_inst("RRC H"),          2, "\xcb\x0c");
    assert_inst(gb_assemble_inst("RRC L"),          2, "\xcb\x0d");
    assert_inst(gb_assemble_inst("RRC (HL)"),       2, "\xcb\x0e");
    assert_inst(gb_assemble_inst("RRC A"),          2, "\xcb\x0f");

    assert_inst(gb_assemble_inst("RL B"),           2, "\xcb\x10");
    assert_inst(gb_assemble_inst("RL C"),           2, "\xcb\x11");
    assert_inst(gb_assemble_inst("RL D"),           2, "\xcb\x12");
    assert_inst(gb_assemble_inst("RL E"),           2, "\xcb\x13");
    assert_inst(gb_assemble_inst("RL H"),           2, "\xcb\x14");
    assert_inst(gb_assemble_inst("RL L"),           2, "\xcb\x15");
    assert_inst(gb_assemble_inst("RL (HL)"),        2, "\xcb\x16");
    assert_inst(gb_assemble_inst("RL A"),           2, "\xcb\x17");

    assert_inst(gb_assemble_inst("RR B"),           2, "\xcb\x18");
    assert_inst(gb_assemble_inst("RR C"),           2, "\xcb\x19");
    assert_inst(gb_assemble_inst("RR D"),           2, "\xcb\x1a");
    assert_inst(gb_assemble_inst("RR E"),           2, "\xcb\x1b");
    assert_inst(gb_assemble_inst("RR H"),           2, "\xcb\x1c");
    assert_inst(gb_assemble_inst("RR L"),           2, "\xcb\x1d");
    assert_inst(gb_assemble_inst("RR (HL)"),        2, "\xcb\x1e");
    assert_inst(gb_assemble_inst("RR A"),           2, "\xcb\x1f");

    assert_inst(gb_assemble_inst("SLA B"),          2, "\xcb\x20");
    assert_inst(gb_assemble_inst("SLA C"),          2, "\xcb\x21");
    assert_inst(gb_assemble_inst("SLA D"),          2, "\xcb\x22");
    assert_inst(gb_assemble_inst("SLA E"),          2, "\xcb\x23");
    assert_inst(gb_assemble_inst("SLA H"),          2, "\xcb\x24");
    assert_inst(gb_assemble_inst("SLA L"),          2, "\xcb\x25");
    assert_inst(gb_assemble_inst("SLA (HL)"),       2, "\xcb\x26");
    assert_inst(gb_assemble_inst("SLA A"),          2, "\xcb\x27");

    assert_inst(gb_assemble_inst("SRA B"),          2, "\xcb\x28");
    assert_inst(gb_assemble_inst("SRA C"),          2, "\xcb\x29");
    assert_inst(gb_assemble_inst("SRA D"),          2, "\xcb\x2a");
    assert_inst(gb_assemble_inst("SRA E"),          2, "\xcb\x2b");
    assert_inst(gb_assemble_inst("SRA H"),          2, "\xcb\x2c");
    assert_inst(gb_assemble_inst("SRA L"),          2, "\xcb\x2d");
    assert_inst(gb_assemble_inst("SRA (HL)"),       2, "\xcb\x2e");
    assert_inst(gb_assemble_inst("SRA A"),          2, "\xcb\x2f");

    assert_inst(gb_assemble_inst("SWAP B"),         2, "\xcb\x30");
    assert_inst(gb_assemble_inst("SWAP C"),         2, "\xcb\x31");
    assert_inst(gb_assemble_inst("SWAP D"),         2, "\xcb\x32");
    assert_inst(gb_assemble_inst("SWAP E"),         2, "\xcb\x33");
    assert_inst(gb_assemble_inst("SWAP H"),         2, "\xcb\x34");
    assert_inst(gb_assemble_inst("SWAP L"),         2, "\xcb\x35");
    assert_inst(gb_assemble_inst("SWAP (HL)"),      2, "\xcb\x36");
    assert_inst(gb_assemble_inst("SWAP A"),         2, "\xcb\x37");

    assert_inst(gb_assemble_inst("SRL B"),          2, "\xcb\x38");
    assert_inst(gb_assemble_inst("SRL C"),          2, "\xcb\x39");
    assert_inst(gb_assemble_inst("SRL D"),          2, "\xcb\x3a");
    assert_inst(gb_assemble_inst("SRL E"),          2, "\xcb\x3b");
    assert_inst(gb_assemble_inst("SRL H"),          2, "\xcb\x3c");
    assert_inst(gb_assemble_inst("SRL L"),          2, "\xcb\x3d");
    assert_inst(gb_assemble_inst("SRL (HL)"),       2, "\xcb\x3e");
    assert_inst(gb_assemble_inst("SRL A"),          2, "\xcb\x3f");

    assert_inst(gb_assemble_inst("BIT 0, B"),       2, "\xcb\x40");
    assert_inst(gb_assemble_inst("BIT 0, C"),       2, "\xcb\x41");
    assert_inst(gb_assemble_inst("BIT 0, D"),       2, "\xcb\x42");
    assert_inst(gb_assemble_inst("BIT 0, E"),       2, "\xcb\x43");
    assert_inst(gb_assemble_inst("BIT 0, H"),       2, "\xcb\x44");
    assert_inst(gb_assemble_inst("BIT 0, L"),       2, "\xcb\x45");
    assert_inst(gb_assemble_inst("BIT 0, (HL)"),    2, "\xcb\x46");
    assert_inst(gb_assemble_inst("BIT 0, A"),       2, "\xcb\x47");

    assert_inst(gb_assemble_inst("BIT 1, B"),       2, "\xcb\x48");
    assert_inst(gb_assemble_inst("BIT 1, C"),       2, "\xcb\x49");
    assert_inst(gb_assemble_inst("BIT 1, D"),       2, "\xcb\x4a");
    assert_inst(gb_assemble_inst("BIT 1, E"),       2, "\xcb\x4b");
    assert_inst(gb_assemble_inst("BIT 1, H"),       2, "\xcb\x4c");
    assert_inst(gb_assemble_inst("BIT 1, L"),       2, "\xcb\x4d");
    assert_inst(gb_assemble_inst("BIT 1, (HL)"),    2, "\xcb\x4e");
    assert_inst(gb_assemble_inst("BIT 1, A"),       2, "\xcb\x4f");

    assert_inst(gb_assemble_inst("BIT 2, B"),       2, "\xcb\x50");
    assert_inst(gb_assemble_inst("BIT 2, C"),       2, "\xcb\x51");
    assert_inst(gb_assemble_inst("BIT 2, D"),       2, "\xcb\x52");
    assert_inst(gb_assemble_inst("BIT 2, E"),       2, "\xcb\x53");
    assert_inst(gb_assemble_inst("BIT 2, H"),       2, "\xcb\x54");
    assert_inst(gb_assemble_inst("BIT 2, L"),       2, "\xcb\x55");
    assert_inst(gb_assemble_inst("BIT 2, (HL)"),    2, "\xcb\x56");
    assert_inst(gb_assemble_inst("BIT 2, A"),       2, "\xcb\x57");

    assert_inst(gb_assemble_inst("BIT 3, B"),       2, "\xcb\x58");
    assert_inst(gb_assemble_inst("BIT 3, C"),       2, "\xcb\x59");
    assert_inst(gb_assemble_inst("BIT 3, D"),       2, "\xcb\x5a");
    assert_inst(gb_assemble_inst("BIT 3, E"),       2, "\xcb\x5b");
    assert_inst(gb_assemble_inst("BIT 3, H"),       2, "\xcb\x5c");
    assert_inst(gb_assemble_inst("BIT 3, L"),       2, "\xcb\x5d");
    assert_inst(gb_assemble_inst("BIT 3, (HL)"),    2, "\xcb\x5e");
    assert_inst(gb_assemble_inst("BIT 3, A"),       2, "\xcb\x5f");

    assert_inst(gb_assemble_inst("BIT 4, B"),       2, "\xcb\x60");
    assert_inst(gb_assemble_inst("BIT 4, C"),       2, "\xcb\x61");
    assert_inst(gb_assemble_inst("BIT 4, D"),       2, "\xcb\x62");
    assert_inst(gb_assemble_inst("BIT 4, E"),       2, "\xcb\x63");
    assert_inst(gb_assemble_inst("BIT 4, H"),       2, "\xcb\x64");
    assert_inst(gb_assemble_inst("BIT 4, L"),       2, "\xcb\x65");
    assert_inst(gb_assemble_inst("BIT 4, (HL)"),    2, "\xcb\x66");
    assert_inst(gb_assemble_inst("BIT 4, A"),       2, "\xcb\x67");

    assert_inst(gb_assemble_inst("BIT 5, B"),       2, "\xcb\x68");
    assert_inst(gb_assemble_inst("BIT 5, C"),       2, "\xcb\x69");
    assert_inst(gb_assemble_inst("BIT 5, D"),       2, "\xcb\x6a");
    assert_inst(gb_assemble_inst("BIT 5, E"),       2, "\xcb\x6b");
    assert_inst(gb_assemble_inst("BIT 5, H"),       2, "\xcb\x6c");
    assert_inst(gb_assemble_inst("BIT 5, L"),       2, "\xcb\x6d");
    assert_inst(gb_assemble_inst("BIT 5, (HL)"),    2, "\xcb\x6e");
    assert_inst(gb_assemble_inst("BIT 5, A"),       2, "\xcb\x6f");

    assert_inst(gb_assemble_inst("BIT 6, B"),       2, "\xcb\x70");
    assert_inst(gb_assemble_inst("BIT 6, C"),       2, "\xcb\x71");
    assert_inst(gb_assemble_inst("BIT 6, D"),       2, "\xcb\x72");
    assert_inst(gb_assemble_inst("BIT 6, E"),       2, "\xcb\x73");
    assert_inst(gb_assemble_inst("BIT 6, H"),       2, "\xcb\x74");
    assert_inst(gb_assemble_inst("BIT 6, L"),       2, "\xcb\x75");
    assert_inst(gb_assemble_inst("BIT 6, (HL)"),    2, "\xcb\x76");
    assert_inst(gb_assemble_inst("BIT 6, A"),       2, "\xcb\x77");

    assert_inst(gb_assemble_inst("BIT 7, B"),       2, "\xcb\x78");
    assert_inst(gb_assemble_inst("BIT 7, C"),       2, "\xcb\x79");
    assert_inst(gb_assemble_inst("BIT 7, D"),       2, "\xcb\x7a");
    assert_inst(gb_assemble_inst("BIT 7, E"),       2, "\xcb\x7b");
    assert_inst(gb_assemble_inst("BIT 7, H"),       2, "\xcb\x7c");
    assert_inst(gb_assemble_inst("BIT 7, L"),       2, "\xcb\x7d");
    assert_inst(gb_assemble_inst("BIT 7, (HL)"),    2, "\xcb\x7e");
    assert_inst(gb_assemble_inst("BIT 7, A"),       2, "\xcb\x7f");


    assert_inst(gb_assemble_inst("RES 0, B"),       2, "\xcb\x80");
    assert_inst(gb_assemble_inst("RES 0, C"),       2, "\xcb\x81");
    assert_inst(gb_assemble_inst("RES 0, D"),       2, "\xcb\x82");
    assert_inst(gb_assemble_inst("RES 0, E"),       2, "\xcb\x83");
    assert_inst(gb_assemble_inst("RES 0, H"),       2, "\xcb\x84");
    assert_inst(gb_assemble_inst("RES 0, L"),       2, "\xcb\x85");
    assert_inst(gb_assemble_inst("RES 0, (HL)"),    2, "\xcb\x86");
    assert_inst(gb_assemble_inst("RES 0, A"),       2, "\xcb\x87");

    assert_inst(gb_assemble_inst("RES 1, B"),       2, "\xcb\x88");
    assert_inst(gb_assemble_inst("RES 1, C"),       2, "\xcb\x89");
    assert_inst(gb_assemble_inst("RES 1, D"),       2, "\xcb\x8a");
    assert_inst(gb_assemble_inst("RES 1, E"),       2, "\xcb\x8b");
    assert_inst(gb_assemble_inst("RES 1, H"),       2, "\xcb\x8c");
    assert_inst(gb_assemble_inst("RES 1, L"),       2, "\xcb\x8d");
    assert_inst(gb_assemble_inst("RES 1, (HL)"),    2, "\xcb\x8e");
    assert_inst(gb_assemble_inst("RES 1, A"),       2, "\xcb\x8f");

    assert_inst(gb_assemble_inst("RES 2, B"),       2, "\xcb\x90");
    assert_inst(gb_assemble_inst("RES 2, C"),       2, "\xcb\x91");
    assert_inst(gb_assemble_inst("RES 2, D"),       2, "\xcb\x92");
    assert_inst(gb_assemble_inst("RES 2, E"),       2, "\xcb\x93");
    assert_inst(gb_assemble_inst("RES 2, H"),       2, "\xcb\x94");
    assert_inst(gb_assemble_inst("RES 2, L"),       2, "\xcb\x95");
    assert_inst(gb_assemble_inst("RES 2, (HL)"),    2, "\xcb\x96");
    assert_inst(gb_assemble_inst("RES 2, A"),       2, "\xcb\x97");

    assert_inst(gb_assemble_inst("RES 3, B"),       2, "\xcb\x98");
    assert_inst(gb_assemble_inst("RES 3, C"),       2, "\xcb\x99");
    assert_inst(gb_assemble_inst("RES 3, D"),       2, "\xcb\x9a");
    assert_inst(gb_assemble_inst("RES 3, E"),       2, "\xcb\x9b");
    assert_inst(gb_assemble_inst("RES 3, H"),       2, "\xcb\x9c");
    assert_inst(gb_assemble_inst("RES 3, L"),       2, "\xcb\x9d");
    assert_inst(gb_assemble_inst("RES 3, (HL)"),    2, "\xcb\x9e");
    assert_inst(gb_assemble_inst("RES 3, A"),       2, "\xcb\x9f");

    assert_inst(gb_assemble_inst("RES 4, B"),       2, "\xcb\xa0");
    assert_inst(gb_assemble_inst("RES 4, C"),       2, "\xcb\xa1");
    assert_inst(gb_assemble_inst("RES 4, D"),       2, "\xcb\xa2");
    assert_inst(gb_assemble_inst("RES 4, E"),       2, "\xcb\xa3");
    assert_inst(gb_assemble_inst("RES 4, H"),       2, "\xcb\xa4");
    assert_inst(gb_assemble_inst("RES 4, L"),       2, "\xcb\xa5");
    assert_inst(gb_assemble_inst("RES 4, (HL)"),    2, "\xcb\xa6");
    assert_inst(gb_assemble_inst("RES 4, A"),       2, "\xcb\xa7");

    assert_inst(gb_assemble_inst("RES 5, B"),       2, "\xcb\xa8");
    assert_inst(gb_assemble_inst("RES 5, C"),       2, "\xcb\xa9");
    assert_inst(gb_assemble_inst("RES 5, D"),       2, "\xcb\xaa");
    assert_inst(gb_assemble_inst("RES 5, E"),       2, "\xcb\xab");
    assert_inst(gb_assemble_inst("RES 5, H"),       2, "\xcb\xac");
    assert_inst(gb_assemble_inst("RES 5, L"),       2, "\xcb\xad");
    assert_inst(gb_assemble_inst("RES 5, (HL)"),    2, "\xcb\xae");
    assert_inst(gb_assemble_inst("RES 5, A"),       2, "\xcb\xaf");

    assert_inst(gb_assemble_inst("RES 6, B"),       2, "\xcb\xb0");
    assert_inst(gb_assemble_inst("RES 6, C"),       2, "\xcb\xb1");
    assert_inst(gb_assemble_inst("RES 6, D"),       2, "\xcb\xb2");
    assert_inst(gb_assemble_inst("RES 6, E"),       2, "\xcb\xb3");
    assert_inst(gb_assemble_inst("RES 6, H"),       2, "\xcb\xb4");
    assert_inst(gb_assemble_inst("RES 6, L"),       2, "\xcb\xb5");
    assert_inst(gb_assemble_inst("RES 6, (HL)"),    2, "\xcb\xb6");
    assert_inst(gb_assemble_inst("RES 6, A"),       2, "\xcb\xb7");

    assert_inst(gb_assemble_inst("RES 7, B"),       2, "\xcb\xb8");
    assert_inst(gb_assemble_inst("RES 7, C"),       2, "\xcb\xb9");
    assert_inst(gb_assemble_inst("RES 7, D"),       2, "\xcb\xba");
    assert_inst(gb_assemble_inst("RES 7, E"),       2, "\xcb\xbb");
    assert_inst(gb_assemble_inst("RES 7, H"),       2, "\xcb\xbc");
    assert_inst(gb_assemble_inst("RES 7, L"),       2, "\xcb\xbd");
    assert_inst(gb_assemble_inst("RES 7, (HL)"),    2, "\xcb\xbe");
    assert_inst(gb_assemble_inst("RES 7, A"),       2, "\xcb\xbf");


    assert_inst(gb_assemble_inst("SET 0, B"),       2, "\xcb\xc0");
    assert_inst(gb_assemble_inst("SET 0, C"),       2, "\xcb\xc1");
    assert_inst(gb_assemble_inst("SET 0, D"),       2, "\xcb\xc2");
    assert_inst(gb_assemble_inst("SET 0, E"),       2, "\xcb\xc3");
    assert_inst(gb_assemble_inst("SET 0, H"),       2, "\xcb\xc4");
    assert_inst(gb_assemble_inst("SET 0, L"),       2, "\xcb\xc5");
    assert_inst(gb_assemble_inst("SET 0, (HL)"),    2, "\xcb\xc6");
    assert_inst(gb_assemble_inst("SET 0, A"),       2, "\xcb\xc7");

    assert_inst(gb_assemble_inst("SET 1, B"),       2, "\xcb\xc8");
    assert_inst(gb_assemble_inst("SET 1, C"),       2, "\xcb\xc9");
    assert_inst(gb_assemble_inst("SET 1, D"),       2, "\xcb\xca");
    assert_inst(gb_assemble_inst("SET 1, E"),       2, "\xcb\xcb");
    assert_inst(gb_assemble_inst("SET 1, H"),       2, "\xcb\xcc");
    assert_inst(gb_assemble_inst("SET 1, L"),       2, "\xcb\xcd");
    assert_inst(gb_assemble_inst("SET 1, (HL)"),    2, "\xcb\xce");
    assert_inst(gb_assemble_inst("SET 1, A"),       2, "\xcb\xcf");

    assert_inst(gb_assemble_inst("SET 2, B"),       2, "\xcb\xd0");
    assert_inst(gb_assemble_inst("SET 2, C"),       2, "\xcb\xd1");
    assert_inst(gb_assemble_inst("SET 2, D"),       2, "\xcb\xd2");
    assert_inst(gb_assemble_inst("SET 2, E"),       2, "\xcb\xd3");
    assert_inst(gb_assemble_inst("SET 2, H"),       2, "\xcb\xd4");
    assert_inst(gb_assemble_inst("SET 2, L"),       2, "\xcb\xd5");
    assert_inst(gb_assemble_inst("SET 2, (HL)"),    2, "\xcb\xd6");
    assert_inst(gb_assemble_inst("SET 2, A"),       2, "\xcb\xd7");

    assert_inst(gb_assemble_inst("SET 3, B"),       2, "\xcb\xd8");
    assert_inst(gb_assemble_inst("SET 3, C"),       2, "\xcb\xd9");
    assert_inst(gb_assemble_inst("SET 3, D"),       2, "\xcb\xda");
    assert_inst(gb_assemble_inst("SET 3, E"),       2, "\xcb\xdb");
    assert_inst(gb_assemble_inst("SET 3, H"),       2, "\xcb\xdc");
    assert_inst(gb_assemble_inst("SET 3, L"),       2, "\xcb\xdd");
    assert_inst(gb_assemble_inst("SET 3, (HL)"),    2, "\xcb\xde");
    assert_inst(gb_assemble_inst("SET 3, A"),       2, "\xcb\xdf");

    assert_inst(gb_assemble_inst("SET 4, B"),       2, "\xcb\xe0");
    assert_inst(gb_assemble_inst("SET 4, C"),       2, "\xcb\xe1");
    assert_inst(gb_assemble_inst("SET 4, D"),       2, "\xcb\xe2");
    assert_inst(gb_assemble_inst("SET 4, E"),       2, "\xcb\xe3");
    assert_inst(gb_assemble_inst("SET 4, H"),       2, "\xcb\xe4");
    assert_inst(gb_assemble_inst("SET 4, L"),       2, "\xcb\xe5");
    assert_inst(gb_assemble_inst("SET 4, (HL)"),    2, "\xcb\xe6");
    assert_inst(gb_assemble_inst("SET 4, A"),       2, "\xcb\xe7");

    assert_inst(gb_assemble_inst("SET 5, B"),       2, "\xcb\xe8");
    assert_inst(gb_assemble_inst("SET 5, C"),       2, "\xcb\xe9");
    assert_inst(gb_assemble_inst("SET 5, D"),       2, "\xcb\xea");
    assert_inst(gb_assemble_inst("SET 5, E"),       2, "\xcb\xeb");
    assert_inst(gb_assemble_inst("SET 5, H"),       2, "\xcb\xec");
    assert_inst(gb_assemble_inst("SET 5, L"),       2, "\xcb\xed");
    assert_inst(gb_assemble_inst("SET 5, (HL)"),    2, "\xcb\xee");
    assert_inst(gb_assemble_inst("SET 5, A"),       2, "\xcb\xef");

    assert_inst(gb_assemble_inst("SET 6, B"),       2, "\xcb\xf0");
    assert_inst(gb_assemble_inst("SET 6, C"),       2, "\xcb\xf1");
    assert_inst(gb_assemble_inst("SET 6, D"),       2, "\xcb\xf2");
    assert_inst(gb_assemble_inst("SET 6, E"),       2, "\xcb\xf3");
    assert_inst(gb_assemble_inst("SET 6, H"),       2, "\xcb\xf4");
    assert_inst(gb_assemble_inst("SET 6, L"),       2, "\xcb\xf5");
    assert_inst(gb_assemble_inst("SET 6, (HL)"),    2, "\xcb\xf6");
    assert_inst(gb_assemble_inst("SET 6, A"),       2, "\xcb\xf7");

    assert_inst(gb_assemble_inst("SET 7, B"),       2, "\xcb\xf8");
    assert_inst(gb_assemble_inst("SET 7, C"),       2, "\xcb\xf9");
    assert_inst(gb_assemble_inst("SET 7, D"),       2, "\xcb\xfa");
    assert_inst(gb_assemble_inst("SET 7, E"),       2, "\xcb\xfb");
    assert_inst(gb_assemble_inst("SET 7, H"),       2, "\xcb\xfc");
    assert_inst(gb_assemble_inst("SET 7, L"),       2, "\xcb\xfd");
    assert_inst(gb_assemble_inst("SET 7, (HL)"),    2, "\xcb\xfe");
    assert_inst(gb_assemble_inst("SET 7, A"),       2, "\xcb\xff");

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

    test_clock_step();

    test_disassemble();
    test_assemble_inst();
    test_assemble_inst_to_buf();

    return 0;
}
