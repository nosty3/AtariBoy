#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <array>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "generated/atari_images.hpp"
#include "engine.h"
#include "audio.h"
#include "video.hpp"
#include "controls.hpp"
#include "app.hpp"
#include "osd.hpp"
#include "settings.hpp"
#include "link_diagnostic.hpp"
extern "C" void usb_disk_mode(void);
extern "C" void flash_fs_set_usb_buffer(void *);
namespace {
constexpr uint32_t DATA=255u<<13,RD4=1u<<26,RD5=1u<<27,S5=1u<<25,RW=1u<<23,PHI=1u<<22;
alignas(4) uint32_t bus_words[256];
// EXE requests via A160; Atari disables DMA, then writes A160 to park COM.
// COM publishes phase 1; EXE switches PLL and publishes phase 2.
alignas(4) uint32_t clock_phase=0;
static bool turbo=false;
alignas(4) uint8_t screens[(video::max_rows+2)*40];
alignas(4) uint32_t cart_words[2][8192];
alignas(4) uint32_t pending=0,ack=0,packet_seq=0;
uint8_t input[controls::packet_size],staged[controls::packet_size];
uint32_t *front=cart_words[0];
// Two independent mailboxes reuse DISJOINT video/PCM regions of cart_words.
// EXE never edits a region until its own mailbox has acknowledged the ticket.
uint32_t *audio_front=cart_words[0];
// Eight-byte granularity covers every PCM gap; constant-time routing in COM.
constexpr auto read_route=[](){std::array<uint8_t,1024> route{};for(unsigned i=0;i<1024;++i){unsigned a=i*8;route[i]=(a>=0x100&&a<0x200)?8:4*!((a>=0x200&&a<0xfe8)||(a>=0x1200&&a<0x1a98)||(a>=0x1aa8&&a<0x1af8)||a<8);}return route;}();
// One branch-free read path for registers, video and both PCM streams.
// Byte offsets select a base address. Integer arithmetic avoids forming an
// out-of-bounds bus_words pointer for the A1xx register address adjustment.
uintptr_t read_bases[3]={uintptr_t(cart_words[0]),uintptr_t(cart_words[0]),uintptr_t(bus_words)-0x100*4};
alignas(4) uint32_t audio_pending=0,audio_ack=0,audio_epoch=1,audio_playing=0;
uint32_t audio_seen=0,audio_ticket_epoch[2]={};
alignas(4) uint32_t audio_underruns=0,presented_frames=0;
uint32_t image_generation[2]={},shown_generation=0;
uint32_t seen=0,screen_rows=video::rows;
alignas(4) uint32_t diag_error=0,diag_frames=0;
alignas(4) uint32_t atari_hz=50,frame_count=0,repeat_count=0,last_row_count=0;
alignas(4) uint32_t render_us=0,max_render_us=0,late_count=0,playing=0;
const uint8_t *stream=atari::loader;
uint32_t stream_pos=0,stream_size=0;
alignas(4) uint32_t boot_tv_request=0;
inline uint32_t acquire(const uint32_t *p){return __atomic_load_n(p,__ATOMIC_ACQUIRE);}
inline void release(uint32_t *p,uint32_t v){__atomic_store_n(p,v,__ATOMIC_RELEASE);}
}
uint32_t stream_out[2]={RD5|DATA,RD5};
// The boot XEX is losslessly packed in SRAM; expanded one prepared byte at a time.
// This path is only used by the page-$06 loader, never by video/PCM DMA reads.
uint32_t packed_pos=0;
unsigned literal_left=0,template_left=0;
uint8_t sample_template[45]={0x8d,0x0a,0xd4,0xad,0,0xbb,0x8d,1,0xd2,0x8d,3,0xd2,0xad,0,0xbd,0x8d,5,0xd2,0xad,0x40,0xbe,0x8d,7,0xd2,0xad,0,0xb0,0x8d,0x11,0xd2,0x8d,0x13,0xd2,0xad,0x38,0xb1,0x8d,0x15,0xd2,0xad,0x78,0,0x8d,0x17,0xd2};
static uint8_t next_program_byte(){
 if(!literal_left&&!template_left){
  unsigned tag=stream[packed_pos++];
  if(tag<128)literal_left=tag+1;
  else {const uint8_t offsets[]={4,13,19,25,34,40};
   for(unsigned n:offsets){unsigned a;
    if(tag==128)a=(sample_template[n]|(sample_template[n+1]<<8))+1;
    else {a=stream[packed_pos]|(stream[packed_pos+1]<<8);packed_pos+=2;}
    sample_template[n]=a;sample_template[n+1]=a>>8;
   }template_left=45;
  }
 }
 if(literal_left){--literal_left;return stream[packed_pos++];}
 return sample_template[45-template_left--];
}
static __attribute__((always_inline)) inline void select_stream(uint8_t value){
 if(value!=0x11 && value!=0x22)return;
 stream=value==0x11?atari::loader:atari::program;
 stream_size=value==0x11?sizeof atari::loader:atari::program_size;
 stream_pos=0;if(value==0x22)packed_pos=literal_left=template_left=0;
 stream_out[0]=RD5|(uint32_t(value==0x22?next_program_byte():stream[0])<<13);stream_out[1]=RD5|(1u<<13);
 bus_words[0xb1]=stream_out[1];
}
__attribute__((noinline)) void __not_in_flash_func(write_register)(uint32_t addr,uint8_t value){
 if(addr==0x60 && value==1 && (__atomic_load_n(bus_words+0x60,__ATOMIC_RELAXED)&DATA)){
  sio_hw->gpio_oe_clr=DATA;
  release(&clock_phase,1);
  while(acquire(&clock_phase)!=2)tight_loop_contents();
  __atomic_store_n(bus_words+0x60,RD5,__ATOMIC_RELAXED);
  __atomic_store_n(bus_words+0x61,RD5,__ATOMIC_RELAXED);
  release(&clock_phase,0);
 }else if(addr==0xb0){
  select_stream(value);
 }else if(addr==0xb2){
  if(value==50||value==60){bus_words[0xb2]=RD5;release(&boot_tv_request,value);}
 }else if(addr==0x3d){if(value<=1)__atomic_store_n(&detected_stereo,value,__ATOMIC_RELAXED);
 }else if(addr==0x3c){if(value>=4&&value<=video::max_rows&&!(value&3))screen_rows=value;
 }else if(addr==0x3e){if(value==50||value==60)release(&atari_hz,value);
 }else if(addr>=0x20&&addr<=0x37){
  staged[addr-0x20]=value;
  if(addr==0x37){
   last_row_count=0; // No read/row counters in direct-mapped video mode.
   uint32_t ticket=acquire(&pending);
   bool fresh=ticket!=seen;
   if(fresh){
    front=cart_words[ticket&1];read_bases[0]=uintptr_t(front);seen=ticket;
    uint32_t generation=image_generation[ticket&1];
    if(acquire(&playing)&&generation!=shown_generation)++presented_frames;
    shown_generation=generation;release(&ack,ticket);
   }
   else if(acquire(&playing))++repeat_count;
   uint32_t aticket=acquire(&audio_pending);
   bool afresh=aticket!=audio_seen;
   bool audible=acquire(&audio_playing)!=0;
   bool valid=afresh&&audio_ticket_epoch[aticket&1]==acquire(&audio_epoch);
   if(afresh){audio_front=cart_words[aticket&1];read_bases[1]=uintptr_t(audio_front);audio_seen=aticket;release(&audio_ack,aticket);}
   if(audible&&!valid)++audio_underruns;
   bus_words[0x3f]=RD5|((valid&&audible)?(255u<<13):0);

   ++frame_count;
   uint32_t seq=__atomic_load_n(&packet_seq,__ATOMIC_RELAXED);
   release(&packet_seq,seq+1);__dmb();
   for(unsigned i=0;i<controls::packet_size;++i)__atomic_store_n(input+i,staged[i],__ATOMIC_RELAXED);
   release(&packet_seq,seq+2);
  }
 }
}
static __attribute__((always_inline)) inline void next_stream_byte(){
 if(stream_pos>=stream_size)return;
 if(++stream_pos<stream_size)stream_out[0]=RD5|(uint32_t(stream==atari::program?next_program_byte():stream[stream_pos])<<13);
 else {stream_out[0]=RD5|DATA;stream_out[1]=RD5;bus_words[0xb1]=RD5;}
}
// Full cartridge image: reads are side-effect free, including screen reads.
// Live A1xx registers use their existing shared table; all other addresses
// directly select a word in the published 8 KiB cartridge image.
static __attribute__((always_inline)) inline void drive_cart_value(uint32_t pins){
 unsigned addr=pins&8191;
 uintptr_t base=read_bases[read_route[addr>>3]>>2];
 sio_hw->gpio_out=*reinterpret_cast<const uint32_t*>(base+addr*4);
}
static void prepare_screen(unsigned b,unsigned mode){
 bool game=mode==1||mode==4||mode==5;
 cart_words[b][0]=RD5|(uint32_t(mode)<<13);
 cart_words[b][5]=RD5|(uint32_t(game&&(settings.audio_stereo||detected_stereo==1))<<13);
 for(unsigned i=0;i<4;++i){unsigned value=(game||mode==2)?((settings.hue<<4)|settings.brightness[i]):(mode==6?(i==0||i==3?0x0e:0x00):(i==0||i==3?0x00:0x0e));cart_words[b][1+i]=RD5|(value<<13);}
 for(unsigned y=0;y<(mode==7?87:mode==6?80:game?(mode>=4?144:video::rows):24);++y){
  unsigned offset=y<89?0x200+y*40:0x1200+(y-89)*40;
  for(unsigned x=0;x<40;++x)cart_words[b][offset+x]=RD5|(uint32_t(screens[y*40+x])<<13);
 }
 if(mode==5)for(unsigned i=0;i<80;++i)cart_words[b][0x1aa8+i]=RD5|(uint32_t(screens[144*40+i])<<13);
}
// Right PCM: 936 bytes in gaps outside framebuffer, header and registers.
static unsigned right_address(unsigned i){
 if(i<512)return 0x1000+i;i-=512;
 if(i<248)return 8+i;i-=248;
 if(i<136)return 0x1f78+i;i-=136;
 if(i<24)return 0xfe8+i;i-=24;
 return 0x1a98+i;
}
static __attribute__((noinline)) void prepare_audio(unsigned b){
 for(unsigned i=0;i<312;++i){
  unsigned level=audio_frame[i]+30;if(level>=61)level-=61;
  unsigned q=level/4,r=level%4;
  unsigned pair=q+(r==3),c2=q+(r==1||r==2),c3=level-2*pair-c2;
  cart_words[b][0x1b00+i]=RD5|((16+pair)<<13);
  cart_words[b][0x1d00+i]=RD5|((16+c2)<<13);
  cart_words[b][0x1e40+i]=RD5|((16+c3)<<13);
  // Mono on two POKEYs: duplicate the final mixed sample, not just GB left.
  // Reuse the proven stereo playback loop; no second APU synthesis.
  if(audio_stereo||detected_stereo==1){unsigned right=(audio_stereo?audio_frame_right[i]:audio_frame[i])+30;if(right>=61)right-=61;unsigned q=right/4,r=right%4;unsigned pair=q+(r==3),c2=q+(r==1||r==2),c3=right-2*pair-c2;
   cart_words[b][right_address(i)]=RD5|((16+pair)<<13);
   cart_words[b][right_address(312+i)]=RD5|((16+c2)<<13);
   cart_words[b][right_address(624+i)]=RD5|((16+c3)<<13);
  }
 }
}
static void pump_audio(App &app){
 release(&audio_epoch,app.cadence.epoch);
 bool audible=app.active&&!app.paused&&!app.in_options&&!app.in_help&&!app.in_states&&!app.pending_operation&&audio_enabled;
 release(&audio_playing,audible);
 uint32_t t=acquire(&audio_pending);
 if(!audible||acquire(&audio_ack)!=t)return;
 if(!app.cadence.output(settings.pal_full&&app.tv_hz==50,app.tv_hz==60))return;
 ++t;prepare_audio(t&1);audio_ticket_epoch[t&1]=app.cadence.epoch;
 release(&audio_pending,t);
}
static uint8_t __not_in_flash_func(diag_byte)(unsigned addr){
 const uint32_t *v=nullptr;
 switch((addr-0x40)>>2){
 case 0:v=&frame_count;break;case 1:v=&repeat_count;break;case 2:v=&render_us;break;
 case 3:v=&max_render_us;break;case 4:v=&late_count;break;case 5:v=&last_row_count;break;
 case 6:v=&diag_error;break;case 7:v=&diag_frames;break;
 }
 return v?uint8_t(acquire(v)>>((addr&3)*8)):255;
}
// A1xx is the sole control window; /CCTL is never decoded or driven.
// Keep the stream handler out of the ROM/DMA loop: no stream bookkeeping or
// PHI2 wait is allowed on the ordinary ROM response path.
static __attribute__((noinline,optimize("O2"))) void __not_in_flash_func(read_stream)(){
 constexpr uint32_t selected=RW|0x1b0,mask=S5|RW|8191;
 sio_hw->gpio_out=stream_out[0];
 uint32_t p=sio_hw->gpio_in;
 if((p&mask)!=selected)return;
 sio_hw->gpio_oe_set=DATA;
 // Address decode is valid already in PHI2 low. Drive the PREPARED byte now,
 // but only consume it after observing the active high phase of this access.
 while((p&mask)==selected && !(p&PHI))p=sio_hw->gpio_in;
 bool accessed=(p&mask)==selected;
 while((p&mask)==selected && (p&PHI))p=sio_hw->gpio_in;
 sio_hw->gpio_oe_clr=DATA;
 if(accessed)next_stream_byte();
}
extern "C" [[noreturn]] __attribute__((optimize("O2"))) void __not_in_flash_func(com_bus)(){
 sio_hw->gpio_oe_clr=DATA;sio_hw->gpio_set=RD5;
 for(;;){
  uint32_t p=sio_hw->gpio_in;
  if(p&S5)continue;
  if(p&RW){
   if((p&8191)==0x1b0){read_stream();continue;}
   drive_cart_value(p);
   p=sio_hw->gpio_in;
   if((p&(S5|RW))!=RW || (p&8191)==0x1b0)continue;
   sio_hw->gpio_oe_set=DATA;
   while(((p=sio_hw->gpio_in)&(S5|RW))==RW && (p&8191)!=0x1b0)
    drive_cart_value(p);
   sio_hw->gpio_oe_clr=DATA;
  }else if((p&0x1f00)==0x100 && (p&PHI)){
   uint32_t addr=p&8191,last=p;
   // Capture only samples belonging to this qualified high-phase write.
   // /S5/address may disappear before PHI2 is observed low: preserve the
   // last valid data, then drain this high phase so it cannot commit twice.
   while(((p=sio_hw->gpio_in)&PHI)!=0){
    if((p&(S5|RW|8191))!=addr){
     while(sio_hw->gpio_in&PHI)tight_loop_contents();
     break;
    }
    last=p;
   }
   if(addr==0x1b0)select_stream(uint8_t(last>>13));
   else write_register(addr&255,uint8_t(last>>13));
  }
 }
}
// Called only from the stopped EXE transaction path. Atari uses RAM code and
// disables ANTIC DMA before acknowledging; its first status read is delayed
// by 512 WSYNC lines (>32 ms PAL), well beyond normal PLL settling time.
static __attribute__((noinline)) bool switch_clock(){
 __atomic_store_n(bus_words+0x61,RD5|(1u<<13),__ATOMIC_RELAXED);
 __atomic_store_n(bus_words+0x60,RD5|(1u<<13),__ATOMIC_RELEASE);
 while(acquire(&clock_phase)!=1)tight_loop_contents();
 // Raise voltage BEFORE increasing frequency; lower only AFTER 250 MHz.
 // COM is parked and Atari DMA is disabled throughout this transaction.
 if(!turbo){vreg_set_voltage(VREG_VOLTAGE_1_15);busy_wait_us_32(1000);}
 bool ok=set_sys_clock_khz(turbo?250000:300000,false);
 if(ok)turbo=!turbo;
 if(!turbo){vreg_set_voltage(VREG_VOLTAGE_1_10);busy_wait_us_32(1000);}
 release(&clock_phase,2);
 while(acquire(&clock_phase)!=0)tight_loop_contents();
 return ok;
}
static void com_entry(){(void)save_and_disable_interrupts();multicore_fifo_pop_blocking();__dmb();com_bus();}
struct Diagnostic {
 LinkDiagnostic link;
 bool enabled_last=false;
 unsigned boot_option=256;
 uint32_t start=0,at_start=0,repeat_start=0,work=0,wait=0,calls=0,maxwork=0;
 uint32_t au_start=0,au=0;
 uint32_t at10=0,rp10=0,work10=0,wait10=0,max10=0,gb10=0,gb=0;
 void sample(uint32_t ex,uint32_t waiting,uint32_t completed){link.waiting(waiting);work+=ex;wait+=waiting;++calls;gb+=completed;if(ex>maxwork)maxwork=ex;}
 void draw(uint8_t *out,unsigned rows,uint32_t now,unsigned hz,unsigned option,bool enabled){
  if(!enabled){if(enabled_last){start=0;link.valid=false;enabled_last=false;}return;}
  enabled_last=true;
  uint32_t at=acquire(&frame_count),rp=acquire(&repeat_count),dt=now-start;
  if(!start){start=now;at_start=at;repeat_start=rp;au_start=acquire(&audio_underruns);work=wait=calls=maxwork=gb=0;return;}
  if(dt>=1000000u){
   uint32_t total_au=acquire(&audio_underruns);au=total_au-au_start;au_start=total_au;
   at10=uint64_t(at-at_start)*10000000/dt;rp10=uint64_t(rp-repeat_start)*10000000/dt;
   work10=calls?work/calls/100:0;wait10=calls?wait/calls/100:0;max10=maxwork/100;
   gb10=gb?work/gb/100:0;start=now;at_start=at;repeat_start=rp;work=wait=calls=maxwork=gb=0;
  }
  char b[21];
  snprintf(b,sizeof b,"AT%lu.%lu RP%lu.%lu",(unsigned long)at10/10,(unsigned long)at10%10,(unsigned long)rp10/10,(unsigned long)rp10%10);osd_text(out,rows,8,b);
  snprintf(b,sizeof b,"EX%lu.%lu WT%lu.%lu",(unsigned long)work10/10,(unsigned long)work10%10,(unsigned long)wait10/10,(unsigned long)wait10%10);osd_text(out,rows,16,b);
  snprintf(b,sizeof b,"MX%lu.%lu GB%lu.%lu",(unsigned long)max10/10,(unsigned long)max10%10,(unsigned long)gb10/10,(unsigned long)gb10%10);osd_text(out,rows,24,b);
  snprintf(b,sizeof b,"TV%u OP%u BO%u",hz,option,boot_option);osd_text(out,rows,32,b);
  snprintf(b,sizeof b,"AR%lu.%lu RX%lu.%lu",(unsigned long)link.ar10/10,(unsigned long)link.ar10%10,(unsigned long)link.rx10/10,(unsigned long)link.rx10%10);osd_text(out,rows,40,b);
  snprintf(b,sizeof b,"LS%lu OV%lu E%lu",(unsigned long)link.ls,(unsigned long)link.ov,(unsigned long)link.errors);osd_text(out,rows,48,b);
  snprintf(b,sizeof b,"IG%lu.%lu MW%lu.%lu",(unsigned long)link.gap10/10,(unsigned long)link.gap10%10,(unsigned long)link.wait10/10,(unsigned long)link.wait10%10);osd_text(out,rows,56,b);
  snprintf(b,sizeof b,"AU%lu",(unsigned long)au);osd_text(out,rows,64,b);

 }
};
extern "C" __attribute__((noinline)) void service_boot_program(){
  uint32_t boot_tv=acquire(&boot_tv_request);
  if(boot_tv){
   const volatile uint8_t *src=boot_tv==60?atari::program_ntsc:atari::program_pal;
   size_t size=boot_tv==60?sizeof atari::program_ntsc:sizeof atari::program_pal;
   for(size_t i=0;i<size;++i)atari::program[i]=src[i];
   atari::program_size=boot_tv==60?atari::program_ntsc_size:atari::program_pal_size;
   release(&atari_hz,boot_tv);release(&boot_tv_request,0);
   __atomic_store_n(bus_words+0xb2,RD5|(boot_tv<<13),__ATOMIC_RELEASE);
  }
}
int main(){
 // As upstream, detect a running Atari before choosing exclusive USB mode.
 gpio_init(22);gpio_set_dir(22,GPIO_IN);gpio_pull_down(22);
 bool atari=false;uint32_t until=time_us_32()+100000;
 while((int32_t)(time_us_32()-until)<0){if(gpio_get(22)){atari=true;break;}}
 gpio_disable_pulls(22);
 if(!atari){static_assert(sizeof cart_words>=61440);flash_fs_set_usb_buffer(cart_words);usb_disk_mode();}
 gpio_init(26);gpio_init(27);sio_hw->gpio_clr=RD4|RD5;sio_hw->gpio_oe_set=RD4|RD5;
 for(unsigned i=0;i<=25;++i){gpio_init(i);gpio_disable_pulls(i);}
 if(!set_sys_clock_khz(250000,false))for(;;)tight_loop_contents();
 static App app;static Diagnostic diagnostic;
 // Publish a valid empty menu before disk scanning; Atari can boot immediately.
 App::draw_title(screens);
 for(unsigned i=0;i<256;++i)bus_words[i]=RD5;
 bus_words[0xb1]=RD5;
 for(unsigned b=0;b<2;++b){for(unsigned i=0;i<8192;++i)cart_words[b][i]=RD5|(uint32_t(i>=0x1ffa?atari::vectors[i-0x1ffa]:(i>=0x1000&&i<0x1000+sizeof atari::boot?atari::boot[i-0x1000]:0))<<13);prepare_screen(b,6);}
 memset(input,255,sizeof input);memset(staged,255,sizeof staged);
 multicore_launch_core1(com_entry);__dmb();multicore_fifo_push_blocking(1);
 (void)save_and_disable_interrupts();
 app.init();
 uint32_t ticket=1,last_packet=0;
 uint32_t out_start=0,out_previous=0;
 bool dirty=false;
 for(;;){
  service_boot_program();
  // PCM has priority over starting another GB frame. Never wait for video ACK.
  pump_audio(app);
  bool fresh_packet=acquire(&packet_seq)!=last_packet;
  bool diagnostic_on=app.debug&&app.active&&!app.in_options&&!app.in_help&&!app.in_states&&!app.paused;
  uint32_t begin=diagnostic_on?time_us_32():0,gb_before=diagnostic_on?engine_frames:0;
  bool worked=false;
  if(fresh_packet){
   uint8_t p[controls::packet_size];uint32_t before,after;
   do{before=acquire(&packet_seq);for(unsigned i=0;i<sizeof p;++i)p[i]=__atomic_load_n(input+i,__ATOMIC_RELAXED);__dmb();after=acquire(&packet_seq);}while((before&1)||before!=after);
   last_packet=after;
   if(diagnostic.boot_option==256)diagnostic.boot_option=p[controls::Option];
   if(diagnostic_on)diagnostic.link.observe(after,p[controls::Frame],begin,true);
   else diagnostic.link.valid=false;
   app.tv_hz=acquire(&atari_hz);
   app.frame(p,screens,time_us_32());dirty=worked=true;
  }else if(app.advance(time_us_32())){
   app.render_game(screens,time_us_32());dirty=worked=true;
  }
  pump_audio(app);
  release(&playing,app.active&&!app.in_options&&!app.in_help&&!app.in_states&&!app.paused);
  diagnostic_on=diagnostic_on&&app.debug&&app.active&&!app.in_options&&!app.in_help&&!app.in_states&&!app.paused;
  if(dirty&&acquire(&ack)==ticket-1){
   uint32_t now=time_us_32();
   if(app.fps.enabled||app.debug){
    uint32_t delivered=acquire(&presented_frames);
    if(!out_start){out_start=now;out_previous=delivered;app.output_fps=0;}
    else if(now-out_start>=1000000u){app.output_fps=uint64_t(delivered-out_previous)*10000000u/(now-out_start);out_start=now;out_previous=delivered;}
   }else out_start=0;
   app.finish_frame(screens,now);
   diagnostic.draw(screens,app.options.height,diagnostic_on?now:0,app.tv_hz,input[controls::Option],diagnostic_on);
   prepare_screen(ticket&1,app.screen_mode());
   image_generation[ticket&1]=engine_frames;
   release(&pending,ticket);
   dirty=false;
   if(app.pending_operation){
    // Show the complete SAVING/options image before flash IO.
    while(acquire(&ack)!=ticket)tight_loop_contents();
    uint32_t shown=acquire(&frame_count);
    while(acquire(&frame_count)==shown)tight_loop_contents();
    if(app.pending_operation==8){
     app.cadence.reset();release(&audio_epoch,app.cadence.epoch);release(&audio_playing,0);
     bool ok=switch_clock();clock_turbo=turbo;
     if(!ok)settings.turbo=clock_turbo;
     app.pending_operation=0;
     // Do not catch up emulated frames/audio after the deliberate blanking gap.
     app.cadence.reset();app.fps.reset(time_us_32());app.can_emulate=false;
     diagnostic.start=0;diagnostic.link.valid=false;out_start=0;
     osd_show(ok?(turbo?"TURBO 300 MHz":"STANDARD 250 MHz"):"CLOCK CHANGE FAILED",time_us_32());
    }else {app.perform_pending(time_us_32());osd_restart(time_us_32());}
   }
   ++ticket;
  }
  if(diagnostic_on&&worked){
   uint32_t elapsed=time_us_32()-begin;
   release(&render_us,elapsed);if(elapsed>acquire(&max_render_us))release(&max_render_us,elapsed);
   if(app.active&&elapsed>1000000u/acquire(&atari_hz))release(&late_count,acquire(&late_count)+1);
   release(&diag_error,engine_error);release(&diag_frames,engine_frames);
   for(unsigned i=0x40;i<0x60;++i)__atomic_store_n(bus_words+i,RD5|(uint32_t(diag_byte(i))<<13),__ATOMIC_RELAXED);
   uint32_t completed=engine_frames>=gb_before?engine_frames-gb_before:0;
   diagnostic.sample(elapsed,0,completed<=1?completed:0);
  }
 }
}
