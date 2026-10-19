#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum SaveResult { SAVE_OK, SAVE_EMPTY, SAVE_INVALID, SAVE_FULL, SAVE_IO };
const char *saves_game_name(void); // basename without extension of active game
int saves_has_game(const char *parent,const char *name); // read-only; preserves active game/slot
void saves_select_game(const char *rom_path);
const uint8_t *saves_game_hash(void);
unsigned saves_slot(void);
enum SaveResult battery_load(void);
enum SaveResult battery_save(void);
void saves_move(int delta);
enum SaveResult saves_write(void);
enum SaveResult saves_load(void);
struct SaveSlot { uint64_t number; uint8_t present; char description[31]; };
enum SaveResult saves_list(struct SaveSlot slots[10]);
void saves_set_slot(unsigned slot);
enum SaveResult saves_preview(unsigned slot,uint8_t image[896]);
enum SaveResult saves_delete(unsigned slot);
enum SaveResult saves_copy(unsigned slot);
enum SaveResult saves_paste(unsigned slot);
enum SaveResult saves_describe(unsigned slot,const char description[30]);
void saves_clear_clipboard(void);
void saves_forget_game(void);
#ifdef __cplusplus
}
#endif
