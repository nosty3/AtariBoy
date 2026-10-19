// Peanut-GB integration; all mutable emulator state belongs to EXE.
#include "audio.h"
#define ENABLE_SOUND 1
#define ENABLE_LCD 1
#define PEANUT_GB_12_COLOUR 0
#include "vendor/peanut_gb.h"
#include "engine.h"
#include <setjmp.h>
#include <string.h>
#include <stdlib.h>
#include "storage.h"
struct Cartridge {uint16_t *map;uint32_t size;uint8_t *ram;uint32_t ram_size;};
static struct Cartridge cart;
static struct gb_s emulator_state;
static struct gb_s *state;
// Fixed cartridge RAM: no heap growth/rounding or game-order dependency.
static uint8_t cartridge_ram[32768];
static char message[40];
const char *engine_message(void){return message;}
static jmp_buf escape;
uint8_t gb_screen[144*40];
uint32_t engine_error,engine_error_addr,engine_frames;
static uint8_t read_rom(struct gb_s *g,uint_fast32_t a){struct Cartridge *c=g->direct.priv;a&=c->size-1;return *(const uint8_t *)(uintptr_t)(0x10100000u+((uint32_t)c->map[a>>9]<<9)+(a&511));}
static uint8_t read_ram(struct gb_s *g,uint_fast32_t a){struct Cartridge *c=g->direct.priv;return c->ram_size?c->ram[a&(c->ram_size-1)]:255;}
static void write_ram(struct gb_s *g,uint_fast32_t a,uint8_t v){struct Cartridge *c=g->direct.priv;if(c->ram_size)c->ram[a&(c->ram_size-1)]=v;}
static void error(struct gb_s *g,enum gb_error_e e,uint16_t a){(void)g;engine_error=(uint32_t)e+1;engine_error_addr=a;longjmp(escape,1);}
static void line(struct gb_s *g,const uint8_t *pixels,const uint_fast8_t y){
 (void)g;if(y>=144)return;
 uint8_t *d=gb_screen+y*40;
 // DMG shade 0 = white, 3 = black. Atari palette uses 0=black, 3=white.
 for(unsigned x=0;x<160;x+=4)*d++=(uint8_t)((((pixels[x]&3)<<6)|((pixels[x+1]&3)<<4)|((pixels[x+2]&3)<<2)|(pixels[x+3]&3))^255);
}
// Validate immutable header bytes before modifying a paused game. These are
// the checks in pinned gb_init, plus our DMG/size restrictions. No second gb_s.
static const char *check_header(const uint8_t *h,uint32_t size){
 if(h[0x143]==0xc0)return "Game Boy Color only";
 if(h[0x148]>8 || (32768u<<h[0x148])!=size || h[0x149]>5)return "Invalid ROM/RAM header";
 unsigned t=h[0x147];
 if(!(t<=3||t==5||t==6||t==8||t==9||(t>=15&&t<=19)||(t>=25&&t<=27)))return "Unsupported cartridge type";
 uint8_t sum=0;for(unsigned i=0x134;i<=0x14c;++i)sum=sum-h[i]-1;
 if(sum!=h[0x14d])return "Invalid ROM checksum/header";
 return 0;
}
int engine_start(const char *path){
 struct Cartridge next={0};uint8_t header[512];
 if(!storage_rom(path,&next.map,&next.size,header)){strcpy(message,storage_error());return 0;}
 const char *why=check_header(header,next.size);
 if(why)goto fail;
 static const uint32_t ram_sizes[]={0,2048,8192,32768,131072,65536};
 next.ram_size=(header[0x147]==5||header[0x147]==6)?512:ram_sizes[header[0x149]];
 // Reject unsupported RAM sizes before altering the paused game.
 if(next.ram_size>sizeof cartridge_ram){why="Cartridge RAM exceeds 32 KiB";goto fail;}
 // Commit only after validation and ROM-map allocation succeed.
 next.ram=cartridge_ram;free(cart.map);cart=next;state=&emulator_state;
 if(cart.ram_size)memset(cart.ram,255,cart.ram_size);
 memset(state,0,sizeof *state);
 return engine_reset();
fail:
 free(next.map);strcpy(message,why);return 0;
}
int engine_reset(void){
 if(!state||!cart.map)return 0;
 union cart_rtc real=state->rtc_real,latched=state->rtc_latched;
 uint32_t rtc_count=state->counter.rtc_count;
 audio_reset();
 memset(state,0,sizeof *state);
 gb_init(state,read_rom,read_ram,write_ram,error,&cart); // Header already checked.
 engine_error=engine_error_addr=engine_frames=0;message[0]=0;
 state->rtc_real=real;state->rtc_latched=latched;state->counter.rtc_count=rtc_count;
 memset(gb_screen,255,sizeof gb_screen);gb_init_lcd(state,line);return 1;
}
int engine_frame(uint8_t joypad){
 if(engine_error)return 0;
 if(setjmp(escape))return 0;
 audio_begin();state->direct.joypad=joypad;gb_run_frame(state);audio_end();++engine_frames;
 if(!(state->hram_io[IO_LCDC]&0x80))memset(gb_screen,255,sizeof gb_screen);
 return 1;
}
size_t engine_state_size(void){return sizeof(struct gb_s);}

