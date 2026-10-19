        opt h-
        opt f+
        org $a000
        dta 0
        org $b700
start   ldx #0
copy    lda $b800,x
        sta $0600,x
        inx
        bne copy
        jmp $0600
init    rts
        org $b800
        ins "generated/ram.bin"
        org $bffa
        dta a(start),0,4,a(init)
