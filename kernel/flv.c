/* FlashKernel FLV video player: FLV container, Sorenson Spark (H.263)
 * video, MP3 / PCM / ADPCM sound.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 */
#include "swf.h"
#include "audio.h"

/* ================================================================ bit reader */
typedef struct { const u8 *p; u32 n, pos; int bad; } bits;

static inline u32 peekb(bits *b, int k) {
    u32 v = 0;
    u32 byte = b->pos >> 3;
    for (int i = 0; i < 4; i++) v = (v << 8) | (byte + i < b->n ? b->p[byte + i] : 0);
    return (v << (b->pos & 7)) >> (32 - k);
}
static inline void skipb(bits *b, int k) { b->pos += (u32)k; if (b->pos > b->n * 8) b->bad = 1; }
static inline u32 getb(bits *b, int k) { u32 v = peekb(b, k); skipb(b, k); return v; }
static inline int getsb(bits *b, int k) { int v = (int)getb(b, k); return v >= (1 << (k - 1)) ? v - (1 << k) : v; }

/* ================================================================ VLC tables (ITU-T H.263) */
static const u8 intra_mcbpc_code[9] = {1, 1, 2, 3, 1, 1, 2, 3, 1};
static const u8 intra_mcbpc_bits[9] = {1, 3, 3, 3, 4, 6, 6, 6, 9};
static const u8 inter_mcbpc_code[28] = {1, 3, 2, 5, 3, 4, 3, 3, 3, 7, 6, 5, 4, 4, 3, 2, 2, 5, 4, 5, 1, 0, 0, 0, 2, 12, 14, 15};
static const u8 inter_mcbpc_bits[28] = {1, 4, 4, 6, 5, 8, 8, 7, 3, 7, 7, 9, 6, 9, 9, 9, 3, 7, 7, 8, 9, 0, 0, 0, 11, 13, 13, 13};
static const u8 cbpy_tab[16][2] = {{3, 4}, {5, 5}, {4, 5}, {9, 4}, {3, 5}, {7, 4}, {2, 6}, {11, 4},
                                   {2, 5}, {3, 6}, {5, 4}, {10, 4}, {4, 4}, {8, 4}, {6, 4}, {3, 2}};
static const u8 mv_tab[33][2] = {{1, 1}, {1, 2}, {1, 3}, {1, 4}, {3, 6}, {5, 7}, {4, 7}, {3, 7}, {11, 9}, {10, 9}, {9, 9},
                                 {17, 10}, {16, 10}, {15, 10}, {14, 10}, {13, 10}, {12, 10}, {11, 10}, {10, 10}, {9, 10},
                                 {8, 10}, {7, 10}, {6, 10}, {5, 10}, {4, 10}, {7, 11}, {6, 11}, {5, 11}, {4, 11}, {3, 11},
                                 {2, 11}, {3, 12}, {2, 12}};
static const u16 tc_code[103] = {
    0x2, 0xf, 0x15, 0x17, 0x1f, 0x25, 0x24, 0x21, 0x20, 0x7, 0x6, 0x20, 0x6, 0x14, 0x1e, 0xf, 0x21, 0x50, 0xe, 0x1d, 0xe,
    0x51, 0xd, 0x23, 0xd, 0xc, 0x22, 0x52, 0xb, 0xc, 0x53, 0x13, 0xb, 0x54, 0x12, 0xa, 0x11, 0x9, 0x10, 0x8, 0x16, 0x55,
    0x15, 0x14, 0x1c, 0x1b, 0x21, 0x20, 0x1f, 0x1e, 0x1d, 0x1c, 0x1b, 0x1a, 0x22, 0x23, 0x56, 0x57, 0x7, 0x19, 0x5, 0xf,
    0x4, 0xe, 0xd, 0xc, 0x13, 0x12, 0x11, 0x10, 0x1a, 0x19, 0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x18, 0x17, 0x16, 0x15,
    0x14, 0x13, 0x12, 0x11, 0x7, 0x6, 0x5, 0x4, 0x24, 0x25, 0x26, 0x27, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x3};
