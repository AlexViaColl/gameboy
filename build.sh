#!/bin/bash

set -xe

CFLAGS="-Wall -Wextra -Werror -pedantic -ggdb `pkg-config --cflags sdl2` -Wno-unused-function -Wno-error=unused-variable -Wno-error=unused-parameter"
#CFLAGS="-ggdb `pkg-config --cflags sdl2`"
LIBS="`pkg-config --libs sdl2`"

clang $CFLAGS -O3 -o gb $LIBS gb_sdl.c gb.c
clang $CFLAGS -o test gb_test.c gb.c

if ! command -v rgbasm &> /dev/null
then
    echo "rgbasm is not installed"
    exit 0
fi

cd test-roms && ./build.sh
