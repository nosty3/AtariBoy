#pragma once
static inline unsigned save_and_disable_interrupts(void){return 0;}
static inline void restore_interrupts(unsigned x){(void)x;}
