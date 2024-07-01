#include <xc.inc>
#include "receive.inc"


; CONFIG1
#if build_for_debug==1
    config WDTE=OFF     ; Mandatory for debug: watchdog disabled
    config PWRTE=OFF    ; Power-up timer disabled
#else
    config WDTE=ON      ; Enable watchdog
    config PWRTE=ON     ; Power-up timer enabled
#endif
    config FOSC=INTOSC  ; RA7 has I/O. High-freq intern osc (HFINTOSC) used.
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
    config LVP=OFF      ; Low-voltage programming disabled
    
    
; If RAM is needed - which it currently isn't, outside of SFRs
; psect variables, class=COMMON, space=SPACE_DATA, delta=1, noexec
; some_var:
    ; ds 1
    
    
; A labelled program section (psect) that does not require ROM paging
#define psect_common class=NEARCODE, space=SPACE_CODE, delta=2
code_psect macro name,optims
#if build_for_debug==1
    psect psect_&name, psect_common
#else
    psect psect_&name, psect_common, optim=optims
#endif
    name:
endm

code_psect por_vec
    ; Oscillator config
    banksel OSCCON  ; 1
    ; IRCF  INTOSC  PRIMUX  PLLMUX  SCS    FOSC
    ; 0111  500kHz       1       0   00  500kHz
    bcf IRCF0
    ; 0110  250kHz       1       0   00  250kHz
    bsf IRCF3
    ; 1110    8MHz       1       1   00   32MHz
    ; At this point, everything should become "ready": MFIOFR, PLLR, HFIOFR;
    ; HFINTOSC PLL should lock within 2% (HFIOFL); 
    ; and it should stabilise within 0.5% (HFIOFS).
    goto init
 
code_psect isr_vec
    ; Unused; interrupts are used to wake up main
    retfie
    
code_psect init
    ; OSCCON and PIEx share bank 1 - set the latter two in sequence here
    ; TRIS could be done this way too but the order doesn't suit this right now
    
init_watchdog:
    banksel WDTCON  ; 1
    ; 8 second watchdog expiry
    movlw (0b01101 << WDTCON_WDTPS_POSN) | (0 << WDTCON_SWDTEN_POSN)
    movwf WDTCON
    
select_interrupts:
    banksel PIE1  ; 1
    bsf TMR2IE  ; Fade disable timer
    bsf SSP1IE  ; SPI receive
    
init_ports:
    ; RA1: ana out OPA1OUT (DAC1)
    ; RA4: ana out DAC4
    ; RB0: dig out COG1A
    ; RB1: ana out OPA2OUT (DAC2)
    ; RB6: dig in  ICSPCLK
    ; RB7: dig in  ICSPDAT
    ; RC1: SDI (MOSI)
    ; RC2: SDO (MISO)
    ; RC3: SCK
    ; RC6: ana out OPA3OUT (DAC5)
    ; RE3: dig in  MCLR
    ; Unused pins ana in weak pullup to 1.
    ; Leave slew rate limitation enabled.
    ; Enable WPUEN for unused inputs.
    
    ; Tristate default is input. Before setting any outputs, configure the other
    ; port properties.
    
    ; Unused or active-low lines set to 1, DAC and PGx set to 0
    banksel LATA  ; 2
    movlw 0b11101101
    movwf LATA
    movlw 0b00111101
    movwf LATB
    movlw 0b10111111
    movwf LATC
    
    ; The only analogue pins are for DAC/OPA, but we also disable the digital
    ; buffers for unused inputs by setting them to analogue. That only leaves
    ; FADE, PG and SPI as digital.
    banksel ANSELA  ; 3
    movlw 0b11111111
    movwf ANSELA
    movlw 0b00111110
    movwf ANSELB
    movlw 0b11110001
    movwf ANSELC
    
    ; Weak pullups on unused ports and MISO
    banksel WPUA  ; 4
    movlw 0b11101101
    movwf WPUA
    movlw 0b00111100
    movwf WPUB
    movlw 0b10110101
    movwf WPUC

    banksel ODCONB  ; 5
    ; Open drain for LED fade.
    bsf ODB0
    ; To do a simple level-shift from our 5.2V to the Rpi's 3.3V on MISO, we
    ; add an external pullup to its 3.3V pin and put MISO on RC2 in open-drain
    bsf ODC2

    ; All Schmitt trigger levels.
    ;  ST in: 1.04-4.16
    ;    out: 0.60-4.50
    banksel INLVLA  ; 7
    comf INLVLA  ; Default TTL; switch to ST
    comf INLVLB  ; Default TTL; switch to ST
    comf INLVLC  ; Default TTL; switch to ST

    ; Enable high drive mode (<100 mA sink)
    banksel HIDRVB  ; 8
    bsf HIDB0
    
    ; Needed for WPUA-C above to take effect
    banksel OPTION_REG  ; 1
    bcf nWPUEN
    
    ; Tristates
    banksel TRISA  ; 1
    movlw 0b11101101
    movwf TRISA
    movlw 0b11000000
    movwf TRISB
    movlw 0b00001010
    movwf TRISC
    
