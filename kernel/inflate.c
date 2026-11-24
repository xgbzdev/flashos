/* FlashKernel DEFLATE decoder (for compressed CWS movies and lossless bitmaps).
 * Copyright (c) 2026 FlashOS. All rights reserved.
 */
#include "kernel.h"

typedef struct {
    const u8 *src, *end;
    u32 bb;
    int bc, err;
    u8 *dst, *dstart, *dend;
} inf_t;

typedef struct { u16 count[16]; u16 sym[288]; } huff_t;

static int bit(inf_t *s) {
    if (!s->bc) {
        if (s->src >= s->end) { s->err = 1; return 0; }
        s->bb = *s->src++;
        s->bc = 8;
    }
    int b = s->bb & 1;
    s->bb >>= 1;
    s->bc--;
    return b;
}
static u32 bits(inf_t *s, int n) {
    u32 v = 0;
    for (int i = 0; i < n; i++) v |= (u32)bit(s) << i;
    return v;
}

static void build(huff_t *h, const u8 *len, int n) {
    u16 offs[16];
    memset(h->count, 0, sizeof h->count);
    for (int i = 0; i < n; i++) h->count[len[i]]++;
    h->count[0] = 0;
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = offs[i] + h->count[i];
    for (int i = 0; i < n; i++)
        if (len[i]) h->sym[offs[len[i]]++] = (u16)i;
}

static int decode(inf_t *s, huff_t *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= bit(s);
        int count = h->count[len];
        if (code - count < first) return h->sym[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    s->err = 1;
    return -1;
}

static const u16 lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                              35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const u8 lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
                            3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const u16 dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129,
                              193, 257, 385, 513, 769, 1025, 1537, 2049, 3073,
                              4097, 6145, 8193, 12289, 16385, 24577};
static const u8 dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                            6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int codes(inf_t *s, huff_t *lh, huff_t *dh) {
    for (;;) {
        int sym = decode(s, lh);
        if (s->err) return 0;
        if (sym < 256) {
            if (s->dst >= s->dend) return 0;
            *s->dst++ = (u8)sym;
        } else if (sym == 256) {
            return 1;
        } else {
            sym -= 257;
            if (sym >= 29) return 0;
            u32 len = lbase[sym] + bits(s, lext[sym]);
            int ds = decode(s, dh);
            if (ds < 0 || ds >= 30) return 0;
            u32 dist = dbase[ds] + bits(s, dext[ds]);
            if (s->err || dist > (u32)(s->dst - s->dstart) || len > (u32)(s->dend - s->dst)) return 0;
            const u8 *from = s->dst - dist;
            while (len--) *s->dst++ = *from++;
        }
    }
}

int inflate_raw(const u8 *src, u32 srclen, u8 *dst, u32 dstlen, u32 *outlen) {
    static huff_t lh, dh;
    inf_t s = {src, src + srclen, 0, 0, 0, dst, dst, dst + dstlen};
    int last;
    do {
        last = bit(&s);
        int type = bits(&s, 2);
        if (s.err) break;
        if (type == 0) {
            s.bc = 0;
            if (s.end - s.src < 4) { s.err = 1; break; }
            u32 len = s.src[0] | (s.src[1] << 8);
            s.src += 4;
            if (len > (u32)(s.end - s.src) || len > (u32)(s.dend - s.dst)) { s.err = 1; break; }
            memcpy(s.dst, s.src, len);
            s.dst += len;
            s.src += len;
        } else if (type == 1) {
            u8 l[288 + 30];
            int i = 0;
            for (; i < 144; i++) l[i] = 8;
            for (; i < 256; i++) l[i] = 9;
            for (; i < 280; i++) l[i] = 7;
            for (; i < 288; i++) l[i] = 8;
            build(&lh, l, 288);
            for (i = 0; i < 30; i++) l[i] = 5;
            build(&dh, l, 30);
            if (!codes(&s, &lh, &dh)) { s.err = 1; break; }
        } else if (type == 2) {
            static const u8 order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            u8 l[320];
            int nlen = bits(&s, 5) + 257, ndist = bits(&s, 5) + 1, ncode = bits(&s, 4) + 4;
            if (nlen > 286 || ndist > 30) { s.err = 1; break; }
            memset(l, 0, 19);
            for (int i = 0; i < ncode; i++) l[order[i]] = (u8)bits(&s, 3);
            build(&lh, l, 19);
            int idx = 0;
            while (idx < nlen + ndist && !s.err) {
                int sym = decode(&s, &lh);
                if (sym < 0) break;
                if (sym < 16) { l[idx++] = (u8)sym; continue; }
                int rep, val = 0;
                if (sym == 16) {
                    if (!idx) { s.err = 1; break; }
                    val = l[idx - 1];
                    rep = 3 + bits(&s, 2);
                } else if (sym == 17) rep = 3 + bits(&s, 3);
                else rep = 11 + bits(&s, 7);
                if (idx + rep > nlen + ndist) { s.err = 1; break; }
                while (rep--) l[idx++] = (u8)val;
            }
            if (s.err) break;
            build(&lh, l, nlen);
            build(&dh, l + nlen, ndist);
            if (!codes(&s, &lh, &dh)) { s.err = 1; break; }
        } else {
            s.err = 1;
        }
    } while (!last && !s.err);
    *outlen = (u32)(s.dst - s.dstart);
    return !s.err;
}
