/* FlashKernel music player: streams an MP3 file into the sound mixer, a
 * little at a time, so a long song needs no more memory than the file.
 * Copyright (c) 2026 FlashOS. All rights reserved. */
#include "kernel.h"
#include "audio.h"
#include "swf.h"

static const u8 *m_data;      /* the whole MP3 file */
static u32 m_size, m_pos;
static int m_playing, m_done;
static u32 m_bitrate;         /* bits per second, from the first frame */
static u8 *m_buf;             /* one buffer for files read from the disk */
static u32 m_cap;

static const u16 BR[2][16] = {     /* MPEG-1 and MPEG-2 layer III, kbit/s */
    {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
    {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0}};

static u32 first_frame(void) {
    u32 i = 0;
    if (m_size > 10 && m_data[0] == 'I' && m_data[1] == 'D' && m_data[2] == '3')   /* skip an ID3 tag */
        i = 10 + ((m_data[6] & 127) << 21 | (m_data[7] & 127) << 14 | (m_data[8] & 127) << 7 | (m_data[9] & 127));
    for (; i + 4 < m_size; i++)
        if (m_data[i] == 0xFF && (m_data[i + 1] & 0xE0) == 0xE0) {
            int v1 = (m_data[i + 1] >> 3) & 1, br = m_data[i + 2] >> 4;
            m_bitrate = BR[v1 ? 0 : 1][br] * 1000u;
            if (m_bitrate) return i;
        }
    return m_size;
}

static void *m_alloc(u32 n) {
    if (!m_buf || m_cap < n) return NULL;
    return m_buf;
}

void music_stop(void) {
    if (m_playing || m_data) audio_stream_mp3_reset();
    m_data = NULL;
    m_size = m_pos = 0;
    m_playing = 0;
}
/* every SWF starts with a new memory: forget the buffer */
void music_forget(void) { music_stop(); m_buf = NULL; m_cap = 0; }

/* "ok" or why not */
const char *music_open(const char *name) {
    music_stop();
    kfile f;
    if (fs_find(name, &f)) { m_data = f.data; m_size = f.size; }
    else {
        if (!m_buf) { m_cap = 24u << 20; m_buf = amalloc(m_cap); }
        if (!m_buf) { m_cap = 8u << 20; m_buf = amalloc(m_cap); }
        if (!m_buf) return "not enough memory";
        u32 n;
        u8 *d = docs_read(name, &n, m_alloc);
        if (!d) return "the file is missing or too big";
        m_data = d; m_size = n;
    }
    m_pos = first_frame();
    if (m_pos >= m_size) { m_data = NULL; return "this is not an MP3 file"; }
    audio_stream_mp3_reset();
    m_playing = 1;
    m_done = 0;
    return "ok";
}
void music_pause(int p) { if (m_data) m_playing = !p; }
/* called every frame: keep about half a second queued */
void music_tick(void) {
    if (!m_data || !m_playing) return;
    int n = 0;
    while (audio_stream_queued() < (u32)audio_rate() / 2 && n++ < 8) {
        if (m_pos >= m_size) { m_playing = 0; m_done = 1; return; }
        u32 chunk = m_size - m_pos < 2048 ? m_size - m_pos : 2048;
        audio_stream_mp3(m_data + m_pos, chunk);
        m_pos += chunk;
    }
}
/* seconds (approximate, from the bitrate) */
int music_length(void) { return m_bitrate ? (int)(m_size / (m_bitrate / 8)) : 0; }
int music_position(void) { return m_bitrate ? (int)(m_pos / (m_bitrate / 8)) : 0; }
int music_playing(void) { return m_playing; }
int music_done(void) { int d = m_done; return d; }
void music_seek(int sec) {
    if (!m_data || !m_bitrate) return;
    u64 p = (u64)(u32)sec * (m_bitrate / 8);
    if (p >= m_size) p = m_size - 1;
    m_pos = (u32)p;
    audio_stream_mp3_reset();
}
