#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -Werror -pedantic -ggdb `pkg-config --cflags sdl2` -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=unused-parameter"
#CFLAGS="-ggdb `pkg-config --cflags sdl2`"
LIBS="`pkg-config --libs sdl2`"

cc $CFLAGS -O3 -o gb $LIBS gb_sdl.c gb.c
cc $CFLAGS -o test gb_test.c gb.c
