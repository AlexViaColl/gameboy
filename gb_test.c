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
    }                                                           \
} while (0)

//////////////////////////////////////////////////////////////////////

void test_inst_nop(void)
{
    test_begin
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x00", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    test_end
}

void test_inst_stop(void)
{
    test_begin
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x10", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    test_end
}

void test_inst_ld_reg16_n(Reg16 reg)
{
    test_begin
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    uint8_t data[] = {0x01, 0x12, 0x34};
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
    //printf("(%-50s)", gb_reg16_to_str(reg));
    uint16_t start_pc = 0x0032;
    uint16_t addr = 0x1234;
    uint8_t a = 0x99;
    GameBoy gb = {0};
    gb_set_reg16(&gb, reg, addr);
    gb_set_reg(&gb, REG_A, a);
    uint8_t data[] = {0x02};
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
    //printf("(%-50s)", gb_reg16_to_str(reg));
    uint16_t start_pc = 0x0032;
    uint16_t addr = 0x1234;
    uint8_t a = 0x33;
    GameBoy gb = {0};
    gb_set_reg16(&gb, reg, addr);
    gb.memory[addr] = a;

    uint8_t data[] = {0x0A};
    data[0] |= (reg << 4);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg(&gb, REG_A) == a);
    test_end
}

void test_inst_ldi_hl_mem_a(void)
{
    test_begin
    uint16_t start_pc = 0x0032;
    uint16_t value = 0x1234;
    uint8_t a = 0x99;
    GameBoy gb = {0};
    gb.HL = value;
    gb_set_reg(&gb, REG_A, a);
    uint8_t data[] = {0x22};
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
    uint16_t start_pc = 0x0032;
    uint16_t value = 0x1234;
    uint8_t a = 0x99;
    GameBoy gb = {0};
    gb.HL = value;
    gb.memory[gb.HL] = a;
    uint8_t data[] = {0x2A};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg(&gb, REG_A) == gb.memory[value]);
    assert(gb.HL == value + 1);
    test_end
}

void test_inst_ldd_hl_mem_a(void)
{
    test_begin
    uint16_t start_pc = 0x0032;
    uint16_t value = 0x1234;
    uint8_t a = 0x99;
    GameBoy gb = {0};
    gb.HL = value;
    gb_set_reg(&gb, REG_A, a);
    uint8_t data[] = {0x32};
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
    uint16_t start_pc = 0x0032;
    uint16_t value = 0x1234;
    uint8_t a = 0x99;
    GameBoy gb = {0};
    gb.HL = value;
    gb.memory[gb.HL] = a;
    uint8_t data[] = {0x3A};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg(&gb, REG_A) == gb.memory[value]);
    assert(gb.HL == value - 1);
    test_end
}

void test_inst_inc_reg16(Reg16 reg)
{
    test_begin
    //printf("(%-50s)", gb_reg16_to_str(reg));
    uint16_t start_pc = 0x0032;
    uint16_t value = 0x1234;
    GameBoy gb = {0};
    gb_set_reg16(&gb, reg, value);
    uint8_t data[] = {0x03};
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
    //printf("(%-50s)", gb_reg_to_str(reg));
    uint16_t start_pc = 0x0032;
    uint8_t value = 0xFF;
    GameBoy gb = {0};
    gb_set_reg(&gb, reg, value);
    gb_set_flag(&gb, Flag_N, 1);
    uint8_t data[] = {0x04};
    data[0] |= (reg << 3);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg(&gb, reg) == (uint8_t)(value + 1));
    // assert_flags(gb, "Z0H-");
    assert(gb_get_flag(&gb, Flag_Z) == 1);
    assert(gb_get_flag(&gb, Flag_N) == 0);
    assert(gb_get_flag(&gb, Flag_H) == 1);
    test_end
}

