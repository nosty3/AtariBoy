#include "settings.hpp"
#include "audio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
extern "C" {
#include "storage/fatfs/ff.h"
#include "storage/flash_fs.h"
}
Settings settings,global_settings;
uint8_t detected_stereo=255;
uint8_t clock_turbo=0;
char last_game_path[1024];
static FIL file;
static const char path[]="/.options/global.cfg",temp[]="/.options/global.tmp",backup[]="/.options/global.bak";
static uint32_t crc(const uint8_t *p,unsigned n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xedb88320u&-(c&1));}return ~c;}
void settings_defaults(){settings=Settings{};}
void settings_apply_audio(){settings.audio_stereo=__atomic_load_n(&detected_stereo,__ATOMIC_RELAXED)==1;audio_set_stereo(settings.audio_stereo);audio_set_enabled(settings.sound);audio_set_volume(settings.volume);audio_set_channels(settings.channels);audio_set_quality(settings.audio_quality);audio_set_effects(settings.audio_effects);}
unsigned settings_encode(const Settings& in,uint8_t *b){
 const uint8_t values[]={in.hue,in.brightness[0],in.brightness[1],in.brightness[2],in.brightness[3],in.sound,in.volume,in.pal_full,in.load_latest,in.boot_last,
 in.channels,in.picture,in.auto_minutes,in.advance_slot,
 in.controls.port[0].type,in.controls.port[0].fire[0],in.controls.port[0].fire[1],in.controls.port[0].fire[2],in.controls.port[0].up,
 in.controls.port[1].type,in.controls.port[1].fire[0],in.controls.port[1].fire[1],in.controls.port[1].fire[2],in.controls.port[1].up,in.controls.space,in.audio_quality,in.turbo};
 memcpy(b,"ATBOYOP1",8);b[8]=sizeof(values)*3;b[9]=0;
 for(unsigned i=0;i<sizeof(values);++i){b[10+3*i]=i<10?i+1:i==26?34:i+7;b[11+3*i]=1;b[12+3*i]=values[i];}
 unsigned n=10+sizeof(values)*3;uint32_t c=crc(b,n);for(unsigned i=0;i<4;++i)b[n+i]=c>>(8*i);return n+4;
}
bool settings_decode(const uint8_t *b,unsigned n,Settings& out,const Settings *base){
 out=Settings{};
 if(n<14||memcmp(b,"ATBOYOP1",8)||unsigned(b[8]|b[9]<<8)+14!=n)return false;
 uint32_t c=0;for(unsigned i=0;i<4;++i)c|=uint32_t(b[n-4+i])<<(8*i);
 if(c!=crc(b,n-4))return false;
 Settings v=base?*base:Settings{};uint8_t old[6]={0,0,1,0,1,3};bool legacy=false,modern=false;
 for(unsigned p=10;p<n-4;){
  if(p+2>n-4)return false;
  unsigned id=b[p++],len=b[p++];if(p+len>n-4)return false;
  if(len==1){unsigned x=b[p];
   if(id==1&&x<=15)v.hue=x;else if(id>=2&&id<=5&&x<=15)v.brightness[id-2]=x;
   else if(id==6&&x<=1)v.sound=x;else if(id==7&&x<=15)v.volume=x;
   else if(id==8&&x<=1)v.pal_full=x;else if(id==9&&x<=1)v.load_latest=x;else if(id==10&&x<=1)v.boot_last=x;
   else if(id>=11&&id<=16){legacy=true;if(x<=(id==11?1:id<=13?3:4))old[id-11]=x;}
   else if(id==17&&x<=15)v.channels=x;else if(id==18&&x<=3)v.picture=x;
   else if(id==19&&x<=10)v.auto_minutes=x;else if(id==20&&x<=1)v.advance_slot=x;
   else if(id>=21&&id<=30){modern=true;auto &port=v.controls.port[(id-21)/5];unsigned field=(id-21)%5;
    if(field==0){if(x<=controls::Joy2B)port.type=x;}
    else if(x<=controls::LoadState){if(field==4)port.up=x;else port.fire[field-1]=x;}}
   else if(id==32&&x<=1)v.audio_quality=x;
   // Legacy field 33 (user-selected output) is deliberately ignored.
   else if(id==34&&x<=1)v.turbo=x;
   else if(id==31){modern=true;if(x<=controls::LoadState)v.controls.space=x;}
  }
  p+=len;
 }
 if(legacy&&!modern){
  using namespace controls;v.controls.space=None;
  if(old[0]){v.controls.port[0].type=Joy2B;for(unsigned i=0;i<3;++i)v.controls.port[0].fire[i]=old[3+i]+1;}
  else {v.controls.port[0].fire[0]=None;v.controls.port[1].fire[0]=None;
   for(unsigned button=0;button<2;++button){unsigned target=old[1+button];uint8_t *dst;
    if(target==0)dst=&v.controls.port[0].fire[0];else if(target==1)dst=&v.controls.space;
    else if(target==2)dst=&v.controls.port[0].up;else {v.controls.port[1].type=Joy;dst=&v.controls.port[1].fire[0];}
    if(*dst==None)*dst=button==0?A:B;
   }
  }
 }
 v.audio_stereo=__atomic_load_n(&detected_stereo,__ATOMIC_RELAXED)==1;v.audio_effects=v.audio_quality?6:4;out=v;return true;
}
static bool read_data(const char *name,uint8_t *b,unsigned capacity,unsigned& size){
 UINT n=0;size=0;
 if(f_open(&file,name,FA_READ)!=FR_OK)return false;
 bool ok=f_size(&file)<=capacity&&f_read(&file,b,capacity,&n)==FR_OK;
 if(f_close(&file)!=FR_OK)ok=false;
 size=n;return ok;
}
static bool read_settings(const char *name,Settings& out){
 uint8_t b[256];unsigned n;
 return read_data(name,b,sizeof b,n)&&settings_decode(b,n,out);
}
void settings_load(){global_settings=Settings{};Settings value;if(read_settings(path,value)||read_settings(backup,value))global_settings=value;settings=global_settings;settings_apply_audio();}
// One bounded transaction helper shared by settings and last-game metadata.
static SaveResult write_record(const char *name,const char *tmp,const char *bak,const uint8_t *b,unsigned n){
 FATFS *fs;DWORD free;
 if(f_getfree("",&free,&fs)!=FR_OK)return SAVE_IO;
 if(free<3)return SAVE_FULL;
 flash_fs_begin_atari_write();SaveResult result=SAVE_IO;UINT wrote=0;bool had=false;
 {FRESULT fr=f_mkdir("/.options");if(fr!=FR_OK&&fr!=FR_EXIST)goto end;}
 if(!strncmp(name,"/.options/",10)&&strchr(name+10,'/')){
  char folder[288];const char *slash=strrchr(name,'/');unsigned len=slash-name;
  if(len>=sizeof folder)goto end;
  memcpy(folder,name,len);folder[len]=0;FRESULT fr=f_mkdir(folder);if(fr!=FR_OK&&fr!=FR_EXIST)goto end;
 }
 if(f_open(&file,tmp,FA_CREATE_ALWAYS|FA_WRITE)!=FR_OK)goto end;
 {bool ok=f_write(&file,b,n,&wrote)==FR_OK&&wrote==n;if(f_close(&file)!=FR_OK)ok=false;if(!ok)goto end;}
 flash_fs_sync();
 if(f_open(&file,tmp,FA_READ)!=FR_OK)goto end;
 {uint8_t check[64];bool ok=f_size(&file)==n;
  for(unsigned pos=0;ok&&pos<n;){unsigned count=n-pos;if(count>sizeof check)count=sizeof check;UINT got=0;
   ok=f_read(&file,check,count,&got)==FR_OK&&got==count&&!memcmp(check,b+pos,count);pos+=count;}
  if(f_close(&file)!=FR_OK)ok=false;
  if(!ok)goto end;}
 {FILINFO info;had=f_stat(name,&info)==FR_OK;}
 if(had){FRESULT fr=f_unlink(bak);if(fr!=FR_OK&&fr!=FR_NO_FILE)goto end;if(f_rename(name,bak)!=FR_OK)goto end;}
 if(f_rename(tmp,name)!=FR_OK){if(had)f_rename(bak,name);goto end;}
 flash_fs_sync();f_unlink(bak);result=SAVE_OK;
end:
 if(result!=SAVE_OK){if(flash_fs_no_space())result=SAVE_FULL;f_unlink(tmp);}
 flash_fs_end_atari_write();return result;
}
SaveResult settings_save(){uint8_t b[128];unsigned n=settings_encode(settings,b);SaveResult result=write_record(path,temp,backup,b,n);if(result==SAVE_OK)global_settings=settings;return result;}
static uint8_t last_record[1038];
static bool read_last(const char *name){
 unsigned n;if(!read_data(name,last_record,sizeof last_record,n)||n<15||memcmp(last_record,"ATBOYLG1",8))return false;
 unsigned len=last_record[8]|unsigned(last_record[9])<<8;
 if(!len||len>=sizeof last_game_path||len+14!=n||last_record[10]!='/')return false;
 uint32_t c=0;for(unsigned i=0;i<4;++i)c|=uint32_t(last_record[n-4+i])<<(8*i);
 if(c!=crc(last_record,n-4)||memchr(last_record+10,0,len))return false;
 memcpy(last_game_path,last_record+10,len);last_game_path[len]=0;return true;
}
void last_game_load(){last_game_path[0]=0;if(!read_last("/.atariboy.last"))read_last("/.atariboy.last.bak");}
SaveResult last_game_save(){
 unsigned len=strlen(last_game_path);if(!len||len>=sizeof last_game_path)return SAVE_IO;
 memcpy(last_record,"ATBOYLG1",8);last_record[8]=len;last_record[9]=len>>8;memcpy(last_record+10,last_game_path,len);
 uint32_t c=crc(last_record,len+10);for(unsigned i=0;i<4;++i)last_record[len+10+i]=c>>(8*i);
 return write_record("/.atariboy.last","/.atariboy.last.tmp","/.atariboy.last.bak",last_record,len+14);
}

