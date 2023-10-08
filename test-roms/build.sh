#!/bin/bash

set -e

build_rom()
{
    rgbasm -L -H -o "$1.o" "$1.asm"
    rgblink -o "$1.gb" "$1.o"
    rgbfix -v -p 0xFF "$1.gb"
    rm "$1.o"
}

build_rom "stop"
build_rom "halt"

build_rom "test"

build_rom "hello"
build_rom "tile"
build_rom "unbricked"
build_rom "window"
build_rom "rotations"
build_rom "interrupts"
