#include <sys/mman.h>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <initializer_list>
#include <vector>
#include <string>
#include <algorithm>
#include "saves.h"
#include "osd.hpp"
extern "C" {
#include "sha256.h"
}
#include "app.hpp"
#include "engine.h"
#include "audio.h"
#include "storage.h"
extern "C" {
#include "storage/fatfs_disk.h"
#include "storage/fatfs/ff.h"
void flash_range_erase(uint32_t o,size_t n){assert(o>=0x100000&&o+n<=0x1000000&&!(o%4096)&&!(n%4096));memset((void *)(uintptr_t)(0x10000000+o),255,n);}
void flash_range_program(uint32_t o,const uint8_t *p,size_t n){assert(o>=0x100000&&o+n<=0x1000000&&!(o%256)&&!(n%256));auto d=(uint8_t *)(uintptr_t)(0x10000000+o);for(size_t i=0;i<n;++i){assert((d[i]&p[i])==p[i]);d[i]=p[i];}}
static uint32_t now;
uint32_t time_us_32(void){return now;}
void tud_msc_set_sense(uint8_t,uint8_t,uint8_t,uint8_t){}
void msc_flush_task(void);
void tud_msc_capacity_cb(uint8_t,uint32_t*,uint16_t*);
int32_t tud_msc_read10_cb(uint8_t,uint32_t,uint32_t,void*,uint32_t);
int32_t tud_msc_write10_cb(uint8_t,uint32_t,uint32_t,uint8_t*,uint32_t);
int32_t tud_msc_scsi_cb(uint8_t,const uint8_t*,void*,uint16_t);
bool tud_msc_start_stop_cb(uint8_t,uint8_t,bool,bool);

}
static unsigned icon_opens,icon_reads,icon_bytes,icon_dirs,icon_hashes;
extern "C" {
FRESULT __real_f_open(FIL*,const TCHAR*,BYTE);
FRESULT __wrap_f_open(FIL *f,const TCHAR *p,BYTE m){++icon_opens;return __real_f_open(f,p,m);}
FRESULT __real_f_read(FIL*,void*,UINT,UINT*);
FRESULT __wrap_f_read(FIL *f,void *b,UINT n,UINT *got){++icon_reads;auto r=__real_f_read(f,b,n,got);icon_bytes+=*got;return r;}
FRESULT __real_f_opendir(DIR*,const TCHAR*);
FRESULT __wrap_f_opendir(DIR *d,const TCHAR *p){++icon_dirs;return __real_f_opendir(d,p);}
void __real_sha256_update(Sha256*,const void*,size_t);
void __wrap_sha256_update(Sha256 *s,const void *p,size_t n){++icon_hashes;__real_sha256_update(s,p,n);}
}
static FILE *artifact(const char *name){char path[2048];snprintf(path,sizeof path,"%s/%s",getenv("ATARIBOY_TEST_OUTPUT"),name);return fopen(path,"wb");}
static void writefile(const char *path,const uint8_t *data,unsigned n){FIL f;UINT wrote;assert(f_open(&f,path,FA_CREATE_ALWAYS|FA_WRITE)==FR_OK);assert(f_write(&f,data,n,&wrote)==FR_OK&&wrote==n);assert(f_close(&f)==FR_OK);}
struct Snapshot {std::vector<uint8_t> bytes;size_t pos=0;};
static int capture(void *v,void *p,size_t n){auto &s=*(Snapshot*)v;auto b=(uint8_t*)p;s.bytes.insert(s.bytes.end(),b,b+n);return 1;}
static int restore(void *v,void *p,size_t n){auto &s=*(Snapshot*)v;assert(s.pos+n<=s.bytes.size());memcpy(p,s.bytes.data()+s.pos,n);s.pos+=n;return 1;}
static Snapshot snapshot(){Snapshot s;assert(engine_transfer(capture,&s,0));assert(s.bytes.size()==engine_payload_size());return s;}
static Snapshot battery_snapshot(){Snapshot s;assert(engine_battery_transfer(capture,&s,0));return s;}
static std::vector<uint8_t> rom_fixture(unsigned variant){
 std::vector<uint8_t> r(32768,0);r[0x100]=0xc3;r[0x101]=0;r[0x102]=1;
 r[0x147]=3;r[0x148]=0;r[0x149]=2;r[0x200]=variant;
 for(unsigned i=0x134;i<=0x14c;++i)r[0x14d]=r[0x14d]-r[i]-1;
 return r;
}
static std::string game_options_path(){
 char path[384],h[65];for(unsigned i=0;i<32;++i)sprintf(h+2*i,"%02x",saves_game_hash()[i]);
 snprintf(path,sizeof path,"/.options/%s/%s.cfg",saves_game_name(),h);return path;
}
int main(){
 auto flash=mmap((void*)0x10000000,16*1024*1024,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0);assert(flash!=(void*)-1);memset(flash,255,16*1024*1024);
 static uint32_t borrowed[16384];flash_fs_set_usb_buffer(borrowed);flash_fs_set_writable(true);create_fatfs_disk();assert(mount_fatfs_disk()&&storage_mount());
 assert(f_mkdir("/A")==FR_OK&&f_mkdir("/B")==FR_OK);
 auto rom=rom_fixture(1);writefile("/A/Same.gb",rom.data(),rom.size());rom=rom_fixture(2);writefile("/B/Same.gb",rom.data(),rom.size());
 for(unsigned i=0;i<25;++i){char name[80];snprintf(name,sizeof name,"/A/Long game title number %02u abcdefghijklmnopqrstuvwxyz.gb",i);writefile(name,rom.data(),rom.size());}
 fatfs_disk_sync();flash_fs_set_writable(false);

 // Every startup uses hardware, independently of persisted preferences.
 uint8_t boot_input[24]={15,1,228,228,15,1,228,228,255,0,255,0,0,0,0,255,255,255,228,228,228,228,0,0};
 uint8_t boot_screen[5840];
 for(unsigned hardware=0;hardware<2;++hardware){
  detected_stereo=255;App setup;setup.init();setup.frame(boot_input,boot_screen,0);
  assert(setup.title&&!setup.pending_operation);
  detected_stereo=hardware;setup.frame(boot_input,boot_screen,20000);
  assert(settings.audio_stereo==hardware&&audio_stereo==hardware&&!setup.pending_operation);
  // Build an old-format record containing the opposite hardware selection.
  uint8_t record[256];Settings old;old.audio_stereo=1-hardware;
  unsigned n=settings_encode(old,record);unsigned payload=record[8];
  record[10+payload]=33;record[11+payload]=1;record[12+payload]=1-hardware;record[8]+=3;n+=3;
  uint32_t crc=~0u;for(unsigned i=0;i<n-4;++i){crc^=record[i];for(unsigned j=0;j<8;++j)crc=(crc>>1)^(0xedb88320u&-(crc&1));}crc=~crc;
  for(unsigned i=0;i<4;++i)record[n-4+i]=crc>>(8*i);
  flash_fs_begin_atari_write();f_mkdir("/.options");writefile("/.options/global.cfg",record,n);flash_fs_end_atari_write();
  settings_load();assert(settings.audio_stereo==hardware&&audio_stereo==hardware);
  assert(settings_save()==SAVE_OK);detected_stereo=1-hardware;settings_load();assert(settings.audio_stereo==1-hardware&&audio_stereo==1-hardware);
 }
 detected_stereo=0;
 puts("PASS RC3 hardware stereo/mono every startup, unknown waits, legacy global output ignored, moving hardware changes output");
 App app;app.init();app.title=false;
 assert(settings.hue==0&&settings.picture==3);
 settings.hue=4;settings.volume=9;settings.boot_last=1;assert(settings_save()==SAVE_OK);
 settings_defaults();settings_load();assert(settings.hue==4&&settings.volume==9&&settings.boot_last==1);
 assert(app.start_game("/A/Same.gb",0));app.perform_pending(0);assert(settings.hue==4);
 settings.hue=8;settings.sound=0;settings.boot_last=0;assert(settings_save_game()==SAVE_OK);assert(settings.boot_last==1);
 std::string first_path=game_options_path();
 settings_load();assert(settings.hue==4&&settings.sound==1);
 assert(settings_load_game()==SAVE_OK&&settings.hue==8&&settings.sound==0&&settings.boot_last==1);
 assert(app.start_game("/B/Same.gb",0));app.perform_pending(0);assert(settings.hue==4&&settings.sound==1);
 settings.hue=12;assert(settings_save_game()==SAVE_OK);assert(game_options_path()!=first_path);
 assert(app.start_game("/A/Same.gb",0));app.perform_pending(0);assert(settings.hue==8&&settings.sound==0);
 // Incomplete valid game record inherits global fields; wrong CRC is rejected.
 uint8_t cfg[256];Settings partial;partial.hue=6;settings_encode(partial,cfg);cfg[8]=3;uint32_t crc=~0u;
 for(unsigned i=0;i<13;++i){crc^=cfg[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&-(crc&1));}crc=~crc;
 for(unsigned i=0;i<4;++i)cfg[13+i]=crc>>(8*i);
 Settings decoded;assert(settings_decode(cfg,17,decoded,&global_settings)&&decoded.hue==6&&decoded.volume==9);
 cfg[12]^=1;assert(!settings_decode(cfg,17,decoded,&global_settings));
 puts("PASS global/game settings, missing fields, CRC, independent same-name ROM configurations");
 // Runtime hotkeys survive entry to options; Esc restores the runtime snapshot.
 uint8_t p[24]={15,1,228,228,15,1,228,228,255,0,255,0,0,0,0,255,255,255,228,228,228,228,0,0};uint8_t screen[5840];
 auto frame=[&](){++p[23];now+=20000;app.frame(p,screen,now);};
 auto key=[&](unsigned k){p[10]=k;frame();p[10]=255;frame();};
 auto option=[&](){p[14]=0;frame();p[14]=1;frame();p[14]=0;frame();};
 app.paused=true;
 // 0.75a bindings: press edges only; no joystick-driven view changes.
 {using namespace controls;Settings defaults;assert(defaults.audio_quality==1&&defaults.audio_effects==6);
  Settings readback;uint8_t encoded[256];defaults.audio_quality=0;
  auto n=settings_encode(defaults,encoded);assert(settings_decode(encoded,n,readback)&&readback.audio_quality==0);
  uint8_t q[24];memcpy(q,p,24);Mapping m;Decoder d;
  for(unsigned j:{14u,13u,11u,7u}){q[Joy1]=j;auto v=d.read(q,m);assert(!v.change_view&&!v.view_up&&!v.view_down);}
  q[Joy1]=15;q[HeldKey]=6;assert(d.read(q,m).view_step==-1);assert(!d.read(q,m).change_view);
  q[HeldKey]=7;assert(d.read(q,m).view_step==1);q[HeldKey]=14;assert(d.read(q,m).view_up);
  q[HeldKey]=15;assert(d.read(q,m).view_down);q[HeldKey]=255;
  for(unsigned port=0;port<2;++port)for(unsigned source=0;source<4;++source){
   m=Mapping{};m.port[port].type=Joy2B;
   if(source==3)m.port[port].up=LoadState;else m.port[port].fire[source]=LoadState;
   d=Decoder{};q[0]=q[4]=15;q[1]=q[5]=1;q[2]=q[3]=q[6]=q[7]=0;d.update(q);d.read(q,m);
   unsigned offset=port*4+(source==3?0:source+1);q[offset]=source==0?0:source==3?14:228;d.update(q);
   assert(d.read(q,m).load_state);assert(!d.read(q,m).load_state);
   q[offset]=source==0?1:source==3?15:0;d.update(q);assert(!d.read(q,m).load_state);
  }
  m=Mapping{};m.space=LoadState;q[HeldKey]=0x21;assert(d.read(q,m).load_state);assert(!d.read(q,m).load_state);
  q[HeldKey]=255;assert(!d.read(q,m).load_state);
  auto original=settings.controls;settings.controls.space=LoadState;app.suppress=false;frame();
  key(0x21);assert(app.pending_operation==2);app.pending_operation=0;
  settings.controls.port[0].fire[0]=LoadState;p[Fire0]=0;frame();assert(app.pending_operation==2);app.pending_operation=0;
  frame();assert(!app.pending_operation);p[Fire0]=1;frame();assert(!app.pending_operation);
  settings.controls=original;
  puts("PASS 0.75a Quality default/Fast persisted, keyboard-only view, mapped load edges and App load operation");
 }
 // Official T: one request per press, no CTRL, runtime state is separate.
 assert(!settings.turbo&&!clock_turbo);
 p[17]=0x2d;p[10]=0x2d;frame();assert(app.pending_operation==8&&settings.turbo==1);
 clock_turbo=1;app.pending_operation=0;
 for(int i=0;i<5;++i)frame();assert(!app.pending_operation&&settings.turbo==1);
 p[10]=255;frame();assert(!app.pending_operation);
 key(0x2d);assert(app.pending_operation==8&&settings.turbo==0);
 clock_turbo=0;app.pending_operation=0;p[17]=255;
 // Options defer the clock transaction until GAME loop resumes. Esc cancels.
 option();assert(app.in_options);settings.turbo=1;frame();assert(!app.pending_operation);
 key(0x1c);assert(!app.in_options&&!settings.turbo&&!app.pending_operation);
 option();settings.turbo=1;key(0x3e);assert(app.pending_operation==3);
 app.perform_pending(now);assert(app.in_options);key(0x1c);assert(!app.in_options);
 frame();assert(app.pending_operation==8);
 clock_turbo=1;app.pending_operation=0;
 settings_defaults();assert(settings_load_game()==SAVE_OK&&settings.turbo==1);
 // Restore 250 for remaining tests, using the same safe request path.
 key(0x2d);assert(app.pending_operation==8&&!settings.turbo);
 clock_turbo=0;app.pending_operation=0;assert(settings_save_game()==SAVE_OK);
 puts("PASS T edge/debounce, paused clock request, options defer/escape/save, game persistence");
 unsigned mode=settings.audio_quality;key(0x23);assert(settings.audio_quality==(mode^1)&&audio_quality==int(mode^1));
 p[10]=0x23;frame();unsigned held_mode=settings.audio_quality;for(int i=0;i<8;++i)frame();assert(settings.audio_quality==held_mode);p[10]=255;frame();
 assert(settings.audio_quality==mode); // second fresh N restored initial mode
 key(0x25);assert(settings.sound==1);settings.hue=0;key(0x12);assert(settings.hue==15);key(0x10);assert(settings.hue==0);
 option();assert(app.in_options&&settings.sound==1&&settings.hue==0);
 key(0x00);assert(settings.hue==8&&settings.sound==0);key(0x1c);assert(!app.in_options&&settings.sound==1&&settings.hue==0);
 option();key(0x3d);assert(settings.hue==4);settings.hue=7;option();assert(app.pending_operation==6);
 app.perform_pending(now);assert(!app.in_options&&global_settings.hue==7&&settings.hue==7);
 option();assert(settings.hue==7);settings.hue=11;key(0x1c);assert(settings.hue==7);
 option();settings.audio_quality=1;settings_apply_audio();settings.hue=9;
 key(0x3e);assert(app.pending_operation==3);app.perform_pending(now);assert(app.in_options);
 settings.hue=11;key(0x1c);assert(!app.in_options&&settings.hue==9);
 settings_load();assert(settings.hue==7);assert(settings_load_game()==SAVE_OK&&settings.hue==9&&settings.audio_quality==1);
 detected_stereo=1;assert(settings_load_game()==SAVE_OK&&settings.audio_stereo==1&&audio_stereo==1);detected_stereo=0;assert(settings_load_game()==SAVE_OK&&settings.audio_stereo==0&&audio_stereo==0);
 {Settings a,b;uint8_t record[256];a.audio_stereo=1;a.turbo=1;unsigned size=settings_encode(a,record);assert(settings_decode(record,size,b)&&b.audio_stereo==0&&b.turbo==1);assert(size==95);
   OptionsUI stereo;stereo.tab=1;stereo.row=7;assert(stereo.fields()==7);
   uint8_t inputs[24];memset(inputs,255,24);inputs[controls::Joy0]=15;inputs[controls::HeldKey]=0x0f;stereo.update(inputs,false);assert(stereo.row==7);}
 // Removed diagnostic shortcuts must leave the processing mode unchanged; N remains.
 for(unsigned k:{0x2fu,0x2eu,0x2au}){unsigned effects=settings.audio_effects;unsigned quality=settings.audio_quality;
  p[controls::Kbcode]=k|0x80;key(k);assert(settings.audio_effects==effects&&settings.audio_quality==quality&&audio_effects==effects);}
 p[controls::Kbcode]=0x23;unsigned old_quality=settings.audio_quality;key(0x23);assert(settings.audio_quality==(old_quality^1)&&audio_effects==(settings.audio_quality?6:4));key(0x23);
 puts("PASS RC3 Sound navigation read-only detection, Ctrl+Q/W/E inactive, N toggles Fast/Quality");
 OptionsUI ui;ui.tab=1;ui.draw(screen);{FILE *sound=artifact("options-sound.bin");fwrite(screen,1,960,sound);fclose(sound);}
 ui.tab=2;assert(ui.fields()==3);ui.draw(screen);{FILE *emu=artifact("options-emu.bin");fwrite(screen,1,960,emu);fclose(emu);}
 ui.tab=5;ui.draw(screen);FILE *f=artifact("options-info.bin");fwrite(screen,1,960,f);fclose(f);
 OptionsUI::help(screen);f=artifact("help-screen.bin");fwrite(screen,1,960,f);fclose(f);
 App::draw_title(screen);f=artifact("title-screen.bin");fwrite(screen,1,3200,f);fclose(f);
 puts("PASS C/V wrap, hotkeys -> Options, L/G/S/Option/Esc semantics and global-only boot flag");
 // RAM persistence, restart, state-load independence and full hash isolation.
 assert(engine_battery_size()>8192);assert(battery_load()==SAVE_EMPTY);
 auto data=battery_snapshot();data.bytes[17]=0x42;data.bytes[8191]=0xa5;assert(engine_battery_transfer(restore,&data,1));
 assert(battery_save()==SAVE_OK);assert(battery_save()==SAVE_EMPTY);
 assert(engine_start("/A/Same.gb"));saves_select_game("/A/Same.gb");assert(battery_load()==SAVE_OK);assert(battery_snapshot().bytes==data.bytes);
 assert(saves_write()==SAVE_OK);auto other=data;other.pos=0;other.bytes[17]=0x53;assert(engine_battery_transfer(restore,&other,1));assert(battery_save()==SAVE_OK);
 assert(saves_load()==SAVE_OK);assert(battery_snapshot().bytes==data.bytes);assert(battery_save()==SAVE_OK);
 assert(engine_start("/B/Same.gb"));saves_select_game("/B/Same.gb");assert(battery_load()==SAVE_EMPTY);assert(battery_snapshot().bytes[17]==255);
 assert(engine_start("/A/Same.gb"));saves_select_game("/A/Same.gb");assert(battery_load()==SAVE_OK&&battery_snapshot().bytes==data.bytes);
 // Remount represents filesystem startup after power loss; no in-memory file handles retained.
 assert(mount_fatfs_disk()&&storage_mount());settings_load();assert(settings.hue==7);assert(settings_load_game()==SAVE_OK&&settings.hue==9&&settings.audio_quality==1);
 detected_stereo=1;assert(settings_load_game()==SAVE_OK&&settings.audio_stereo==1&&audio_stereo==1);detected_stereo=0;assert(settings_load_game()==SAVE_OK&&settings.audio_stereo==0&&audio_stereo==0);assert(battery_load()==SAVE_OK);
 // Corrupt battery payload: validation must leave RAM completely untouched.
 char hash[65],battery[384];for(unsigned i=0;i<32;++i)sprintf(hash+2*i,"%02x",saves_game_hash()[i]);snprintf(battery,sizeof battery,"/.saves/Same/battery-%s.sav",hash);
 flash_fs_begin_atari_write();FIL bf;UINT got;assert(f_open(&bf,battery,FA_READ|FA_WRITE)==FR_OK);assert(f_lseek(&bf,80)==FR_OK);uint8_t byte;assert(f_read(&bf,&byte,1,&got)==FR_OK);byte^=1;assert(f_lseek(&bf,80)==FR_OK);assert(f_write(&bf,&byte,1,&got)==FR_OK);f_close(&bf);flash_fs_end_atari_write();
 auto untouched=battery_snapshot();assert(battery_load()==SAVE_INVALID&&battery_snapshot().bytes==untouched.bytes);
 puts("PASS battery RAM roundtrip, no-op saves, remount, state compatibility, ROM identity and corruption rejection");
 Browser browser;browser.ready=true;strcpy(browser.path,"/A");browser.scan();
 unsigned opens=icon_opens,reads=icon_reads,dirs=icon_dirs,hashes=icon_hashes;
 for(unsigned i=0;i<100;++i){browser.draw(screen,true);browser.move(i%2?1:-1);}
 assert(icon_opens==opens&&icon_reads==reads&&icon_dirs==dirs&&icon_hashes==hashes);
 while(browser.selected+1<browser.count)browser.move(1);
 auto &e=browser.page[browser.selected-browser.top];assert(!strcmp(e.name,"Same.gb")&&e.has_save&&e.has_options);
 browser.draw(screen,true);unsigned y=7+browser.selected-browser.top;
 assert(screen[y*40]==0x60&&screen[y*40+1]==0x50&&screen[y*40+2]==0);for(unsigned x=3;x<40;++x)assert(screen[y*40+x]&128);
 f=artifact("menu-directory.bin");fwrite(screen,1,960,f);fclose(f);
 puts("PASS browser icon cache (zero filesystem calls per redraw/cursor within page), 4th-column titles and inverse boundaries");
 f=artifact("flash-test.bin");fwrite(flash,1,16*1024*1024,f);fclose(f);

 // New state format: metadata, immutable clipboard, transactional edits and load.
 assert(engine_start("/A/Same.gb"));saves_select_game("/A/Same.gb");
 for(unsigned y=0;y<144;++y)for(unsigned x=0;x<40;++x)gb_screen[y*40+x]=uint8_t(y+x);
 saves_set_slot(3);assert(saves_write()==SAVE_OK);auto frozen=snapshot();
 SaveSlot slots[10];assert(saves_list(slots)==SAVE_OK&&slots[2].present);
 for(unsigned i=0;i<30;++i)assert(slots[2].description[i]==' ');
 uint8_t thumb[896];assert(saves_preview(3,thumb)==SAVE_OK);
 for(unsigned y=0;y<64;++y)for(unsigned x=0;x<53;++x){unsigned sx=x*160/53,sy=y*9/4;assert(((thumb[y*14+x/4]>>(6-2*(x%4)))&3)==((gb_screen[sy*40+sx/4]>>(6-2*(sx%4)))&3));}
 char description[30];memset(description,' ',30);memcpy(description,"before boss",11);
 uint64_t sequence=slots[2].number;assert(saves_describe(3,description)==SAVE_OK);
 assert(saves_list(slots)==SAVE_OK&&slots[2].number==sequence&&!memcmp(slots[2].description,description,30));
 assert(saves_copy(3)==SAVE_OK);assert(saves_delete(3)==SAVE_OK);assert(saves_list(slots)==SAVE_OK&&!slots[2].present);
 assert(saves_paste(7)==SAVE_OK);assert(saves_list(slots)==SAVE_OK&&slots[6].present&&!memcmp(slots[6].description,description,30));
 saves_set_slot(7);assert(saves_load()==SAVE_OK);assert(snapshot().bytes==frozen.bytes);
 assert(saves_paste(7)==SAVE_OK);assert(saves_list(slots)==SAVE_OK&&slots[6].number>sequence);
 saves_clear_clipboard();assert(saves_paste(8)!=SAVE_OK);
 StateUI stateui;stateui.open();assert(stateui.row==6);stateui.draw(screen);
 for(unsigned x=2;x<40;++x)assert(screen[(3+6)*40+x]&128);
 assert(!(screen[(3+6)*40]&128));
 f=artifact("states-screen.bin");fwrite(screen,1,3480,f);fclose(f);
 for(unsigned x=0;x<40;++x)assert(screen[x]&128);
 p[controls::HeldKey]=0x2a;p[controls::Kbcode]=0x2a;stateui.update(p);assert(stateui.editing);
 char unchanged[31];memcpy(unchanged,stateui.edit,31);
 p[controls::HeldKey]=255;stateui.update(p);assert(stateui.cursor==0&&!memcmp(unchanged,stateui.edit,31));
 p[controls::HeldKey]=0x3f;p[controls::Kbcode]=0x7f;stateui.update(p);assert(stateui.edit[0]=='A'&&stateui.cursor==1);
 p[controls::HeldKey]=255;stateui.update(p);assert(stateui.cursor==1);
 p[controls::HeldKey]=0x3f;p[controls::Kbcode]=0x3f;stateui.update(p);assert(stateui.edit[1]=='a'&&stateui.cursor==2);
 p[controls::HeldKey]=255;stateui.update(p);assert(stateui.cursor==2);
 memcpy(unchanged,stateui.edit,31);
 for(unsigned key: {6u,7u}){p[controls::HeldKey]=p[controls::Kbcode]=key;stateui.update(p);unsigned cursor=stateui.cursor;p[controls::HeldKey]=255;stateui.update(p);assert(stateui.cursor==cursor&&!memcmp(unchanged,stateui.edit,31));}
 p[controls::HeldKey]=p[controls::Kbcode]=0x3d;stateui.update(p);assert(stateui.edit[2]=='g');
 p[controls::HeldKey]=255;stateui.update(p);assert(stateui.cursor==3);
 p[controls::HeldKey]=0x1c;stateui.update(p);assert(!stateui.editing);
 assert(saves_list(slots)==SAVE_OK&&slots[6].description[0]=='b');
 // No framebuffer corruption: independent repeated rows for bottom OSD only.
 uint8_t image[5840],raw[5760];memset(raw,0x55,sizeof raw);
 video::Options v;v.height=144;v.method=video::Method::Full192;
 assert(video::convert(raw,image,v));osd_show("SAVED IN SLOT 03",100);osd_overlay(image,144,100,true);
 uint8_t expected[320];memset(expected,0x55,sizeof expected);osd_text(expected,8,0,"SAVED IN SLOT 03");
 unsigned map[8]={138,139,140,144,141,142,143,145};for(unsigned y=0;y<8;++y)assert(!memcmp(image+map[y]*40,expected+y*40,40));
 puts("PASS state thumbnails, descriptions, immutable clipboard, paste/delete, exact state restore, editor cancel, 192-line OSD");

 app.active=app.loaded=true;app.in_help=app.in_options=app.in_states=false;app.paused=true;app.previous_key=255;
 p[controls::Joy0]=15;p[controls::Option]=0;p[controls::Help]=0;p[controls::Fire0]=1;p[controls::HeldKey]=p[controls::Kbcode]=0x3d;
 app.frame(p,screen,1000);assert(app.in_states&&app.screen_mode()==7&&!app.can_emulate);
 app.frame(p,screen,1100);assert(app.in_states); // holding G must not immediately close
 p[controls::HeldKey]=255;app.frame(p,screen,1200);assert(app.in_states);
 app.state_ui.row=8;settings.advance_slot=1;p[controls::HeldKey]=p[controls::Kbcode]=0x3e;
 app.frame(p,screen,2000);assert(app.pending_operation==7);app.perform_pending(3000);
 assert(app.in_states&&app.state_ui.row==8&&saves_slot()==9);
 p[controls::HeldKey]=p[controls::Kbcode]=0x3d;app.frame(p,screen,4000);assert(!app.in_states&&app.paused);
 assert(saves_copy(9)==SAVE_OK);
 flash_fs_begin_atari_write();{FIL corrupt;UINT n;assert(f_open(&corrupt,"/.saves/Same/clipboard.tmp",FA_WRITE)==FR_OK);assert(f_lseek(&corrupt,90)==FR_OK);uint8_t byte=0xff;assert(f_write(&corrupt,&byte,1,&n)==FR_OK&&n==1);f_close(&corrupt);}flash_fs_end_atari_write();
 assert(saves_paste(9)==SAVE_INVALID);saves_set_slot(9);assert(saves_load()==SAVE_OK);saves_clear_clipboard();
 puts("PASS release retains KBCODE without typing, repeated same letter, arrows, G toggle/title inverse; G/ESC preserves pause, manager S ignores auto-advance, corrupt clipboard does not replace destination");

 // v0.75a: offline optimizer confirmation, busy image, retained game and settings.
 {
  App opt;opt.title=false;opt.active=opt.loaded=opt.in_options=true;
  opt.options_ui.tab=6;opt.options_ui.row=1;
  uint8_t ip[24];memcpy(ip,p,24);ip[controls::Option]=ip[controls::Help]=0;
  ip[controls::HeldKey]=255;ip[controls::Fire0]=1;ip[controls::Joy0]=15;
  opt.frame(ip,screen,4100);
  ip[controls::HeldKey]=0x0c;opt.frame(ip,screen,4200);
  assert(opt.options_ui.confirm&&!opt.options_ui.yes&&!opt.pending_operation);
  ip[controls::HeldKey]=0x07;opt.frame(ip,screen,4300);
  assert(opt.options_ui.yes&&!opt.pending_operation);
  ip[controls::HeldKey]=255;opt.frame(ip,screen,4400);
  ip[controls::Fire0]=0;opt.frame(ip,screen,4500);
  assert(opt.pending_operation==10&&opt.options_ui.optimize&&!opt.can_emulate);
  {FILE *o=artifact("optimize-busy-screen.bin");fwrite(screen,1,960,o);fclose(o);}
  Settings before=settings;opt.perform_pending(4600);
  assert(opt.active&&opt.loaded&&opt.in_options&&!opt.pending_operation&&!opt.options_ui.optimize&&!opt.options_ui.confirm);
  assert(!memcmp(&before,&settings,sizeof settings));assert(!strncmp(opt.options_ui.message,"Done:",5));
  opt.options_ui.draw(screen);
  {FILE *o=artifact("optimize-done-screen.bin");fwrite(screen,1,960,o);fclose(o);}
  puts("PASS Optimize UI: YES+Fire required; busy screen before IO; game and settings preserved; result shown");
 }

 // RC4: destructive reset is inaccessible until explicit YES + new fire edge.
 app.in_states=app.in_help=app.in_options=false;app.pending_operation=0;
 app.active=app.loaded=true;app.prev_option=false;app.option_armed=true;app.prev_fire=false;app.prev_esc=false;app.previous_key=255;
 p[controls::HeldKey]=255;p[controls::Fire0]=1;p[controls::Joy0]=15;p[controls::Help]=0;p[controls::Option]=1;
 app.frame(p,screen,5000);assert(app.in_options);
 app.options_ui.tab=6;app.options_ui.row=2;
 p[controls::Option]=0;p[controls::HeldKey]=0x0c;
 app.frame(p,screen,5100);assert(app.options_ui.confirm&&!app.options_ui.yes&&!app.pending_operation);
 app.frame(p,screen,5200);assert(!app.pending_operation); // held Return
 p[controls::HeldKey]=255;app.frame(p,screen,5300);
 p[controls::HeldKey]=0x0c;app.frame(p,screen,5400);
 assert(!app.options_ui.confirm&&!app.pending_operation); // NO + Return cancels
 p[controls::HeldKey]=255;app.frame(p,screen,5500);
 p[controls::HeldKey]=0x0c;app.frame(p,screen,5600);
 p[controls::HeldKey]=0x1c;app.frame(p,screen,5700);
 assert(app.in_options&&!app.options_ui.confirm&&!app.pending_operation); // Esc cancels dialog
 p[controls::HeldKey]=255;app.frame(p,screen,5800);
 p[controls::HeldKey]=0x0c;app.frame(p,screen,5900);
 p[controls::HeldKey]=0x07;app.frame(p,screen,6000);
 assert(app.options_ui.yes&&!app.pending_operation); // choosing YES alone does nothing
 p[controls::HeldKey]=255;app.frame(p,screen,6100);
 p[controls::Fire0]=0;app.frame(p,screen,6200);
 assert(app.pending_operation==9&&app.options_ui.erase&&!app.can_emulate);
 f=artifact("disk-busy-screen.bin");fwrite(screen,1,960,f);fclose(f);
 memset(flash,0x5a,0x100000); // firmware sentinel
 app.perform_pending(6300);
 assert(!app.active&&!app.loaded&&!app.in_options&&!app.paused&&!app.pending_operation);
 assert(app.browser.ready&&app.browser.count==0&&!strcmp(app.browser.path,"/"));
 assert(!last_game_path[0]&&!app.state_ui.clipboard&&saves_slot()==1);
 for(unsigned i=0;i<0x100000;++i)assert(((uint8_t*)flash)[i]==0x5a);
 assert(mount_fatfs_disk()&&storage_mount());
 {DIR d;FILINFO info;assert(f_opendir(&d,"/")==FR_OK);assert(f_readdir(&d,&info)==FR_OK&&!info.fname[0]);f_closedir(&d);}
 app.browser.draw(screen,false);f=artifact("disk-empty-screen.bin");fwrite(screen,1,960,f);fclose(f);
 // Recovery is reachable even without any game to load.
 p[controls::Fire0]=1;p[controls::HeldKey]=255;p[controls::Option]=0;app.frame(p,screen,6400);
 p[controls::Option]=1;app.frame(p,screen,6500);assert(app.in_options&&app.options_ui.tab==6);
 app.options_ui.draw(screen);f=artifact("disk-screen.bin");fwrite(screen,1,960,f);fclose(f);
 // Files can be written and a game started again after the reset.
 flash_fs_begin_atari_write();writefile("/After.gb",rom.data(),rom.size());flash_fs_end_atari_write();
 assert(engine_start("/After.gb"));assert(engine_frame(255));engine_stop();
 puts("PASS RC4 NO/ESC cancel, YES+Return/Fire confirmation, no early erase, firmware preserved, empty FAT, RAM reset, remount/write/restart, empty-menu Disk access");
 puts("ALL v0.75a TESTS PASSED");
}
