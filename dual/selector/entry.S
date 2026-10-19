.syntax unified
.cpu cortex-m0plus
.thumb
.section .text.selector_entry,"ax",%progbits
.global selector_entry
.type selector_entry,%function
.thumb_func
selector_entry:
 cpsid i
 // Consume a one-shot decision after a real watchdog reset. Cold power-up
 // always runs the selector. Do not use scratch4..7: these belong to boot ROM.
 ldr r4, =0x40058000
 ldr r0, [r4, #8]
 ldr r1, [r4, #12]
 ldr r5, [r4, #16]
 ldr r2, [r4, #20]
 ldr r3, [r4, #24]
 movs r6, #0
 str r6, [r4, #12]
 str r6, [r4, #16]
 str r6, [r4, #20]
 str r6, [r4, #24]
 cmp r0, #0
 beq cold_selector
 ldr r0, =0x53454c31
 cmp r1, r0
 bne cold_selector
 mvns r0, r0
 cmp r3, r0
 bne cold_selector
 cmp r5, #1
 bhi cold_selector
 mvns r0, r5
 cmp r2, r0
 bne cold_selector
 ldr r4, =0x100bff00
 ldr r0, [r4]
 ldr r1, =0x314c5544
 cmp r0, r1
 bne cold_selector
 ldr r0, [r4, #4]
 cmp r0, #1
 bne cold_selector
 cmp r5, #0
 bne target_a8
 ldr r0, =0x10000100
 ldr r1, [r4, #8]
 ldr r2, [r4, #12]
 b enter_firmware
 target_a8:
 ldr r0, =0x100c0100
 ldr r1, [r4, #16]
 ldr r2, [r4, #20]
 b enter_firmware
cold_selector:
 ldr r0, =0x100bc100
 ldr r1, [r0]
 ldr r2, [r0, #4]
 b enter_firmware
.ltorg

// Enter ONLY after ROM/boot2 on a fresh reset, never from a running SDK.
// r0=vectors, r1=MSP, r2=reset handler. No SRAM code or stack dependency.
.section .text.enter_firmware,"ax",%progbits
.global enter_firmware
.type enter_firmware,%function
.thumb_func
enter_firmware:
 cpsid i
 ldr r3, =0xe000ed08
 str r0, [r3]
 movs r3, #0
 msr control, r3
 msr psp, r3
 msr msp, r1
 dsb
 isb
 cpsie i
 bx r2
.ltorg
