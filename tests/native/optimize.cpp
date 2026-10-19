#include <sys/mman.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <string>
#include <cstdlib>
extern "C" {
#include "storage.h"
#include "storage/fatfs_disk.h"
#include "storage/fatfs/ff.h"
static unsigned erased,programmed;
void flash_range_erase(uint32_t o,size_t n){assert(o>=0x100000&&o+n<=0x1000000&&o%4096==0&&n%4096==0);++erased;memset((void*)(uintptr_t)(0x10000000+o),255,n);}
void flash_range_program(uint32_t o,const uint8_t *p,size_t n){assert(o>=0x100000&&o+n<=0x1000000&&o%256==0&&n%256==0);++programmed;auto d=(uint8_t*)(uintptr_t)(0x10000000+o);for(size_t i=0;i<n;++i){assert((d[i]&p[i])==p[i]);d[i]=p[i];}}
}
static void writefile(const char *name,const std::vector<uint8_t>& data){FIL f;UINT n;assert(f_open(&f,name,FA_CREATE_ALWAYS|FA_WRITE)==FR_OK);assert(f_write(&f,data.data(),data.size(),&n)==FR_OK&&n==data.size());assert(f_close(&f)==FR_OK);}
static void verify(const char *name,const std::vector<uint8_t>& data){FIL f;UINT n;std::vector<uint8_t>b(data.size());assert(f_open(&f,name,FA_READ)==FR_OK);assert(f_read(&f,b.data(),b.size(),&n)==FR_OK&&n==b.size());assert(f_close(&f)==FR_OK);assert(b==data);}
static std::vector<uint8_t> disk(){auto p=(uint8_t*)0x10100000;return {p,p+0xf00000};}
static void export_disk(const char *name){FILE*f=fopen(name,"wb");assert(f);fwrite((void*)0x10000000,1,0x1000000,f);fclose(f);}
int main(int argc,char **argv){
 assert(argc==2||argc==3);bool legacy=argc==3;assert(mmap((void*)0x10000000,0x1000000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED,-1,0)!=(void*)-1);memset((void*)0x10000000,255,0x1000000);
 uint32_t borrowed[15360];flash_fs_set_usb_buffer(borrowed);flash_fs_set_writable(true);create_fatfs_disk();
 if(legacy){ // Exact original A8PicoCart f_mkfs call: default parameters.
  BYTE work[FF_MAX_SS];extern bool fatfs_formatting;
  fatfs_formatting=true;assert(f_mkfs("",nullptr,work,sizeof work)==FR_OK);fatfs_formatting=false;
 }
 assert(storage_mount());
 std::vector<uint8_t>keep(1024*1024),trash(10*1024*1024),small(731);
 for(unsigned i=0;i<keep.size();++i)keep[i]=(i*19+i/512)%251;
 // Valid 1 MiB MBC3 ROM, 32 KiB RAM: exercises optimization with a live engine.
 keep[0x147]=0x13;keep[0x148]=5;keep[0x149]=3;keep[0x14d]=0;for(unsigned i=0x134;i<=0x14c;++i)keep[0x14d]-=keep[i]+1;
 for(unsigned i=0;i<trash.size();++i)trash[i]=i*37+11;
 for(unsigned i=0;i<small.size();++i)small[i]=i*17;
 assert(f_mkdir("/.saves")==FR_OK);assert(f_mkdir("/.saves/Long game name")==FR_OK);assert(f_mkdir("/.options")==FR_OK);
 writefile("/Keep.gb",keep);writefile("/.saves/Long game name/01_0000.sav",small);writefile("/.options/global.cfg",small);
 for(unsigned i=0;i<24;++i){char n[100];sprintf(n,"/.saves/Long game name/Long filename %02u.cfg",i);writefile(n,small);}
 writefile("/Delete.gb",trash);assert(f_unlink("/Delete.gb")==FR_OK);
 fatfs_disk_sync();flash_fs_set_writable(false);export_disk((std::string(argv[1])+"/optimize-before.bin").c_str());
 std::vector<uint16_t>map(30716);std::vector<uint8_t>logical(30716*512);
 for(unsigned i=0;i<30716;++i){map[i]=flash_fs_entry(i);flash_fs_read_FAT_sector(i,logical.data()+i*512);}
 unsigned olderase=erased;uint32_t freed=0;assert(storage_optimize_disk(&freed));assert(freed>=trash.size()/512);assert(erased-olderase<=15);
 unsigned removed=0;for(unsigned i=0;i<30716;++i){auto e=flash_fs_entry(i);if(map[i]&&!e){++removed;continue;}assert(e==map[i]);uint8_t b[512];flash_fs_read_FAT_sector(i,b);assert(!memcmp(b,logical.data()+i*512,512));}
 assert(removed==freed);verify("/Keep.gb",keep);verify("/.saves/Long game name/01_0000.sav",small);verify("/.options/global.cfg",small);
 printf("PASS reclaimed %u KiB; %u map block erases; ALL retained LBA bytes and physical addresses unchanged\n",freed/2,erased-olderase);
 auto optimized=disk();olderase=erased;assert(storage_optimize_disk(&freed)&&freed==0);assert(erased==olderase&&disk()==optimized);puts("PASS repeated optimize: no writes");
 // Refuse mismatched FAT, duplicate FTL and an allocated directory entry whose FAT says free.
 auto baseline=disk();uint8_t boot[512];flash_fs_read_FAT_sector(0,boot);unsigned volbase=0;
 if((boot[11]|boot[12]<<8)!=512){volbase=boot[454]|boot[455]<<8|boot[456]<<16|boot[457]<<24;flash_fs_read_FAT_sector(volbase,boot);}
 unsigned copies=boot[16],fatsize=boot[22]|boot[23]<<8,firstfat=volbase+(boot[14]|boot[15]<<8);
 assert(copies==(legacy?1:2));printf("LAYOUT: volume=%u FAT copies=%u cluster sectors=%u\n",volbase,copies,boot[13]);
 auto corrupt=[&](unsigned lba,unsigned offset,uint8_t value){flash_fs_begin_atari_write();uint8_t b[512];flash_fs_read_FAT_sector(lba,b);b[offset]=value;flash_fs_write_FAT_sector(lba,b);flash_fs_end_atari_write();};
 std::vector<uint8_t> bad;
 if(copies==2){corrupt(firstfat+fatsize,10,0x55);bad=disk();olderase=erased;assert(!storage_optimize_disk(&freed));assert(disk()==bad&&erased==olderase);puts("PASS mismatching FAT copies: refused without writes");}memcpy((void*)0x10100000,baseline.data(),baseline.size());
 // FAT16 entry 1 flags are metadata, not an end-of-chain value.
 for(unsigned flags: {0xffffu,0x7fffu,0xbfffu,0x3fffu}){
  for(unsigned copy=0;copy<copies;++copy){corrupt(firstfat+copy*fatsize,2,flags&255);corrupt(firstfat+copy*fatsize,3,flags>>8);}
  auto flagged=disk();olderase=erased;assert(storage_optimize_disk(&freed));assert(freed==0&&erased==olderase&&disk()==flagged);
  verify("/Keep.gb",keep);printf("PASS FAT[1]=%04X accepted; flags and all bytes retained\n",flags);
 }
 for(unsigned copy=0;copy<copies;++copy)corrupt(firstfat+copy*fatsize,2,0xfe);
 bad=disk();olderase=erased;assert(!storage_optimize_disk(&freed));assert(disk()==bad&&erased==olderase);assert(strstr(storage_error(),"FAT reserved"));puts("PASS malformed reserved bits refused without writes");
 memcpy((void*)0x10100000,baseline.data(),baseline.size());
 auto raw=(uint16_t*)0x10100008;raw[20000]=raw[0];bad=disk();olderase=erased;assert(!storage_optimize_disk(&freed));assert(disk()==bad&&erased==olderase);puts("PASS aliased FTL pages: refused without writes");memcpy((void*)0x10100000,baseline.data(),baseline.size());
 // Find a real root directory's start cluster, then mark it free in both FATs.
 uint8_t root[512];flash_fs_read_FAT_sector(firstfat+copies*fatsize,root);unsigned dircluster=0;
 for(unsigned e=0;e<512;e+=32)if(root[e]&&root[e]!=0xe5&&root[e+11]!=0x0f&&(root[e+11]&16)){dircluster=root[e+26]|root[e+27]<<8;break;}
 assert(dircluster>=2);
 for(unsigned copy=0;copy<copies;++copy){unsigned lba=firstfat+copy*fatsize+dircluster/256,off=2*(dircluster%256);corrupt(lba,off,0);corrupt(lba,off+1,0);}
 bad=disk();olderase=erased;assert(!storage_optimize_disk(&freed));assert(disk()==bad&&erased==olderase);puts("PASS referenced free directory cluster: refused without writes");memcpy((void*)0x10100000,baseline.data(),baseline.size());
 for(unsigned round=0;round<2;++round){flash_fs_begin_atari_write();writefile("/New.gb",trash);assert(f_unlink("/New.gb")==FR_OK);flash_fs_end_atari_write();assert(storage_optimize_disk(&freed));verify("/Keep.gb",keep);verify("/.saves/Long game name/01_0000.sav",small);}
 puts("PASS two copy/delete/optimize cycles; file contents retained");export_disk((std::string(argv[1])+"/optimize-after.bin").c_str());
}
