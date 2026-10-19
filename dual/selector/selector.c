#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "hardware/sync.h"
#include "hardware/structs/sio.h"
#include "generated/selector_rom.h"
#define DATA (255u<<13)
#define PHI (1u<<22)
#define RW (1u<<23)
#define S5 (1u<<25)
#define RD4 (1u<<26)
#define RD5 (1u<<27)
static uint32_t words[8192];
extern void enter_firmware(uint32_t vector,uint32_t sp,uint32_t pc) __attribute__((noreturn));
extern void selector_entry(void);
// Force linker retention of the entry used by the merged image's reset vector.
void (*volatile retained_entry)(void)=selector_entry;
static void launch(unsigned choice) __attribute__((noreturn));
static void launch(unsigned choice){
 const volatile uint32_t *m=(const volatile uint32_t *)0x100bff00;
 // Manifest: magic, version, GB SP/reset, A8 SP/reset. All addresses validated.
 unsigned k=choice?4:2;
 uint32_t sp=m[k],pc=m[k+1],lo=choice?0x100c0000:0x10000000,hi=choice?0x10100000:0x100bc000;
 if(m[0]!=0x314c5544||m[1]!=1||sp<0x20000000||sp>0x20042000||(sp&7)||!(pc&1)||pc<lo||pc>=hi)for(;;)tight_loop_contents();
 save_and_disable_interrupts();
 sio_hw->gpio_oe_clr=DATA; // high impedance during firmware startup
 *(volatile uint32_t *)0xe000e010=0; // SysTick off
 *(volatile uint32_t *)0xe000e180=~0u; // disable all NVIC IRQs
 *(volatile uint32_t *)0xe000e280=~0u; // clear all pending IRQs
 *(volatile uint32_t *)0xe000ed04=(1u<<25)|(1u<<27); // PendST/PendSV clear
 // Only scratch0..3 belong to us. Boot ROM owns scratch4..7.
 // The next boot consumes this token before either SDK runtime runs.
 watchdog_hw->scratch[0]=0x53454c31u;
 watchdog_hw->scratch[1]=choice;
 watchdog_hw->scratch[2]=~choice;
 watchdog_hw->scratch[3]=~0x53454c31u;
 __dmb();
 watchdog_reboot(0,0,0); // real reset, then normal ROM -> boot2 -> selector_entry
 for(;;)tight_loop_contents();

}
// One core only. No mounted FAT, USB, interrupts or background work at handoff.
unsigned __not_in_flash_func(selector_bus)(void){
 sio_hw->gpio_oe_clr=DATA;sio_hw->gpio_set=RD5;
 for(;;){
  uint32_t p=sio_hw->gpio_in;
  if(p&S5)continue;
  if(p&RW){
   sio_hw->gpio_out=words[p&8191];
   p=sio_hw->gpio_in;
   if((p&(S5|RW))!=RW)continue;
   sio_hw->gpio_oe_set=DATA;
   while(((p=sio_hw->gpio_in)&(S5|RW))==RW)sio_hw->gpio_out=words[p&8191];
   sio_hw->gpio_oe_clr=DATA;
  }else if((p&8191)==0x1fe&&(p&PHI)){
   uint32_t last=p;
   while(((p=sio_hw->gpio_in)&PHI)!=0){
    if((p&(S5|RW|8191))!=0x1fe){while(sio_hw->gpio_in&PHI)tight_loop_contents();break;}
    last=p;
   }
   unsigned cmd=(last>>13)&255;
   if(cmd==0x47||cmd==0xa8)return cmd==0xa8;
  }
 }
}
int main(void){
 gpio_init(22);gpio_set_dir(22,GPIO_IN);gpio_pull_down(22);
 bool atari=false;uint32_t until=time_us_32()+100000;
 while((int32_t)(time_us_32()-until)<0)if(gpio_get(22)){atari=true;break;}
 gpio_disable_pulls(22);
 if(!atari)launch(0); // AtariBoy owns the one shared USB disk implementation.
 for(unsigned i=0;i<=27;++i){gpio_init(i);gpio_disable_pulls(i);}
 sio_hw->gpio_clr=RD4|RD5;sio_hw->gpio_oe_set=RD4|RD5;
 if(!set_sys_clock_khz(250000,false))for(;;)tight_loop_contents();
 for(unsigned i=0;i<8192;++i)words[i]=RD5;
 for(unsigned i=0;i<512;++i)words[0x1700+i]=RD5|((uint32_t)selector_boot[i]<<13);
 for(unsigned i=0;i<6;++i)words[0x1ffa+i]=RD5|((uint32_t)selector_vectors[i]<<13);
 save_and_disable_interrupts();
 launch(selector_bus());
}