void test_inst_dec_reg8(Reg8 reg)
{
    test_begin
    //printf("(%-50s)", gb_reg_to_str(reg));
    uint16_t start_pc = 0x0032;
    uint8_t value = 0xFF;
    GameBoy gb = {0};
    gb_set_reg(&gb, reg, value); // REG = $FF
    uint8_t data[] = {0x05};
    data[0] |= (reg << 3);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg(&gb, reg) == (uint8_t)(value - 1));
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
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    uint8_t data[] = {0x06, 0x78};
    data[0] |= (reg << 3);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 2);
    assert(gb_get_reg(&gb, reg) == 0x78);
    test_end
}

void test_inst_rot(GameBoy *gb, uint8_t opcode, uint8_t value)
{
    uint16_t start_pc = 0x0032;
    uint8_t data[] = {opcode};
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
    assert(gb_get_reg(&gb, REG_A) == 0x03);
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
    assert(gb_get_reg(&gb, REG_A) == 0xC0);
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
    assert(gb_get_reg(&gb, REG_A) == 0x02);
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
    assert(gb_get_reg(&gb, REG_A) == 0x40);
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
    uint16_t start_pc = 0x0032;
    uint8_t value = 0xFE;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x2F", .size = 1};
    gb.PC = start_pc;
    gb_set_reg(&gb, REG_A, value);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg(&gb, REG_A) == (uint8_t)~value);
    assert(gb_get_flag(&gb, Flag_N) == 1);
    assert(gb_get_flag(&gb, Flag_H) == 1);
    test_end
}

void test_inst_scf(void)
{
    test_begin
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x37", .size = 1};
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
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x3F", .size = 1};

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
    test_end
}

void test_inst_jr_n(void)
{
    test_begin
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    uint8_t n = 0x05;
    uint8_t data[] = {0x18, n};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + inst.size + 5);
    test_end
}

void test_inst_jr_nz_n_taken(void)
{
    test_begin
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    uint8_t n = 0xFE;
    uint8_t data[] = {0x20, n};
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
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    uint8_t n = 0xFE;
    uint8_t data[] = {0x20, n};
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
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    uint8_t n = 0xFE;
    uint8_t data[] = {0x28, n};
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
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    uint8_t n = 0xFE;
    uint8_t data[] = {0x28, n};
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
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    uint8_t n = 0xFE;
    uint8_t data[] = {0x30, n};
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
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    uint8_t n = 0xFE;
    uint8_t data[] = {0x30, n};
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
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    uint8_t n = 0xFE;
    uint8_t data[] = {0x38, n};
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
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    uint8_t n = 0xFE;
    uint8_t data[] = {0x38, n};
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
    uint16_t start_pc = 0x0032;
    uint16_t hl = 0x9A34;
    uint16_t value = 0xDE78;
    //  0x9A34
    // +0xDE78
    // -------
    //  0x78AC
    GameBoy gb = {0};
    uint8_t data[] = {0x09};
    data[0] |= (reg << 4);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg16(&gb, reg, value);
    gb_set_reg16(&gb, REG_HL, hl);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_HL) {
        assert(gb_get_reg16(&gb, REG_HL) == (uint16_t)(hl + hl));
    } else {
        assert(gb_get_reg16(&gb, REG_HL) == (uint16_t)(hl + value));
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
    uint16_t start_pc = 0x0032;
    uint16_t value = 0xDE78;
    GameBoy gb = {0};
    uint8_t data[] = {0x0B};
    data[0] |= (reg << 4);
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg16(&gb, reg, value);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg16(&gb, reg) == (uint16_t)(value - 1));
    test_end
}

void test_inst_ld_reg8_reg8(Reg8 dst, Reg8 src)
{
    if (dst == REG_HL_MEM && src == REG_HL_MEM) {
        return;
    }
    test_begin
    //printf("(%s <- %s)", gb_reg_to_str(dst), gb_reg_to_str(src));
    uint16_t start_pc = 0x0032;
    uint8_t value = 0x69;
    GameBoy gb = {0};
    uint8_t data[] = {0x40};
    data[0] |= (dst << 3) | src;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, src, value);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    assert(gb_get_reg(&gb, dst) == value);
    test_end
}