static const u8 tc_bits[103] = {
    2, 4, 6, 7, 8, 9, 9, 10, 10, 11, 11, 11, 3, 6, 8, 10, 11, 12, 4, 8, 10, 12, 5, 9, 10, 5, 9, 12, 5, 10, 12, 6, 10, 12, 6,
    10, 6, 10, 6, 10, 7, 12, 7, 7, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 11, 11, 12, 12, 4, 9, 11, 6, 11, 6, 6, 6, 7, 7, 7, 7, 8,
    8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 7};
static const u8 tc_level[102] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 1, 2, 3, 4, 5, 6, 1, 2, 3, 4, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 3, 1, 2, 1, 2,
    1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static const u8 tc_run[102] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 5, 6, 6, 6, 7, 7, 8, 8,
    9, 9, 10, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 0, 0, 0, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
    39, 40};
static const u8 zigzag[64] = {0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48,
                              41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22,
                              15, 23, 30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

/* lookup tables: entry = (symbol << 5) | length, 0 = invalid */
#define LUT_BITS 13
static u16 lut_imcbpc[1 << 9], lut_pmcbpc[1 << LUT_BITS], lut_cbpy[1 << 6], lut_mv[1 << 12], lut_tc[1 << 12];
static int luts_ready;

static void lut_fill(u16 *lut, int lbits, u32 code, int len, int sym) {
    if (!len || len > lbits) return;
    u32 first = code << (lbits - len), cnt = 1u << (lbits - len);
    for (u32 i = 0; i < cnt; i++) lut[first + i] = (u16)((sym << 5) | len);
}
static void luts_init(void) {
    if (luts_ready) return;
    for (int i = 0; i < 9; i++) lut_fill(lut_imcbpc, 9, intra_mcbpc_code[i], intra_mcbpc_bits[i], i);
    for (int i = 0; i < 28; i++) lut_fill(lut_pmcbpc, LUT_BITS, inter_mcbpc_code[i], inter_mcbpc_bits[i], i);
    for (int i = 0; i < 16; i++) lut_fill(lut_cbpy, 6, cbpy_tab[i][0], cbpy_tab[i][1], i);
    for (int i = 0; i < 33; i++) lut_fill(lut_mv, 12, mv_tab[i][0], mv_tab[i][1], i);
    for (int i = 0; i < 103; i++) lut_fill(lut_tc, 12, tc_code[i], tc_bits[i], i);
    luts_ready = 1;
}
static int vlc(bits *b, const u16 *lut, int lbits) {
    u16 e = lut[peekb(b, lbits)];
    if (!e) { b->bad = 1; return -1; }
    skipb(b, e & 31);
    return e >> 5;
}

/* ================================================================ IDCT (integer, separable) */
#define C_BITS 13
#define P1_BITS 2
static void idct8x8(int *d) {
    int ws[64];
    for (int c = 0; c < 8; c++) {
        int *in = d + c, *o = ws + c;
        if (!in[8] && !in[16] && !in[24] && !in[32] && !in[40] && !in[48] && !in[56]) {
            int dc = in[0] << P1_BITS;
            for (int k = 0; k < 8; k++) o[k * 8] = dc;
            continue;
        }
        int z2 = in[16], z3 = in[48];
        int z1 = (z2 + z3) * 4433;
        int t2 = z1 + z3 * -15137, t3 = z1 + z2 * 6270;
        int t0 = (in[0] + in[32]) << C_BITS, t1 = (in[0] - in[32]) << C_BITS;
        int t10 = t0 + t3, t13 = t0 - t3, t11 = t1 + t2, t12 = t1 - t2;
        t0 = in[56]; t1 = in[40]; t2 = in[24]; t3 = in[8];
        z1 = t0 + t3; z2 = t1 + t2; z3 = t0 + t2; int z4 = t1 + t3;
        int z5 = (z3 + z4) * 9633;
        t0 *= 2446; t1 *= 16819; t2 *= 25172; t3 *= 12299;
        z1 *= -7373; z2 *= -20995; z3 *= -16069; z4 *= -3196;
        z3 += z5; z4 += z5;
        t0 += z1 + z3; t1 += z2 + z4; t2 += z2 + z3; t3 += z1 + z4;
        const int sh = C_BITS - P1_BITS, rnd = 1 << (sh - 1);
        o[0] = (t10 + t3 + rnd) >> sh; o[56] = (t10 - t3 + rnd) >> sh;
        o[8] = (t11 + t2 + rnd) >> sh; o[48] = (t11 - t2 + rnd) >> sh;
        o[16] = (t12 + t1 + rnd) >> sh; o[40] = (t12 - t1 + rnd) >> sh;
        o[24] = (t13 + t0 + rnd) >> sh; o[32] = (t13 - t0 + rnd) >> sh;
    }
    for (int r = 0; r < 8; r++) {
        int *in = ws + r * 8, *o = d + r * 8;
        const int sh = C_BITS + P1_BITS + 3, rnd = 1 << (sh - 1);
        if (!in[1] && !in[2] && !in[3] && !in[4] && !in[5] && !in[6] && !in[7]) {
            int v = (in[0] + (1 << (P1_BITS + 2))) >> (P1_BITS + 3);
            for (int k = 0; k < 8; k++) o[k] = v;
            continue;
        }
        int z2 = in[2], z3 = in[6];
        int z1 = (z2 + z3) * 4433;
        int t2 = z1 + z3 * -15137, t3 = z1 + z2 * 6270;
        int t0 = (in[0] + in[4]) << C_BITS, t1 = (in[0] - in[4]) << C_BITS;
        int t10 = t0 + t3, t13 = t0 - t3, t11 = t1 + t2, t12 = t1 - t2;
        t0 = in[7]; t1 = in[5]; t2 = in[3]; t3 = in[1];
        z1 = t0 + t3; z2 = t1 + t2; z3 = t0 + t2; int z4 = t1 + t3;
        int z5 = (z3 + z4) * 9633;
        t0 *= 2446; t1 *= 16819; t2 *= 25172; t3 *= 12299;
        z1 *= -7373; z2 *= -20995; z3 *= -16069; z4 *= -3196;
        z3 += z5; z4 += z5;
        t0 += z1 + z3; t1 += z2 + z4; t2 += z2 + z3; t3 += z1 + z4;
        o[0] = (t10 + t3 + rnd) >> sh; o[7] = (t10 - t3 + rnd) >> sh;
        o[1] = (t11 + t2 + rnd) >> sh; o[6] = (t11 - t2 + rnd) >> sh;
        o[2] = (t12 + t1 + rnd) >> sh; o[5] = (t12 - t1 + rnd) >> sh;
        o[3] = (t13 + t0 + rnd) >> sh; o[4] = (t13 - t0 + rnd) >> sh;
    }
}

/* ================================================================ H.263 / Sorenson decoder */
typedef struct {
    int w, h, mbw, mbh, ys, cs;        /* size, macroblocks, plane strides */
    u8 *buf[3][3];                     /* three frames x (Y, Cb, Cr) */
    int ref, cur;                      /* indices into buf */
    s16 *mvx, *mvy;
    int have_ref;
} h263;

static inline u8 clip8(int v) { return (u8)(v < 0 ? 0 : v > 255 ? 255 : v); }

static int h263_alloc(h263 *d, int w, int h) {
    d->w = w; d->h = h;
    d->mbw = (w + 15) / 16; d->mbh = (h + 15) / 16;
    d->ys = d->mbw * 16; d->cs = d->mbw * 8;
    u32 ysz = (u32)d->ys * d->mbh * 16, csz = (u32)d->cs * d->mbh * 8;
    for (int f = 0; f < 3; f++) {
        d->buf[f][0] = amalloc(ysz);
        d->buf[f][1] = amalloc(csz);
        d->buf[f][2] = amalloc(csz);
        if (!d->buf[f][0] || !d->buf[f][1] || !d->buf[f][2]) return 0;
        memset(d->buf[f][0], 16, ysz);
        memset(d->buf[f][1], 128, csz);
        memset(d->buf[f][2], 128, csz);
    }
    d->mvx = amalloc(sizeof(s16) * d->mbw * d->mbh);
    d->mvy = amalloc(sizeof(s16) * d->mbw * d->mbh);
    d->ref = 0; d->cur = 1;
    d->have_ref = 0;
    return d->mvx && d->mvy;
}

/* motion compensated prediction of an n x n block (half-pel), edges extended */
static void mc(const u8 *ref, int stride, int pw, int ph, int x, int y, int hx, int hy, int n, u8 *out) {
    for (int j = 0; j < n; j++) {
        int y0 = y + j, y1 = y0 + hy;
        y0 = y0 < 0 ? 0 : y0 >= ph ? ph - 1 : y0;
        y1 = y1 < 0 ? 0 : y1 >= ph ? ph - 1 : y1;
        const u8 *r0 = ref + y0 * stride, *r1 = ref + y1 * stride;
        for (int i = 0; i < n; i++) {
            int x0 = x + i, x1 = x0 + hx;
            x0 = x0 < 0 ? 0 : x0 >= pw ? pw - 1 : x0;
            x1 = x1 < 0 ? 0 : x1 >= pw ? pw - 1 : x1;
            int v;
            if (!hx && !hy) v = r0[x0];
            else if (hx && !hy) v = (r0[x0] + r0[x1] + 1) >> 1;
            else if (!hx) v = (r0[x0] + r1[x0] + 1) >> 1;
            else v = (r0[x0] + r0[x1] + r1[x0] + r1[x1] + 2) >> 2;
            out[j * n + i] = (u8)v;
        }
    }
}

static int read_block(bits *b, int *blk, int intra, int coded, int qp, int esc11) {
    memset(blk, 0, sizeof(int) * 64);
    int i = 0;
    if (intra) {
        int dc = (int)getb(b, 8);
        if (dc == 255) dc = 128;
        blk[0] = dc * 8;
        i = 1;
    }
    if (!coded) return 1;
    for (;;) {
        int sym = vlc(b, lut_tc, 12);
        if (sym < 0) return 0;
        int last, run, level;
        if (sym == 102) { /* escape */
            if (esc11) {
                int is11 = (int)getb(b, 1);
                last = (int)getb(b, 1);
                run = (int)getb(b, 6);
                level = getsb(b, is11 ? 11 : 7);
            } else {
                last = (int)getb(b, 1);
                run = (int)getb(b, 6);
                level = getsb(b, 8);
                if (level == -128) { level = (int)getb(b, 5); level |= getsb(b, 6) * 32; }
            }
        } else {
            last = sym >= 58;
            run = tc_run[sym];
            level = tc_level[sym];
            if (getb(b, 1)) level = -level;
        }
        i += run;
        if (i > 63) return 0;
        int a = level < 0 ? -level : level;
        int v = qp * (2 * a + 1) - ((qp & 1) ? 0 : 1);
        if (v > 2047) v = 2047;
        blk[zigzag[i]] = level < 0 ? -v : v;
        i++;
        if (last) break;
        if (b->bad) return 0;
    }
    return !b->bad;
}

static inline int median3(int a, int b, int c) {
    int mx = a > b ? a : b, mn = a < b ? a : b;
    return c > mx ? mx : c < mn ? mn : c;
}
static inline int wrap_mv(int v) { v &= 63; return v >= 32 ? v - 64 : v; }

/* decode one picture; returns 1 when cur holds a new frame (0: error), *disposable set */
static int h263_decode(h263 *d, const u8 *p, u32 n, int *disposable) {
    bits B = {p, n, 0, 0}, *b = &B;
    if (getb(b, 17) != 1) return 0;
    int version = (int)getb(b, 5);
    if (version > 1) return 0;
    getb(b, 8);
    int fmt = (int)getb(b, 3), w = 0, h = 0;
    switch (fmt) {
    case 0: w = (int)getb(b, 8); h = (int)getb(b, 8); break;
    case 1: w = (int)getb(b, 16); h = (int)getb(b, 16); break;
    case 2: w = 352; h = 288; break;
    case 3: w = 176; h = 144; break;
    case 4: w = 128; h = 96; break;
    case 5: w = 320; h = 240; break;
    case 6: w = 160; h = 120; break;
    default: return 0;
    }
    if (w < 1 || h < 1 || w > 2048 || h > 2048) return 0;
    int ptype = (int)getb(b, 2);
    *disposable = ptype == 2;
    int intra_pic = ptype == 0;
    getb(b, 1); /* deblocking */
    int qp = (int)getb(b, 5);
    while (getb(b, 1)) getb(b, 8); /* extra information */
    if (w != d->w || h != d->h) {
        if (!intra_pic) return 0;
        if (!h263_alloc(d, w, h)) return 0;
    }
    if (!intra_pic && !d->have_ref) return 0;
    int out = *disposable ? 3 - d->ref - d->cur : d->cur; /* the spare buffer */
    u8 *Y = d->buf[out][0], *U = d->buf[out][1], *V = d->buf[out][2];
    const u8 *RY = d->buf[d->ref][0], *RU = d->buf[d->ref][1], *RV = d->buf[d->ref][2];
    int ph = d->mbh * 16, pw = d->mbw * 16;
    int blk[64];
    u8 pred[256];
    for (int my = 0; my < d->mbh; my++) {
        for (int mx = 0; mx < d->mbw; mx++) {
            int mbi = my * d->mbw + mx;
            int intra = intra_pic, cbp = 0, mvx = 0, mvy = 0, skipped = 0;
            if (!intra_pic) {
                int cb;
                do {
                    if (getb(b, 1)) { skipped = 1; break; }
                    cb = vlc(b, lut_pmcbpc, LUT_BITS);
                    if (cb < 0) return 0;
                } while (cb == 20);
                if (!skipped) {
                    intra = (cb & 4) != 0;
                    int cbpy = vlc(b, lut_cbpy, 6);
                    if (cbpy < 0) return 0;
                    if (!intra) cbpy ^= 15;
                    cbp = (cb & 3) | (cbpy << 2);
                    if (cb & 8) { static const int dq[4] = {-1, -2, 1, 2}; qp += dq[getb(b, 2)]; if (qp < 1) qp = 1; if (qp > 31) qp = 31; }
                    if (!intra) {
                        if (cb & 16) return 0; /* 4MV needs advanced prediction: not in Sorenson */
                        int A0 = mx ? d->mvx[mbi - 1] : 0, A1 = mx ? d->mvy[mbi - 1] : 0, px, py;
                        if (!my) { px = A0; py = A1; }
                        else {
                            int up = mbi - d->mbw, ur = mx + 1 < d->mbw ? up + 1 : -1;
                            px = median3(A0, d->mvx[up], ur >= 0 ? d->mvx[ur] : 0);
                            py = median3(A1, d->mvy[up], ur >= 0 ? d->mvy[ur] : 0);
                        }
                        for (int c = 0; c < 2; c++) {
                            int code = vlc(b, lut_mv, 12), pr = c ? py : px, v = pr;
                            if (code < 0) return 0;
                            if (code) { v = (int)code; if (getb(b, 1)) v = -v; v = wrap_mv(v + pr); }
                            if (c) mvy = v; else mvx = v;
                        }
                    }
                }
            } else {
                int cb;
                do { cb = vlc(b, lut_imcbpc, 9); if (cb < 0) return 0; } while (cb == 8);
                int cbpy = vlc(b, lut_cbpy, 6);
                if (cbpy < 0) return 0;
                cbp = (cb & 3) | (cbpy << 2);
                if (cb & 4) { static const int dq[4] = {-1, -2, 1, 2}; qp += dq[getb(b, 2)]; if (qp < 1) qp = 1; if (qp > 31) qp = 31; }
            }
            d->mvx[mbi] = (s16)(intra || skipped ? 0 : mvx);
            d->mvy[mbi] = (s16)(intra || skipped ? 0 : mvy);
            if (b->bad) return 0;
            /* luma */
            for (int k = 0; k < 6; k++) {
                int coded = (cbp >> (5 - k)) & 1;
                u8 *dst;
                int stride, n = 8, bx, by;
                if (k < 4) { stride = d->ys; bx = mx * 16 + (k & 1) * 8; by = my * 16 + (k >> 1) * 8; dst = Y + by * stride + bx; }
                else { stride = d->cs; bx = mx * 8; by = my * 8; dst = (k == 4 ? U : V) + by * stride + bx; }
                if (skipped) {
                    const u8 *src = (k < 4 ? RY : k == 4 ? RU : RV) + by * stride + bx;
                    for (int j = 0; j < 8; j++) memcpy(dst + j * stride, src + j * stride, 8);
                    continue;
                }
                if (!read_block(b, blk, intra, coded, qp, version == 1)) return 0;
                if (intra) {
                    idct8x8(blk);
                    for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) dst[j * stride + i] = clip8(blk[j * 8 + i]);
                    continue;
                }
                if (k < 4) {
                    mc(RY, d->ys, pw, ph, bx + (mvx >> 1), by + (mvy >> 1), mvx & 1, mvy & 1, n, pred);
                } else {
                    int cx = (mvx >> 2), cy = (mvy >> 2), hx = (mvx & 3) != 0, hy = (mvy & 3) != 0;
                    mc(k == 4 ? RU : RV, d->cs, pw / 2, ph / 2, bx + cx, by + cy, hx, hy, n, pred);
                }
                if (coded) {
                    idct8x8(blk);
                    for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) dst[j * stride + i] = clip8(pred[j * 8 + i] + blk[j * 8 + i]);
                } else {
                    for (int j = 0; j < 8; j++) memcpy(dst + j * stride, pred + j * 8, 8);
                }
            }
        }
    }
    if (!*disposable) { int t = d->ref; d->ref = d->cur; d->cur = t; d->have_ref = 1; out = d->ref; }
    d->cur = d->cur == out ? 3 - d->ref - out : d->cur;
    return out + 1;
}

