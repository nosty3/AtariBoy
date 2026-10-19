#pragma once
#include <stdint.h>
// EXE-only observer. Sequence is the seqlock value (+2 per COM commit).
// All time measurements are EXE observation times, NOT GPIO timestamps.
struct LinkDiagnostic {
 bool valid=false;
 uint8_t previous_frame=0;
 uint32_t previous_seq=0,previous_time=0,start=0;
 uint32_t frames=0,received=0,lost=0,overwritten=0,bad=0,max_gap=0,max_wait=0;
 uint32_t ar10=0,rx10=0,ls=0,ov=0,errors=0,gap10=0,wait10=0;
 void waiting(uint32_t us){if(valid&&us>max_wait)max_wait=us;}
 void observe(uint32_t seq,uint8_t frame,uint32_t now,bool enabled){
  if(!enabled){valid=false;return;}
  if(!valid){*this=LinkDiagnostic{};valid=true;previous_seq=seq;previous_frame=frame;previous_time=start=now;return;}
  uint32_t dt=now-previous_time,commits=(seq-previous_seq)/2;
  unsigned advance=uint8_t(frame-previous_frame);
  if(dt>max_gap)max_gap=dt;
  received+=commits;
  if(commits>1)overwritten+=commits-1;
  // >2s is ambiguous with an 8-bit counter. Duplicate/backward/reset or
  // fewer Atari iterations than COM commits must not be called packet loss.
  if(dt>2000000u||!commits||!advance||advance>=128||advance<commits)++bad;
  else {frames+=advance;lost+=advance-commits;}
  previous_seq=seq;previous_frame=frame;previous_time=now;
  uint32_t window=now-start;
  if(window>=1000000u){
   ar10=uint64_t(frames)*10000000u/window;
   rx10=uint64_t(received)*10000000u/window;
   ls=lost;ov=overwritten;errors=bad;gap10=max_gap/100;wait10=max_wait/100;
   frames=received=lost=overwritten=bad=max_gap=max_wait=0;start=now;
  }
 }
};