void test_inst_halt(void)
{
    test_begin
    uint16_t start_pc = 0x0032;
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x76", .size = 1};
    gb.PC = start_pc;

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    test_end
}

void test_inst_add_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    uint16_t start_pc = 0x0032;
    uint8_t a = 0x19;
    uint8_t value = 0xC3;
    GameBoy gb = {0};
    uint8_t data[] = {0x80};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg(&gb, REG_A) == (uint8_t)(a + a));
    } else {
        assert(gb_get_reg(&gb, REG_A) == (uint8_t)(a + value));
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
    Inst inst = {.data = (uint8_t*)"\x80", .size = 1};

    gb_exec(&gb, inst);

    assert(gb_get_flag(&gb, Flag_Z) == 1);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    test_end
}

void test_inst_add_reg8_half_carry_flag(void)
{
    test_begin
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x80", .size = 1};

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
    uint8_t c = 1;
    uint8_t a = 0x19;
    uint8_t value = 0xC3;
    GameBoy gb = {0};
    uint8_t data[] = {0x88};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);
    gb_set_flag(&gb, Flag_C, c);

    gb_exec(&gb, inst);

    if (reg == REG_A) {
        assert(gb_get_reg(&gb, REG_A) == (uint8_t)(a + a + c));
    } else {
        assert(gb_get_reg(&gb, REG_A) == (uint8_t)(a + value + c));
    }
    assert(gb_get_flag(&gb, Flag_N) == 0);
    test_end
}

void test_inst_sub_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    uint16_t start_pc = 0x0032;
    uint8_t a = 0xDD;
    uint8_t value = 0xC3;
    GameBoy gb = {0};
    uint8_t data[] = {0x90};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg(&gb, REG_A) == 0);
    } else {
        assert(gb_get_reg(&gb, REG_A) == (uint8_t)(a - value));
    }
    assert(gb_get_flag(&gb, Flag_N) == 1);
    test_end
}

void test_inst_sbc_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    uint16_t start_pc = 0x0032;
    uint8_t c = 1;
    uint8_t a = 0xDD;
    uint8_t value = 0xC3;
    GameBoy gb = {0};
    uint8_t data[] = {0x98};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);
    gb_set_flag(&gb, Flag_C, c);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg(&gb, REG_A) == (uint8_t)-1);
    } else {
        assert(gb_get_reg(&gb, REG_A) == (uint8_t)(a - value - c));
    }
    assert(gb_get_flag(&gb, Flag_N) == 1);
    test_end
}

