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

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "flash_fs.h"

// Implements 512 byte FAT sectors on 4096 byte flash sectors.
// Doesn't really implement wear levelling (e.g. the fs_map) so not for heavy use but should be
// fine for the intended use case.

#define HW_FLASH_STORAGE_BASE  (1024 * 1024)
#define MAGIC_8_BYTES "RHE!FS30"

#define NUM_FAT_SECTORS 30716   // 15megs / 512bytes = 30720, but we used 4 records for the header (8 bytes)
#define NUM_FLASH_SECTORS 3840  // 15megs / 4096bytes = 3840
 
typedef struct {
    uint8_t header[8];
    uint16_t sectors[NUM_FAT_SECTORS];  // map FAT sectors -> flash sectors
} sector_map;

static sector_map *map_ram;
static sector_map *usb_buffer;
void flash_fs_set_usb_buffer(void *buffer){usb_buffer=buffer;}
#define fs_map (*map_ram)
static bool writable;
static bool paged;
static bool write_full;
bool flash_fs_no_space(void){return write_full;}
static uint16_t page_data[2048];
static int page_index=-1;
static bool page_dirty;
static void page_flush(void);
static void init_used_bitmap(void);

void flash_fs_set_writable(bool value) {
 if(!value){page_flush();paged=false;page_index=-1;}
 if(!value && map_ram){flash_fs_sync();if(map_ram!=usb_buffer)free(map_ram);map_ram=0;}
 writable=value;
}
static const sector_map *map_read(void) { return map_ram ? map_ram : (const sector_map *)(XIP_BASE+HW_FLASH_STORAGE_BASE); }
uint16_t flash_fs_entry(uint32_t sector) {
 if(sector>=NUM_FAT_SECTORS)return 0;
 unsigned index=sector+4;
 if(paged && page_index==(int)(index/2048))return page_data[index%2048];
 return map_read()->sectors[sector];
}
void flash_fs_begin_atari_write(void){
 paged=true;writable=true;page_index=-1;page_dirty=false;write_full=false;init_used_bitmap();
}
void flash_fs_end_atari_write(void){flash_fs_sync();paged=false;writable=false;page_index=-1;}

bool flash_fs_blank(void) {
 const uint8_t *p=(const uint8_t *)(XIP_BASE+HW_FLASH_STORAGE_BASE);
 for(unsigned i=0;i<15*4096;++i)if(p[i]!=255)return false;
 return true;
}
bool fs_map_needs_written[15];

uint8_t used_bitmap[NUM_FLASH_SECTORS];    // we will use 256 flash sectors for 2048 fat sectors

uint16_t write_sector = 0;   // which flash sector we are writing to
uint8_t write_sector_bitmap = 0;   // 1 for each free 512 byte page on the sector

// each sector entry in the sector map is:
//  13 bits of sector (indexing 8192 4k flash sectors)
//   3 bits of offset (0->7 512 byte FAT sectors in each 4k flash sector)
uint16_t getMapSector(uint16_t mapEntry) { return (mapEntry & 0xFFF8) >> 3; }
uint8_t getMapOffset(uint16_t mapEntry) { return mapEntry & 0x7; }
uint16_t makeMapEntry(uint16_t sector, uint8_t offset) { return (sector << 3) | offset; };

// forward declns
void flash_read_sector(uint16_t sector, uint8_t offset, void *buffer, uint16_t size);
void flash_erase_sector(uint16_t sector);
void flash_write_sector(uint16_t sector, uint8_t offset, const void *buffer, uint16_t size);
void flash_erase_with_copy_sector(uint16_t sector, uint8_t preserve_bitmap);

static void page_flush(void){
 if(!page_dirty)return;
 flash_erase_sector(page_index);
 flash_write_sector(page_index,0,page_data,4096);
 page_dirty=false;
}
static void set_entry(unsigned sector,uint16_t value){
 unsigned index=sector+4,p=index/2048;
 if(paged){
  if(page_index!=(int)p){page_flush();flash_read_sector(p,0,page_data,4096);page_index=p;}
  page_data[index%2048]=value;page_dirty=true;
 }else {fs_map.sectors[sector]=value;fs_map_needs_written[p]=true;}
}

void debug_print_in_use() {
    return;
    // just shows first 1meg
    printf("IN USE-----------------------------------\n");
    for (int i=0; i<16; i++) {
        printf("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
            used_bitmap[i*16+0], used_bitmap[i*16+1], used_bitmap[i*16+2], used_bitmap[i*16+3],
            used_bitmap[i*16+4], used_bitmap[i*16+5], used_bitmap[i*16+6], used_bitmap[i*16+7],
            used_bitmap[i*16+8], used_bitmap[i*16+9], used_bitmap[i*16+10], used_bitmap[i*16+11],
            used_bitmap[i*16+12], used_bitmap[i*16+13], used_bitmap[i*16+14], used_bitmap[i*16+15]);
    }
    printf("END--------------------------------------\n");
}