/* ================================================================ FLV container */
typedef struct { u32 off, size, ts; u8 type, key; } ftag;

typedef struct flvvid {
    const u8 *data;
    u32 size;
    ftag *tags;
    int ntags;
    int w, h;
    u32 duration;
    h263 dec;
    bitmap *bm;
    int next;          /* next tag to process */
    u32 pos;           /* current time in ms */
    u32 start;         /* plat_ms() at time 0 while playing */
    int playing, ended, inuse;
    int afmt, arate;   /* sound */
    int audio_next;
} flvvid;

static flvvid vids[8];
static flvvid *audio_owner;

static u32 be24(const u8 *p) { return ((u32)p[0] << 16) | (p[1] << 8) | p[2]; }
static u32 be32(const u8 *p) { return ((u32)p[0] << 24) | ((u32)p[1] << 16) | (p[2] << 8) | p[3]; }

static void to_rgb(flvvid *v, int plane_set) {
    h263 *d = &v->dec;
    const u8 *Y = d->buf[plane_set][0], *U = d->buf[plane_set][1], *V = d->buf[plane_set][2];
    for (int y = 0; y < v->h; y++) {
        const u8 *yr = Y + y * d->ys, *ur = U + (y >> 1) * d->cs, *vr = V + (y >> 1) * d->cs;
        u32 *o = v->bm->px + y * v->w;
        for (int x = 0; x < v->w; x++) {
            int yy = (yr[x] - 16) * 298, u = ur[x >> 1] - 128, vv = vr[x >> 1] - 128;
            int r = (yy + 409 * vv + 128) >> 8, g = (yy - 100 * u - 208 * vv + 128) >> 8, bl = (yy + 516 * u + 128) >> 8;
            o[x] = 0xFF000000u | ((u32)clip8(r) << 16) | ((u32)clip8(g) << 8) | clip8(bl);
        }
    }
}

