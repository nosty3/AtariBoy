; Mixed state manager: 13 text rows, 72 bitmap lines, 2 text rows.
        org $4400
statesdl dta $70,$70,$70
        dta $42,a($a200)
        dta $82 ; DLI: dim column header
        dta $82 ; DLI: restore body
        :9 dta $02
        dta $82 ; DLI: thumbnail palette
        dta $4e,a($a408)
        dta $4e,a($a430)
        dta $4e,a($a458)
        dta $4e,a($a480)
        dta $4e,a($a4a8)
        dta $4e,a($a4d0)
        dta $4e,a($a4f8)
        dta $4e,a($a520)
        dta $4e,a($a548)
        dta $4e,a($a570)
        dta $4e,a($a598)
        dta $4e,a($a5c0)
        dta $4e,a($a5e8)
        dta $4e,a($a610)
        dta $4e,a($a638)
        dta $4e,a($a660)
        dta $4e,a($a688)
        dta $4e,a($a6b0)
        dta $4e,a($a6d8)
        dta $4e,a($a700)
        dta $4e,a($a728)
        dta $4e,a($a750)
        dta $4e,a($a778)
        dta $4e,a($a7a0)
        dta $4e,a($a7c8)
        dta $4e,a($a7f0)
        dta $4e,a($a818)
        dta $4e,a($a840)
        dta $4e,a($a868)
        dta $4e,a($a890)
        dta $4e,a($a8b8)
        dta $4e,a($a8e0)
        dta $4e,a($a908)
        dta $4e,a($a930)
        dta $4e,a($a958)
        dta $4e,a($a980)
        dta $4e,a($a9a8)
        dta $4e,a($a9d0)
        dta $4e,a($a9f8)
        dta $4e,a($aa20)
        dta $4e,a($aa48)
        dta $4e,a($aa70)
        dta $4e,a($aa98)
        dta $4e,a($aac0)
        dta $4e,a($aae8)
        dta $4e,a($ab10)
        dta $4e,a($ab38)
        dta $4e,a($ab60)
        dta $4e,a($ab88)
        dta $4e,a($abb0)
        dta $4e,a($abd8)
        dta $4e,a($ac00)
        dta $4e,a($ac28)
        dta $4e,a($ac50)
        dta $4e,a($ac78)
        dta $4e,a($aca0)
        dta $4e,a($acc8)
        dta $4e,a($acf0)
        dta $4e,a($ad18)
        dta $4e,a($ad40)
        dta $4e,a($ad68)
        dta $4e,a($ad90)
        dta $4e,a($adb8)
        dta $4e,a($ade0)
        dta $4e,a($ae08)
        dta $4e,a($ae30)
        dta $4e,a($ae58)
        dta $4e,a($ae80)
        dta $4e,a($aea8)
        dta $4e,a($aed0)
        dta $4e,a($aef8)
        dta $ce,a($af20)
        dta $42,a($a200+85*40)
        dta $02
        dta $41,a(statesdl)
        ert *>$4500
        org $4500
statesdli
        pha
        lda dliphase
        beq statesdim
        cmp #1
        beq statesbody
        cmp #2
        beq statesimage
        sta $d40a
        lda #$00
        sta $d01a
        sta $d018
        lda #$0f
        sta $d016
        sta $d017
        jmp statesdone
statesdim
        sta $d40a
        lda #$0a
        sta $d017
        jmp statesdone
statesbody
        sta $d40a
        lda #$0f
        sta $d017
        jmp statesdone
statesimage
        sta $d40a
        lda #$a0
        sta $d01a
        lda #$a6
        sta $d016
        lda #$aa
        sta $d017
        lda #$ae
        sta $d018
statesdone
        inc dliphase
        pla
        rti
        ert *>$4600
        org $4600
statesdisplay
        lda #<statesdl
        sta $d402
        lda #>statesdl
        sta $d403
        lda #<statesdli
        sta $0200
        lda #>statesdli
        sta $0201
        jsr textpalette
        lda #$0f
        sta $d017
        jmp displayend
        ert *>$4700
