#pragma once
#include <stdint.h>
// Monotonic session clock; no game/reset hooks, no writes to flash.
struct ColorClock {
 uint32_t last=0,elapsed=0,random=0x6d2b79f5u;uint8_t minutes=0;bool started=false;
 void tick(uint32_t now,uint8_t interval,uint8_t &hue,bool allow){
  if(!started){started=true;last=now;minutes=interval;random^=now;return;}
  uint32_t dt=now-last;last=now;
  if(interval!=minutes){minutes=interval;elapsed=0;return;}
  if(!minutes){elapsed=0;return;}
  uint32_t period=uint32_t(minutes)*60000000u;
  // No catch-up burst after an options visit; expire once upon returning.
  if(dt>=period-elapsed)elapsed=period;else elapsed+=dt;
  if(allow&&elapsed>=period){
   random^=random<<13;random^=random>>17;random^=random<<5;
   if(!random)random=0x6d2b79f5u;
   hue=(hue+1+random%15)&15;elapsed=0;
  }
 }
};
