#pragma once
#include <stdint.h>
#include "saves.h"
#include "controls.hpp"
struct Settings {
 uint8_t hue=0,brightness[4]={0,4,8,14},sound=1,volume=15;
 uint8_t pal_full=1,load_latest=0,boot_last=0;
 uint8_t audio_quality=1,audio_stereo=0,turbo=0;
 uint8_t audio_effects=6; // diagnostic runtime mask, not persisted
 uint8_t channels=15,picture=3,auto_minutes=0,advance_slot=0;
 controls::Mapping controls;
};
extern Settings settings,global_settings;
extern uint8_t detected_stereo;
extern uint8_t clock_turbo; // actual clock; never serialized // 0 mono, 1 stereo, 255 not reported yet
extern char last_game_path[1024];
void settings_defaults();void settings_load();SaveResult settings_save();
bool settings_decode(const uint8_t *data,unsigned size,Settings& out,const Settings *base=nullptr);
unsigned settings_encode(const Settings& in,uint8_t *data);
void settings_apply_audio();void last_game_load();SaveResult last_game_save();

SaveResult settings_load_game(bool fallback=false);
SaveResult settings_save_game();
bool settings_has_game(const char *name);
