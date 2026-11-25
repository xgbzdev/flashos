/* FlashKernel image decoders for SWF bitmaps: baseline JPEG, PNG, GIF.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Output is 0xAARRGGBB, non-premultiplied.
 */
#include "swf.h"

/* ================================================================ JPEG (baseline, Huffman) */
typedef struct { u8 bits[17]; u8 vals[256]; u16 code[256]; u8 len[256]; int n; u16 maxcode[18]; int valptr[17]; u16 mincode[17]; } jhuff;
typedef struct {
    const u8 *d;
    u32 n, pos;
    u32 bitbuf;
    int bitcnt;
    u16 qt[4][64];
    jhuff dc[4], ac[4];
    int w, h, ncomp, hmax, vmax, restart;
    struct { int id, h, v, tq, td, ta, pred; u8 *buf; int bw, bh; } c[3];
    int ok_tables;
} jpeg;

static const u8 zz[64] = {0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48,
                          41, 34, 27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23,
                          30, 37, 44, 51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

static void jhuff_build(jhuff *h) {
    int code = 0, k = 0;
    for (int l = 1; l <= 16; l++) {
        h->valptr[l] = k;
        h->mincode[l] = (u16)code;
        for (int i = 0; i < h->bits[l]; i++) { h->code[k] = (u16)code; h->len[k] = (u8)l; code++; k++; }
        h->maxcode[l] = h->bits[l] ? (u16)(code - 1) : 0xFFFF;
        code <<= 1;
    }
    h->n = k;
}

static int jbit(jpeg *j) {
    if (!j->bitcnt) {
        if (j->pos >= j->n) return 0;
        u8 b = j->d[j->pos++];
        if (b == 0xFF) {
            u8 nx = j->pos < j->n ? j->d[j->pos] : 0;
            if (nx == 0) j->pos++;
            else { j->pos--; b = 0; } /* a marker (restart / EOI): stop here, feed zero bits */
        }
        j->bitbuf = b;
        j->bitcnt = 8;
    }
    j->bitcnt--;
    return (j->bitbuf >> j->bitcnt) & 1;
}
static int jbits(jpeg *j, int n) { int v = 0; while (n--) v = (v << 1) | jbit(j); return v; }
static int jdecode(jpeg *j, jhuff *h) {
    int code = 0;
    for (int l = 1; l <= 16; l++) {
        code = (code << 1) | jbit(j);
        if (h->maxcode[l] != 0xFFFF && code <= h->maxcode[l]) return h->vals[h->valptr[l] + code - h->mincode[l]];
    }
    return 0;
}
static int jextend(int v, int t) { return v < (1 << (t - 1)) ? v - (1 << t) + 1 : v; }

/* integer IDCT (simple separable float version: few blocks per SWF) */
static void idct(const int *in, u8 *out, int stride) {
    static float cs[8][8];
    static int init;
    if (!init) {
        for (int x = 0; x < 8; x++)
            for (int u = 0; u < 8; u++) {
                /* cos((2x+1) u pi / 16) via table of the 32 needed values */
                static const float c16[32] = {1.0f, 0.980785f, 0.923880f, 0.831470f, 0.707107f, 0.555570f, 0.382683f, 0.195090f,
                                              0.0f, -0.195090f, -0.382683f, -0.555570f, -0.707107f, -0.831470f, -0.923880f, -0.980785f,
                                              -1.0f, -0.980785f, -0.923880f, -0.831470f, -0.707107f, -0.555570f, -0.382683f, -0.195090f,
                                              0.0f, 0.195090f, 0.382683f, 0.555570f, 0.707107f, 0.831470f, 0.923880f, 0.980785f};
                float c = c16[((2 * x + 1) * u) % 32];
                cs[x][u] = (u == 0 ? 0.353553f : 0.5f) * c;
            }
        init = 1;
    }
    float tmp[64];
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) {
            float s = 0;
            for (int u = 0; u < 8; u++) s += cs[x][u] * in[y * 8 + u];
            tmp[y * 8 + x] = s;
        }
    for (int x = 0; x < 8; x++)
        for (int y = 0; y < 8; y++) {
            float s = 0;
            for (int v = 0; v < 8; v++) s += cs[y][v] * tmp[v * 8 + x];
            int p = (int)(s + 128.5f);
            out[y * stride + x] = (u8)(p < 0 ? 0 : p > 255 ? 255 : p);
        }
}

