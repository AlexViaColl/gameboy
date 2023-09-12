#include <stdio.h>

#include "gb.h"

#define GB_TEST(name, setup, checks) void test_##name (void) { \
    printf("%s\n", __func__);       \
    Inst inst = {0};                \
    uint16_t start_pc = 0x0032;     \
    GameBoy gb = {0};               \
    setup                           \
    gb.PC = start_pc;               \
    gb_exec(&gb, inst);             \
    assert(gb.PC == start_pc + inst.size);  \
    checks                          \
    printf("End of test\n");        \
}

GB_TEST(NOP, {
    // setup
    inst.data = (uint8_t*)"\x00";
    inst.size = 1;
}, {
    // validations
})

GB_TEST(LD_BC_16, {
    // setup
    inst.data = (uint8_t*)"\x01\x12\x34";
    inst.size = 3;
}, {
    // validations
    assert(gb.BC == 0x3412);
})

void test_inst_nop(void)
{
    printf("%s\n", __func__);
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x00", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
}

void test_inst_ld_bc_16(void)
{
    printf("%s\n", __func__);
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x01\x12\x34", .size = 3};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 3);
    assert(gb.BC == 0x3412);
}

void test_inst_ld_bc_mem_a(void)
{
    printf("%s\n", __func__);
    uint16_t start_pc = 0x0032;
    uint16_t bc = 0x1234;
    uint8_t a = 0x99;
    GameBoy gb = {0};
    gb_set_reg16(&gb, REG_BC, bc);
    gb_set_reg(&gb, REG_A, a);
    Inst inst = {.data = (uint8_t*)"\x02", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb.memory[bc] == a);
}


void test_inst_inc_bc(void)
{
    printf("%s\n", __func__);
    uint16_t start_pc = 0x0032;
    uint16_t bc = 0x1234;
    GameBoy gb = {0};
    gb_set_reg16(&gb, REG_BC, bc);
    Inst inst = {.data = (uint8_t*)"\x03", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb.BC == bc + 1);
}

void test_inst_inc_b(void)
{
    printf("%s\n", __func__);
    uint16_t start_pc = 0x0032;
    uint8_t b = 0xFF;
    GameBoy gb = {0};
    gb_set_reg(&gb, REG_B, b);
    gb_set_flag(&gb, Flag_N, 1);
    Inst inst = {.data = (uint8_t*)"\x04", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg(&gb, REG_B) == (uint8_t)(b + 1));
    // assert_flags(gb, "Z0H-");
    assert(gb_get_flag(&gb, Flag_Z) == 1);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 1);
}

void test_inst_dec_b(void)
{
    printf("%s\n", __func__);
    uint16_t start_pc = 0x0032;
    uint8_t b = 0xFF;
    GameBoy gb = {0};
    gb_set_reg(&gb, REG_B, b);
    Inst inst = {.data = (uint8_t*)"\x05", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg(&gb, REG_B) == (uint8_t)(b - 1));
    // assert_flags(gb, "Z1H-");
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 1);
    assert(gb_get_flag(&gb, Flag_H) == 1);
}

void test_inst_ld_b_8(void)
{
    printf("%s\n", __func__);
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x06\x78", .size = 2};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 2);
    assert(gb_get_reg(&gb, REG_B) == 0x78);
}

void test_inst_rlca(void)
{
    printf("%s\n", __func__);
    uint16_t start_pc = 0x0032;
    uint8_t a = 0x81;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x07", .size = 1};
    gb.PC = start_pc;
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg(&gb, REG_A) == 0x03);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    assert(gb_get_flag(&gb, Flag_Z) == 0);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0);
}

void test_inst_ld_mem16_sp(void)
{
    printf("%s\n", __func__);
    uint16_t start_pc = 0x0032;
    uint16_t sp = 0x8754;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x08\x34\x12", .size = 3};
    gb.PC = start_pc;
    gb_set_reg16(&gb, REG_SP, sp);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 3);
    assert(gb.memory[0x1234] == (sp & 0xff));
    assert(gb.memory[0x1235] == (sp >> 8));
}

void test_inst_add_hl_bc(void)
{
    printf("%s\n", __func__);
    uint16_t start_pc = 0x0032;
    uint16_t hl = 0x9A34;
    uint16_t bc = 0xDE78;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x09", .size = 1};
    gb.PC = start_pc;
    gb_set_reg16(&gb, REG_HL, hl);
    gb_set_reg16(&gb, REG_BC, bc);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg16(&gb, REG_HL) == (uint16_t)(hl + bc));
    // 0 H C
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 0); // Sould this be set??
    assert(gb_get_flag(&gb, Flag_C) == 1);
}

int main(void)
{
    //test_NOP();
    //test_LD_BC_16();

    test_inst_nop();
    test_inst_ld_bc_16();
    test_inst_ld_bc_mem_a();
    test_inst_inc_bc();
    test_inst_inc_b();
    test_inst_dec_b();
    test_inst_ld_b_8();
    test_inst_rlca();
    test_inst_ld_mem16_sp();
    test_inst_add_hl_bc();
    return 0;
}
