/* Offline FAT16 discard. No FAT writes, no relocation, no disk format change.
 * Every validation completes before the first map write. The host must be
 * disconnected; call only via the acknowledged Atari maintenance screen.
 */
#include "storage/fatfs/ff.h"
#include "storage/flash_fs.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#define LBAS 30716u
#define MASK_BYTES ((LBAS+7)/8)
#define MAX_DEPTH 32
struct frame { uint32_t cluster,sector,entry; };
struct work {
 uint8_t free_lbas[MASK_BYTES],seen[MASK_BYTES];
 uint8_t fat[512],dir[512];
 uint32_t fat_cached,dir_cached;
 struct frame stack[MAX_DEPTH];
};
static unsigned word(const uint8_t *p){return p[0]|((unsigned)p[1]<<8);}
static uint32_t dword(const uint8_t *p){return word(p)|((uint32_t)word(p+2)<<16);}
static unsigned fat(const FATFS *fs,struct work *w,unsigned cluster){
 unsigned sector=fs->fatbase+cluster/256;
 if(w->fat_cached!=sector){flash_fs_read_FAT_sector(sector,w->fat);w->fat_cached=sector;}
 return word(w->fat+2*(cluster%256));
}
static int visit(struct work *w,unsigned cluster){
 unsigned bit=1u<<(cluster&7);if(w->seen[cluster>>3]&bit)return 0;
 w->seen[cluster>>3]|=bit;return 1;
}
static int cluster_ok(const FATFS *fs,unsigned c){return c>=2&&c<fs->n_fatent;}
static int file_chain(const FATFS *fs,struct work *w,unsigned c,uint32_t bytes){
 if(!c)return bytes==0;
 uint32_t capacity=0;
 for(;;){
  if(!cluster_ok(fs,c)||!visit(w,c))return 0;
  capacity+=(uint32_t)fs->csize*512;
  unsigned next=fat(fs,w,c);
  if(next>=0xfff8)return capacity>=bytes;
  c=next;
 }
}
static int directories(const FATFS *fs,struct work *w){
 unsigned depth=0;w->stack[0]=(struct frame){0,0,0};
 for(;;){
  struct frame *f=&w->stack[depth];
  unsigned sectors=f->cluster?fs->csize:fs->n_rootdir/16;
  if(f->sector>=sectors){
   if(f->cluster){unsigned n=fat(fs,w,f->cluster);
    if(n<0xfff8){if(!cluster_ok(fs,n))return 0;f->cluster=n;f->sector=f->entry=0;continue;}
   }
   if(!depth)return 1;
   --depth;continue;
  }
  unsigned sector=f->cluster?fs->database+(f->cluster-2)*fs->csize+f->sector:fs->dirbase+f->sector;
  if(w->dir_cached!=sector){flash_fs_read_FAT_sector(sector,w->dir);w->dir_cached=sector;}
  const uint8_t *e=w->dir+f->entry*32;
  if(++f->entry==16){f->entry=0;++f->sector;}
  if(!e[0]){if(!depth)return 1;
   --depth;continue;}
  if(e[0]==0xe5||e[11]==0x0f||(e[11]&8))continue;
  if((e[11]&16) && e[0]=='.' && (e[1]==' '||(e[1]=='.'&&e[2]==' ')))continue;
  unsigned c=word(e+26);uint32_t size=dword(e+28);
  if(word(e+20))return 0;
  if(e[11]&16){
   if(depth+1>=MAX_DEPTH||!cluster_ok(fs,c)||!file_chain(fs,w,c,0))return 0;
   w->stack[++depth]=(struct frame){c,0,0};
  }else if(!file_chain(fs,w,c,size))return 0;
 }
}
const char *disk_optimize(const FATFS *fs,uint32_t *released,char *detail,unsigned detail_size){
 *released=0;
 // Refuse unfamiliar layouts instead of guessing what may be discarded.
 if(fs->fs_type!=FS_FAT16)return "Optimize needs FAT16 - no changes";
 if(!fs->csize||(fs->csize&(fs->csize-1))||fs->n_fatent<4087||fs->n_fatent>LBAS||
    (fs->n_fats!=1&&fs->n_fats!=2)||!fs->n_rootdir||fs->n_rootdir%16||
    fs->fatbase<=fs->volbase||fs->fatbase>=LBAS||!fs->fsize||fs->fsize>LBAS/2||
    fs->fsize*256<fs->n_fatent||fs->dirbase!=fs->fatbase+fs->n_fats*fs->fsize||
    fs->database!=fs->dirbase+fs->n_rootdir/16||fs->database>=LBAS||
    fs->n_fatent-2>(LBAS-fs->database)/fs->csize)
  return "Invalid FAT layout - no changes";
 struct work *w=calloc(1,sizeof *w);
 if(!w)return "Optimize: not enough RAM";
 const char *error="Invalid FAT data - no changes";
 w->fat_cached=w->dir_cached=0xffffffffu;
 // A8PicoCart defaults to one FAT; AtariBoy formatting creates two.
 // Compare copies only when present, including reserved entries/padding.
 for(unsigned i=0;fs->n_fats==2&&i<fs->fsize;++i){
  flash_fs_read_FAT_sector(fs->fatbase+i,w->fat);
  flash_fs_read_FAT_sector(fs->fatbase+fs->fsize+i,w->dir);
  if(memcmp(w->fat,w->dir,512)){error="FAT copies differ - no changes";goto done;}
 }
 // FAT[1] is not a cluster link: its top two bits are clean-shutdown
 // and no-I/O-error flags. Windows may clear them (e.g. 0x7fff).
 // Validate reserved low bits, retain both flags exactly as stored.
 unsigned reserved0=fat(fs,w,0),reserved1=fat(fs,w,1);
 if((reserved0&0xff00)!=0xff00 || (reserved0&255)<0xf0 ||
    ((reserved0&255)>0xf0 && (reserved0&255)<0xf8) ||
    (reserved1&0x3fff)!=0x3fff){
  snprintf(detail,detail_size,"FAT reserved %04X/%04X; unchanged",reserved0,reserved1);
  error=detail;goto done;
 }
 for(unsigned c=2;c<fs->n_fatent;++c){
  unsigned n=fat(fs,w,c);
  if(!n){
   unsigned start=fs->database+(c-2)*fs->csize;
   for(unsigned j=0;j<fs->csize;++j)w->free_lbas[(start+j)>>3]|=1u<<((start+j)&7);
  }else if(n<0xfff0){if(!cluster_ok(fs,n)||!fat(fs,w,n)){
   snprintf(detail,detail_size,"FAT link %04X->%04X; unchanged",c,n);
   error=detail;goto done;
  }}
 }
 if(!directories(fs,w)){error="Invalid file chains - no changes";goto done;}
 // Keep metadata, allocated/orphan/bad clusters, reserved sectors and tail.
 if(!flash_fs_discard_free(w->free_lbas,released)){error="FLASH map write/verify failed";goto done;}
 error=0;
 done:free(w);return error;
}
