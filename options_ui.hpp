#pragma once
#include <stdint.h>
struct OptionsUI {
 bool confirm=false,yes=false,erase=false,optimize=false;
 unsigned tab=0,row=0,ticks=0;int held=0;
 char message[40]={};
 unsigned fields()const;
 void open(){confirm=yes=erase=optimize=false;tab=row=ticks=0;held=0;message[0]=0;}
 void update(const uint8_t *p,bool fire_edge);
 void draw(uint8_t *s)const;
 static void help(uint8_t *s);
};
