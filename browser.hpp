#pragma once
#include <stdint.h>
struct Browser {
 struct Entry {char name[256];bool dir,has_save,has_options;};
 // Only retain the visible page. Alphabetical rank is found by rescanning FAT.
 Entry page[15];char path[1024]="/";unsigned selected=0,top=0,count=0,page_count=0,ticks=0;
 char message[40]={};bool ready=false;
 void note_options(const char *basename);void refresh_saves();void note_save(const char *basename);
 bool neighbour(const Entry& anchor,int direction,Entry& out);
 void init();void scan();void refresh();void move(int delta);void back();
 bool choose(char *file,unsigned capacity);
 void draw(uint8_t *screen,bool resume=false);
};
