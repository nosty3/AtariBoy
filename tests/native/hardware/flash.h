#pragma once
#include <stddef.h>
#include <stdint.h>
#define FLASH_SECTOR_SIZE 4096
void flash_range_erase(uint32_t,size_t);
void flash_range_program(uint32_t,const uint8_t*,size_t);
