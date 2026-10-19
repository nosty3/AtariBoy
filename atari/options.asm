; Options-only raster loop. Audio is silent; DLI owns palette during sample rows.
        org $4000
optionsstart
        lda #0
        sta $d40e
        sta $d400
        sta $d208
        sta $d218
        lda #$18
        sta $d201
        sta $d203
        sta $d211
        sta $d213
        lda #$17
        sta $d205
        sta $d207
        sta $d215
        sta $d217
optwaitlast
        lda $d40b
        cmp #LAST_VCOUNT
        bne optwaitlast
optwaitzero
        lda $d40b
        bne optwaitzero
optframe
        jsr display
        lda #$22
        sta $d400
optbottom
        lda $d40b
        cmp #124
        bcc optbottom
        jsr readinputs
        lda $a000
        cmp #2
        beq optwaitlast
        cmp #3
        beq optwaitlast
        cmp #6
        beq optwaitlast
        cmp #7
        beq optwaitlast
        lda #0
        sta $d40e
        lda #$4c
        sta $d409
        jsr textpalette
        jmp audiostart
        ert *>$4100
        org $4100
graphdl
        dta $70,$70,$70
        dta $42,a($a200)
        :8 dta $02
        dta $82                  ; DLI at end of text row 9
        dta $c5,a($a200+10*40)
        dta $c5,a($a200+11*40)
        dta $c5,a($a200+12*40)
        dta $c5,a($a200+13*40)    ; DLI restores text palette/font
        dta $42,a($a200+18*40)
        :5 dta $02
        dta $41,a(graphdl)
        org $4200
optionsdli
        pha
        txa
        pha
        ldx dliphase
        cpx #4
        beq restoretext
        lda optcolors,x
        sta $d40a
        sta $d016              ; PF0 only: BK/border keeps UI colour
        lda #$48
        sta $d409
        inc dliphase
        pla
        tax
        pla
        rti
restoretext
        sta $d40a
        lda #$00
        sta $d01a
        sta $d018
        lda #$0e
        sta $d016
        lda #$0e
        sta $d017
        lda #$4c
        sta $d409
        pla
        tax
        pla
        rti
        org $4800
        :8 dta $00
        :8 dta $55
        :8 dta $aa
        :8 dta $ff