static int jpeg_markers(jpeg *j, const u8 *d, u32 n, int *sos_at) {
    u32 p = 0;
    while (p + 4 <= n) {
        if (d[p] != 0xFF) { p++; continue; }
        u8 m = d[p + 1];
        p += 2;
        if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7) || m == 0xFF) { if (m == 0xFF) p--; continue; }
        if (m == 0xD9) continue; /* EOI (SWF sometimes has EOI SOI at the start) */
        if (p + 2 > n) return 0;
        u32 len = (d[p] << 8) | d[p + 1];
        const u8 *s = d + p + 2;
        u32 sl = len - 2;
        if (p + len > n) return 0;
        if (m == 0xDB) {
            for (u32 i = 0; i + 65 <= sl;) {
                int pq = s[i] >> 4, tq = s[i] & 3;
                i++;
                for (int k = 0; k < 64; k++) { j->qt[tq][zz[k]] = pq ? (u16)((s[i] << 8) | s[i + 1]) : s[i]; i += pq ? 2 : 1; }
            }
        } else if (m == 0xC4) {
            for (u32 i = 0; i + 17 <= sl;) {
                int tc = s[i] >> 4, th = s[i] & 3;
                jhuff *h = tc ? &j->ac[th] : &j->dc[th];
                int total = 0;
                for (int l = 1; l <= 16; l++) { h->bits[l] = s[i + l]; total += h->bits[l]; }
                i += 17;
                for (int k = 0; k < total && k < 256; k++) h->vals[k] = s[i + k];
                i += (u32)total;
                jhuff_build(h);
            }
        } else if (m == 0xC0 || m == 0xC1) {
            j->h = (s[1] << 8) | s[2];
            j->w = (s[3] << 8) | s[4];
            j->ncomp = s[5];
            if (j->ncomp != 1 && j->ncomp != 3) return 0;
            j->hmax = j->vmax = 1;
            for (int c = 0; c < j->ncomp; c++) {
                j->c[c].id = s[6 + c * 3];
                j->c[c].h = s[7 + c * 3] >> 4;
                j->c[c].v = s[7 + c * 3] & 15;
                j->c[c].tq = s[8 + c * 3] & 3;
                if (j->c[c].h > j->hmax) j->hmax = j->c[c].h;
                if (j->c[c].v > j->vmax) j->vmax = j->c[c].v;
            }
        } else if (m == 0xC2 || m == 0xC6 || m == 0xCA) {
            return -1; /* progressive: not supported */
        } else if (m == 0xDD) {
            j->restart = (s[0] << 8) | s[1];
        } else if (m == 0xDA) {
            int ns = s[0];
            for (int k = 0; k < ns; k++)
                for (int c = 0; c < j->ncomp; c++)
                    if (j->c[c].id == s[1 + k * 2]) { j->c[c].td = s[2 + k * 2] >> 4; j->c[c].ta = s[2 + k * 2] & 15; }
            *sos_at = (int)(p + len);
            return 1;
        }
        p += len;
    }
    return 0;
}

static int jpeg_block(jpeg *j, int c, u8 *out, int stride) {
    int blk[64];
    memset(blk, 0, sizeof blk);
    int t = jdecode(j, &j->dc[j->c[c].td]);
    int diff = t ? jextend(jbits(j, t), t) : 0;
    j->c[c].pred += diff;
    blk[0] = j->c[c].pred * j->qt[j->c[c].tq][0];
    for (int k = 1; k < 64;) {
        int rs = jdecode(j, &j->ac[j->c[c].ta]);
        int r = rs >> 4, s = rs & 15;
        if (!s) { if (r == 15) { k += 16; continue; } break; }
        k += r;
        if (k > 63) break;
        blk[zz[k]] = jextend(jbits(j, s), s) * j->qt[j->c[c].tq][zz[k]];
        k++;
    }
    idct(blk, out, stride);
    return 1;
}