void test_inst_and_reg8(Reg8 reg)
{
    test_begin
    //printf("(%s)", gb_reg_to_str(reg));
    uint16_t start_pc = 0x0032;
    uint8_t a = 0xDD;
    uint8_t value = 0xC3;
    GameBoy gb = {0};
    uint8_t data[] = {0xA0};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg(&gb, REG_A) == a);
    } else {
        assert(gb_get_reg(&gb, REG_A) == (uint8_t)(a & value));
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
    uint16_t start_pc = 0x0032;
    uint8_t a = 0xDD;
    uint8_t value = 0xC3;
    GameBoy gb = {0};
    uint8_t data[] = {0xA8};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg(&gb, REG_A) == 0);
    } else {
        assert(gb_get_reg(&gb, REG_A) == (uint8_t)(a ^ value));
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
    uint16_t start_pc = 0x0032;
    uint8_t a = 0xDD;
    uint8_t value = 0xC3;
    GameBoy gb = {0};
    uint8_t data[] = {0xB0};
    data[0] |= reg;
    Inst inst = {.data = data, .size = sizeof(data)};
    gb.PC = start_pc;
    gb_set_reg(&gb, reg, value);
    gb_set_reg(&gb, REG_A, a);

    gb_exec(&gb, inst);

    assert(gb.PC == start_pc + 1);
    if (reg == REG_A) {
        assert(gb_get_reg(&gb, REG_A) == a);
    } else {
        assert(gb_get_reg(&gb, REG_A) == (uint8_t)(a | value));
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
    uint8_t a = 0xDD;
    uint8_t value = 0xC3;
    GameBoy gb = {0};
    uint8_t data[] = {0xB8};
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
    uint8_t data[] = {0xE9};
    Inst inst = {.data = data, .size = sizeof(data)};
    uint16_t addr = 0x1234;
    gb.HL = addr;

    gb_exec(&gb, inst);

    assert(gb.PC == addr);
    test_end
}

void test_inst_rrc(void)
{
    test_begin
    GameBoy gb = {0};
    uint8_t data[] = {0xCB, 0x08};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb_set_reg(&gb, REG_B, 0x01);

    gb_exec(&gb, inst);

    assert(gb_get_reg(&gb, REG_B) == 0x80);
    assert(gb_get_flag(&gb, Flag_C) == 1);
    test_end
}

void test_inst_sra(void)
{
    test_begin
    GameBoy gb = {0};
    uint8_t data[] = {0xCB, 0x28};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb_set_reg(&gb, REG_B, 0x80);

    gb_exec(&gb, inst);

    assert(gb_get_reg(&gb, REG_B) == 0xC0);
    test_end
}

void test_inst_add_a_hl_mem(void)
{
    test_begin
    GameBoy gb = {0};
    uint8_t data[] = {0x86};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb_set_reg(&gb, REG_A, 0xFF);
    gb.HL = 0x1000;
    gb_write_memory(&gb, gb.HL, 0x01);
    // A = FF
    // +   01
    // ------
    //   1 00 => Z, C and H are set

    gb_exec(&gb, inst);

    assert(gb_get_reg(&gb, REG_A) == 0x00);
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
        uint8_t data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00);

        gb_exec(&gb, inst);

        assert(gb_get_reg(&gb, REG_A) == 0x00);
        assert(gb_get_flag(&gb, Flag_Z) == 1);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        uint8_t data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x0F); // BCD: we can only represent 0-9

        gb_exec(&gb, inst);

        assert(gb_get_reg(&gb, REG_A) == 0x15);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        uint8_t data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0xF0); // BCD: we can only represent 0-9

        gb_exec(&gb, inst);

        assert(gb_get_reg(&gb, REG_A) == 0x50);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        uint8_t data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x10); // BCD: we can only represent 0-9

        gb_exec(&gb, inst);

        assert(gb_get_reg(&gb, REG_A) == 0x10);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        uint8_t data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 0, 0, 1);
        // A = 80
        //   + 80
        // ----------
        //   1 00 C=1
        gb_exec(&gb, inst);

        assert(gb_get_reg(&gb, REG_A) == 0x60);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        uint8_t data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0xF0); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 0, 0, 1);
        gb_exec(&gb, inst);

        assert(gb_get_reg(&gb, REG_A) == 0x50);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        uint8_t data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 1, 1, 1, 0); // F = ZNH- (After subtraction)
        gb_exec(&gb, inst);

        assert(gb_get_reg(&gb, REG_A) == 0xFA);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_N) == 1);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        uint8_t data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 1, 1, 1, 1); // F = ZNHC (After subtraction)
        gb_exec(&gb, inst);

        assert(gb_get_reg(&gb, REG_A) == 0x9A);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_N) == 1);
        assert(gb_get_flag(&gb, Flag_H) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        uint8_t data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x9A); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 0, 0, 0); // F = ---- (After addition)
        gb_exec(&gb, inst);

        assert(gb_get_reg(&gb, REG_A) == 0x00);
        assert(gb_get_flag(&gb, Flag_Z) == 1);
        assert(gb_get_flag(&gb, Flag_N) == 0);
        assert(gb_get_flag(&gb, Flag_H) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 1);
    }

    {
        GameBoy gb = {0};
        uint8_t data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 0, 1, 0); // F = ---- (After addition)
        gb_exec(&gb, inst);

        assert(gb_get_reg(&gb, REG_A) == 0x06);
        assert(gb_get_flag(&gb, Flag_Z) == 0);
        assert(gb_get_flag(&gb, Flag_N) == 0);
        assert(gb_get_flag(&gb, Flag_H) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

    {
        GameBoy gb = {0};
        uint8_t data[] = {0x27};
        Inst inst = {.data = data, .size = sizeof(data)};
        gb_set_reg(&gb, REG_A, 0x00); // BCD: we can only represent 0-9
        gb_set_flags(&gb, 0, 1, 0, 0); // F = -N-- (After subtraction)
        gb_exec(&gb, inst);

        assert(gb_get_reg(&gb, REG_A) == 0x00);
        assert(gb_get_flag(&gb, Flag_Z) == 1);
        assert(gb_get_flag(&gb, Flag_N) == 1);
        assert(gb_get_flag(&gb, Flag_H) == 0);
        assert(gb_get_flag(&gb, Flag_C) == 0);
    }

#if 0
    uint16_t start_pc = 0x0032;
    uint8_t value = 0x33;
    GameBoy gb = {0};
    uint8_t data[] = {0x27};
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
    uint8_t data[] = {0xFE, 0x90};
    Inst inst = {.data = data, .size = sizeof(data)};
    gb_set_reg(&gb, REG_A, 0x90);

    gb_exec(&gb, inst);

    assert(gb_get_reg(&gb, REG_A) == 0x90);
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

    gb_tick(&gb, 0);
    assert(gb.PC == 0);

    gb_tick(&gb, MCYCLE_MS);
    assert(gb.PC == 1);
    test_end
}