void write_fs_map()
{
    if(paged){page_flush();return;}
    if(!map_ram || !writable)return;   
    debug_print_in_use();
    for (int i=0; i<15; i++) {
        if (fs_map_needs_written[i]) {
//          printf("Writing FS Map %d\n", i);
            flash_erase_sector(i);
            flash_write_sector(i, 0, (uint8_t*)&fs_map+(4096*i), 4096);
            fs_map_needs_written[i] = false;
        }
    }
}

uint16_t getNextWriteSector()
{
    static uint16_t search_start_pos = 0;
    int i;
    if (write_sector == 0 || write_sector_bitmap == 0)
    {   // first try to find a completely free sector
        for (i=0; i<NUM_FLASH_SECTORS; i++) {
            if (used_bitmap[(i + search_start_pos) % NUM_FLASH_SECTORS] == 0)
                break;
        }
        if (i < NUM_FLASH_SECTORS) {
           write_sector = (i + search_start_pos) % NUM_FLASH_SECTORS;
           write_sector_bitmap = 0xFF;
           flash_erase_sector(write_sector);
        }
        else
        {   // no completely free sector, just return the first sector with space
            for (i=0; i<NUM_FLASH_SECTORS; i++) {
                if (used_bitmap[(i + search_start_pos) % NUM_FLASH_SECTORS] != 0xFF)
                    break;
            }
            if(i==NUM_FLASH_SECTORS)return 0;
            write_sector = (i + search_start_pos) % NUM_FLASH_SECTORS;
            write_sector_bitmap = ~used_bitmap[write_sector];
            flash_erase_with_copy_sector(write_sector, used_bitmap[write_sector]);
        }
        search_start_pos = (i + search_start_pos) % NUM_FLASH_SECTORS;
    }
    // if we get here, then at least one 512 byte page is free on the write_sector
    for (i=0; i<8; i++) {
        if (write_sector_bitmap & (1 << i))
            break;
    }
    // mark the offset used
    write_sector_bitmap &= ~(1 << i);
    return makeMapEntry(write_sector, i);
}

static void init_used_bitmap(void) {
    memset(used_bitmap, 0, NUM_FLASH_SECTORS);
    for (int i=0; i<15; i++)
        used_bitmap[i] = 0xFF;    // first 15 flash sectors used by fs map

    for (int i=0; i<NUM_FAT_SECTORS; i++) {
        uint16_t mapEntry = flash_fs_entry(i);
        if (mapEntry)
            used_bitmap[getMapSector(mapEntry)] |= (1 << getMapOffset(mapEntry));
    }
    write_sector = 0;
}

int flash_fs_mount()
{
 const sector_map *stored=(const sector_map *)(XIP_BASE+HW_FLASH_STORAGE_BASE);
 if(memcmp(stored->header,MAGIC_8_BYTES,8))return 1;
 // Reject invalid sector pointers before touching the allocation bitmap.
 for(unsigned i=0;i<NUM_FAT_SECTORS;++i){
  uint16_t e=stored->sectors[i];
  if(e && (getMapSector(e)<15 || getMapSector(e)>=NUM_FLASH_SECTORS))return 2;
 }
 if(writable&&!paged){
  if(!map_ram)map_ram=usb_buffer?usb_buffer:malloc(sizeof *map_ram);
  if(!map_ram)return 3;
  memcpy(map_ram,stored,sizeof *map_ram);
  memset(fs_map_needs_written,0,sizeof fs_map_needs_written);
  init_used_bitmap();
 }
 return 0;
}

void flash_fs_create()
{
    if(!writable)return;
    if(!map_ram)map_ram=usb_buffer?usb_buffer:malloc(sizeof *map_ram);
    if(!map_ram)return;
    memset(&fs_map, 0, sizeof(fs_map));
    memcpy(fs_map.header, MAGIC_8_BYTES,8);
    for (int i=0; i<15; i++)
        fs_map_needs_written[i] = true;
    write_fs_map();
    init_used_bitmap();
}

void flash_fs_sync()
{
    write_fs_map();
}

void flash_fs_read_FAT_sector(uint16_t fat_sector, void *buffer)
{
    int mapEntry = flash_fs_entry(fat_sector);
    if (mapEntry)
        flash_read_sector(getMapSector(mapEntry), getMapOffset(mapEntry), buffer, 512);
    else
        memset(buffer, 0, 512);
    return;
}

void flash_fs_write_FAT_sector(uint16_t fat_sector, const void *buffer)
{
    if(!writable || (!map_ram&&!paged) || fat_sector>=NUM_FAT_SECTORS)return;
    uint16_t mapEntry = flash_fs_entry(fat_sector);
    if (mapEntry)
    {   // mark any previous flash allocated as unused
        used_bitmap[getMapSector(mapEntry)] &= ~(1 << getMapOffset(mapEntry));
    }
    uint16_t oldEntry=mapEntry;
    mapEntry = getNextWriteSector();
    if(!mapEntry){write_full=true;if(oldEntry)used_bitmap[getMapSector(oldEntry)]|=1<<getMapOffset(oldEntry);return;}
    set_entry(fat_sector,mapEntry);

    used_bitmap[getMapSector(mapEntry)] |= (1 << getMapOffset(mapEntry));

    flash_write_sector(getMapSector(mapEntry), getMapOffset(mapEntry), buffer, 512);
}