/* tables: optional JPEGTables data (DefineBits) parsed before the image */
static bitmap *jpeg_decode(const u8 *tables, u32 tn, const u8 *d, u32 n) {
    static jpeg j;
    memset(&j, 0, sizeof j);
    int sos = 0;
    if (tables && tn) { int dummy; jpeg_markers(&j, tables, tn, &dummy); }
    if (jpeg_markers(&j, d, n, &sos) != 1 || !j.w || !j.h || j.w > 4096 || j.h > 4096) return NULL;
    int mcuw = 8 * j.hmax, mcuh = 8 * j.vmax;
    int mx = (j.w + mcuw - 1) / mcuw, my = (j.h + mcuh - 1) / mcuh;
    for (int c = 0; c < j.ncomp; c++) {
        j.c[c].bw = mx * j.c[c].h * 8;
        j.c[c].bh = my * j.c[c].v * 8;
        j.c[c].buf = amalloc((u32)(j.c[c].bw * j.c[c].bh));
        if (!j.c[c].buf) return NULL;
    }
    j.d = d;
    j.n = n;
    j.pos = (u32)sos;
    int left = j.restart;
    for (int y = 0; y < my; y++)
        for (int x = 0; x < mx; x++) {
            if (j.restart && !left) {
                j.bitcnt = 0;
                while (j.pos + 1 < j.n && !(j.d[j.pos] == 0xFF && j.d[j.pos + 1] >= 0xD0 && j.d[j.pos + 1] <= 0xD7)) j.pos++;
                j.pos += 2;
                for (int c = 0; c < j.ncomp; c++) j.c[c].pred = 0;
                left = j.restart;
            }
            for (int c = 0; c < j.ncomp; c++)
                for (int v = 0; v < j.c[c].v; v++)
                    for (int h = 0; h < j.c[c].h; h++) {
                        int bx = (x * j.c[c].h + h) * 8, by = (y * j.c[c].v + v) * 8;
                        jpeg_block(&j, c, j.c[c].buf + by * j.c[c].bw + bx, j.c[c].bw);
                    }
            if (j.restart) left--;
        }
    bitmap *bm = amalloc(sizeof *bm);
    if (!bm) return NULL;
    bm->w = j.w;
    bm->h = j.h;
    bm->px = amalloc((u32)(j.w * j.h * 4));
    if (!bm->px) return NULL;
    for (int y = 0; y < j.h; y++)
        for (int x = 0; x < j.w; x++) {
            int Y = j.c[0].buf[(y * j.c[0].v / j.vmax) * j.c[0].bw + x * j.c[0].h / j.hmax];
            int R = Y, G = Y, B = Y;
            if (j.ncomp == 3) {
                int cb = j.c[1].buf[(y * j.c[1].v / j.vmax) * j.c[1].bw + x * j.c[1].h / j.hmax] - 128;
                int cr = j.c[2].buf[(y * j.c[2].v / j.vmax) * j.c[2].bw + x * j.c[2].h / j.hmax] - 128;
                R = Y + ((91881 * cr) >> 16);
                G = Y - ((22554 * cb + 46802 * cr) >> 16);
                B = Y + ((116130 * cb) >> 16);
                R = R < 0 ? 0 : R > 255 ? 255 : R;
                G = G < 0 ? 0 : G > 255 ? 255 : G;
                B = B < 0 ? 0 : B > 255 ? 255 : B;
            }
            bm->px[y * j.w + x] = 0xFF000000u | ((u32)R << 16) | ((u32)G << 8) | (u32)B;
        }
    return bm;
}

