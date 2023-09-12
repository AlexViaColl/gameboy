#include "gb.h"

void test_inst_nop(void)
{
    GameBoy gb = {0};
    Inst inst = {.data = (uint8_t*)"\x00", .size = 1};
    gb_exec(&gb, inst);
}

int main(void)
{
    return 0;
}
