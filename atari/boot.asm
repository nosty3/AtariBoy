        icl "../generated/loader_crc.asm"
CRCLO = $0700
CRCHI = $0701
        opt h-
        org $b000
boot    sei
        cld
        lda #0
        sta $d40e
        sta $d400
        lda #$11
        sta $a1b0
        ldx #0
copy    lda $a1b0
        sta $0600,x
        inx
        bne copy
; Verify the received loader before executing it. CRC-16/CCITT-FALSE,
; initial FFFF, polynomial 1021. Temporary bytes $0700/$0701 die on handoff.
        lda #$ff
        sta CRCLO
        sta CRCHI
        ldx #0
verify  lda $0600,x
        eor CRCHI
        sta CRCHI
        ldy #8
bit     asl CRCLO
        rol CRCHI
        bcc plain
        lda CRCLO
        eor #$21
        sta CRCLO
        lda CRCHI
        eor #$10
        sta CRCHI
plain   dey
        bne bit
        inx
        bne verify
        lda CRCLO
        cmp #<LOADER_CRC
        bne corrupt
        lda CRCHI
        cmp #>LOADER_CRC
        bne corrupt
        jmp $0600
corrupt lda #$28            ; orange: loader transfer failed CRC
        sta $d01a
        jmp corrupt
init    rts
