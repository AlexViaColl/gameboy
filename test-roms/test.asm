include "hardware.inc"

section "main", ROM0[$100]
    jp start
    ds $150 - @, 0 ; Make room for the header

start:
    call wait_vbl
    call enable_lcd

    call infinite_loop

enable_lcd:
    push af
    ld a,LCDCF_ON|LCDCF_BG8000|LCDCF_BGON  ;$91
    ld [rLCDC],a
    pop af
    ret

disable_lcd:
    push af
    ld a,LCDCF_OFF|LCDCF_BGOFF  ;$00
    ld [rLCDC],a
    pop af
    ret

wait_vbl:
    push af
.loop
    ld a,[rLY]
    cp SCRN_Y ; 144
    jp c, .loop
    pop af
    ret

infinite_loop:
    jp infinite_loop
