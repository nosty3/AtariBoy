#pragma once
#include <stdint.h>
#include "engine.h"
#ifdef __cplusplus
extern "C" {
#endif
#define AUDIO_FRAME_SAMPLES 312
#define AUDIO_STATE_BYTES 492
extern uint8_t audio_frame[AUDIO_FRAME_SAMPLES];
extern int16_t audio_source[AUDIO_FRAME_SAMPLES],audio_source_right[AUDIO_FRAME_SAMPLES];
extern uint8_t audio_frame_right[AUDIO_FRAME_SAMPLES];
extern int audio_stereo;
void audio_set_stereo(unsigned enabled);
uint8_t audio_quantize(int32_t sample);
extern int audio_enabled, audio_quality;
extern unsigned audio_effects;
void audio_set_effects(unsigned mask);
void audio_set_quality(unsigned quality);
void audio_set_channels(unsigned mask);
void audio_set_volume(unsigned volume);
void audio_set_enabled(int enabled);
void audio_reset(void);
void audio_begin(void);
void audio_end(void);
void audio_tick(uint32_t cycles);
uint8_t audio_read(uint16_t addr);
void audio_write(uint16_t addr,uint8_t value);
int audio_transfer(engine_io io,void *context,int load);
void audio_restore_legacy(const uint8_t *io);
#ifdef __cplusplus
}
#endif