/* decode video tag i; show it if requested */
static void video_tag(flvvid *v, int i, int show) {
    ftag *t = &v->tags[i];
    const u8 *p = v->data + t->off;
    if (t->size < 2 || (p[0] & 15) != 2) return;
    int disp = 0;
    int r = h263_decode(&v->dec, p + 1, t->size - 1, &disp);
    if (r && show && v->bm && v->dec.w == v->w && v->dec.h == v->h) to_rgb(v, r - 1);
}

static s16 pcmbuf[8192];
static void audio_tag(flvvid *v, int i) {
    ftag *t = &v->tags[i];
    const u8 *p = v->data + t->off;
    if (t->size < 2) return;
    int f = p[0] >> 4, rate = (p[0] >> 2) & 3, is16 = (p[0] >> 1) & 1, st = p[0] & 1;
    const u8 *d = p + 1;
    u32 n = t->size - 1;
    if (f == 2 || f == 14) { audio_stream_mp3(d, n); return; }
    static const int rates[4] = {5512, 11025, 22050, 44100};
    if (f == 0 || f == 3) {
        u32 frames = n / ((is16 ? 2 : 1) * (st ? 2 : 1)), k = 0;
        if (frames > 4096) frames = 4096;
        for (u32 j = 0; j < frames; j++) {
            int s;
            if (is16) {
                const u8 *q = d + j * (st ? 4 : 2);
                s = (s16)(q[0] | (q[1] << 8));
                if (st) s = (s + (s16)(q[2] | (q[3] << 8))) / 2;
            } else {
                const u8 *q = d + j * (st ? 2 : 1);
                s = ((int)q[0] - 128) << 8;
                if (st) s = (s + (((int)q[1] - 128) << 8)) / 2;
            }
            pcmbuf[k++] = (s16)s;
        }
        audio_stream_push(pcmbuf, frames, rates[rate]);
        return;
    }
    if (f == 1) { /* ADPCM: one allocation per tag */
        u32 fr = 0;
        int r = 0;
        s16 *pcm = audio_decode(1, rate, 1, st, 0, d, n, &fr, &r);
        if (pcm && fr) audio_stream_push(pcm, fr, r);
    }
}