init_pps:
    ; RB0: COG1A
    ; RC1: SDI (MOSI)
    ; RC2: SDO (MISO)
    ; RC3: SCK
    banksel RC2PPS  ; 29
    movlw 0b100011  ; SDO
    movwf RC2PPS
    movlw 0b000101  ; COG1A
    movwf RB0PPS
    
    banksel PPSLOCK  ; 28
    movlw 0b010001  ; RC1
    movwf SSPDATPPS
    movlw 0b010011  ; RC3
    movwf SSPCLKPPS
    
    movlw 0x55
    movwf PPSLOCK
    movlw 0xAA
    movwf PPSLOCK
    bsf PPSLOCKED
    
init_fade_cog:
    ; Complementary waveform generator setup to output linear duty cycle growth
    ; based on beat frequency between PWM5 and PWM6. Output goes to a FET driver
    ; on the low side of all backlight LEDs.
    banksel COG1CON0  ; 13
    bsf G1RIS9   ; PWM5 for rise
    bsf G1FIS10  ; PWM6 for fall
    bsf G1STRA   ; Steering out on channel A
    bsf G1POLA   ; Active-low
    bsf G1ASDAC1 ; Logic out if shutdown
    bcf G1ASDAC0 ; Low out if shutdown
    bsf G1CS1    ; Clocked by HFINTOSC
    
    ; Blanking is possible: we only care about events after a minimum period
    ; of 0.9/200Hz=4.5ms. At 16MHz and 6 bits, blanking time maxes out at
    ; (2**6-1)/16MHz = 3.9us. Not even worth it.
    
    bsf G1EN     ; Enable
    
init_fade_pwm:
    ; Beat frequency is chosen between these two PWM modules for a linear DC
    ; from 0 to 100%
    banksel PWM5CON  ; 27
    
    ; HFINTOSC/2**1 = 8 MHz
    movlw (1 << PWM5CLKCON_PS_POSN) \
     | (0b01 << PWM5CLKCON_CS_POSN)
    movwf PWM5CLKCON
    movwf PWM6CLKCON
    
    ; set period = (PWM5PR + 1)*PS / 16MHz
    ; for 200Hz, PS=2, PR5 = 39,999 = 0x9C3F
    
    ; reset period = (PWM6PR + 1)*PS / 16MHz
    ; 5s = tset**2/(treset - tset)
    ; treset - tset = 5us
    ; freset ~ 199.8 Hz
    ; PS=2, PR6 = 40,039 = 0x9C67
    
    movlw 0x9C
    movwf PWM5PRH
    movwf PWM6PRH
    movlw 0x3F
    movwf PWM5PRL
    movlw 0x67
    movwf PWM6PRL
    
    ; 0 phase, 0 offset
    clrf PWM5PHL
    clrf PWM5PHH
    clrf PWM5OFL
    clrf PWM5OFH
    clrf PWM6PHL
    clrf PWM6PHH
    clrf PWM6OFL
    clrf PWM6OFH
    
    ; Minimal duty cycle for both
    movlw 1
    movwf PWM5DCL
    movwf PWM6DCL
    clrf PWM5DCH  
    clrf PWM6DCH
    
    ; Since the falling COG input block is active-low,
    ; PWM6 needs to have its polarity inverted
    bsf PWM6POL
    
    ; Arm-load PWM registers
    bsf PWM5LD
    bsf PWM6LD
    
    ; To enable 5 and 6 at the same time, use mirror registers
    movlw PWMEN_MPWM5EN_MASK \
        | PWMEN_MPWM6EN_MASK
    movwf PWMEN

