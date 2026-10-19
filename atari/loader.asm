        opt h-
        org $0600
loader  sei
        cld
        lda #0
        sta $02e0
        sta $02e1
; Wait for a complete raster, then distinguish 262 from 312 scanlines.
tvzero lda $d40b
        bne tvzero
tvbegin lda $d40b
        beq tvbegin
tvtest  cmp #140
        bcs tvpal
        lda $d40b
        bne tvtest
        lda #60
        bne tvsend
tvpal   lda #50
tvsend  sta $a1b2           ; EXE copies selected packed XEX into one RAM buffer
tvready cmp $a1b2           ; COM answers only after EXE has published the copy
        bne tvready
        lda #$22
        sta $a1b0
segment lda $a1b1           ; bit 0: byte available; no stream consumption
        and #1
        beq finish
        jsr get
        sta put+1
        jsr get
        sta put+2
        and put+1
        cmp #$ff            ; optional repeated XEX $ffff marker
        beq segment
        jsr get
        sta last
        jsr get
        sta last+1
        lda put+2
        cmp last+1
        bcc valid
        bne error
        lda put+1
        cmp last
        bcc valid
        bne error
valid   lda #0
        sta $02e2
        sta $02e3
bytes   jsr get
put     sta $ffff           ; self modifying destination: no ZP state
        lda put+1
        cmp last
        bne advance
        lda put+2
        cmp last+1
        beq loaded
advance inc put+1
        bne bytes
        inc put+2
        jmp bytes
loaded  lda $02e2
        ora $02e3
        beq segment
        jsr callinit        ; INIT may freely clobber A/X/Y and ZP
        sei
        cld
        jmp segment
callinit jmp ($02e2)
finish  lda $02e0
        ora $02e1
        beq error
        jmp ($02e0)         ; RUN only after EOF, not when vector is loaded
get     lda $a1b1
        and #1
        beq error           ; truncated header/segment
        lda $a1b0
        rts
error   lda #$46
        sta $d01a
        jmp error
last    dta a(0)
        ert *>$0700
        :($0700-*) dta 0