// Portable emulator payload codec, pinned Peanut-GB 8e656982. Never serialize
// callbacks, host pointers, bitfield storage, padding or uint_fast* ABI widths.
#include "sha256.h"
uint32_t engine_rom_size(void){return cart.size;}
uint32_t engine_ram_size(void){return cart.ram_size;}
void engine_rom_hash(uint8_t hash[32]){
 Sha256 h;sha256_init(&h);
 for(uint32_t i=0;i<cart.size/512;++i)
  sha256_update(&h,(const void *)(uintptr_t)(0x10100000u+((uint32_t)cart.map[i]<<9)),512);
 sha256_final(&h,hash);
}
static int scalar(engine_io io,void *c,uint32_t *v,int load){
 uint8_t b[4];for(unsigned i=0;i<4;++i)b[i]=*v>>(i*8);
 if(!io(c,b,4))return 0;
 if(load)*v=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
 return 1;
}
static int legacy_load;
void engine_set_legacy_load(int value){legacy_load=value;}
int engine_transfer(engine_io io,void *c,int load){
 if(!state)return 0;
 uint32_t v;
#define FIELD(f) do{v=state->f;if(!scalar(io,c,&v,load))return 0;if(load)state->f=v;}while(0)
 FIELD(gb_halt);
 FIELD(gb_ime);
 FIELD(gb_frame);
 FIELD(lcd_blank);
 FIELD(cart_is_mbc3O);
 FIELD(selected_rom_bank);
 FIELD(cart_ram_bank);
 FIELD(enable_cart_ram);
 FIELD(cart_mode_select);
 FIELD(cpu_reg.f.reg);
 FIELD(cpu_reg.a);
 FIELD(cpu_reg.bc.reg);
 FIELD(cpu_reg.de.reg);
 FIELD(cpu_reg.hl.reg);
 FIELD(cpu_reg.sp.reg);
 FIELD(cpu_reg.pc.reg);
 FIELD(counter.lcd_count);
 FIELD(counter.div_count);
 FIELD(counter.tima_count);
 FIELD(counter.serial_count);
 FIELD(counter.rtc_count);
 FIELD(counter.lcd_off_count);
 FIELD(display.window_clear);
 FIELD(display.WY);
 FIELD(display.frame_skip_count);
 FIELD(display.interlace_count);
 FIELD(direct.interlace);
 FIELD(direct.frame_skip);
 FIELD(direct.joypad);
#undef FIELD
 if(!io(c,state->rtc_latched.bytes,sizeof state->rtc_latched.bytes))return 0;
 if(!io(c,state->rtc_real.bytes,sizeof state->rtc_real.bytes))return 0;
 if(!io(c,state->wram,sizeof state->wram))return 0;
 if(!io(c,state->vram,sizeof state->vram))return 0;
 if(!io(c,state->oam,sizeof state->oam))return 0;
 if(!io(c,state->hram_io,sizeof state->hram_io))return 0;
 if(!io(c,state->display.bg_palette,sizeof state->display.bg_palette))return 0;
 if(!io(c,state->display.sp_palette,sizeof state->display.sp_palette))return 0;
 if(cart.ram_size&&!io(c,cart.ram,cart.ram_size))return 0;
 if(!io(c,gb_screen,sizeof gb_screen))return 0;
 v=engine_frames;if(!scalar(io,c,&v,load))return 0;
 if(load){engine_frames=v;engine_error=engine_error_addr=0;}
 if(load&&legacy_load){audio_restore_legacy(state->hram_io);legacy_load=0;return 1;}
 return audio_transfer(io,c,load);
}
uint32_t engine_payload_size(void){return AUDIO_STATE_BYTES+120+sizeof state->rtc_latched.bytes+sizeof state->rtc_real.bytes+sizeof state->wram+sizeof state->vram+sizeof state->oam+sizeof state->hram_io+sizeof state->display.bg_palette+sizeof state->display.sp_palette+sizeof gb_screen+cart.ram_size;}

// Battery-backed RAM and RTC only: independent of CPU/VRAM/APU save states.
uint32_t engine_battery_size(void){
 if(!state||!cart.map)return 0;
 unsigned type=read_rom(state,0x147);
 if(!(type==3||type==6||type==9||type==15||type==16||type==19||type==27))return 0;
 return cart.ram_size+sizeof state->rtc_real.bytes+4;
}
int engine_battery_transfer(engine_io io,void *context,int loading){
 if(!engine_battery_size())return 0;
 if(cart.ram_size&&!io(context,cart.ram,cart.ram_size))return 0;
 if(!io(context,state->rtc_real.bytes,sizeof state->rtc_real.bytes))return 0;
 uint32_t count=state->counter.rtc_count;
 if(!scalar(io,context,&count,loading))return 0;
 if(loading){state->counter.rtc_count=count;state->rtc_latched=state->rtc_real;}
 return 1;
}

void engine_stop(void){free(cart.map);memset(&emulator_state,0,sizeof emulator_state);memset(&cart,0,sizeof cart);state=0;memset(cartridge_ram,0,sizeof cartridge_ram);memset(gb_screen,0,sizeof gb_screen);audio_reset();}