/* ================================================================ PNG */
static u32 be32(const u8 *p) { return ((u32)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

static bitmap *png_decode(const u8 *d, u32 n) {
    if (n < 33) return NULL;
    u32 w = be32(d + 16), h = be32(d + 20);
    int depth = d[24], ct = d[25], inter = d[28];
    if (!w || !h || w > 4096 || h > 4096 || inter || depth > 8 || (depth != 8 && ct != 3 && ct != 0)) return NULL;
    /* collect IDAT, PLTE, tRNS */
    u32 zlen = 0;
    for (u32 p = 8; p + 12 <= n;) {
        u32 l = be32(d + p);
        if (!memcmp(d + p + 4, "IDAT", 4)) zlen += l;
        p += 12 + l;
    }
    u8 *z = amalloc(zlen + 1);
    u32 pal[256];
    for (int i = 0; i < 256; i++) pal[i] = 0xFF000000u;
    u32 zp = 0;
    if (!z) return NULL;
    for (u32 p = 8; p + 12 <= n;) {
        u32 l = be32(d + p);
        const u8 *c = d + p + 8;
        if (p + 12 + l > n) break;
        if (!memcmp(d + p + 4, "IDAT", 4)) { memcpy(z + zp, c, l); zp += l; }
        if (!memcmp(d + p + 4, "PLTE", 4)) for (u32 i = 0; i < l / 3 && i < 256; i++) pal[i] = 0xFF000000u | ((u32)c[i * 3] << 16) | (c[i * 3 + 1] << 8) | c[i * 3 + 2];
        if (!memcmp(d + p + 4, "tRNS", 4) && ct == 3) for (u32 i = 0; i < l && i < 256; i++) pal[i] = (pal[i] & 0xFFFFFFu) | ((u32)c[i] << 24);
        p += 12 + l;
    }
    int ch = ct == 2 ? 3 : ct == 6 ? 4 : ct == 4 ? 2 : 1;
    u32 bpp_bits = (u32)ch * (u32)depth;
    u32 stride = (w * bpp_bits + 7) / 8;
    u32 rawlen = (stride + 1) * h, got = 0;
    u8 *raw = amalloc(rawlen);
    if (!raw || zp < 2 || !inflate_raw(z + 2, zp - 2, raw, rawlen, &got) || got < rawlen) return NULL;
    u32 bpp = (bpp_bits + 7) / 8;
    for (u32 y = 0; y < h; y++) {
        u8 *row = raw + y * (stride + 1), f = row[0];
        u8 *cur = row + 1, *prev = y ? raw + (y - 1) * (stride + 1) + 1 : NULL;
        for (u32 x = 0; x < stride; x++) {
            int a = x >= bpp ? cur[x - bpp] : 0, b = prev ? prev[x] : 0, c = (prev && x >= bpp) ? prev[x - bpp] : 0;
            int v = cur[x];
            if (f == 1) v += a;
            else if (f == 2) v += b;
            else if (f == 3) v += (a + b) / 2;
            else if (f == 4) {
                int pp = a + b - c, pa = pp > a ? pp - a : a - pp, pb = pp > b ? pp - b : b - pp, pc = pp > c ? pp - c : c - pp;
                v += (pa <= pb && pa <= pc) ? a : pb <= pc ? b : c;
            }
            cur[x] = (u8)v;
        }
    }
    bitmap *bm = amalloc(sizeof *bm);
    if (!bm) return NULL;
    bm->w = (int)w;
    bm->h = (int)h;
    bm->px = amalloc(w * h * 4);
    if (!bm->px) return NULL;
    for (u32 y = 0; y < h; y++) {
        const u8 *r = raw + y * (stride + 1) + 1;
        for (u32 x = 0; x < w; x++) {
            u32 c;
            if (ct == 2) c = 0xFF000000u | ((u32)r[x * 3] << 16) | (r[x * 3 + 1] << 8) | r[x * 3 + 2];
            else if (ct == 6) c = ((u32)r[x * 4 + 3] << 24) | ((u32)r[x * 4] << 16) | (r[x * 4 + 1] << 8) | r[x * 4 + 2];
            else if (ct == 4) c = ((u32)r[x * 2 + 1] << 24) | ((u32)r[x * 2] << 16) | (r[x * 2] << 8) | r[x * 2];
            else {
                u32 bit = x * (u32)depth, v = (r[bit / 8] >> (8 - depth - (bit % 8))) & ((1u << depth) - 1);
                if (ct == 3) c = pal[v];
                else { u32 g = v * 255 / ((1u << depth) - 1); c = 0xFF000000u | (g << 16) | (g << 8) | g; }
            }
            bm->px[y * w + x] = c;
        }
    }
    return bm;
}

/* ================================================================ GIF (first frame) */
static bitmap *gif_decode(const u8 *d, u32 n) {
    if (n < 13) return NULL;
    u32 gw = d[6] | (d[7] << 8), gh = d[8] | (d[9] << 8);
    u32 pal[256];
    u32 p = 13;
    int transp = -1;
    if (d[10] & 0x80) { int sz = 2 << (d[10] & 7); for (int i = 0; i < sz && p + 3 <= n; i++, p += 3) pal[i] = 0xFF000000u | ((u32)d[p] << 16) | (d[p + 1] << 8) | d[p + 2]; }
    while (p < n && d[p] != 0x2C) {
        if (d[p] == 0x21) {
            if (d[p + 1] == 0xF9 && (d[p + 3] & 1)) transp = d[p + 6];
            p += 2;
            while (p < n && d[p]) p += d[p] + 1u;
            p++;
        } else return NULL;
    }
    if (p + 10 > n) return NULL;
    u32 ix = d[p + 1] | (d[p + 2] << 8), iy = d[p + 3] | (d[p + 4] << 8), iw = d[p + 5] | (d[p + 6] << 8), ih = d[p + 7] | (d[p + 8] << 8);
    int flags = d[p + 9], inter = flags & 0x40;
    p += 10;
    if (flags & 0x80) { int sz = 2 << (flags & 7); for (int i = 0; i < sz && p + 3 <= n; i++, p += 3) pal[i] = 0xFF000000u | ((u32)d[p] << 16) | (d[p + 1] << 8) | d[p + 2]; }
    if (transp >= 0) pal[transp] = 0;
    if (!gw || !gh || gw > 4096 || gh > 4096) return NULL;
    int minsz = d[p++];
    /* gather sub-blocks */
    u32 total = 0, q = p;
    while (q < n && d[q]) { total += d[q]; q += d[q] + 1u; }
    u8 *data = amalloc(total + 4);
    u8 *idx = amalloc(iw * ih + 1);
    if (!data || !idx) return NULL;
    u32 k = 0;
    for (q = p; q < n && d[q]; q += d[q] + 1u) { memcpy(data + k, d + q + 1, d[q]); k += d[q]; }
    /* LZW */
    static u16 prefix[4096];
    static u8 suffix[4096], stack[4097];
    int clear = 1 << minsz, eoi = clear + 1, avail = clear + 2, csize = minsz + 1, old = -1;
    u32 bitpos = 0, outp = 0, npix = iw * ih;
    u8 first = 0;
    for (int i = 0; i < clear; i++) { prefix[i] = 0; suffix[i] = (u8)i; }
    while (outp < npix && bitpos + (u32)csize <= total * 8) {
        int code = 0;
        for (int b = 0; b < csize; b++, bitpos++) code |= ((data[bitpos >> 3] >> (bitpos & 7)) & 1) << b;
        if (code == clear) { csize = minsz + 1; avail = clear + 2; old = -1; continue; }
        if (code == eoi) break;
        if (old < 0) { idx[outp++] = suffix[code]; first = suffix[code]; old = code; continue; }
        int in = code, sp = 0;
        if (code >= avail) { stack[sp++] = first; code = old; }
        while (code >= clear && sp < 4096) { stack[sp++] = suffix[code]; code = prefix[code]; }
        first = suffix[code];
        stack[sp++] = first;
        while (sp && outp < npix) idx[outp++] = stack[--sp];
        if (avail < 4096) { prefix[avail] = (u16)old; suffix[avail] = first; avail++; }
        if (avail == (1 << csize) && csize < 12) csize++;
        old = in;
    }
    bitmap *bm = amalloc(sizeof *bm);
    if (!bm) return NULL;
    bm->w = (int)gw;
    bm->h = (int)gh;
    bm->px = amalloc(gw * gh * 4);
    if (!bm->px) return NULL;
    static const int istart[4] = {0, 4, 2, 1}, istep[4] = {8, 8, 4, 2};
    u32 src = 0;
    for (int pass = 0; pass < (inter ? 4 : 1); pass++)
        for (u32 yy = inter ? (u32)istart[pass] : 0; yy < ih; yy += inter ? (u32)istep[pass] : 1)
            for (u32 xx = 0; xx < iw; xx++, src++) {
                u32 X = ix + xx, Y = iy + yy;
                if (X < gw && Y < gh && src < outp) bm->px[Y * gw + X] = pal[idx[src]];
            }
    return bm;
}

/* ================================================================ SWF bitmap tags */
static const u8 *jpeg_tables;
static u32 jpeg_tables_len;
void image_set_tables(const u8 *d, u32 n) { jpeg_tables = d; jpeg_tables_len = n; }

/* code: 6 DefineBits, 21 DefineBitsJPEG2, 35 DefineBitsJPEG3, 90 DefineBitsJPEG4.
 * d points after the character id. */
bitmap *image_decode_tag(int code, const u8 *d, u32 n) {
    u32 img_len = n, alpha_off = 0;
    if (code == 35 || code == 90) {
        if (n < 4) return NULL;
        img_len = d[0] | (d[1] << 8) | (d[2] << 16) | ((u32)d[3] << 24);
        u32 hdr = code == 90 ? 6 : 4;
        d += hdr;
        n -= hdr;
        if (img_len > n) return NULL;
        alpha_off = img_len;
    }
    bitmap *bm;
    if (n >= 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') bm = png_decode(d, img_len);
    else if (n >= 6 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F') bm = gif_decode(d, img_len);
    else bm = jpeg_decode(code == 6 ? jpeg_tables : NULL, code == 6 ? jpeg_tables_len : 0, d, img_len);
    if (!bm) return NULL;
    /* DefineBitsJPEG3/4: zlib alpha plane after the JPEG */
    if (alpha_off && n > alpha_off + 2 && !(d[0] == 0x89 && d[1] == 'P')) {
        u32 np = (u32)(bm->w * bm->h), got = 0;
        u8 *a = amalloc(np);
        if (a && inflate_raw(d + alpha_off + 2, n - alpha_off - 2, a, np, &got) && got == np)
            for (u32 i = 0; i < np; i++) bm->px[i] = (bm->px[i] & 0xFFFFFFu) | ((u32)a[i] << 24);
    }
    return bm;
}

/* ---------------------------------------------------------------- BMP */
static u32 le32b(const u8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24); }
static bitmap *bmp_decode(const u8 *d, u32 n) {
    if (n < 54) return NULL;
    u32 off = le32b(d + 10), hs = le32b(d + 14);
    s32 w = (s32)le32b(d + 18), h = (s32)le32b(d + 22);
    int bpp = d[28] | (d[29] << 8);
    u32 comp = le32b(d + 30);
    if (hs < 40 || w <= 0 || w > 8192 || !h || h > 8192 || h < -8192 || (comp != 0 && comp != 3)) return NULL;
    int flip = h > 0;
    if (h < 0) h = -h;
    if (bpp != 24 && bpp != 32 && bpp != 8) return NULL;
    u32 stride = (((u32)w * bpp + 31) / 32) * 4;
    if (off + stride * (u32)h > n) return NULL;
    const u8 *pal = d + 14 + hs;
    bitmap *bm = amalloc(sizeof *bm);
    if (!bm) return NULL;
    bm->w = w; bm->h = h;
    bm->px = amalloc((u32)w * h * 4);
    if (!bm->px) return NULL;
    for (s32 y = 0; y < h; y++) {
        const u8 *row = d + off + stride * (u32)(flip ? h - 1 - y : y);
        u32 *o = bm->px + (u32)y * w;
        for (s32 x = 0; x < w; x++) {
            if (bpp == 24) o[x] = 0xFF000000u | (row[x * 3 + 2] << 16) | (row[x * 3 + 1] << 8) | row[x * 3];
            else if (bpp == 32) o[x] = 0xFF000000u | (row[x * 4 + 2] << 16) | (row[x * 4 + 1] << 8) | row[x * 4];
            else { const u8 *q = pal + row[x] * 4; o[x] = 0xFF000000u | (q[2] << 16) | (q[1] << 8) | q[0]; }
        }
    }
    return bm;
}

bitmap *image_decode_file(const u8 *d, u32 n) {
    if (n >= 2 && d[0] == 'B' && d[1] == 'M') return bmp_decode(d, n);
    if (n < 4) return NULL;
    return image_decode_tag(21, d, n);
}

u8 *bmp_encode(const bitmap *b, u32 *len) {
    u32 stride = ((u32)b->w * 3 + 3) & ~3u, size = 54 + stride * b->h;
    u8 *o = amalloc(size);
    if (!o) return NULL;
    memset(o, 0, 54);
    o[0] = 'B'; o[1] = 'M';
    u32 f[] = {size, 0, 54, 40, (u32)b->w, (u32)b->h};
    for (int i = 0; i < 6; i++) { u32 v = f[i]; u8 *q = o + 2 + i * 4; q[0] = (u8)v; q[1] = (u8)(v >> 8); q[2] = (u8)(v >> 16); q[3] = (u8)(v >> 24); }
    o[26] = 1; o[28] = 24;
    u32 isz = stride * b->h;
    o[34] = (u8)isz; o[35] = (u8)(isz >> 8); o[36] = (u8)(isz >> 16); o[37] = (u8)(isz >> 24);
    o[38] = 0x13; o[39] = 0x0B; o[42] = 0x13; o[43] = 0x0B; /* 72 dpi */
    for (int y = 0; y < b->h; y++) {
        u8 *row = o + 54 + stride * (u32)(b->h - 1 - y);
        const u32 *s = b->px + (u32)y * b->w;
        for (int x = 0; x < b->w; x++) {
            u32 c = s[x], a = c >> 24;
            /* flatten transparency onto white */
            u32 r = ((c >> 16 & 255) * a + 255 * (255 - a)) / 255, g = ((c >> 8 & 255) * a + 255 * (255 - a)) / 255, bl = ((c & 255) * a + 255 * (255 - a)) / 255;
            row[x * 3] = (u8)bl; row[x * 3 + 1] = (u8)g; row[x * 3 + 2] = (u8)r;
        }
    }
    *len = size;
    return o;
}
