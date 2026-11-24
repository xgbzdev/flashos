/* FlashKernel audio: Intel AC'97 driver, software mixer and the Flash sound
 * decoders (raw PCM, Flash ADPCM, MP3).
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * MP3 decoding uses minimp3 (public domain, CC0 - see minimp3.h).
 * The card is fed by polling: audio_pump() keeps ~150 ms of sound queued.
 */
#include "kernel.h"
#include "audio.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "minimp3.h"

/* ================================================================ mixer */
#define MAXV 32
typedef struct {
    const s16 *pcm;     /* interleaved stereo */
    u32 frames;
    u32 pos;            /* 16.16 */
    u32 step;           /* 16.16 */
    int loops, vol, active, tag, sound;
} voice;
static voice V[MAXV];
static int out_rate = 44100;
static int master = 256, muted;
void audio_set_master(int v) { master = v < 0 ? 0 : v > 256 ? 256 : v; }
int audio_get_master(void) { return master; }
void audio_set_muted(int m) { muted = m; }
int audio_get_muted(void) { return muted; }

/* streaming sound (timeline music): ring of stereo frames at stream rate */
#define SRING (1 << 16) /* 1.5 s of stereo at 44.1 kHz */
static s16 sring[SRING * 2];
static u32 s_w, s_r;    /* frames written / read (16.16 for read) */
static u32 s_rfrac, s_step;
static int s_active;
u32 diag_voices, diag_voice_frames, diag_pushed, diag_mixed, diag_pumps, diag_restarts, diag_blocks, diag_mp3frames;

static u32 step_for(int rate) { return ((u32)rate << 16) / (u32)out_rate; } /* rates < 65536 */

int audio_play(const s16 *pcm, u32 frames, int rate, int loops, int vol, int tag, int sound) {
    if (!pcm || !frames) return -1;
    diag_voices++;
    for (int i = 0; i < MAXV; i++)
        if (!V[i].active) {
            V[i].pcm = pcm;
            V[i].frames = frames;
            V[i].pos = 0;
            V[i].step = step_for(rate);
            V[i].loops = loops < 1 ? 1 : loops;
            V[i].vol = vol;
            V[i].tag = tag;
            V[i].sound = sound;
            V[i].active = 1;
            return i;
        }
    return -1;
}
int audio_playing(int sound) {
    for (int i = 0; i < MAXV; i++) if (V[i].active && V[i].sound == sound) return 1;
    return 0;
}
void audio_stop_sound(int sound) { for (int i = 0; i < MAXV; i++) if (V[i].sound == sound) V[i].active = 0; }
void audio_stop_tag(int tag) { for (int i = 0; i < MAXV; i++) if (V[i].tag == tag) V[i].active = 0; }
void audio_set_volume(int tag, int vol) { for (int i = 0; i < MAXV; i++) if (V[i].active && V[i].tag == tag) V[i].vol = vol; }
void audio_stop_all(void) {
    for (int i = 0; i < MAXV; i++) V[i].active = 0;
    s_w = s_r = s_rfrac = 0;
    s_active = 0;
}

void audio_stream_push(const s16 *pcm, u32 frames, int rate) {
    diag_pushed += frames;
#ifdef HOSTED_TRACE
    fprintf(stderr, "push t=%u frames=%u queued=%u peak=%d\n", plat_ms(), frames, s_w - s_r, pcm[400]);
#endif
    s_step = step_for(rate);
    s_active = 1;
    if (s_w - s_r + frames > SRING) return; /* too far ahead: drop */
    for (u32 i = 0; i < frames; i++) {
        u32 k = (s_w + i) & (SRING - 1);
        sring[k * 2] = pcm[i * 2];
        sring[k * 2 + 1] = pcm[i * 2 + 1];
    }
    s_w += frames;
}
u32 audio_stream_queued(void) { return s_w - s_r; }

static void mix(s16 *out, u32 frames) {
    static s32 acc[4096 * 2];
    if (frames > 4096) frames = 4096;
    memset(acc, 0, frames * 8);
    for (int v = 0; v < MAXV; v++) {
        voice *c = &V[v];
        if (!c->active) continue;
        for (u32 i = 0; i < frames; i++) {
            u32 p = c->pos >> 16;
            if (p >= c->frames) {
                if (--c->loops > 0) { c->pos = 0; p = 0; }
                else { c->active = 0; break; }
            }
            diag_voice_frames++;
            acc[i * 2] += c->pcm[p * 2] * c->vol >> 8;
            acc[i * 2 + 1] += c->pcm[p * 2 + 1] * c->vol >> 8;
            c->pos += c->step;
        }
    }
    if (s_active) {
        for (u32 i = 0; i < frames && s_r < s_w; i++) {
            u32 k = s_r & (SRING - 1);
            acc[i * 2] += sring[k * 2];
            acc[i * 2 + 1] += sring[k * 2 + 1];
            diag_mixed++;
            s_rfrac += s_step;
            s_r += s_rfrac >> 16;
            s_rfrac &= 0xFFFF;
        }
    }
    for (u32 i = 0; i < frames * 2; i++) {
        s32 x = muted ? 0 : acc[i] * master >> 8;
        out[i] = (s16)(x > 32767 ? 32767 : x < -32768 ? -32768 : x);
    }
}