init_fade_timer:
    banksel T2CON  ; 9
    
    ; We can't use one-shot mode because it doesn't respect the postscaler
    ; LFINTOSC=31kHz
    bsf T2CS2
    ; For 8 bits, 31kHz, 5s, prescale=2**7, postscale=5: tmr ~ 242
    movlw 242
    movwf T2PR
    
    ; On, prescale=2**7, postscale=5
    movlw T2CON_ON_MASK \
        | (7 << T2CON_CKPS_POSN) \
	| ((5 - 1) << T2CON_OUTPS_POSN)
    movwf T2CON
    
init_gauge_dac:
    ; DAC*OUT1 available on pins; four DACs can be internally
    ; buffered through three op-amps:
    ; dac    1   4   2   3   5   7
    ; port RA2 RA4 RA5 RB2 RC0 RC1
    ; pin    4   6   7  23  11  12
    ; oa1    *   *   *   *
    ; oa2    *   *   *   *
    ; oa3                    *   *
    ; bits  10   5  10   5  10   5
    banksel DAC1CON0  ; 11
    ; Start off with all gauges halfway; other bits start as 0
    bsf DAC1REF9 ; 512/1024
    bsf DAC2REF9 ; 512/1024
    bsf DAC4REF4 ; 16/32
    bsf DAC5REF9 ; 512/1024
    ; The 10-bit double-buffers need explicit load
    movlw DACLD_DAC1LD_MASK \
        | DACLD_DAC2LD_MASK \
	| DACLD_DAC5LD_MASK
    movwf DACLD
    ; Only the 5-bit DAC4 will be exposed unbuffered.
    bsf DAC4OE1
    ; Enabled; right-justified; Vdd+; Vss-
    bsf DAC1EN
    bsf DAC2EN
    bsf DAC4EN
    bsf DAC5EN
    
init_gauge_opamp:
    banksel OPA1CON  ; 10
    movlw 0b0010 ; DAC1
    movwf OPA1PCHS
    movlw 0b0011 ; DAC2
    movwf OPA2PCHS
    movlw 0b0010 ; DAC5
    movwf OPA3PCHS
    ; OPACON: enabled, unity gain, no overrides
    movlw OPA1CON_OPA1EN_MASK | OPA1CON_OPA1UG_MASK
    movwf OPA1CON
    movwf OPA2CON
    movwf OPA3CON
    
init_rpi_spi:
    ; MSSP SPI child mode on all-PPS selected pins
    ; SCK, SDI (MOSI), SDO (MISO)
    banksel SSP1CON1  ; 4

    ; Data read on low-to-high clock
    bsf CKE

    ; SPI child mode, SS pin control disabled
    ; clock = SCK pin with idle-low polarity
    movlw SSP1CON1_SSPEN_MASK | 0b0101
    movwf SSP1CON1
    
    ; TSCHSCK input high (also low) time, child mode, >= Tcy + 20 (ns)
    ; Instruction cycle time Tcy = 125 ns
    ; Sclk <= 3.45MHz from the Pi

enable_interrupts:
    ; Every interrupt we use is on a "peripheral"
    bsf PEIE  ; cross-bank
    ; We don't actually need an interrupt vector; we just use interupts to wake
    ; bsf GIE
    
main:
    banksel PIR1  ; 0
fade_wait:
    sleep
    btfss TMR2IF
    bra fade_wait
    
fade_disable:
    ; Per 27.6.1, regardless of EN, if shutdown is active, output will take the
    ; "shutdown override" value from ASDAC - which we've set to 1
    banksel COG1CON0  ; 13
    bsf G1ASE  ; Auto-shutdown enable
    bcf G1EN   ; Disable the COG
    banksel PWMEN  ; 27
    clrf PWMEN ; Disable PWM5, 6
    banksel T2CON  ; 9
    bcf T2ON   ; Disable the timer (manual due to free-run mode) 
    banksel PIE1  ; 1
    bcf TMR2IE ; Disable timer int
    banksel PIR1  ; 0
    bcf TMR2IF ; Clear the timer int flag
    
rx_reset:
    ; Reset the receiver state to here if something smells
    dac_10b_rx 1
    dac_10b_rx 2
    dac_10b_rx 5
    dac_5b_rx 4
    
    ; until we get a serial interrupt for the first byte of the next packet
    sleep
    
    ; Since we were in sleep, we need to manually check for a timeout
    btfsc nTO 
    goto rx_reset
    
    reset
    
    end por_vec
