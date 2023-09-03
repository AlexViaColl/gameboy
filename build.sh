#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -Werror -pedantic -ggdb `pkg-config --cflags sdl2`"
LIBS="`pkg-config --libs sdl2`"

cc $CFLAGS -o gb $LIBS gb.c