/* ================================================================ decoders */
/* Decode a Flash sound to interleaved stereo s16. Returns frames. */
static s16 *to_stereo(const s16 *in, u32 frames, int stereo) {
    s16 *o = audio_alloc(frames * 4);
    if (!o) return NULL;
    for (u32 i = 0; i < frames; i++) {
        o[i * 2] = in[stereo ? i * 2 : i];
        o[i * 2 + 1] = in[stereo ? i * 2 + 1 : i];
    }
    return o;
}

static const int adpcm_index2[] = {-1, 2};
static const int adpcm_index3[] = {-1, -1, 2, 4};
static const int adpcm_index4[] = {-1, -1, -1, -1, 2, 4, 6, 8};
static const int adpcm_index5[] = {-1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 4, 6, 8, 10, 13, 16};
static const int *adpcm_tabs[] = {adpcm_index2, adpcm_index3, adpcm_index4, adpcm_index5};
static const int step_tab[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80,
    88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
    598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749,
    3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635,
    13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

typedef struct { const u8 *d; u32 n, pos; } bitr;
static u32 br_get(bitr *b, int n) {
    u32 v = 0;
    while (n--) {
        u32 byte = b->pos >> 3;
        int bit = byte < b->n ? (b->d[byte] >> (7 - (b->pos & 7))) & 1 : 0;
        v = (v << 1) | (u32)bit;
        b->pos++;
    }
    return v;
}
static s32 br_sget(bitr *b, int n) {
    u32 v = br_get(b, n);
    if (v & (1u << (n - 1))) v |= ~0u << n;
    return (s32)v;
}

static u32 adpcm_decode(const u8 *d, u32 n, int stereo, u32 want, s16 *out) {
    bitr b = {d, n, 0};
    int bits = (int)br_get(&b, 2) + 2;
    const int *tab = adpcm_tabs[bits - 2];
    int ch = stereo ? 2 : 1;
    u32 got = 0;
    while (got < want && b.pos + 22 * ch <= n * 8) {
        s32 smp[2];
        int idx[2];
        for (int c = 0; c < ch; c++) {
            smp[c] = br_sget(&b, 16);
            idx[c] = (int)br_get(&b, 6);
            out[got * ch + c] = (s16)smp[c];
        }
        got++;
        for (int k = 1; k < 4096 && got < want && b.pos + (u32)(bits * ch) <= n * 8; k++, got++) {
            for (int c = 0; c < ch; c++) {
                u32 code = br_get(&b, bits);
                int step = step_tab[idx[c]];
                s32 diff = step >> (bits - 1);
                for (int s = bits - 2, m = 1; s >= 0; s--, m++)
                    if (code & (1u << s)) diff += step >> m;
                if (code & (1u << (bits - 1))) smp[c] -= diff; else smp[c] += diff;
                if (smp[c] > 32767) smp[c] = 32767;
                if (smp[c] < -32768) smp[c] = -32768;
                idx[c] += tab[code & ((1u << (bits - 1)) - 1)];
                if (idx[c] < 0) idx[c] = 0;
                if (idx[c] > 88) idx[c] = 88;
                out[got * ch + c] = (s16)smp[c];
            }
        }
    }
    return got;
}

static int rate_of(int code) { return code == 0 ? 5512 : code == 1 ? 11025 : code == 2 ? 22050 : 44100; }

/* format: SWF sound format, rate code, 16-bit flag, stereo flag */
s16 *audio_decode(int format, int rate_code, int is16, int stereo, u32 count, const u8 *d, u32 n,
                  u32 *frames, int *rate) {
    *rate = rate_of(rate_code);
    *frames = 0;
    if (format == 0 || format == 3) { /* raw */
        u32 bps = (is16 ? 2u : 1u) * (stereo ? 2u : 1u);
        u32 fr = n / bps;
        s16 *tmp = audio_alloc(fr * (stereo ? 4 : 2) + 4);
        if (!tmp) return NULL;
        for (u32 i = 0; i < fr * (stereo ? 2 : 1); i++)
            tmp[i] = is16 ? (s16)(d[i * 2] | (d[i * 2 + 1] << 8)) : (s16)((d[i] - 128) << 8);
        *frames = fr;
        return to_stereo(tmp, fr, stereo);
    }
    if (format == 1) { /* ADPCM */
        if (count > 50000000u) return NULL;
        s16 *tmp = audio_alloc(count * (stereo ? 4 : 2) + 16);
        if (!tmp) return NULL;
        u32 fr = adpcm_decode(d, n, stereo, count, tmp);
        *frames = fr;
        return to_stereo(tmp, fr, stereo);
    }
    if (format == 2) { /* MP3: DefineSound data starts with SeekSamples */
        if (n < 2) return NULL;
        return audio_decode_mp3(d + 2, n - 2, frames, rate);
    }
    return NULL; /* Nellymoser / Speex: not supported */
}

s16 *audio_decode_mp3(const u8 *d, u32 n, u32 *frames, int *rate) {
    static mp3dec_t dec;
    static s16 pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    mp3dec_init(&dec);
    /* first pass: count samples */
    u32 total = 0, off = 0;
    int ch = 0, hz = 0;
    mp3dec_frame_info_t info;
    while (off < n) {
        int s = mp3dec_decode_frame(&dec, d + off, (int)(n - off), NULL, &info);
        if (!info.frame_bytes) break;
        off += (u32)info.frame_bytes;
        if (s > 0) { total += (u32)s; if (!hz) { hz = info.hz; ch = info.channels; } }
    }
    if (!total || !hz) return NULL;
    s16 *out = audio_alloc(total * 4);
    if (!out) return NULL;
    mp3dec_init(&dec);
    u32 pos = 0;
    off = 0;
    while (off < n && pos < total) {
        int s = mp3dec_decode_frame(&dec, d + off, (int)(n - off), pcm, &info);
        if (!info.frame_bytes) break;
        off += (u32)info.frame_bytes;
        for (int i = 0; i < s && pos < total; i++, pos++) {
            out[pos * 2] = pcm[info.channels == 2 ? i * 2 : i];
            out[pos * 2 + 1] = pcm[info.channels == 2 ? i * 2 + 1 : i];
        }
    }
    (void)ch;
    *frames = pos;
    *rate = hz;
    return out;
}

/* streaming MP3: decoder state survives between SoundStreamBlocks */
static mp3dec_t sdec;
static u8 sbuf[16384];
static u32 sbuf_n;
void audio_stream_mp3_reset(void) { mp3dec_init(&sdec); sbuf_n = 0; }
void audio_stream_mp3(const u8 *d, u32 n) {
    diag_blocks++;
    static s16 pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    static s16 st[MINIMP3_MAX_SAMPLES_PER_FRAME];
    if (sbuf_n + n > sizeof sbuf) sbuf_n = 0;
    memcpy(sbuf + sbuf_n, d, n);
    sbuf_n += n;
    u32 off = 0;
    mp3dec_frame_info_t info;
    while (off < sbuf_n) {
        int s = mp3dec_decode_frame(&sdec, sbuf + off, (int)(sbuf_n - off), pcm, &info);
        if (!info.frame_bytes) break;
        off += (u32)info.frame_bytes;
        diag_mp3frames++;
        if (s > 0) {
            for (int i = 0; i < s; i++) {
                st[i * 2] = pcm[info.channels == 2 ? i * 2 : i];
                st[i * 2 + 1] = pcm[info.channels == 2 ? i * 2 + 1 : i];
            }
            audio_stream_push(st, (u32)s, info.hz);
        }
    }
    memmove(sbuf, sbuf + off, sbuf_n - off);
    sbuf_n -= off;
}

/* ================================================================ AC'97 driver */
#define NBUF 32
#define BUFFR 1024 /* stereo frames per buffer */

#ifndef HOSTED
static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(p)); }
static inline void outl(u16 p, u32 v) { __asm__ volatile("outl %0, %1" : : "a"(v), "Nd"(p)); }
static inline u8 inb(u16 p) { u8 v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline u16 inw(u16 p) { u16 v; __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline u32 inl(u16 p) { u32 v; __asm__ volatile("inl %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static u32 pci_read(int bus, int dev, int fn, int off) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    return inl(0xCFC);
}
static void pci_write(int bus, int dev, int fn, int off, u32 v) {
    outl(0xCF8, 0x80000000u | (bus << 16) | (dev << 11) | (fn << 8) | (off & 0xFC));
    outl(0xCFC, v);
}

static u16 nam, nabm;
static int card_ok;
static struct { u32 addr; u16 samples, flags; } __attribute__((packed)) bdl[NBUF] __attribute__((aligned(8)));
static s16 dbuf[NBUF][BUFFR * 2] __attribute__((aligned(16)));
static int fill_idx;

int audio_init(void) {
    if (card_ok) return 1;
    for (int bus = 0; bus < 8 && !nam; bus++)
        for (int dev = 0; dev < 32 && !nam; dev++) {
            u32 id = pci_read(bus, dev, 0, 0);
            if (id == 0x24158086u || id == 0x24258086u || id == 0x24458086u || id == 0x24858086u ||
                id == 0x24C58086u || id == 0x24D58086u || id == 0x266E8086u || id == 0x27DE8086u) {
                pci_write(bus, dev, 0, 0x04, pci_read(bus, dev, 0, 0x04) | 0x05); /* I/O + bus master */
                nam = (u16)(pci_read(bus, dev, 0, 0x10) & ~1u);
                nabm = (u16)(pci_read(bus, dev, 0, 0x14) & ~1u);
            }
        }
    if (!nam || !nabm) return 0;
    outl((u16)(nabm + 0x2C), 0x02);                    /* cold reset done */
    for (int i = 0; i < 100 && !(inl((u16)(nabm + 0x30)) & 0x100); i++) plat_sleep(2);
    outw((u16)(nam + 0x00), 0);                         /* reset mixer */
    plat_sleep(5);
    outw((u16)(nam + 0x02), 0x0000);                    /* master volume: max, unmuted */
    outw((u16)(nam + 0x04), 0x0000);                    /* headphones */
    outw((u16)(nam + 0x18), 0x0808);                    /* PCM out: 0 dB */
    if (inw((u16)(nam + 0x28)) & 1) {                   /* variable rate audio */
        outw((u16)(nam + 0x2A), (u16)(inw((u16)(nam + 0x2A)) | 1));
        outw((u16)(nam + 0x2C), 44100);
        out_rate = inw((u16)(nam + 0x2C));
        if (out_rate < 8000) out_rate = 48000;
    } else {
        out_rate = 48000;
    }
    outb((u16)(nabm + 0x1B), 0x02);                     /* reset PCM-out box */
    for (int i = 0; i < 1000 && (inb((u16)(nabm + 0x1B)) & 0x02); i++) {}
    for (int i = 0; i < NBUF; i++) {
        memset(dbuf[i], 0, sizeof dbuf[i]);
        bdl[i].addr = (u32)dbuf[i];
        bdl[i].samples = BUFFR * 2;
        bdl[i].flags = 0;
    }
    outl((u16)(nabm + 0x10), (u32)bdl);
    fill_idx = 0;
    card_ok = 1;
    return 1;
}

int audio_rate(void) { return out_rate; }

void audio_pump(void) {
    if (!card_ok) return;
    diag_pumps++;
    u8 civ = inb((u16)(nabm + 0x14));
    u8 cr = inb((u16)(nabm + 0x1B));
    int running = cr & 1;
    int queued = running ? (fill_idx - civ + NBUF) % NBUF : 0;
    if (!running) fill_idx = civ;
    while (queued < 6) {
        mix(dbuf[fill_idx], BUFFR);
        outb((u16)(nabm + 0x15), (u8)fill_idx);         /* last valid index */
        fill_idx = (fill_idx + 1) % NBUF;
        queued++;
    }
    outw((u16)(nabm + 0x16), 0x1C);                     /* clear status */
    if (!running || (inw((u16)(nabm + 0x16)) & 1)) { outb((u16)(nabm + 0x1B), 0x01); diag_restarts++; } /* run */
}

/* End of a movie: silence everything still queued. The card is not stopped:
 * it plays the silence, halts by itself at the last valid buffer, and
 * audio_pump() resumes it for the next movie (stopping a running AC'97
 * channel and restarting it is not reliable on every implementation). */
void audio_halt(void) {
    audio_stop_all();
    if (!card_ok) return;
    for (int i = 0; i < NBUF; i++) memset(dbuf[i], 0, sizeof dbuf[i]);
}
#else
/* simulator: mix into a capture file instead of a sound card */
#include <stdio.h>
static FILE *cap;
static u32 cap_t;
int audio_init(void) { if (!cap) cap = fopen("sim_audio.raw", "wb"); return 1; }
int audio_rate(void) { return out_rate; }
void audio_pump(void) {
    static s16 buf[4096 * 2];
    u32 now = plat_ms();
    if (!cap_t) cap_t = now;
    u32 want = (now - cap_t) * (u32)out_rate / 1000;
    while (want > 0) {
        u32 n = want > 4096 ? 4096 : want;
        mix(buf, n);
        if (cap) fwrite(buf, 4, n, cap);
        want -= n;
        cap_t += n * 1000 / (u32)out_rate;
        if (!(n * 1000 / (u32)out_rate)) break;
    }
}
void audio_halt(void) { audio_stop_all(); if (cap) fflush(cap); }
#endif
