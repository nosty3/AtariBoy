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

#ifndef __FLASH_FS_H__
#define __FLASH_FS_H__

#include <stdbool.h>
#include <stdint.h>
void flash_fs_set_writable(bool);
void flash_fs_begin_atari_write(void);
void flash_fs_end_atari_write(void);
bool flash_fs_blank(void);
uint16_t flash_fs_entry(uint32_t);

int flash_fs_mount();
void flash_fs_create();
void flash_fs_sync();
void flash_fs_read_FAT_sector(uint16_t fat_sector, void *buffer);
void flash_fs_write_FAT_sector(uint16_t fat_sector, const void *buffer);
bool flash_fs_verify_FAT_sector(uint16_t fat_sector, const void *buffer);

// USB-only mode can borrow an unused, aligned 61440-byte cartridge buffer.
void flash_fs_set_usb_buffer(void *buffer);
bool flash_fs_no_space(void);

#endif

bool flash_fs_erase_all(void);

// Offline optimizer: validates unique physical pages before any changes.
bool flash_fs_validate_map(void);
bool flash_fs_discard_free(const uint8_t *free_lbas, uint32_t *released);
