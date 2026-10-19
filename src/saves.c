// AtariBoy state format 2. All integers little endian. No host pointers.
#include "saves.h"
#include "engine.h"
#include "audio.h"
#include "storage.h"
#include "storage/flash_fs.h"
#include "storage/fatfs/ff.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#define HEADER 80
#define META (896+30)
static char directory[288];
static uint8_t rom_hash[32];
static unsigned selected=1;
static uint64_t newest[10],maximum;
static uint16_t present;
static int any,scan_ok;
static uint32_t crc_add(uint32_t crc,const uint8_t *p,size_t n){
 while(n--){crc^=*p++;for(unsigned i=0;i<8;++i)crc=(crc>>1)^(0xedb88320u&-(crc&1));}return crc;
}
static void put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=v>>(8*i);}
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void path_for(char *out,unsigned slot,uint64_t n,const char *ext){
 snprintf(out,384,"%s/%02u_%04llu.%s",directory,slot,(unsigned long long)n,ext);
}
static int parse(const char *s,unsigned *slot,uint64_t *n){
 if(s[0]<'0'||s[0]>'1'||s[1]<'0'||s[1]>'9'||s[2]!='_')return 0;
 *slot=(s[0]-'0')*10+s[1]-'0';if(*slot<1||*slot>10)return 0;
 s+=3;unsigned count=0;*n=0;
 while(*s>='0'&&*s<='9'){if(*n>(UINT64_MAX-9)/10)return 0;*n=*n*10+*s++-'0';++count;}
 return count>=4&&!strcmp(s,".sav");
}
static int header_valid(const uint8_t *h,uint32_t size){
 return !memcmp(h,"ATBOYST2",8)&&get32(h+8)==2&&get32(h+12)==HEADER
  &&get32(h+16)==0x8e656982u&&get32(h+20)==engine_payload_size()
  &&get32(h+24)==engine_rom_size()&&get32(h+28)==engine_ram_size()
  &&!memcmp(h+32,rom_hash,32)&&get32(h+68)==~crc_add(~0u,h,68)
  &&get32(h+72)==META&&get32(h+76)==0&&size==HEADER+get32(h+20)+META;
}
// 1: matching state, -1: intact identity of another ROM, 0: invalid header.
static int identity(const char *path){
 FIL f;UINT got;uint8_t h[HEADER];
 if(f_open(&f,path,FA_READ)!=FR_OK)return 0;
 int ok=0;
 if(f_read(&f,h,sizeof h,&got)==FR_OK&&got==sizeof h){
  if(header_valid(h,f_size(&f)))ok=1;
  else if(!memcmp(h,"ATBOYST2",8)&&get32(h+68)==~crc_add(~0u,h,68)&&memcmp(h+32,rom_hash,32))ok=-1;
 }
 f_close(&f);return ok;
}
static int same_rom(const char *path){return identity(path)==1;}
static void scan(void){
 DIR d;FILINFO info;present=0;maximum=0;any=0;scan_ok=0;
 memset(newest,0,sizeof newest);
 FRESULT r=f_opendir(&d,directory);
 if(r==FR_NO_PATH||r==FR_NO_FILE){scan_ok=1;return;}
 if(r!=FR_OK)return;
 while((r=f_readdir(&d,&info))==FR_OK&&info.fname[0]){
  unsigned slot;uint64_t n;
  if((info.fattrib&AM_DIR)||!parse(info.fname,&slot,&n))continue;
  // Include all names in the sequence to avoid collisions between different
  // ROM contents sharing a basename. Only matching ROMs participate in slots.
  if(!any||n>maximum)maximum=n;
  any=1;
  char p[384];snprintf(p,sizeof p,"%s/%.80s",directory,info.fname);
  if(!same_rom(p))continue;
  unsigned bit=1u<<(slot-1);
  if(!(present&bit)||n>newest[slot-1])newest[slot-1]=n;
  present|=bit;
 }
 f_closedir(&d);scan_ok=r==FR_OK;
}
void saves_select_game(const char *path){
 const char *base=strrchr(path,'/');base=base?base+1:path;
 snprintf(directory,sizeof directory,"/.saves/%s",base);
 char *dot=strrchr(directory,'/');dot=strrchr(dot?dot+1:directory,'.');if(dot)*dot=0;
 engine_rom_hash(rom_hash);scan();selected=1;uint64_t best=0;int found=0;
 for(unsigned i=0;i<10;++i)if((present&(1u<<i))&&(!found||newest[i]>best)){selected=i+1;best=newest[i];found=1;}
}
const uint8_t *saves_game_hash(void){return rom_hash;}
unsigned saves_slot(void){return selected;}
void saves_move(int delta){selected=(selected+9+(delta<0?9:1))%10+1;}

