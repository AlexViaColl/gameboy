#include <stdio.h>

#include "gb.h"

int main(int argc, char **argv)
{
    GameBoy gb = {0};
    gb_init_with_args(&gb, argc, argv);

    while (gb.running) {
        gb_update(&gb);
    }

    return 0;
}
