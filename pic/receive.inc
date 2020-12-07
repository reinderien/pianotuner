; Upper three bits in protocol contain the DAC index (1, 2, 4, or 5)
index_posn equ 5

; Define some macros for serial read and DAC write -------------------------
    
dac_10b_rx macro index
    ; Receive two bytes to set a 10-bit DAC with another 6-bit check value
    banksel PIR1
spi_rx_&index:
    sleep  ; until we get a serial interrupt for the first byte
    ; Only one interrupt is interesting at this point, so don't bother checking
    ; btfss SSP1IF 
    ; goto spi_rx_&index
    bcf SSP1IF
    
    ; FM=0 Right-justified 10-bit DAC value, starting with (check*3, 0*3, MSB*2)
    ; The upper six bits of DACxREFH are unimplemented so this is safe. 
    ; Don't load via DACxLD until this is validated.
    banksel SSP1BUF
    movf SSP1BUF           ; Load from MSSP receive buffer, into...
    banksel DAC&index&REFH
    movwf DAC&index&REFH   ; 2 LSBs of high DAC byte (MSBs of 10-bit DAC value)
    
    andlw ~0b11                ; Drop the actual DAC data
    xorlw index << index_posn  ; Compare with DAC index
    btfss ZERO                 ; Check for equality
    goto rx_reset              ; Unexpected check value; reset receiver state
    
    banksel PIR1
    sleep  ; until we get a serial interrupt for the second byte
    bcf SSP1IF
    
    banksel SSP1BUF
    movf SSP1BUF          ; Load from MSSP receive buffer, into...
    banksel DAC&index&REFL
    movwf DAC&index&REFL  ; 8 LSBs of 10-bit DAC value
    ; Load all 10 bits of DAC value
    ; The DACxLD bits are done via #define so we need to use long-form bsf
    bsf BANKMASK(DACLD), DACLD_DAC&index&LD_POSN
endm

dac_5b_rx macro index
    ; Receive one byte to set a 5-bit DAC with a 3-bit check value, currently
    ; ignored
    banksel PIR1
spi_rx_&index:
    sleep  ; until we get a serial interrupt for the byte
    bcf SSP1IF
    
    banksel SSP1BUF
    movf SSP1BUF         ; Load from MSSP receive buffer, into...
    banksel DAC&index&REF
    movwf DAC&index&REF  ; 5 bits of DAC value
    
    ; The 5-bit DACs do not have a load bit, so just assume that this last
    ; message is OK rather than juggling RAM
endm
