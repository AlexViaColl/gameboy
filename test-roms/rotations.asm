INCLUDE "hardware.inc"

SECTION "header", ROM0[$100]
	jp start
	ds $150 - @, 0 ; Make room for the header

start:
	ld a, 0
    rlca
    rrca
    rla
    rra

loop:
    jp loop
