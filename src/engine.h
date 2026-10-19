#pragma once
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
extern uint8_t gb_screen[144*40];
extern uint32_t engine_error, engine_error_addr, engine_frames;
int engine_start(const char *path);
const char *engine_message(void);
int engine_reset(void); // restart current ROM; retain cartridge RAM/RTC
int engine_frame(uint8_t joypad);
size_t engine_state_size(void);
typedef int (*engine_io)(void *,void *,size_t);
int engine_transfer(engine_io io,void *context,int loading);
uint32_t engine_payload_size(void);
uint32_t engine_battery_size(void);
int engine_battery_transfer(engine_io io,void *context,int loading);
uint32_t engine_rom_size(void);
uint32_t engine_ram_size(void);
void engine_rom_hash(uint8_t hash[32]);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C"
#endif
void engine_set_legacy_load(int value);

#ifdef __cplusplus
extern "C"
#endif
void engine_stop(void);
