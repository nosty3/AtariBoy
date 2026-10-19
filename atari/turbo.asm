; A160: EXE clock request / Atari acknowledgement, A161: busy.
; No cartridge access between acknowledgement and end of the guard interval.
; Runs in Atari RAM; normal audio path resumes at an absolute raster boundary.
        org $4700
turbocheck
        lda $a160
        beq turboend
        jsr turbohold
turboend rts
turboaudiopause
        jsr turbohold
        jmp audiostart
turbohold
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
        lda #1
        sta $a160
        ldy #2
        ldx #0
turbodelay
        sta $d40a
        dex
        bne turbodelay
        dey
        bne turbodelay
turboready
        lda $a161
        bne turboready
        jsr display
        lda #$22
        sta $d400
        rts
        ert *>$4800