// Map a closed, immutable save to physical flash. This tiny allocation avoids
// a second whole-state buffer. Validate first, then restore without fallible IO.
struct Reader {uint16_t *map;uint32_t size,pos;};
static int mapped_read(void *v,void *out,size_t n){
 struct Reader *r=v;if(n>r->size-r->pos)return 0;
 uint8_t *p=out;
 while(n){unsigned take=512-(r->pos&511);if(take>n)take=n;
  const void *src=(const void *)(uintptr_t)(0x10100000u+((uint32_t)r->map[r->pos>>9]<<9)+(r->pos&511));
  memcpy(p,src,take);p+=take;r->pos+=take;n-=take;
 }return 1;
}
static int open_map_size(const char *path,struct Reader *r,unsigned expected,unsigned legacy){
 FIL f;UINT got;uint8_t byte;r->map=0;r->pos=0;
 if(f_open(&f,path,FA_READ)!=FR_OK)return 0;
 r->size=f_size(&f);
 if(r->size!=expected&&r->size+legacy!=expected){f_close(&f);return 0;}
 unsigned count=(r->size+511)/512;r->map=malloc(count*2);
 if(!r->map){f_close(&f);return 0;}
 for(unsigned i=0;i<count;++i){
  if(f_lseek(&f,i*512)!=FR_OK||f_read(&f,&byte,1,&got)!=FR_OK||got!=1)goto fail;
  r->map[i]=flash_fs_entry(f.sect);if(!r->map[i])goto fail;
 }
 f_close(&f);return 1;
fail:f_close(&f);free(r->map);r->map=0;return 0;
}
static int open_map(const char *path,struct Reader *r){return open_map_size(path,r,HEADER+engine_payload_size()+META,0);}
static int validate(struct Reader *r){
 uint8_t h[HEADER],buf[512];r->pos=0;
 if(!mapped_read(r,h,sizeof h)||!header_valid(h,r->size))return 0;
 uint32_t crc=~0u;
 while(r->pos<r->size){unsigned n=r->size-r->pos;if(n>sizeof buf)n=sizeof buf;
  if(!mapped_read(r,buf,n))return 0;
  crc=crc_add(crc,buf,n);
 }
 return ~crc==get32(h+64);
}
enum SaveResult saves_load(void){
 scan();if(!scan_ok)return SAVE_IO;
 // Retain corrupt files with a valid identity header in the slot listing.
 if(!(present&(1u<<(selected-1)))){
  // A damaged header cannot prove identity; report failure if that slot exists.
  DIR d;FILINFO i;if(f_opendir(&d,directory)==FR_OK){
   while(f_readdir(&d,&i)==FR_OK&&i.fname[0]){unsigned s;uint64_t n;
    if(parse(i.fname,&s,&n)&&s==selected){
     char p[384];snprintf(p,sizeof p,"%s/%.80s",directory,i.fname);
     if(identity(p)==0){f_closedir(&d);return SAVE_INVALID;}
    }}
   f_closedir(&d);
  }return SAVE_EMPTY;
 }
 char path[384];path_for(path,selected,newest[selected-1],"sav");
 struct Reader r;if(!open_map(path,&r))return SAVE_INVALID;
 if(!validate(&r)){free(r.map);return SAVE_INVALID;}
 engine_set_legacy_load(0);
 r.pos=HEADER;int ok=engine_transfer(mapped_read,&r,1)&&r.pos+META==r.size;
 free(r.map);return ok?SAVE_OK:SAVE_INVALID;
}
struct Writer {FIL *f;uint32_t crc;enum SaveResult error;};
static int write_bytes(void *v,void *p,size_t n){
 struct Writer *w=v;UINT got;
 FRESULT r=f_write(w->f,p,n,&got);
 if(r!=FR_OK||got!=n){w->error=r==FR_OK?SAVE_FULL:SAVE_IO;return 0;}
 w->crc=crc_add(w->crc,p,n);return 1;
}
static enum SaveResult classify(FRESULT r){
 FATFS *fs;DWORD free;
 if(f_getfree("",&free,&fs)==FR_OK&&!free)return SAVE_FULL;
 (void)r;return SAVE_IO;
}
static __attribute__((noinline)) enum SaveResult write_state(void){
 uint64_t number=any?maximum+1:0;
 char path[384],tmp[384];path_for(path,selected,number,"sav");path_for(tmp,selected,number,"tmp");
 FATFS *fs;DWORD free_clusters;
 if(f_getfree("",&free_clusters,&fs)!=FR_OK)return SAVE_IO;
 // Keep the previous save until a complete replacement has been verified.
 uint32_t cluster_bytes=fs->csize*512u;
 uint32_t needed=(HEADER+engine_payload_size()+META+cluster_bytes-1)/cluster_bytes+3;
 if(free_clusters<needed)return SAVE_FULL;
 flash_fs_begin_atari_write();
 enum SaveResult result=SAVE_IO;FRESULT fr;
 fr=f_mkdir("/.saves");if(fr!=FR_OK&&fr!=FR_EXIST){result=classify(fr);goto end;}
 fr=f_mkdir(directory);if(fr!=FR_OK&&fr!=FR_EXIST){result=classify(fr);goto end;}
 static FIL f; // EXE-only: keep the deep FatFs write chain off the 4 KiB stack.
 fr=f_open(&f,tmp,FA_WRITE|FA_CREATE_ALWAYS);if(fr!=FR_OK){result=classify(fr);goto end;}
 uint8_t h[HEADER]={0};memcpy(h,"ATBOYST2",8);put32(h+8,2);put32(h+72,META);put32(h+12,HEADER);
 put32(h+16,0x8e656982);put32(h+20,engine_payload_size());put32(h+24,engine_rom_size());
 put32(h+28,engine_ram_size());memcpy(h+32,rom_hash,32);
 struct Writer w={&f,~0u,SAVE_IO};
 int ok=write_bytes(&w,h,sizeof h);w.crc=~0u;
 if(ok)ok=engine_transfer(write_bytes,&w,0);
 // Thumbnail samples the 192-line view, without overlays; 53 pixels/14 bytes.
 uint8_t row[30];
 for(unsigned y=0;ok&&y<64;++y){memset(row,0,14);unsigned sy=(y*3)*3/4;
  for(unsigned x=0;x<53;++x){unsigned sx=x*160/53;unsigned px=(gb_screen[sy*40+sx/4]>>(6-2*(sx%4)))&3;row[x/4]|=px<<(6-2*(x%4));}
  ok=write_bytes(&w,row,14);
 }
 memset(row,' ',30);if(ok)ok=write_bytes(&w,row,30);
 put32(h+64,~w.crc);put32(h+68,~crc_add(~0u,h,68));
 if(ok)ok=f_lseek(&f,0)==FR_OK&&write_bytes(&w,h,sizeof h);
 if(!ok)result=w.error;
 if(f_close(&f)!=FR_OK)ok=0;
 flash_fs_sync();
 if(!ok)goto cleanup;
 struct Reader reader;
 if(!open_map(tmp,&reader))goto cleanup;
 ok=validate(&reader);free(reader.map);if(!ok)goto cleanup;
 if(f_rename(tmp,path)!=FR_OK)goto cleanup;
 flash_fs_sync();result=SAVE_OK;
 // Normally only one older file; also recover duplicates left by interruption.
 // Reopen directory after each deletion (FatFs directory cursor is not reused).
 for(;;){
  DIR d;FILINFO info;int removed=0;
  if(f_opendir(&d,directory)!=FR_OK)break;
  while(f_readdir(&d,&info)==FR_OK&&info.fname[0]){
   unsigned slot;uint64_t n;if(!parse(info.fname,&slot,&n)||slot!=selected||n>=number)continue;
   char old[384];snprintf(old,sizeof old,"%s/%.80s",directory,info.fname);
   if(same_rom(old)){f_closedir(&d);removed=f_unlink(old)==FR_OK;goto again;}
  }
  f_closedir(&d);
again:if(!removed)break;
 }
 goto end;
cleanup:f_unlink(tmp);
end:if(result!=SAVE_OK&&flash_fs_no_space())result=SAVE_FULL;
 flash_fs_end_atari_write();return result;
}

