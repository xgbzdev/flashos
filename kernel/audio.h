/* FlashKernel audio API.  Copyright (c) 2026 FlashOS. All rights reserved. */
#ifndef FLASH_AUDIO_H
#define FLASH_AUDIO_H
#include "kernel.h"

int audio_init(void);
int audio_rate(void);
void audio_pump(void);
void audio_halt(void);
void audio_set_master(int v);   /* 0..256 */
int audio_get_master(void);
void audio_set_muted(int m);
int audio_get_muted(void);
void *audio_alloc(u32 n); /* provided by the player (ActionScript heap) */

int audio_play(const s16 *pcm, u32 frames, int rate, int loops, int vol, int tag, int sound);
int audio_playing(int sound);
void audio_stop_sound(int sound);
void audio_stop_tag(int tag);
void audio_stop_all(void);
void audio_set_volume(int tag, int vol);

s16 *audio_decode(int format, int rate_code, int is16, int stereo, u32 count, const u8 *d, u32 n,
                  u32 *frames, int *rate);
s16 *audio_decode_mp3(const u8 *d, u32 n, u32 *frames, int *rate);

void audio_stream_push(const s16 *pcm, u32 frames, int rate);
u32 audio_stream_queued(void);
void audio_stream_mp3_reset(void);
void audio_stream_mp3(const u8 *d, u32 n);
#endif