bool flash_fs_verify_FAT_sector(uint16_t fat_sector, const void *buffer)
{
    uint8_t read_buf[512];
    flash_fs_read_FAT_sector(fat_sector, read_buf);
    if (memcmp(buffer, read_buf, 512) == 0) return true;
    return false;
}

/* Low level flash functions */

void flash_read_sector(uint16_t sector, uint8_t offset, void *buffer, uint16_t size)
{
//  printf("[FS] READ: %d, %d (%d)\n", sector, offset, size);
    uint32_t fs_start = XIP_BASE + HW_FLASH_STORAGE_BASE;
    uint32_t addr = fs_start + (sector * FLASH_SECTOR_SIZE) + (offset * 512);   
    memcpy(buffer, (unsigned char *)(uintptr_t)addr, size);
}

void flash_erase_sector(uint16_t sector)
{
//  printf("[FS] ERASE: %d\n", sector);
    uint32_t fs_start = HW_FLASH_STORAGE_BASE;
    uint32_t offset = fs_start + (sector * FLASH_SECTOR_SIZE);
    uint32_t ints = save_and_disable_interrupts();   
    flash_range_erase(offset, FLASH_SECTOR_SIZE);  
    restore_interrupts(ints);
}

void flash_write_sector(uint16_t sector, uint8_t offset, const void *buffer, uint16_t size)
{
//  printf("[FS] WRITE: %d, %d (%d)\n", sector, offset, size);
    uint32_t fs_start = HW_FLASH_STORAGE_BASE;
    uint32_t addr = fs_start + (sector * FLASH_SECTOR_SIZE) + (offset * 512);
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(addr, (const uint8_t *)buffer, size);
    restore_interrupts(ints);
}

void flash_erase_with_copy_sector(uint16_t sector, uint8_t preserve_bitmap)
{
//  printf("[FS] ERASE with COPY: %d\n", sector);
    static uint8_t buf[FLASH_SECTOR_SIZE];
    flash_read_sector(sector, 0, buf, FLASH_SECTOR_SIZE);
    flash_erase_sector(sector);
    for (int i=0; i<8; i++) {
        if (preserve_bitmap & (1 << i))
           flash_write_sector(sector, i, buf + (i * 512), 512);
    }
}
// EXE only, after the busy screen was acknowledged by COM. Never touch
// offsets below 1 MiB (AtariBoy, selector and A8PicoCart firmware).
bool flash_fs_erase_all(void){
 page_dirty=false;page_index=-1;paged=false;writable=false;
 if(map_ram){if(map_ram!=usb_buffer)free(map_ram);map_ram=0;}
 memset(fs_map_needs_written,0,sizeof fs_map_needs_written);
 write_sector=0;write_sector_bitmap=0;write_full=false;
 for(unsigned sector=0;sector<NUM_FLASH_SECTORS;++sector){
  flash_erase_sector(sector);
  const volatile uint32_t *v=(const volatile uint32_t *)(XIP_BASE+HW_FLASH_STORAGE_BASE+sector*4096u);
  for(unsigned i=0;i<1024;++i)if(v[i]!=0xffffffffu)return false;
 }
 // Rebuild the translation map one page at a time, without a 60 KiB allocation.
 memset(page_data,0,sizeof page_data);
 for(unsigned sector=0;sector<15;++sector){
  if(sector==0)memcpy(page_data,MAGIC_8_BYTES,8);
  flash_write_sector(sector,0,page_data,sizeof page_data);
  if(sector==0)memset(page_data,0,8);
 }
 return flash_fs_mount()==0;
}

// No relocation: retained file sectors (and cached ROM addresses) never move.
bool flash_fs_validate_map(void){
 if(writable || paged || map_ram || flash_fs_mount()!=0)return false;
 memset(used_bitmap,0,sizeof used_bitmap);
 for(unsigned i=0;i<NUM_FAT_SECTORS;++i){
  uint16_t e=flash_fs_entry(i);if(!e)continue;
  unsigned block=getMapSector(e),bit=1u<<getMapOffset(e);
  if(block<15 || block>=NUM_FLASH_SECTORS || (used_bitmap[block]&bit))return false;
  used_bitmap[block]|=bit;
 }
 return true;
}
bool flash_fs_discard_free(const uint8_t *free_lbas,uint32_t *released){
 *released=0;
 if(!flash_fs_validate_map())return false;
 flash_fs_begin_atari_write();
 for(unsigned i=0;i<NUM_FAT_SECTORS;++i){
  if(!(free_lbas[i>>3]&(1u<<(i&7))))continue;
  uint16_t e=flash_fs_entry(i);if(!e)continue;
  set_entry(i,0);used_bitmap[getMapSector(e)]&=~(1u<<getMapOffset(e));++*released;
 }
 flash_fs_end_atari_write();
 for(unsigned i=0;i<NUM_FAT_SECTORS;++i)
  if((free_lbas[i>>3]&(1u<<(i&7))) && flash_fs_entry(i))return false;
 write_sector=0;write_sector_bitmap=0;
 return true;
}
