        icl "../generated/timing_config.asm"
        icl "../generated/display_config.asm"
; Standard segmented XEX, loaded through $A1B0 by the page-$06 loader.
; Full framebuffer: rows 0..88 at A200, rows 89..107 at B200.
; Every DL row has LMS; no 40-byte row crosses a 4 KiB boundary.
        org $2600
init    lda #$a5
        sta $2700
        rts
        ini init
        org $2000
start   sei
        cld
        lda #0
        sta $d40e
        sta $d400
        sta $022f
        sta $d01d
        sta $d01b
        sta $d01a
        sta $d20e           ; no keyboard IRQ; use hardware scan in VBI
        sta $d21e           ; second POKEY IRQs disabled too (mono: mirror)
        sta frame
        sta keyseq
        jsr detect_stereo
        stx $a13d
        jsr initfont
        lda #2
        sta potwarm
        lda #$ff
        sta prevkey
        sta lastkey
        lda #$04
        sta $d016
        lda #$08
        sta $d017
        lda #$0e
        sta $d018
        lda #<textdl
        sta $d402
        sta $0230
        lda #>textdl
        sta $d403
        sta $0231
        lda #<vblank
        sta $0222
        lda #>vblank
        sta $0223
        lda #$4c
        sta $d409
        lda #2
        sta $d401
        lda #$ff
        sta displaymode
        jsr display
        lda #3
        sta $d20f
        lda #0
        sta $d20b
        lda #DISPLAY_ROWS
        sta $a13c           ; image rows, validated by COM
        lda #TV_HZ
        sta tvstandard
        sta $a13e
        lda #$22
        sta $d400
        sta $022f
        lda displaymode
        cmp #6
        bne audioactive
        jmp optionsstart
audioactive jmp audiostart
vblank  jsr turbocheck
        jsr readinputs
        jsr display
        jmp $e462
readinputs lda $d300
        sta packet+15
        and #$0f
        sta packet
        lda $d300
        lsr
        lsr
        lsr
        lsr
        sta packet+4
        lda $d010
        and #1
        sta packet+1
        lda $d011
        and #1
        sta packet+5
        lda $d200
        sta packet+2
        lda $d201
        sta packet+3
        lda $d202
        sta packet+6
        lda $d203
        sta packet+7
        ldx #3
pots    lda $d204,x
        sta packet+18,x
        dex
        bpl pots
        lda potwarm         ; first scan may have begun mid-frame
        beq potsready
        dec potwarm
        lda #228
        sta packet+2
        sta packet+3
        sta packet+6
        sta packet+7
        ldx #3
potfill sta packet+18,x
        dex
        bpl potfill
potsready
        lda $d20f           ; SKSTAT: bit 2 low while key held
        sta packet+16
        lda $d209
        sta packet+17
        lda packet+16
        and #4
        bne nokey
        lda packet+17
        and #$3f
        jmp gotkey
nokey   lda #$ff
gotkey  sta packet+10
        cmp prevkey
        beq samekey
        sta prevkey
        cmp #$ff
        beq samekey
        sta lastkey
        inc keyseq
samekey lda lastkey
        sta packet+8
        lda keyseq
        sta packet+9
        lda #0
        sta packet+11
        lda packet+10
        cmp #$11            ; XL/XE HELP hardware scan code
        bne console
        inc packet+11
console lda $d01f
        eor #7
        and #7
        sta temp
        and #1
        sta packet+12
        lda temp
        lsr
        and #1
        sta packet+13
        lda temp
        lsr
        lsr
        and #1
        sta packet+14
        lda #0
        sta packet+22
        sta $d20b           ; start paddle conversion for next frame
        inc frame
        lda frame
        sta packet+23
        ldx #0
send    lda packet,x
        sta $a120,x
        inx
        cpx #24
        bne send            ; $A137 last: publish inputs and swap full cart image
        rts
frame   dta 0
keyseq  dta 0
prevkey dta $ff
lastkey dta $ff
temp    dta 0
potwarm dta 2
        ert *>$2200
        org $2200
dl
        icl "../generated/display_list.asm"
        ert *>$2400
        org $2500
packet  :24 dta 0
        org $2400
textdl  dta $70,$70,$70,$42,a($a200)
        :23 dta $02
        dta $41,a(textdl)
        org $2800
display lda #0
        sta $d40e
        sta dliphase
        lda #$4c
        sta $d409
        lda $a000
        sta displaymode
        cmp #7
        bne notstates
        jmp statesdisplay
notstates cmp #2
        beq graphoptions
        cmp #4
        bcs gamemode
        cmp #1
        beq gamemode
        lda #<textdl
        sta $d402
        lda #>textdl
        sta $d403
        jsr textpalette
        jmp displayend
gamemode
        tax
        lda displaylo,x
        sta $d402
        lda displayhi,x
        sta $d403
        lda $a001
        sta $d01a
        lda $a002
        sta $d016
        lda $a003
        sta $d017
        lda $a004
        sta $d018
        jmp displayend
graphoptions
        ldx #3
copycolors lda $a001,x
        sta optcolors,x
        dex
        bpl copycolors
        lda #<graphdl
        sta $d402
        lda #>graphdl
        sta $d403
        lda #<optionsdli
        sta $0200
        lda #>optionsdli
        sta $0201
        jsr textpalette
displayend
        lda #0
        ldx displaymode
        cpx #7
        beq enabledli
        cpx #2
        bne nodli
enabledli
        ora #$80
nodli
setnmi  sta $d40e
        rts
textpalette
        lda #$00
        sta $d01a
        sta $d018
        lda #$0e
        sta $d016
        lda #$0e
        sta $d017
        rts
dliphase dta 0
optcolors dta 0,4,8,14
displaymode dta $ff
tvstandard dta TV_HZ
displaylo dta <textdl,<dl,<graphdl,<textdl,<dl144,<dl192,<titledl,<statesdl
displayhi dta >textdl,>dl,>graphdl,>textdl,>dl144,>dl192,>titledl,>statesdl
displayui dta 0,0,1,1,0,0,1,1
        ert *>$2900
        org $2900
initfont
        ldx #7
fontpatch
        lda diskicon,x
        sta $4c00+$60*8,x
        lda joyicon,x
        sta $4c00+$50*8,x
        dex
        bpl fontpatch
        rts
joyicon dta $38,$38,$10,$10,$10,$ee,$fe,$00
diskicon dta $fe,$82,$fe,$6e,$fe,$ee,$ee,$00
        ert *>$2a00
        org $2f00
titledl dta $70,$70,$70
        dta $4f,a($a200)
        :63 dta $0f
        dta $42,a($ac00)
        :15 dta $02
        dta $41,a(titledl)
        ert *>$3000
        org $2a00
dl144
        icl "../generated/display_list_144.asm"
        ert *>$2c00
        org $2c00
dl192
        icl "../generated/display_list_192.asm"
        ert *>$3000
        icl "../generated/audio_player.asm"
        icl "options.asm"
        icl "../generated/stereo_player.asm"
        org $4300
detect_stereo
        ldx #0
        stx $d20f
        stx $d21f
        ldy #3
        sty $d21f
        sta $d40a
        sta $d40a
        lda #$ff
detectloop and $d20a
        inx
        bne detectloop
        sty $d20f
        cmp #$ff
        bne detectmono
        inx
detectmono
        rts
        ert *>$4400
        icl "states.asm"
        icl "turbo.asm"
        org $4c00
        ins "../assets/menu-font.fnt"
        run start
