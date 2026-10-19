/**
 *    _   ___ ___ _       ___          _   
 *   /_\ ( _ ) _ (_)__ _ / __|__ _ _ _| |_ 
 *  / _ \/ _ \  _/ / _/_\ (__/ _` | '_|  _|
 * /_/ \_\___/_| |_\__\_/\___\__,_|_|  \__|
 *                                         
 * 
 * Atari 8-bit cartridge for Raspberry Pi Pico
 *
 * Robin Edwards 2023
 *
 * Needs to be a release NOT debug build for the cartridge emulation to work
 */

#include "ff.h"
#include "diskio.h"
#include "fatfs_disk.h"

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"

bool flashfs_is_mounted = false;
bool fatfs_formatting = false;

bool mount_fatfs_disk()
{
    int err = flash_fs_mount();
    if (err)
        return false;
        
    flashfs_is_mounted = true;
    return true;
}

bool fatfs_is_mounted() { return flashfs_is_mounted; }

void create_fatfs_disk()
{
    flash_fs_create();
    flashfs_is_mounted = flash_fs_mount()==0;
    if(!flashfs_is_mounted)return;

    // now create a fatfs on the flash_fs filesystem :-)

    FATFS fs;           /* Filesystem object */
    FIL fil;            /* File object */
    FRESULT res;        /* API result code */
    BYTE work[FF_MAX_SS]; /* Work area (larger is better for processing time) */

    /* Format the default drive with default parameters */
    printf("making fatfs\n");
    MKFS_PARM opt={.fmt=FM_FAT|FM_SFD,.n_fat=2,.align=1,.n_root=512,.au_size=2048};
    fatfs_formatting=true;
    res = f_mkfs("", &opt, work, sizeof work);
    fatfs_formatting=false;
    if(res!=FR_OK){flashfs_is_mounted=false;return;}
    f_mount(&fs, "", 0);
    f_setlabel("ATARIBOY");
    res = f_open(&fil, "WELCOME.TXT", FA_CREATE_NEW | FA_WRITE);
    f_puts("AtariBoy v.0.45\r\nCopy Game Boy .gb files and folders here.\r\nSafely eject before unplugging.\r\nUSB/FAT based on A8PicoCart by Robin Edwards.\r\n", &fil);
    f_close(&fil);
    f_mount(0, "", 0);
    fatfs_disk_sync();
}

uint32_t fatfs_disk_read(uint8_t* buff, uint32_t sector, uint32_t count)
{	
//	printf("fatfs_disk_read sector=%d, count=%d\n", sector, count);
    if (!flashfs_is_mounted) return RES_ERROR;
    if (count == 0 || sector >= SECTOR_NUM || count > SECTOR_NUM-sector)
			return RES_PARERR;

    /* copy data to buffer */
    for (uint32_t i=0; i<count; i++)
        flash_fs_read_FAT_sector(sector + i, buff + (i*SECTOR_SIZE));
    return RES_OK;
}

uint32_t fatfs_disk_write(const uint8_t* buff, uint32_t sector, uint32_t count)
{
// 	printf("fatfs_disk_write sector=%d, count=%d\n", sector, count);
    if (!flashfs_is_mounted) return RES_ERROR;
    if (count == 0 || sector >= SECTOR_NUM || count > SECTOR_NUM-sector)
        return RES_PARERR;

    /* copy data to buffer */
    for (uint32_t i=0; i<count; i++) {
        flash_fs_write_FAT_sector(sector + i, buff + (i*SECTOR_SIZE));
        // verify
        if (!flash_fs_verify_FAT_sector(sector + i, buff + (i*SECTOR_SIZE))) {
            printf("VERIFY ERROR!");
            return RES_ERROR;
        }
    }
    return RES_OK;
}

void fatfs_disk_sync()
{
    flash_fs_sync();
}