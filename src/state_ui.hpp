#pragma once
#include "saves.h"
#include <stdint.h>
struct StateUI {
 SaveSlot slots[10]{};uint8_t preview[896]{};char edit[31]{},message[41]{};
 unsigned row=0,cursor=0,previous=255,repeat=0;int direction=0;
 bool editing=false,clipboard=false;unsigned operation=0;
 void open();void refresh();void draw(uint8_t *out);void move(int d);
 // true means Esc exit. Load/save/filesystem operations are deferred until published.
 bool update(const uint8_t *input);
 SaveResult perform();
};
