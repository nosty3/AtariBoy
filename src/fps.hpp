#pragma once
#include <stdint.h>
struct Fps {
 bool enabled=false;
 uint32_t start=0,frames=0,tenths=0;
 void reset(uint32_t now){start=now;frames=tenths=0;}
 void toggle(uint32_t now){enabled=!enabled;reset(now);}
 void completed(bool diagnostic=false){if(enabled||diagnostic)++frames;}
 void update(uint32_t now){
  uint32_t elapsed=now-start;
  if(elapsed>=1000000u){tenths=(uint64_t(frames)*10000000u+elapsed/2)/elapsed;frames=0;start=now;}
 }
};
