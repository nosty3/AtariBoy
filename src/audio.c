// Timestamped lightweight APU. EXE only. 312 samples per 70224 GB clocks;
// Cadence resamples for PAL/NTSC; PAL 50-fps mode slows pitch and tempo.
#ifndef MINIGB_APU_AUDIO_FORMAT_S16SYS
#define MINIGB_APU_AUDIO_FORMAT_S16SYS
#endif
#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 18635
#endif
#include "audio.h"
#include "vendor/minigb_apu/minigb_apu.h"
#include <string.h>
int audio_enabled=1, audio_quality=0, audio_stereo=0;
void audio_set_stereo(unsigned enabled){audio_stereo=!!enabled;}
unsigned audio_effects=4;
static unsigned audio_volume=15,audio_channels=15;
void audio_set_channels(unsigned mask){audio_channels=mask&15;}
void audio_set_volume(unsigned volume){audio_volume=volume>15?15:volume;}
void audio_set_enabled(int enabled){audio_enabled=!!enabled;memset(audio_frame,0,sizeof audio_frame);memset(audio_source,0,sizeof audio_source);memset(audio_source_right,0,sizeof audio_source_right);memset(audio_frame_right,0,sizeof audio_frame_right);}
static struct minigb_apu_ctx apu;
void audio_set_effects(unsigned mask){audio_effects=mask&7;apu.quality=!!(mask&1);apu.wave_fix=!!(mask&4);}
void audio_set_quality(unsigned quality){audio_quality=!!quality;audio_set_effects(audio_quality?6:4);}
static uint32_t phase,position;
uint8_t audio_frame[AUDIO_FRAME_SAMPLES];
int16_t audio_source[AUDIO_FRAME_SAMPLES],audio_source_right[AUDIO_FRAME_SAMPLES];
uint8_t audio_frame_right[AUDIO_FRAME_SAMPLES];
void audio_reset(void){
 memset(&apu,0,sizeof apu);apu.audio_mem[0x16]=0x80;
 minigb_apu_audio_init(&apu);audio_set_effects(audio_effects);phase=position=0;memset(audio_frame,0,sizeof audio_frame);memset(audio_source,0,sizeof audio_source);memset(audio_source_right,0,sizeof audio_source_right);memset(audio_frame_right,0,sizeof audio_frame_right);
}
uint8_t audio_read(uint16_t addr){return minigb_apu_audio_read(&apu,addr);}
void audio_write(uint16_t addr,uint8_t value){minigb_apu_audio_write(&apu,addr,value);}
void audio_begin(void){position=0;}
void audio_tick(uint32_t cycles){
 if(!audio_enabled)return;
 phase+=cycles*312;
 while(phase>=70224){
  phase-=70224;int16_t stereo[2];minigb_apu_generate_masked(&apu,stereo,1,audio_channels);
  int32_t mixed=((int32_t)stereo[0]+stereo[1])/2;
  if(position<312){audio_source[position]=audio_stereo?stereo[0]:(int16_t)mixed;if(audio_stereo)audio_source_right[position]=stereo[1];++position;}
 }
}
void audio_end(void){int16_t last=position?audio_source[position-1]:0,right=position?audio_source_right[position-1]:0;while(position<312){audio_source[position]=last;if(audio_stereo)audio_source_right[position]=right;++position;}}
// Single quantization after resampling. Volume belongs here as well: the FIFO
// retains full signed PCM resolution even for quiet playback.
uint8_t audio_quantize(int32_t sample){
 int32_t scaled=sample*(int32_t)audio_volume;
 int delta=scaled>=0?(scaled+8192)/16384:-((-scaled+8192)/16384);
 int level=30+delta;if(level<0)level=0;if(level>60)level=60;
 int code=level+31;if(code>=61)code-=61;return (uint8_t)code;
}
static int word(engine_io io,void *ctx,uint32_t *v,int load){
 uint8_t b[4];for(unsigned i=0;i<4;++i)b[i]=*v>>(8*i);
 if(!io(ctx,b,4))return 0;
 if(load)*v=(uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);
 return 1;
}
int audio_transfer(engine_io io,void *ctx,int load){
 uint32_t v;
#define F(x) do{v=(x);if(!word(io,ctx,&v,load))return 0;if(load)(x)=v;}while(0)
 for(unsigned i=0;i<4;++i){struct chan *c=&apu.chans[i];
 F(c->enabled);F(c->powered);F(c->on_left);F(c->on_right);F(c->volume);F(c->volume_init);
 F(c->freq);F(c->freq_counter);F(c->freq_inc);F(c->val);
 F(c->len.load);F(c->len.enabled);F(c->len.counter);F(c->len.inc);
 F(c->env.step);F(c->env.up);F(c->env.counter);F(c->env.inc);
 F(c->sweep.rate);F(c->sweep.shift);F(c->sweep.down);F(c->sweep.freq);F(c->sweep.counter);F(c->sweep.inc);
 if(i<2){F(c->square.duty);F(c->square.duty_counter);v=0;if(!word(io,ctx,&v,load))return 0;}
 else if(i==2){F(c->wave.sample);v=0;if(!word(io,ctx,&v,load)||!word(io,ctx,&v,load))return 0;}
 else {F(c->noise.lfsr_reg);F(c->noise.lfsr_wide);F(c->noise.lfsr_div);}
 }
 F(apu.vol_l);F(apu.vol_r);F(phase);
#undef F
 if(!io(ctx,apu.audio_mem,sizeof apu.audio_mem))return 0;
 if(load){position=0;memset(audio_frame,0,sizeof audio_frame);memset(audio_source,0,sizeof audio_source);memset(audio_source_right,0,sizeof audio_source_right);memset(audio_frame_right,0,sizeof audio_frame_right);}
 return 1;
}
void audio_restore_legacy(const uint8_t *regs){
 audio_reset();audio_write(0xff26,0x80);
 for(unsigned i=0x10;i<=0x3f;++i)if(i!=0x26)audio_write(0xff00+i,regs[i]);
 audio_write(0xff26,regs[0x26]);
}