enum SaveResult saves_write(void){
 scan();if(!scan_ok||maximum==UINT64_MAX)return SAVE_IO;
 enum SaveResult result=write_state();scan();return result;
}

// Informational icon only: any correctly named save in the basename folder.
// Actual loading still validates ROM identity and payload CRC as before.
struct SaveProbe {DIR dir;FILINFO info;char folder[288];};
const char *saves_game_name(void){return directory+8;}
int saves_has_game(const char *parent,const char *name){
 (void)parent;
 struct SaveProbe *w=malloc(sizeof *w);if(!w)return 0;
 int result=0,len=snprintf(w->folder,sizeof w->folder,"/.saves/%s",name);
 if(len<0||(unsigned)len>=sizeof w->folder)goto done;
 char *dot=strrchr(w->folder+8,'.');if(dot)*dot=0;
 if(f_opendir(&w->dir,w->folder)!=FR_OK)goto done;
 while(f_readdir(&w->dir,&w->info)==FR_OK&&w->info.fname[0]){
  unsigned slot;uint64_t number;
  if(!(w->info.fattrib&AM_DIR)&&(parse(w->info.fname,&slot,&number)||(strlen(w->info.fname)==76&&!strncmp(w->info.fname,"battery-",8)&&!strcmp(w->info.fname+72,".sav")))){result=1;break;}
 }
 f_closedir(&w->dir);
done:free(w);return result;
}