flvvid *flv_open(const u8 *data, u32 size) {
    luts_init();
    if (size < 13 || data[0] != 'F' || data[1] != 'L' || data[2] != 'V') return NULL;
    flvvid *v = NULL;
    for (int i = 0; i < 8; i++) if (!vids[i].inuse) { v = &vids[i]; break; }
    if (!v) return NULL;
    memset(v, 0, sizeof *v);
    v->data = data;
    v->size = size;
    u32 o = be32(data + 5) + 4;
    int cap = 0;
    for (u32 q = o; q + 11 <= size; q += 11 + be24(data + q + 1) + 4) cap++;
    v->tags = amalloc(sizeof(ftag) * (cap + 1));
    if (!v->tags) return NULL;
    int first_video = -1;
    while (o + 11 <= size) {
        u8 type = data[o];
        u32 len = be24(data + o + 1);
        u32 ts = be24(data + o + 4) | ((u32)data[o + 7] << 24);
        if (o + 11 + len > size) break;
        if ((type == 8 || type == 9) && len) {
            ftag *t = &v->tags[v->ntags++];
            t->off = o + 11; t->size = len; t->ts = ts; t->type = type;
            t->key = type == 9 && (data[o + 11] >> 4) == 1;
            if (type == 9 && first_video < 0) first_video = v->ntags - 1;
            if (ts > v->duration) v->duration = ts;
        }
        o += 11 + len + 4;
    }
    if (first_video < 0) return NULL;
    const u8 *vp = data + v->tags[first_video].off;
    if ((vp[0] & 15) != 2) return NULL; /* only Sorenson Spark */
    int disp;
    if (!h263_decode(&v->dec, vp + 1, v->tags[first_video].size - 1, &disp)) return NULL;
    v->w = v->dec.w;
    v->h = v->dec.h;
    v->bm = amalloc(sizeof(bitmap));
    if (!v->bm) return NULL;
    v->bm->w = v->w; v->bm->h = v->h;
    v->bm->px = amalloc((u32)v->w * v->h * 4);
    if (!v->bm->px) return NULL;
    to_rgb(v, v->dec.ref);
    v->next = first_video + 1;
    v->inuse = 1;
    /* duration: last timestamp plus one frame */
    v->duration += 40;
    return v;
}