void test_time_inc_reg16(void)
{
    test_begin
    GameBoy gb = {0};
    gb.memory[0] = 0x03;

    gb_tick(&gb, MCYCLE_MS);
    assert(gb.PC == 0);
    gb_tick(&gb, MCYCLE_MS);
    assert(gb.PC == 1);

    test_end
}

#if 0
#include <time.h>
#include <sys/time.h>
void foo(void)
{
    double freq = 4194304.0;
    {
    clock_t t1, t2;
    t1 = t2 = clock();
    while (t1 == t2) {
        t2 = clock();
    }
    printf("%lf ms\n", (double)(t2 - t1) / CLOCKS_PER_SEC * 1000);
    }

    {
    struct timeval t1, t2;
    gettimeofday(&t1, 0);
    t2 = t1;
    while (t1.tv_sec == t2.tv_sec && t1.tv_usec == t2.tv_usec) {
        gettimeofday(&t2, 0);
    }
    printf("(t2-t1).tv_sec: %ld, (t2-t1).tv_usec: %ld\n", t2.tv_sec-t1.tv_sec, t2.tv_usec-t1.tv_usec);

    printf("t1 {.tv_sec: %ld, .tv_usec: %ld}\n", t1.tv_sec, t1.tv_usec);
    printf("t2 {.tv_sec: %ld, .tv_usec: %ld}\n", t2.tv_sec, t2.tv_usec);
    // tv_usec => microseconds
    // 1 s = 1000 ms = 1000 * 1000 us

    int cycles = 0;
    //uint64_t us_elapsed = 0;
    double us_per_cycle = 1000000.0 / 4194304.0;
    double us_cycle = 0.0;
    printf("Timing 1s\n");
    gettimeofday(&t1, 0);
    while (true) {
        gettimeofday(&t2, 0);
        uint64_t us = (t2.tv_sec - t1.tv_sec) * 1000 * 1000 + (t2.tv_usec - t1.tv_usec);
        t1 = t2;
        //us_elapsed += us;
        us_cycle += (double)us;
        while (us_cycle >= us_per_cycle) {
            us_cycle -= us_per_cycle;
            cycles += 1;
        }


        if (cycles >= 4194304) break;
    }
    printf("Done!\n");
    }
}
#endif

int main(void)
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

    test_time_nop();
    test_time_inc_reg16();

    return 0;
}
