SECTION "main", ROM0[$100]
    jp start
    ds $150 - @, 0

start:
    ld a, 0
    ld [$ff80], a

    stop
    ; stop mode finishes when receiving a LOW signal to terminal P10, P11, P12, or P1
    ; i.e. a button is pressed

    ld a, $66
    ld [$ff80], a

done:
    jp done
