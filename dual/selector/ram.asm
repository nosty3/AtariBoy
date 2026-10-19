        opt h-
        org $0600
ramstart
        php
        sei
        lda #0
        sta $d40e
        sta $d400
        sta choice
; Enable POKEY keyboard scanning; sample held A across three scan frames.
; SKSTAT must indicate a currently held key: stale KBCODE alone is insufficient.
        lda #3
        sta $d20f
        ldy #3
scanframe
        lda $d40b
        beq scanframe
waitzero
        lda $d40b
        bne waitzero
        lda $d20f
        and #4
        bne nokey
        lda $d209
        and #$3f
        cmp #$3f            ; A (Shift/Ctrl do not change the selection)
        bne nokey
        lda #1
        sta choice
nokey   dey
        bne scanframe
        ldx choice
        lda commands,x
        sta $a1fe
; Both target INIT entries are RTS. Wait for all SIX target vector bytes,
; not for a single floating-bus value. Selector's own vectors differ.
retry   ldy #0
        lda choice
        beq compare
        ldy #6
compare ldx #0
next    lda $bffa,x
        cmp expected,y
        bne retry
        iny
        inx
        cpx #6
        bne next
        jsr initcall
        lda $022f
        sta $d400
        lda #$40
        sta $d40e
        plp
        jmp ($bffa)
initcall
        jmp ($bffe)
choice  dta 0
commands dta $47,$a8
expected
        icl "generated/vectors.asm"
        ert *>=$0700
        :($0700-*) dta 0
