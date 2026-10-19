// Separate USB/Atari operating modes, following A8PicoCart main.c.
#include "pico/stdlib.h"
#include "tusb.h"
#include "storage/fatfs_disk.h"
void msc_flush_task(void);
void usb_disk_mode(void){
 flash_fs_set_writable(true);
 if(!mount_fatfs_disk() && flash_fs_blank())create_fatfs_disk();
 tud_init(BOARD_TUD_RHPORT);
 for(;;){
  tud_task();msc_flush_task();
  uint8_t discard[64];if(tud_cdc_available())tud_cdc_read(discard,sizeof discard);
 }
}
