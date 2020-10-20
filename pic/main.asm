#include <xc.inc>

; CONFIG1
    config FOSC=INTOSC  ; RA7 has I/O. High-freq intern osc (HFINTOSC) used.
    
#if IsDebug==true
    #warning Programming for debug mode
    config WDTE=OFF     ; Mandatory for debug: watchdog disabled
    config PWRTE=OFF    ; Power-up timer disabled
#else
    config WDTE=ON      ; Watchdog timer enabled even in sleep
    config PWRTE=ON     ; Power-up timer enabled
#endif
    
    config MCLRE=ON     ; Memory clear enabled, weak pull-up enabled
    config CP=OFF       ; Code protection off
    config BOREN=ON     ; Brown-out reset enabled
    config CLKOUTEN=OFF ; RA6 has I/O, no clock out
    config IESO=OFF     ; No internal-external clock switchover
    config FCMEN=OFF    ; No fail-safe clock monitoring
    
; CONFIG2
    config WRT=ALL      ; All flash memory self-writes disabled
    config PPS1WAY=ON   ; Peripheral pin select only unlocked once
    config ZCD=OFF      ; Zero-crossing detect disabled by default
    config PLLEN=ON     ; Enable 4x phase-locked loop for internal osc
    config STVREN=ON    ; Stack over/underflow reset enabled
    config BORV=HI      ; Brown-out threshold is 2.7V typ
    config LPBOR=ON     ; Low-power brown-out reset enabled
    ; leave DEBUG up to the programmer
    config LVP=ON       ; Low-voltage programming disabled
    
code_psect macro name
    psect psect_&name, class=NEARCODE, space=SPACE_CODE, delta=2
    name:
endm

code_psect por_vec
    goto init
 
code_psect isr_vec
    retfie
    
code_psect init
    ; Leave WDT at default 2s
    
    ; Oscillator config
    ; IRCF  INTOSC  PRIMUX  PLLMUX  SCS    FOSC
    ; 0111  500kHz       1       0   00  500kHz
    banksel OSCCON
    bcf OSCCON, OSCCON_IRCF0_POSN
    ; 0110  250kHz       1       0   00  250kHz
    bsf OSCCON, OSCCON_IRCF3_POSN
    ; 1110  8MHz         1       1   00   32MHz
    ; At this point, everything should become "ready": MFIOFR, PLLR, HFIOFR;
    ; HFINTOSC PLL should lock within 2% (HFIOFL); 
    ; and it should stabilise within 0.5% (HFIOFS).
    
main:
    goto main

    end por_vec