; Tests DI, EI, and HALT (STOP proved untestable)

.include "shell.inc"

main:
     wreg IE,$04
     
     set_test 2,"EI"
     ei                     ; IME = 1
     ld   bc,0              ; bc  = 0
     push bc                ;
     pop  bc                ; 
     inc  b                 ; b = 1
     wreg IF,$04            ; IF = %00000100    => Set IF.2 (simulate timer triggered)
interrupt_addr:
     dec  b                 ; b -= 1
     jp   nz,test_failed    ; if b != 0 then test_failed
     ld   hl,sp-2           ; hl = sp-2
     ldi  a,(hl)            ; a = (*hl++)
     cp   <interrupt_addr   ;
     jp   nz,test_failed    ;
     ld   a,(hl)            ; a = (*hl)
     cp   >interrupt_addr
     jp   nz,test_failed
     lda  IF                ; a = IF
     and  $04               ; if IF.2 == 1 then test_failed
     jp   nz,test_failed
     
     set_test 3,"DI"
     di                     ; IME = 0
     ld   bc,0              ; bc  = 0
     push bc
     pop  bc
     wreg IF,$04            ; IF  = %00000100   => Set IF.2 (timer)
     ld   hl,sp-2           ; hl  = sp-2
     ldi  a,(hl)            ; a   = (*hl++)
     or   (hl)              ; a   = a | (*hl)
     jp   nz,test_failed    ; if a != 0 then test_failed
     lda  IF                ; a   = IF
     and  $04               ;
     jp   z,test_failed     ; if IF.2 == 0 then test_failed
     
     set_test 4,"Timer doesn't work"
     wreg TAC,$05           ; TAC  = %00000101  => Enable at 262144 Hz
     wreg TIMA,0            ; TIMA = 0          => Reset counter
     wreg IF,0              ; IF   = 0          => Disable interrupt flags
     delay 500              ; Wait 500 cycles ?
     lda  IF                ; Check interrupts
     delay 500              ; Wait 500 cycles ?
     and  $04               ; IF.2 should be 0 (not set)
     jp   nz,test_failed
     delay 500              ; Wait 500 cycles ?
     lda  IF                ; Check interrupts
     and  $04               ; IF.2 should be 1 (set)
     jp   z,test_failed
     pop  af
     
     set_test 5,"HALT"
     wreg TAC,$05
     wreg TIMA,0
     wreg IF,0
     halt      ; timer interrupt will exit halt
     nop       ; avoids DMG bug
     lda  IF
     and  $04
     jp   z,test_failed
     
     jp   tests_passed

.bank 0 slot 0
.org $50
     inc a
     ret