static uint32_t battery_baseline;
static int battery_crc(void *context,void *bytes,size_t n){uint32_t *crc=context;*crc=crc_add(*crc,bytes,n);return 1;}
static uint32_t battery_fingerprint(void){uint32_t crc=~0u;engine_battery_transfer(battery_crc,&crc,0);return ~crc;}
static void battery_path(char *out,const char *ext){
 char hash[65];for(unsigned i=0;i<32;++i)snprintf(hash+2*i,3,"%02x",rom_hash[i]);
 snprintf(out,384,"%s/battery-%s.%s",directory,hash,ext);
}
static int battery_valid(struct Reader *r){
 uint8_t h[HEADER],buffer[512];r->pos=0;
 if(!mapped_read(r,h,HEADER)||memcmp(h,"ATBOYBA1",8)||get32(h+8)!=1||get32(h+12)!=HEADER
  ||get32(h+20)!=engine_battery_size()||get32(h+24)!=engine_rom_size()||get32(h+28)!=engine_ram_size()
  ||memcmp(h+32,rom_hash,32)||get32(h+68)!=~crc_add(~0u,h,68))return 0;
 uint32_t crc=~0u;
 while(r->pos<r->size){unsigned n=r->size-r->pos;if(n>sizeof buffer)n=sizeof buffer;
  if(!mapped_read(r,buffer,n))return 0;
  crc=crc_add(crc,buffer,n);}
 return ~crc==get32(h+64);
}
enum SaveResult battery_load(void){
 battery_baseline=battery_fingerprint();
 if(!engine_battery_size())return SAVE_EMPTY;
 char path[384];int found=0;
 for(unsigned attempt=0;attempt<2;++attempt){
  battery_path(path,attempt?"bak":"sav");FILINFO info;
  if(f_stat(path,&info)!=FR_OK)continue;
  found=1;struct Reader r;
  if(!open_map_size(path,&r,HEADER+engine_battery_size(),0))continue;
  int valid=battery_valid(&r);
  if(valid){r.pos=HEADER;valid=engine_battery_transfer(mapped_read,&r,1)&&r.pos==r.size;}
  free(r.map);
  if(valid){battery_baseline=battery_fingerprint();return SAVE_OK;}
 }
 return found?SAVE_INVALID:SAVE_EMPTY;
}
static __attribute__((noinline)) enum SaveResult battery_write(unsigned size,uint32_t fingerprint){
 FATFS *fs;DWORD free_clusters;
 if(f_getfree("",&free_clusters,&fs)!=FR_OK)return SAVE_IO;
 unsigned needed=(HEADER+size+fs->csize*512-1)/(fs->csize*512)+3;
 if(free_clusters<needed)return SAVE_FULL;
 // Deep filesystem calls use a static FIL, as in state writes. No RAM-sized copy.
 static FIL f;char path[384],tmp[384],bak[384];
 battery_path(path,"sav");battery_path(tmp,"tmp");battery_path(bak,"bak");
 uint8_t h[HEADER]={0};memcpy(h,"ATBOYBA1",8);put32(h+8,1);put32(h+12,HEADER);
 put32(h+20,size);put32(h+24,engine_rom_size());put32(h+28,engine_ram_size());memcpy(h+32,rom_hash,32);
 enum SaveResult result=SAVE_IO;int had=0;FRESULT fr;
 flash_fs_begin_atari_write();
 fr=f_mkdir("/.saves");if(fr!=FR_OK&&fr!=FR_EXIST)goto end;
 fr=f_mkdir(directory);if(fr!=FR_OK&&fr!=FR_EXIST)goto end;
 if(f_open(&f,tmp,FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK)goto end;
 struct Writer w={&f,~0u,SAVE_IO};int ok=write_bytes(&w,h,HEADER);w.crc=~0u;
 if(ok)ok=engine_battery_transfer(write_bytes,&w,0);
 put32(h+64,~w.crc);put32(h+68,~crc_add(~0u,h,68));
 if(ok)ok=f_lseek(&f,0)==FR_OK&&write_bytes(&w,h,HEADER);
 if(!ok)result=w.error;
 if(f_close(&f)!=FR_OK)ok=0;
 flash_fs_sync();if(!ok)goto end;
 struct Reader reader;if(!open_map_size(tmp,&reader,HEADER+size,0))goto end;
 ok=battery_valid(&reader);free(reader.map);if(!ok)goto end;
 {FILINFO info;had=f_stat(path,&info)==FR_OK;}
 if(had){fr=f_unlink(bak);if(fr!=FR_OK&&fr!=FR_NO_FILE)goto end;if(f_rename(path,bak)!=FR_OK)goto end;}
 if(f_rename(tmp,path)!=FR_OK){if(had)f_rename(bak,path);goto end;}
 flash_fs_sync();f_unlink(bak);battery_baseline=fingerprint;result=SAVE_OK;
end:
 if(result!=SAVE_OK){if(flash_fs_no_space())result=SAVE_FULL;f_unlink(tmp);}
 flash_fs_end_atari_write();return result;
}

enum SaveResult battery_save(void){
 unsigned size=engine_battery_size();if(!size)return SAVE_EMPTY;
 uint32_t fingerprint=battery_fingerprint();if(fingerprint==battery_baseline)return SAVE_EMPTY;
 return battery_write(size,fingerprint);
}

// State manager: metadata IO is bounded; enumeration never runs while drawing.
void saves_set_slot(unsigned slot){if(slot>=1&&slot<=10)selected=slot;}
static __attribute__((noinline)) enum SaveResult read_slot_metadata(struct SaveSlot slots[10]){
 memset(slots,0,10*sizeof *slots);if(!scan_ok)return SAVE_IO;
 for(unsigned i=0;i<10;++i)if(present&(1u<<i)){
  slots[i].present=1;slots[i].number=newest[i];char path[384];path_for(path,i+1,newest[i],"sav");
  FIL f;UINT got;if(f_open(&f,path,FA_READ)!=FR_OK)return SAVE_IO;
  int ok=f_lseek(&f,f_size(&f)-30)==FR_OK&&f_read(&f,slots[i].description,30,&got)==FR_OK&&got==30;
  f_close(&f);if(!ok)return SAVE_IO;slots[i].description[30]=0;
 }return SAVE_OK;
}
enum SaveResult saves_list(struct SaveSlot slots[10]){scan();return read_slot_metadata(slots);}
enum SaveResult saves_preview(unsigned slot,uint8_t image[896]){
 memset(image,0,896);if(slot<1||slot>10||!(present&(1u<<(slot-1))))return SAVE_EMPTY;
 char path[384];path_for(path,slot,newest[slot-1],"sav");FIL f;UINT got;
 if(f_open(&f,path,FA_READ)!=FR_OK)return SAVE_IO;
 int ok=f_lseek(&f,HEADER+engine_payload_size())==FR_OK&&f_read(&f,image,896,&got)==FR_OK&&got==896;
 f_close(&f);return ok?SAVE_OK:SAVE_IO;
}
static int remove_slot(unsigned slot,uint64_t keep,int retain){
 for(;;){DIR d;FILINFO info;char target[384];target[0]=0;
  if(f_opendir(&d,directory)!=FR_OK)return 0;
  while(f_readdir(&d,&info)==FR_OK&&info.fname[0]){unsigned s;uint64_t n;
   if(parse(info.fname,&s,&n)&&s==slot&&(!retain||n!=keep)){
    snprintf(target,sizeof target,"%s/%.80s",directory,info.fname);
    if(same_rom(target))break;target[0]=0;
   }
  }f_closedir(&d);if(!target[0])return 1;if(f_unlink(target)!=FR_OK)return 0;
 }
}
enum SaveResult saves_delete(unsigned slot){
 flash_fs_begin_atari_write();int ok=remove_slot(slot,0,0);flash_fs_end_atari_write();scan();return ok?SAVE_OK:SAVE_IO;
}
static void clip_path(char *path){snprintf(path,384,"%s/clipboard.tmp",directory);}
void saves_clear_clipboard(void){char path[384];clip_path(path);FILINFO info;if(f_stat(path,&info)!=FR_OK)return;flash_fs_begin_atari_write();f_unlink(path);flash_fs_end_atari_write();}
// Stream through a small temporary workspace, never a second emulator state.
struct CloneWork {FIL in,out;uint8_t bytes[512],header[HEADER];char tmp[384],bak[384];};
static enum SaveResult clone_state(const char *src,const char *dest,const char *desc){
 struct Reader reader;if(!open_map(src,&reader))return SAVE_INVALID;
 int valid=validate(&reader);free(reader.map);if(!valid)return SAVE_INVALID;
 FATFS *fs;DWORD free_clusters;
 if(f_getfree("",&free_clusters,&fs)!=FR_OK)return SAVE_IO;
 unsigned cluster=fs->csize*512u;
 if(free_clusters<(reader.size+cluster-1)/cluster+3)return SAVE_FULL;
 struct CloneWork *w=calloc(1,sizeof *w);if(!w)return SAVE_IO;
 snprintf(w->tmp,sizeof w->tmp,"%s.new",dest);snprintf(w->bak,sizeof w->bak,"%s.bak",dest);
 enum SaveResult result=SAVE_IO;int in=0,out=0,ok=0;UINT got,written;uint32_t crc=~0u;
 flash_fs_begin_atari_write();
 if(f_open(&w->in,src,FA_READ)!=FR_OK)goto done;in=1;
 if(f_open(&w->out,w->tmp,FA_WRITE|FA_CREATE_ALWAYS)!=FR_OK)goto done;out=1;
 if(f_read(&w->in,w->header,HEADER,&got)!=FR_OK||got!=HEADER)goto done;
 if(f_write(&w->out,w->header,HEADER,&written)!=FR_OK||written!=HEADER)goto done;
 unsigned remain=f_size(&w->in)-HEADER;
 while(remain){unsigned n=remain>sizeof w->bytes?sizeof w->bytes:remain;
  if(f_read(&w->in,w->bytes,n,&got)!=FR_OK||got!=n)goto done;
  if(desc){unsigned pos=f_tell(&w->in)-n,start=f_size(&w->in)-30;for(unsigned i=0;i<n;++i)if(pos+i>=start)w->bytes[i]=desc[pos+i-start];}
  crc=crc_add(crc,w->bytes,n);
  if(f_write(&w->out,w->bytes,n,&written)!=FR_OK||written!=n){result=SAVE_FULL;goto done;}remain-=n;
 }
 put32(w->header+64,~crc);put32(w->header+68,~crc_add(~0u,w->header,68));
 if(f_lseek(&w->out,0)!=FR_OK||f_write(&w->out,w->header,HEADER,&written)!=FR_OK||written!=HEADER)goto done;
 f_close(&w->in);in=0;ok=f_close(&w->out)==FR_OK;out=0;flash_fs_sync();if(!ok)goto done;
 if(!open_map(w->tmp,&reader))goto done;ok=validate(&reader);free(reader.map);if(!ok)goto done;
 {FILINFO info;int had=f_stat(dest,&info)==FR_OK;
  if(had){FRESULT fr=f_unlink(w->bak);if(fr!=FR_OK&&fr!=FR_NO_FILE)goto done;if(f_rename(dest,w->bak)!=FR_OK)goto done;}
  if(f_rename(w->tmp,dest)!=FR_OK){if(had)f_rename(w->bak,dest);goto done;}
  flash_fs_sync();if(had)f_unlink(w->bak);result=SAVE_OK;
 }
done:if(in)f_close(&w->in);if(out)f_close(&w->out);if(result!=SAVE_OK){if(flash_fs_no_space())result=SAVE_FULL;f_unlink(w->tmp);}flash_fs_end_atari_write();free(w);return result;
}
enum SaveResult saves_copy(unsigned slot){
 if(slot<1||slot>10||!(present&(1u<<(slot-1))))return SAVE_EMPTY;
 char src[384],dest[384];path_for(src,slot,newest[slot-1],"sav");clip_path(dest);return clone_state(src,dest,0);
}
enum SaveResult saves_paste(unsigned slot){
 scan();if(!scan_ok||maximum==UINT64_MAX)return SAVE_IO;
 char src[384],dest[384];uint64_t n=any?maximum+1:0;clip_path(src);path_for(dest,slot,n,"sav");
 enum SaveResult result=clone_state(src,dest,0);
 if(result==SAVE_OK){flash_fs_begin_atari_write();remove_slot(slot,n,1);flash_fs_end_atari_write();}scan();return result;
}
enum SaveResult saves_describe(unsigned slot,const char desc[30]){
 if(slot<1||slot>10||!(present&(1u<<(slot-1))))return SAVE_EMPTY;
 char path[384];path_for(path,slot,newest[slot-1],"sav");enum SaveResult result=clone_state(path,path,desc);scan();return result;
}

void saves_forget_game(void){directory[0]=0;memset(rom_hash,0,sizeof rom_hash);selected=1;memset(newest,0,sizeof newest);maximum=present=any=scan_ok=battery_baseline=0;}
