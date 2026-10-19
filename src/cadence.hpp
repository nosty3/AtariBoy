#pragma once
#include <stdint.h>
#include "audio.h"
// EXE-owned clock and source PCM FIFO. Transport consumes independently of video.
struct Cadence {
 static constexpr uint32_t ratio=uint64_t(4194304)*35568*65536/(uint64_t(1773447)*70224);
 int16_t samples[1024]={},right_samples[1024]={};
 uint32_t phase=0,read=0,write=0,next_us=0,fraction=0,epoch=1;
 bool running=false;
 void reset(){phase=read=write=next_us=fraction=0;running=false;++epoch;}
 bool due(uint32_t now){
  if(!running){running=true;next_us=now;}
  if(int32_t(now-next_us)<0)return false;
  // No burst of historical work after IO or sustained overload.
  if(uint32_t(now-next_us)>4*16743u)next_us=now;
  // 70224 * 1,000,000 / 4,194,304, exact rational accumulator.
  constexpr uint64_t period=uint64_t(70224)*1000000u;
  next_us+=uint32_t(period/4194304u);
  fraction+=uint32_t(period%4194304u);
  if(fraction>=4194304u){fraction-=4194304u;++next_us;}
  return true;
 }
 void append(){
  uint32_t available=write-read;
  if(available>1024-AUDIO_FRAME_SAMPLES)read+=available-(1024-AUDIO_FRAME_SAMPLES);
  for(unsigned i=0;i<AUDIO_FRAME_SAMPLES;++i){unsigned index=write++&1023;samples[index]=audio_source[i];if(audio_stereo)right_samples[index]=audio_source_right[i];}
 }
 bool output(bool full=true,bool ntsc=false){
  // APU source remains 312 samples/GB frame. NTSC consumes 262 per raster,
  // at CPU/114 Hz, independently of PAL's full-speed/slowdown preference.
  constexpr uint32_t ntsc_ratio=uint64_t(4194304)*312*114*65536/(uint64_t(1789773)*70224);
  unsigned count=ntsc?262:AUDIO_FRAME_SAMPLES;
  uint32_t step=ntsc?ntsc_ratio:full?ratio:65536u;
  bool interpolate=(audio_effects&2) && (full||ntsc);
  if(write-read<((phase+step*count)>>16)+(interpolate?1u:0u))return false;
  for(unsigned i=0;i<count;++i){int32_t value=samples[read&1023];
   if(interpolate){
    int32_t a=value,b=samples[(read+1)&1023];
    value=(a*(int32_t)(65536-phase)+b*(int32_t)phase+32768)>>16;
   }
   audio_frame[i]=audio_quantize(value);
   if(audio_stereo){int32_t right=right_samples[read&1023];if(interpolate)right=(right*(int32_t)(65536-phase)+right_samples[(read+1)&1023]*(int32_t)phase+32768)>>16;audio_frame_right[i]=audio_quantize(right);}
   phase+=step;read+=phase>>16;phase&=65535;}

  return true;
 }
};
