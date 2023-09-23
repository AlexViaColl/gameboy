include "hardware.inc"

section "main", ROM0[$100]
    jp start
    ds $150 - @, 0 ; Make room for the header

start:
    call wait_vbl
    call disable_lcd
    call clear_screen
    call enable_lcd

    call infinite_loop

clear_screen:
    ld hl,_VRAM8000 ; $8000
    ld a,0
    ld bc,$10
    call memset

    ld hl,_SCRN0 ; $9800
    ld a,0
    ld bc,1024   ; $400
    call memset  ; clear $9800-9BFF
    ret

; @hl: dst   (preserved)
; @a:  value
; @bc: size  (preserved)
memset:
    push hl
    push bc
    ld d,a
.loop
    ld a,d
    ld [hli],a
    dec bc
    ld a,b
    or a,c
    jp nz,.loop
    pop bc
    pop hl
    ret

; @hl: dst
; @de: src
; @bc: size
memcpy:
    ld a,[de]
    ld [hli],a
    inc de
    dec bc
    ld a,b
    or a,c
    jp nz,memcpy
    ret

enable_lcd:
    push af
    ld a,LCDCF_ON|LCDCF_BG8000|LCDCF_BGON  ;$91 BG&Win tile in $8000-$8FFF
    ;ld a,LCDCF_ON|LCDCF_BG8800|LCDCF_BGON   ;$81 BG&Win tile in $8800-$97FF
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