// Full ROM hash in filename preserves separate configurations of identically
// named ROMs. The envelope also verifies identity before applying any fields.
static void game_path(char *out,const char *extension){
 char hash[65];const uint8_t *h=saves_game_hash();
 for(unsigned i=0;i<32;++i)snprintf(hash+i*2,3,"%02x",h[i]);
 snprintf(out,384,"/.options/%s/%s.%s",saves_game_name(),hash,extension);
}
SaveResult settings_load_game(bool fallback){
 // Missing new fields inherit global values. Runtime hotkeys never trigger IO.
 Settings value;uint8_t data[256];unsigned n;char name[384];
 SaveResult result=SAVE_EMPTY;
 for(unsigned attempt=0;attempt<2;++attempt){
  game_path(name,attempt?"bak":"cfg");FILINFO info;
  if(f_stat(name,&info)!=FR_OK)continue;
  result=SAVE_INVALID;
  if(read_data(name,data,sizeof data,n)&&n>40&&!memcmp(data,"ATBOYGO1",8)
     &&!memcmp(data+8,saves_game_hash(),32)&&settings_decode(data+40,n-40,value,&global_settings)){
   value.boot_last=global_settings.boot_last;settings=value;settings_apply_audio();return SAVE_OK;
  }
 }
 if(fallback){settings=global_settings;settings_apply_audio();}
 return result;
}
SaveResult settings_save_game(){
 char name[384],tmp[384],bak[384];uint8_t b[168];Settings value=settings;
 value.boot_last=global_settings.boot_last;
 game_path(name,"cfg");game_path(tmp,"tmp");game_path(bak,"bak");
 memcpy(b,"ATBOYGO1",8);memcpy(b+8,saves_game_hash(),32);
 unsigned n=40+settings_encode(value,b+40);
 SaveResult result=write_record(name,tmp,bak,b,n);
 if(result==SAVE_OK)settings.boot_last=global_settings.boot_last;
 return result;
}
bool settings_has_game(const char *name){
 // Informational basename icon, just like saves. No ROM hashing or content IO
 // in the browser; only call when a new row enters the cached visible page.
 struct Probe {char folder[288];DIR dir;FILINFO info;};
 Probe *p=(Probe*)malloc(sizeof(Probe));if(!p)return false;
 snprintf(p->folder,sizeof p->folder,"/.options/%s",name);
 char *dot=strrchr(p->folder+10,'.');if(dot)*dot=0;
 bool found=false;
 if(f_opendir(&p->dir,p->folder)==FR_OK){
  while(f_readdir(&p->dir,&p->info)==FR_OK&&p->info.fname[0]){
   const char *n=p->info.fname;
   if(p->info.fattrib&AM_DIR)continue;
   if(strlen(n)!=68||strcmp(n+64,".cfg"))continue;
   bool hex=true;for(unsigned i=0;i<64;++i)if(!((n[i]>='0'&&n[i]<='9')||(n[i]>='a'&&n[i]<='f')))hex=false;
   if(hex){found=true;break;}
  }
  f_closedir(&p->dir);
 }
 free(p);return found;
}
