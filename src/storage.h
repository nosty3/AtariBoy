#pragma once
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
bool storage_mount(void);
bool storage_erase_disk(void);
bool storage_optimize_disk(uint32_t *released);
const char *storage_error(void);
// Allocates a compact ROM-sector map. Caller owns it; file bytes stay in flash.
bool storage_rom(const char *path,uint16_t **map,uint32_t *size,uint8_t header[512]);
uint8_t storage_rom_byte(const uint16_t *map,uint32_t addr);
#ifdef __cplusplus
}
#endif
