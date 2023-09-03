#!/bin/sh

set -xe

CFLAGS="-Wall -Wextra -Werror -pedantic -ggdb"

cc $CFLAGS -o gb gb.c
