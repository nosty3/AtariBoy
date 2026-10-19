#include "storage.h"
#include "storage/fatfs_disk.h"
#include "storage/fatfs/ff.h"
#include <stdlib.h>
#include <string.h>
#include "hardware/regs/addressmap.h"
static FATFS fs;
static char error[40];
const char *storage_error(void){return error;}
bool storage_mount(void){
 if(!fatfs_is_mounted() && !mount_fatfs_disk()){strcpy(error,"Connect USB to prepare disk");return false;}
 if(f_mount(&fs,"",1)!=FR_OK){strcpy(error,"Cannot read FAT disk");return false;}
 error[0]=0;return true;
}
uint8_t storage_rom_byte(const uint16_t *map,uint32_t addr){
 return *(const uint8_t *)(uintptr_t)(XIP_BASE+0x100000u+((uint32_t)map[addr>>9]<<9)+(addr&511));
}
bool storage_rom(const char *path,uint16_t **map,uint32_t *size,uint8_t header[512]){
 FIL f;UINT got;uint8_t byte;
 *map=0;*size=0;
 if(f_open(&f,path,FA_READ)!=FR_OK){strcpy(error,"Cannot open ROM");return false;}
 uint32_t n=f_size(&f);
 if(n<32768 || n>8*1024*1024 || (n&(n-1))){strcpy(error,"Invalid ROM file size");f_close(&f);return false;}
 uint16_t *m=malloc((n/512)*sizeof *m);
 if(!m){strcpy(error,"Not enough RAM for ROM map");f_close(&f);return false;}
 for(uint32_t i=0;i<n/512;++i){
  // A one-byte read makes FatFs record the actual sector in FIL.sect.
  if(f_lseek(&f,i*512)!=FR_OK || f_read(&f,&byte,1,&got)!=FR_OK || got!=1)goto fail;
  m[i]=flash_fs_entry(f.sect);
  if(!m[i])goto fail;
 }
 f_close(&f);for(unsigned i=0;i<512;++i)header[i]=storage_rom_byte(m,i);
 *map=m;*size=n;error[0]=0;return true;
fail:
 f_close(&f);free(m);strcpy(error,"Damaged ROM cluster chain");return false;
}

bool storage_erase_disk(void){
 f_mount(0,"",0);
 extern bool flashfs_is_mounted;
 flashfs_is_mounted=false;
 if(!flash_fs_erase_all())return false;
 if(!mount_fatfs_disk())return false;
 flash_fs_begin_atari_write();
 BYTE work[FF_MAX_SS];
 MKFS_PARM opt={.fmt=FM_FAT|FM_SFD,.n_fat=2,.align=1,.n_root=512,.au_size=2048};
 extern bool fatfs_formatting;
 fatfs_formatting=true;
 FRESULT result=f_mkfs("",&opt,work,sizeof work);
 fatfs_formatting=false;
 if(result==FR_OK)result=f_mount(&fs,"",1);
 if(result==FR_OK)result=f_setlabel("ATARIBOY");
 flash_fs_end_atari_write();
 return result==FR_OK;
}

extern const char *disk_optimize(const FATFS *,uint32_t *,char *,unsigned);
bool storage_optimize_disk(uint32_t *released){
 *released=0;
 if(!flash_fs_validate_map()){strcpy(error,"Invalid FLASH map - no changes");return false;}
 if(!storage_mount())return false;
 const char *why=disk_optimize(&fs,released,error,sizeof error);
 if(why){if(why!=error)strncpy(error,why,sizeof error-1);error[sizeof error-1]=0;return false;}
 error[0]=0;return true;
}
