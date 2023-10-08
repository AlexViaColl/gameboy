INCLUDE "hardware.inc"

SECTION "timer interrupt", ROM0[$50]
timer_handler:
    call timer_handler_full
    reti

SECTION "main", ROM0[$100]
    jp start
    ds $150 - @, 0

start:
    ld a, 0
    ld [$ff80], a

    ei
    ld a, IEF_TIMER
    ld [rIE], a

    ; Enable timer at 4096 Hz, timer counter (TIMA) increases by one every ~0.244ms
    ; 256 * ~0.244ms = ~62.6ms (1 frame is ~16.74ms)
    ld a, %00000100 | %00000000 ; Enable timer at   4096 Hz
    ;ld a, %00000100 | %00000001 ; Enable timer at 262144 Hz
    ;ld a, %00000100 | %00000010 ; Enable timer at  65536 Hz
    ;ld a, %00000100 | %00000011 ; Enable timer at  16384 Hz
    ld [rTAC], a

    ; Set timer modulo to interrupt every X increments
    ; TIMA = 00  => every 256 increments
    ; TIMA = fe  => every   2 increments
    ; TIMA = ff  => every   1 increments
    ld a, $00
    ld [rTMA], a

    halt
    ; halt mode finishes when the interrupt-enable flag and its corresponding
    ; interrupt request flag are set

    ld a, $66
    ld [$ff80], a

done:
    jp done

timer_handler_full:
    ;ld a, 0
    ;ld [rTAC], a ; Disable the timer
    ld a, [rSCX]
    inc a
    ld [rSCX], a

    ret