bitmap *flv_bitmap(flvvid *v) { return v ? v->bm : NULL; }
int flv_width(flvvid *v) { return v->w; }
int flv_height(flvvid *v) { return v->h; }
u32 flv_duration(flvvid *v) { return v->duration; }
u32 flv_time(flvvid *v) { return v->pos; }
int flv_playing(flvvid *v) { return v->playing; }

static void audio_restart(flvvid *v) {
    audio_stream_mp3_reset();
    audio_owner = v;
    /* first sound tag at or after the current position */
    v->audio_next = 0;
    while (v->audio_next < v->ntags && (v->tags[v->audio_next].type != 8 || v->tags[v->audio_next].ts < v->pos)) v->audio_next++;
}

void flv_play(flvvid *v) {
    if (!v || v->playing) return;
    if (v->ended) { v->ended = 0; v->pos = 0; v->next = 0; v->dec.have_ref = 0; }
    v->start = plat_ms() - v->pos;
    v->playing = 1;
    audio_restart(v);
}
void flv_pause(flvvid *v) {
    if (!v || !v->playing) return;
    v->playing = 0;
    if (audio_owner == v) { audio_stream_mp3_reset(); audio_owner = NULL; }
}

/* move to ms: decode from the keyframe before it */
void flv_seek(flvvid *v, u32 ms) {
    if (!v) return;
    if (ms > v->duration) ms = v->duration;
    int key = -1, upto = -1;
    for (int i = 0; i < v->ntags; i++) {
        if (v->tags[i].type != 9) continue;
        if (v->tags[i].ts > ms) break;
        if (v->tags[i].key) key = i;
        upto = i;
    }
    if (key < 0) return;
    v->dec.have_ref = 0;
    for (int i = key; i <= upto; i++) if (v->tags[i].type == 9) video_tag(v, i, i == upto);
    v->next = upto + 1;
    v->pos = ms;
    v->ended = 0;
    v->start = plat_ms() - ms;
    if (v->playing) audio_restart(v);
}
void flv_stop(flvvid *v) {
    if (!v) return;
    flv_pause(v);
    flv_seek(v, 0);
}
void flv_close(flvvid *v) {
    if (!v) return;
    flv_pause(v);
    v->inuse = 0;
}

static void tick(flvvid *v) {
    if (!v->playing) return;
    v->pos = plat_ms() - v->start;
    if (v->pos >= v->duration) { v->pos = v->duration; v->playing = 0; v->ended = 1; }
    /* sound runs a little ahead of the picture so the queue never empties */
    if (audio_owner == v) {
        while (v->audio_next < v->ntags && v->tags[v->audio_next].ts <= v->pos + 250 && audio_stream_queued() < 44100u) {
            if (v->tags[v->audio_next].type == 8) audio_tag(v, v->audio_next);
            v->audio_next++;
        }
    }
    /* all video frames up to now must be decoded; only the last one is shown */
    int last = -1;
    for (int i = v->next; i < v->ntags && v->tags[i].ts <= v->pos; i++) if (v->tags[i].type == 9) last = i;
    for (int i = v->next; i <= last; i++) if (v->tags[i].type == 9) video_tag(v, i, i == last);
    if (last >= 0) v->next = last + 1;
}

void flv_tick_all(void) { for (int i = 0; i < 8; i++) if (vids[i].inuse) tick(&vids[i]); }
void flv_reset_all(void) { for (int i = 0; i < 8; i++) vids[i].inuse = 0; audio_owner = NULL; }
