/* FlashKernel SWF player.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Plays the timeline of a SWF movie: vector shapes (straight + curved edges,
 * solid / linear / radial gradient / bitmap fills, line styles), sprites with
 * their own timelines, colour transforms, lossless bitmaps, FWS and CWS (zlib)
 * files. Rendering is anti-aliased (4x vertical supersampling) in software.
 * Not supported: ActionScript, text/fonts, JPEG bitmaps, masks, sound, morphs.
 */
#include "kernel.h"

/* ---------------------------------------------------------------- types */
typedef struct { float a, b, c, d, tx, ty; } mat;
typedef struct { float rm, gm, bm, am, ra, ga, ba, aa; } cxf;

static const mat MAT_ID = {1, 0, 0, 1, 0, 0};
static const cxf CX_ID = {1, 1, 1, 1, 0, 0, 0, 0};

typedef struct {
    u8 type, ng, spread;
    u32 color;
    mat m;
    u8 ratio[16];
    u32 gcol[16];
    u16 bmp;
} fstyle;
typedef struct { float w; u32 color; } lstyle;
typedef struct { float x0, y0, x1, y1; u16 f0, f1, ln; } edge;

typedef struct {
    int nfill, nline, nedge, nonzero;
    fstyle *fills;
    lstyle *lines;
    edge *edges;
    u32 *fofs, *fidx, *lofs, *lidx;
} shape;

typedef struct { const u8 *tags; u32 len; u16 frames; } sprite_def;
typedef struct { int w, h; u32 *px; } bitmap;

enum { CH_NONE, CH_SHAPE, CH_SPRITE, CH_BITMAP };
typedef struct { u8 type; void *p; } chr;

typedef struct inst inst;
typedef struct { u16 depth, id, clip, ratio; mat m; cxf cx; inst *spr; } dent;
#define MAXD 384
struct inst {
    int n, fresh;
    const u8 *tags;
    u32 len, pos;
    inst *next_free;
    dent e[MAXD];
};

typedef struct { float xt, yt, xb, yb, dxdy; int dir; } redge;

/* ---------------------------------------------------------------- state */
static chr *chars;
static inst *pool_free;
static redge *RE;
static u32 RE_cap;
static int *ACT;
static u32 bg_color;
static int swf_version;
static const char *fail;

/* scratch for shape parsing (static, in .bss) */
#define MAX_STYLES 2048
#define MAX_EDGES 65536
static fstyle sf[MAX_STYLES];
static lstyle sl[MAX_STYLES];
static edge escr[MAX_EDGES];

/* ---------------------------------------------------------------- reader */
typedef struct { const u8 *d; u32 n, i, bb; int bn; } rd;

static void rinit(rd *r, const u8 *d, u32 n) { r->d = d; r->n = n; r->i = 0; r->bn = 0; r->bb = 0; }
static u32 r8(rd *r) { r->bn = 0; return r->i < r->n ? r->d[r->i++] : 0; }
static u32 r16(rd *r) { u32 a = r8(r); return a | (r8(r) << 8); }
static void ralign(rd *r) { r->bn = 0; }
static u32 rub(rd *r, int n) {
    u32 v = 0;
    while (n-- > 0) {
        if (!r->bn) { r->bb = r->i < r->n ? r->d[r->i++] : 0; r->bn = 8; }
        r->bn--;
        v = (v << 1) | ((r->bb >> r->bn) & 1);
    }
    return v;
}
static s32 rsb(rd *r, int n) {
    if (n <= 0) return 0;
    u32 v = rub(r, n);
    if (n < 32 && (v & (1u << (n - 1)))) v |= ~0u << n;
    return (s32)v;
}
static void rrect(rd *r, s32 *q) {
    ralign(r);
    int n = rub(r, 5);
    for (int i = 0; i < 4; i++) q[i] = rsb(r, n);
    ralign(r);
}
static mat rmat(rd *r) {
    mat m = MAT_ID;
    ralign(r);
    if (rub(r, 1)) { int n = rub(r, 5); m.a = rsb(r, n) / 65536.0f; m.d = rsb(r, n) / 65536.0f; }
    if (rub(r, 1)) { int n = rub(r, 5); m.b = rsb(r, n) / 65536.0f; m.c = rsb(r, n) / 65536.0f; }
    int n = rub(r, 5);
    m.tx = (float)rsb(r, n);
    m.ty = (float)rsb(r, n);
    ralign(r);
    return m;
}
static cxf rcx(rd *r, int alpha) {
    cxf c = CX_ID;
    ralign(r);
    int add = rub(r, 1), mul = rub(r, 1), n = rub(r, 4);
    if (mul) {
        c.rm = rsb(r, n) / 256.0f; c.gm = rsb(r, n) / 256.0f; c.bm = rsb(r, n) / 256.0f;
        if (alpha) c.am = rsb(r, n) / 256.0f;
    }
    if (add) {
        c.ra = (float)rsb(r, n); c.ga = (float)rsb(r, n); c.ba = (float)rsb(r, n);
        if (alpha) c.aa = (float)rsb(r, n);
    }
    ralign(r);
    return c;
}
static u32 rrgb(rd *r) { u32 R = r8(r), G = r8(r), B = r8(r); return 0xFF000000u | (R << 16) | (G << 8) | B; }
static u32 rrgba(rd *r) { u32 R = r8(r), G = r8(r), B = r8(r), A = r8(r); return (A << 24) | (R << 16) | (G << 8) | B; }

/* ---------------------------------------------------------------- math */
static mat mat_mul(mat p, mat q) { /* apply q, then p */
    mat r;
    r.a = p.a * q.a + p.c * q.b;
    r.b = p.b * q.a + p.d * q.b;
    r.c = p.a * q.c + p.c * q.d;
    r.d = p.b * q.c + p.d * q.d;
    r.tx = p.a * q.tx + p.c * q.ty + p.tx;
    r.ty = p.b * q.tx + p.d * q.ty + p.ty;
    return r;
}
static mat mat_inv(mat m) {
    mat r = MAT_ID;
    float det = m.a * m.d - m.b * m.c;
    if (kabs(det) < 1e-12f) return r;
    r.a = m.d / det;
    r.b = -m.b / det;
    r.c = -m.c / det;
    r.d = m.a / det;
    r.tx = -(r.a * m.tx + r.c * m.ty);
    r.ty = -(r.b * m.tx + r.d * m.ty);
    return r;
}
static cxf cx_mul(cxf p, cxf c) { /* apply c, then p */
    cxf r;
    r.rm = p.rm * c.rm; r.ra = p.rm * c.ra + p.ra;
    r.gm = p.gm * c.gm; r.ga = p.gm * c.ga + p.ga;
    r.bm = p.bm * c.bm; r.ba = p.bm * c.ba + p.ba;
    r.am = p.am * c.am; r.aa = p.am * c.aa + p.aa;
    return r;
}
static int clamp255(float v) { int i = (int)v; return i < 0 ? 0 : i > 255 ? 255 : i; }
static u32 cx_apply(const cxf *c, u32 col) {
    int a = clamp255((col >> 24) * c->am + c->aa);
    int r = clamp255(((col >> 16) & 255) * c->rm + c->ra);
    int g = clamp255(((col >> 8) & 255) * c->gm + c->ga);
    int b = clamp255((col & 255) * c->bm + c->ba);
    return ((u32)a << 24) | ((u32)r << 16) | ((u32)g << 8) | (u32)b;
}
static int cx_is_id(const cxf *c) {
    return c->rm == 1 && c->gm == 1 && c->bm == 1 && c->am == 1 && !c->ra && !c->ga && !c->ba && !c->aa;
}

/* ---------------------------------------------------------------- shapes */
static int nf, nl, ne;

static void read_fill(rd *r, int v, fstyle *f) {
    memset(f, 0, sizeof *f);
    f->type = (u8)r8(r);
    f->m = MAT_ID;
    if (f->type == 0x00) {
        f->color = v >= 3 ? rrgba(r) : rrgb(r);
    } else if (f->type == 0x10 || f->type == 0x12 || f->type == 0x13) {
        f->m = rmat(r);
        u32 b = r8(r);
        f->spread = (u8)(b >> 6);
        f->ng = (u8)(b & 15);
        for (int i = 0; i < f->ng; i++) {
            f->ratio[i] = (u8)r8(r);
            f->gcol[i] = v >= 3 ? rrgba(r) : rrgb(r);
        }
        if (f->type == 0x13) r16(r);
        f->color = f->ng ? f->gcol[0] : 0;
        if (!f->ng) f->type = 0;
    } else if (f->type >= 0x40 && f->type <= 0x43) {
        f->bmp = (u16)r16(r);
        f->m = rmat(r);
    } else {
        f->type = 0;
        f->color = 0;
    }
}

static void read_styles(rd *r, int v) {
    u32 n = r8(r);
    if (n == 0xFF && v >= 2) n = r16(r);
    for (u32 i = 0; i < n; i++) {
        fstyle tmp;
        read_fill(r, v, nf < MAX_STYLES ? &sf[nf] : &tmp);
        if (nf < MAX_STYLES) nf++;
    }
    n = r8(r);
    if (n == 0xFF && v >= 2) n = r16(r);
    for (u32 i = 0; i < n; i++) {
        lstyle L;
        L.w = (float)r16(r);
        if (v < 4) {
            L.color = v >= 3 ? rrgba(r) : rrgb(r);
        } else {
            u32 b1 = r8(r);
            r8(r);
            if (((b1 >> 4) & 3) == 2) r16(r);
            if (!(b1 & 8)) L.color = rrgba(r);
            else { fstyle t; read_fill(r, v, &t); L.color = t.color; }
        }
        if (nl < MAX_STYLES) sl[nl++] = L;
    }
}

static void add_edge(float x0, float y0, float x1, float y1, u16 f0, u16 f1, u16 ln) {
    if (ne >= MAX_EDGES || (!f0 && !f1 && !ln)) return;
    edge *e = &escr[ne++];
    e->x0 = x0; e->y0 = y0; e->x1 = x1; e->y1 = y1;
    e->f0 = f0; e->f1 = f1; e->ln = ln;
}

static shape *parse_shape(rd *r, int v) {
    s32 rc[4];
    rrect(r, rc);
    int nonzero = 0;
    if (v == 4) { rrect(r, rc); nonzero = (r8(r) >> 2) & 1; }
    nf = nl = ne = 0;
    int fbase = 0, lbase = 0;
    read_styles(r, v);
    ralign(r);
    int fbits = rub(r, 4), lbits = rub(r, 4);
    float x = 0, y = 0;
    u16 f0 = 0, f1 = 0, ln = 0;
    while (r->i < r->n) {
        if (!rub(r, 1)) {
            u32 fl = rub(r, 5);
            if (!fl) break;
            if (fl & 1) { int n = rub(r, 5); x = (float)rsb(r, n); y = (float)rsb(r, n); }
            if (fl & 2) { u32 s = rub(r, fbits); f0 = s ? (u16)(fbase + s) : 0; }
            if (fl & 4) { u32 s = rub(r, fbits); f1 = s ? (u16)(fbase + s) : 0; }
            if (fl & 8) { u32 s = rub(r, lbits); ln = s ? (u16)(lbase + s) : 0; }
            if (fl & 16) {
                ralign(r);
                fbase = nf;
                lbase = nl;
                read_styles(r, v);
                ralign(r);
                fbits = rub(r, 4);
                lbits = rub(r, 4);
            }
        } else if (rub(r, 1)) { /* straight */
            int n = rub(r, 4) + 2;
            float dx = 0, dy = 0;
            if (rub(r, 1)) { dx = (float)rsb(r, n); dy = (float)rsb(r, n); }
            else if (rub(r, 1)) dy = (float)rsb(r, n);
            else dx = (float)rsb(r, n);
            add_edge(x, y, x + dx, y + dy, f0, f1, ln);
            x += dx; y += dy;
        } else { /* quadratic curve, flattened */
            int n = rub(r, 4) + 2;
            float cx = x + rsb(r, n), cy = y + rsb(r, n);
            float ax = cx + rsb(r, n), ay = cy + rsb(r, n);
            float len = kabs(cx - x) + kabs(cy - y) + kabs(ax - cx) + kabs(ay - cy);
            int segs = (int)(len / 120.0f);
            if (segs < 3) segs = 3;
            if (segs > 24) segs = 24;
            float px = x, py = y;
            for (int i = 1; i <= segs; i++) {
                float t = (float)i / segs, u = 1 - t;
                float qx = u * u * x + 2 * u * t * cx + t * t * ax;
                float qy = u * u * y + 2 * u * t * cy + t * t * ay;
                add_edge(px, py, qx, qy, f0, f1, ln);
                px = qx; py = qy;
            }
            x = ax; y = ay;
        }
    }

    shape *s = kmalloc(sizeof *s);
    if (!s) return NULL;
    s->nfill = nf; s->nline = nl; s->nedge = ne; s->nonzero = nonzero;
    s->fills = kmalloc(sizeof(fstyle) * (nf + 1));
    s->lines = kmalloc(sizeof(lstyle) * (nl + 1));
    s->edges = kmalloc(sizeof(edge) * (ne + 1));
    s->fofs = kmalloc(4 * (nf + 2));
    s->lofs = kmalloc(4 * (nl + 2));
    if (!s->fills || !s->lines || !s->edges || !s->fofs || !s->lofs) return NULL;
    memcpy(s->fills, sf, sizeof(fstyle) * nf);
    memcpy(s->lines, sl, sizeof(lstyle) * nl);
    memcpy(s->edges, escr, sizeof(edge) * ne);
    /* index edges per fill style and per line style */
    memset(s->fofs, 0, 4 * (nf + 2));
    memset(s->lofs, 0, 4 * (nl + 2));
    u32 tf = 0, tl = 0;
    for (int i = 0; i < ne; i++) {
        edge *e = &escr[i];
        if (e->f0 != e->f1) {
            if (e->f0 && e->f0 <= nf) { s->fofs[e->f0 + 1]++; tf++; }
            if (e->f1 && e->f1 <= nf) { s->fofs[e->f1 + 1]++; tf++; }
        }
        if (e->ln && e->ln <= nl) { s->lofs[e->ln + 1]++; tl++; }
    }
    for (int i = 1; i <= nf + 1; i++) s->fofs[i] += s->fofs[i - 1];
    for (int i = 1; i <= nl + 1; i++) s->lofs[i] += s->lofs[i - 1];
    s->fidx = kmalloc(4 * (tf + 1));
    s->lidx = kmalloc(4 * (tl + 1));
    u32 *fpos = kmalloc(4 * (nf + 2)), *lpos = kmalloc(4 * (nl + 2));
    if (!s->fidx || !s->lidx || !fpos || !lpos) return NULL;
    memcpy(fpos, s->fofs, 4 * (nf + 2));
    memcpy(lpos, s->lofs, 4 * (nl + 2));
    for (int i = 0; i < ne; i++) {
        edge *e = &escr[i];
        if (e->f0 != e->f1) {
            if (e->f0 && e->f0 <= nf) s->fidx[fpos[e->f0]++] = (u32)i;
            if (e->f1 && e->f1 <= nf) s->fidx[fpos[e->f1]++] = (u32)i;
        }
        if (e->ln && e->ln <= nl) s->lidx[lpos[e->ln]++] = (u32)i;
    }
    return s;
}

/* ---------------------------------------------------------------- bitmaps */
static bitmap *parse_lossless(rd *r, int v2) {
    int fmt = r8(r), w = r16(r), h = r16(r);
    int ct = fmt == 3 ? (int)r8(r) + 1 : 0;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return NULL;
    int cb = v2 ? 4 : 3;
    u32 rowb = fmt == 3 ? ((u32)w + 3) & ~3u : fmt == 4 ? ((u32)w * 2 + 3) & ~3u : (u32)w * 4;
    u32 need = (u32)ct * cb + rowb * h;
    u8 *tmp = kmalloc(need);
    bitmap *bm = kmalloc(sizeof *bm);
    if (!tmp || !bm) return NULL;
    bm->px = kmalloc((u32)w * h * 4);
    if (!bm->px) return NULL;
    bm->w = w; bm->h = h;
    u32 got = 0;
    const u8 *z = r->d + r->i;
    u32 zl = r->n - r->i;
    if (zl < 2 || !inflate_raw(z + 2, zl - 2, tmp, need, &got)) return NULL;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            u32 A = 255, R, G, B;
            if (fmt == 3) {
                const u8 *c = tmp + tmp[ct * cb + y * rowb + x] * cb;
                R = c[0]; G = c[1]; B = c[2];
                if (v2) A = c[3];
            } else if (fmt == 4) {
                const u8 *p = tmp + y * rowb + x * 2;
                u32 pix = (p[0] << 8) | p[1];
                R = ((pix >> 10) & 31) * 255 / 31; G = ((pix >> 5) & 31) * 255 / 31; B = (pix & 31) * 255 / 31;
            } else {
                const u8 *p = tmp + y * rowb + x * 4;
                if (v2) A = p[0];
                R = p[1]; G = p[2]; B = p[3];
            }
            if (v2 && A && A < 255) { R = R * 255 / A; G = G * 255 / A; B = B * 255 / A; }
            if (R > 255) R = 255;
            if (G > 255) G = 255;
            if (B > 255) B = 255;
            bm->px[y * w + x] = (A << 24) | (R << 16) | (G << 8) | B;
        }
    }
    return bm;
}

/* ---------------------------------------------------------------- raster */
enum { P_SOLID, P_LINEAR, P_RADIAL, P_BITMAP };
typedef struct {
    int type, spread, repeat;
    u32 col;
    mat inv;
    u32 ramp[256];
    bitmap *bm;
    const cxf *cx;
    int cxid;
} paint;

static u16 cov[4096];
static float xs[8192];
static int ws[8192];

static void blend(u32 *d, u32 s, u32 a) {
    a = (a * (s >> 24) + 127) / 255;
    if (!a) return;
    if (a >= 256) { *d = s | 0xFF000000u; return; }
    u32 D = *d, ia = 256 - a;
    u32 rb = ((s & 0xFF00FF) * a + (D & 0xFF00FF) * ia) >> 8;
    u32 g = ((s & 0x00FF00) * a + (D & 0x00FF00) * ia) >> 8;
    *d = 0xFF000000u | (rb & 0xFF00FF) | (g & 0x00FF00);
}

static u32 paint_at(paint *p, int x, int y) {
    float fx = x + 0.5f, fy = y + 0.5f;
    float u = p->inv.a * fx + p->inv.c * fy + p->inv.tx;
    float v = p->inv.b * fx + p->inv.d * fy + p->inv.ty;
    if (p->type == P_BITMAP) {
        bitmap *b = p->bm;
        int bx = kfloor(u), by = kfloor(v);
        if (p->repeat) {
            bx %= b->w; if (bx < 0) bx += b->w;
            by %= b->h; if (by < 0) by += b->h;
        } else {
            if (bx < 0) bx = 0;
            if (bx >= b->w) bx = b->w - 1;
            if (by < 0) by = 0;
            if (by >= b->h) by = b->h - 1;
        }
        u32 c = b->px[by * b->w + bx];
        return p->cxid ? c : cx_apply(p->cx, c);
    }
    float t = p->type == P_LINEAR ? (u + 16384.0f) / 32768.0f : ksqrt(u * u + v * v) / 16384.0f;
    if (p->spread == 1) { /* reflect */
        t = t - 2.0f * kfloor(t * 0.5f);
        if (t > 1) t = 2 - t;
    } else if (p->spread == 2) { /* repeat */
        t = t - kfloor(t);
    }
    int i = (int)(t * 255.0f);
    if (i < 0) i = 0;
    if (i > 255) i = 255;
    return p->ramp[i];
}

static void span(float xa, float xb) {
    if (xa < 0) xa = 0;
    if (xb > scr_w) xb = (float)scr_w;
    if (xb <= xa) return;
    int ia = (int)xa, ib = (int)xb;
    if (ia == ib) { cov[ia] += (u16)((xb - xa) * 64); return; }
    cov[ia] += (u16)((ia + 1 - xa) * 64);
    for (int i = ia + 1; i < ib; i++) cov[i] += 64;
    if (ib < scr_w) cov[ib] += (u16)((xb - ib) * 64);
}

static void sort_edges(redge *e, int n) { /* shell sort by top y */
    for (int gap = n / 2; gap > 0; gap /= 2)
        for (int i = gap; i < n; i++) {
            redge t = e[i];
            int j = i;
            while (j >= gap && e[j - gap].yt > t.yt) { e[j] = e[j - gap]; j -= gap; }
            e[j] = t;
        }
}

static void raster(int n, int nonzero, paint *p) {
    int m = 0;
    float ymaxf = 0;
    for (int i = 0; i < n; i++) {
        if (RE[i].yt == RE[i].yb) continue;
        if (RE[i].yt > RE[i].yb) {
            float t = RE[i].yt; RE[i].yt = RE[i].yb; RE[i].yb = t;
            t = RE[i].xt; RE[i].xt = RE[i].xb; RE[i].xb = t;
            RE[i].dir = -RE[i].dir;
        }
        if (RE[i].yb <= 0 || RE[i].yt >= scr_h) continue;
        RE[i].dxdy = (RE[i].xb - RE[i].xt) / (RE[i].yb - RE[i].yt);
        if (RE[i].yb > ymaxf) ymaxf = RE[i].yb;
        RE[m++] = RE[i];
    }
    if (!m) return;
    sort_edges(RE, m);
    int y0 = kfloor(RE[0].yt);
    if (y0 < 0) y0 = 0;
    int y1 = kfloor(ymaxf) + 1;
    if (y1 > scr_h) y1 = scr_h;
    int next = 0, na = 0;
    u32 *bb = video_bb();
    for (int y = y0; y < y1; y++) {
        while (next < m && RE[next].yt < y + 1) ACT[na++] = next++;
        for (int i = 0; i < na;) {
            if (RE[ACT[i]].yb <= y) ACT[i] = ACT[--na];
            else i++;
        }
        if (!na) { if (next >= m) break; continue; }
        int minx = scr_w, maxx = -1;
        for (int s = 0; s < 4; s++) {
            float sy = y + (s + 0.5f) * 0.25f;
            int nc = 0;
            for (int i = 0; i < na && nc < 8192; i++) {
                redge *e = &RE[ACT[i]];
                if (e->yt <= sy && sy < e->yb) {
                    float x = e->xt + (sy - e->yt) * e->dxdy;
                    int j = nc++;
                    while (j > 0 && xs[j - 1] > x) { xs[j] = xs[j - 1]; ws[j] = ws[j - 1]; j--; }
                    xs[j] = x;
                    ws[j] = e->dir;
                }
            }
            if (nc < 2) continue;
            if (xs[0] < minx) minx = xs[0] < 0 ? 0 : (int)xs[0];
            if (xs[nc - 1] > maxx) maxx = (int)xs[nc - 1];
            if (nonzero) {
                int w = 0;
                float start = 0;
                for (int i = 0; i < nc; i++) {
                    int pw = w;
                    w += ws[i];
                    if (!pw && w) start = xs[i];
                    else if (pw && !w) span(start, xs[i]);
                }
            } else {
                for (int i = 0; i + 1 < nc; i += 2) span(xs[i], xs[i + 1]);
            }
        }
        if (maxx >= scr_w) maxx = scr_w - 1;
        u32 *row = bb + y * scr_w;
        for (int x = minx; x <= maxx; x++) {
            u32 c = cov[x];
            if (!c) continue;
            cov[x] = 0;
            if (c > 256) c = 256;
            blend(&row[x], p->type == P_SOLID ? p->col : paint_at(p, x, y), c);
        }
    }
}

static int setup_paint(paint *p, fstyle *f, mat m, const cxf *cx) {
    p->cx = cx;
    p->cxid = cx_is_id(cx);
    if (f->type == 0x10 || f->type == 0x12 || f->type == 0x13) {
        p->type = f->type == 0x10 ? P_LINEAR : P_RADIAL;
        p->spread = f->spread;
        p->inv = mat_inv(mat_mul(m, f->m));
        for (int i = 0, k = 0; i < 256; i++) {
            while (k + 1 < f->ng && f->ratio[k + 1] < i) k++;
            u32 c;
            if (i <= f->ratio[0] || f->ng == 1) c = f->gcol[0];
            else if (k + 1 >= f->ng) c = f->gcol[f->ng - 1];
            else {
                int r0 = f->ratio[k], r1 = f->ratio[k + 1];
                int t = r1 > r0 ? (i - r0) * 256 / (r1 - r0) : 256;
                u32 a = f->gcol[k], b = f->gcol[k + 1];
                c = 0;
                for (int sh = 0; sh < 32; sh += 8) {
                    int ca = (a >> sh) & 255, cb = (b >> sh) & 255;
                    c |= (u32)((ca * (256 - t) + cb * t) >> 8) << sh;
                }
            }
            p->ramp[i] = p->cxid ? c : cx_apply(cx, c);
        }
        return 1;
    }
    if (f->type >= 0x40 && f->type <= 0x43 && chars[f->bmp].type == CH_BITMAP && chars[f->bmp].p) {
        p->type = P_BITMAP;
        p->bm = chars[f->bmp].p;
        p->repeat = f->type == 0x40 || f->type == 0x42;
        p->inv = mat_inv(mat_mul(m, f->m));
        return 1;
    }
    p->type = P_SOLID;
    p->col = cx_apply(cx, f->type >= 0x40 ? 0xFF808080u : f->color);
    return (p->col >> 24) != 0;
}

static paint pnt;

static void draw_shape(shape *s, mat m, const cxf *cx) {
    for (int g = 1; g <= s->nfill; g++) {
        u32 a = s->fofs[g], b = s->fofs[g + 1];
        if (a == b || !setup_paint(&pnt, &s->fills[g - 1], m, cx)) continue;
        int n = 0;
        for (u32 k = a; k < b && n < (int)RE_cap; k++) {
            edge *e = &s->edges[s->fidx[k]];
            redge *r = &RE[n++];
            r->xt = m.a * e->x0 + m.c * e->y0 + m.tx;
            r->yt = m.b * e->x0 + m.d * e->y0 + m.ty;
            r->xb = m.a * e->x1 + m.c * e->y1 + m.tx;
            r->yb = m.b * e->x1 + m.d * e->y1 + m.ty;
            r->dir = e->f1 == g ? 1 : -1;
        }
        raster(n, s->nonzero, &pnt);
    }
    float scale = ksqrt(kabs(m.a * m.d - m.b * m.c));
    for (int g = 1; g <= s->nline; g++) {
        u32 a = s->lofs[g], b = s->lofs[g + 1];
        if (a == b) continue;
        pnt.type = P_SOLID;
        pnt.col = cx_apply(cx, s->lines[g - 1].color);
        if (!(pnt.col >> 24)) continue;
        float w = s->lines[g - 1].w * scale;
        if (w < 1) w = 1;
        float hw = w * 0.5f;
        int n = 0;
        for (u32 k = a; k < b && n + 4 <= (int)RE_cap; k++) {
            edge *e = &s->edges[s->lidx[k]];
            float x0 = m.a * e->x0 + m.c * e->y0 + m.tx, y0 = m.b * e->x0 + m.d * e->y0 + m.ty;
            float x1 = m.a * e->x1 + m.c * e->y1 + m.tx, y1 = m.b * e->x1 + m.d * e->y1 + m.ty;
            float dx = x1 - x0, dy = y1 - y0, l = ksqrt(dx * dx + dy * dy);
            if (l < 1e-4f) continue;
            dx = dx / l * hw; dy = dy / l * hw;
            float px[4] = {x0 - dx - dy, x1 + dx - dy, x1 + dx + dy, x0 - dx + dy};
            float py[4] = {y0 - dy + dx, y1 + dy + dx, y1 + dy - dx, y0 - dy - dx};
            for (int i = 0; i < 4; i++) {
                redge *r = &RE[n++];
                r->xt = px[i]; r->yt = py[i];
                r->xb = px[(i + 1) & 3]; r->yb = py[(i + 1) & 3];
                r->dir = 1;
            }
        }
        raster(n, 1, &pnt);
    }
}

/* ---------------------------------------------------------------- timeline */
static inst *inst_new(const u8 *tags, u32 len) {
    inst *in = pool_free;
    if (!in) return NULL;
    pool_free = in->next_free;
    in->n = 0; in->fresh = 1; in->tags = tags; in->len = len; in->pos = 0;
    return in;
}
static void inst_clear(inst *in);
static void inst_free(inst *in) {
    if (!in) return;
    inst_clear(in);
    in->next_free = pool_free;
    pool_free = in;
}
static void inst_clear(inst *in) {
    for (int i = 0; i < in->n; i++) inst_free(in->e[i].spr);
    in->n = 0;
}

static dent *find_depth(inst *in, u16 d) {
    for (int i = 0; i < in->n; i++) if (in->e[i].depth == d) return &in->e[i];
    return NULL;
}
static void remove_depth(inst *in, u16 d) {
    for (int i = 0; i < in->n; i++)
        if (in->e[i].depth == d) {
            inst_free(in->e[i].spr);
            memmove(&in->e[i], &in->e[i + 1], sizeof(dent) * (in->n - i - 1));
            in->n--;
            return;
        }
}
static void run_frame(inst *in, int root);

static void attach_char(dent *e, u16 id) {
    e->id = id;
    e->spr = NULL;
    if (chars[id].type == CH_SPRITE) {
        sprite_def *sd = chars[id].p;
        e->spr = inst_new(sd->tags, sd->len);
        if (e->spr) run_frame(e->spr, 0);
    }
}
static dent *insert_depth(inst *in, u16 d) {
    if (in->n >= MAXD) return NULL;
    int i = 0;
    while (i < in->n && in->e[i].depth < d) i++;
    memmove(&in->e[i + 1], &in->e[i], sizeof(dent) * (in->n - i));
    in->n++;
    dent *e = &in->e[i];
    e->depth = d; e->id = 0; e->clip = 0; e->ratio = 0;
    e->m = MAT_ID; e->cx = CX_ID; e->spr = NULL;
    return e;
}

static void place(inst *in, const u8 *b, u32 len, int ver) {
    rd r;
    rinit(&r, b, len);
    if (ver == 1) {
        u16 id = (u16)r16(&r), depth = (u16)r16(&r);
        mat m = rmat(&r);
        cxf cx = r.i < r.n ? rcx(&r, 0) : CX_ID;
        remove_depth(in, depth);
        dent *e = insert_depth(in, depth);
        if (!e) return;
        e->m = m; e->cx = cx;
        attach_char(e, id);
        return;
    }
    u32 fl = r8(&r), fl2 = ver == 3 ? r8(&r) : 0;
    u16 depth = (u16)r16(&r);
    if (ver == 3 && ((fl2 & 8) || ((fl2 & 16) && (fl & 2)))) while (r.i < r.n && r8(&r)) {}
    u16 id = (fl & 2) ? (u16)r16(&r) : 0;
    mat m = (fl & 4) ? rmat(&r) : MAT_ID;
    cxf cx = (fl & 8) ? rcx(&r, 1) : CX_ID;
    u16 ratio = (fl & 16) ? (u16)r16(&r) : 0;
    if (fl & 32) while (r.i < r.n && r8(&r)) {}
    u16 clip = (fl & 64) ? (u16)r16(&r) : 0;

    dent *e = find_depth(in, depth);
    if (!(fl & 1) || !e) { /* new placement */
        if (!(fl & 2)) return;
        if (e) remove_depth(in, depth);
        e = insert_depth(in, depth);
        if (!e) return;
        e->m = m; e->cx = cx; e->ratio = ratio; e->clip = clip;
        attach_char(e, id);
        return;
    }
    if ((fl & 2) && id != e->id) { inst_free(e->spr); attach_char(e, id); }
    if (fl & 4) e->m = m;
    if (fl & 8) e->cx = cx;
    if (fl & 16) e->ratio = ratio;
    if (fl & 64) e->clip = clip;
}

static void define(u32 code, const u8 *b, u32 len) {
    rd r;
    rinit(&r, b, len);
    u16 id = (u16)r16(&r);
    if (chars[id].type != CH_NONE) return; /* already defined (timeline looped) */
    switch (code) {
    case 2: case 22: case 32: case 83: {
        shape *s = parse_shape(&r, code == 2 ? 1 : code == 22 ? 2 : code == 32 ? 3 : 4);
        if (s) { chars[id].type = CH_SHAPE; chars[id].p = s; }
        break;
    }
    case 39: {
        sprite_def *sd = kmalloc(sizeof *sd);
        if (!sd || len < 4) break;
        sd->frames = (u16)r16(&r);
        sd->tags = b + 4;
        sd->len = len - 4;
        chars[id].type = CH_SPRITE;
        chars[id].p = sd;
        break;
    }
    case 20: case 36:
        chars[id].type = CH_BITMAP;
        chars[id].p = parse_lossless(&r, code == 36);
        break;
    case 6: case 21: case 35: case 90: /* JPEG: not decoded, drawn grey */
        chars[id].type = CH_BITMAP;
        chars[id].p = NULL;
        break;
    }
}

static void run_frame(inst *in, int root) {
    int guard = 0;
    for (;;) {
        if (in->pos + 2 > in->len) goto loop;
        const u8 *p = in->tags + in->pos;
        u32 hdr = p[0] | (p[1] << 8), code = hdr >> 6, len = hdr & 63;
        in->pos += 2;
        if (len == 63) {
            if (in->pos + 4 > in->len) goto loop;
            p = in->tags + in->pos;
            len = p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24);
            in->pos += 4;
        }
        const u8 *body = in->tags + in->pos;
        if (len > in->len - in->pos) len = in->len - in->pos;
        in->pos += len;
        switch (code) {
        case 0: goto loop;
        case 1: return;
        case 4: place(in, body, len, 1); break;
        case 26: place(in, body, len, 2); break;
        case 70: place(in, body, len, 3); break;
        case 5: if (len >= 4) remove_depth(in, (u16)(body[2] | (body[3] << 8))); break;
        case 28: if (len >= 2) remove_depth(in, (u16)(body[0] | (body[1] << 8))); break;
        case 9: if (root && len >= 3) bg_color = 0xFF000000u | (body[0] << 16) | (body[1] << 8) | body[2]; break;
        case 2: case 22: case 32: case 83: case 39: case 20: case 36:
        case 6: case 21: case 35: case 90:
            if (root) define(code, body, len);
            break;
        default: break;
        }
        continue;
    loop: /* end of timeline: start again from frame 1 */
        inst_clear(in);
        in->pos = 0;
        if (++guard > 1) return; /* timeline without frames */
        continue;
    }
}

static void advance(inst *in) {
    for (int i = 0; i < in->n; i++) {
        inst *s = in->e[i].spr;
        if (!s) continue;
        if (s->fresh) s->fresh = 0;
        else run_frame(s, 0);
        advance(s);
    }
}

static void render(inst *in, mat m, cxf cx, int level) {
    if (level > 24) return;
    for (int i = 0; i < in->n; i++) {
        dent *e = &in->e[i];
        if (e->clip) continue; /* mask layers are hidden */
        mat mm = mat_mul(m, e->m);
        cxf cc = cx_mul(cx, e->cx);
        if (chars[e->id].type == CH_SHAPE) draw_shape(chars[e->id].p, mm, &cc);
        else if (e->spr) render(e->spr, mm, cc, level + 1);
    }
}

/* ---------------------------------------------------------------- player */
static int exit_key(void) {
    int k;
    while ((k = plat_key()) >= 0)
        if (k == KEY_ESC || k == KEY_F12 || k == KEY_CTRL_Q) return 1;
    return 0;
}

int swf_play(const u8 *data, u32 size, const char **err) {
    u32 mark = heap_mark();
    fail = NULL;
    const u8 *swf = data;
    u32 total = size;
    if (size < 8) { *err = "file too small"; return -1; }
    swf_version = data[3];
    if (data[0] == 'Z') { *err = "LZMA-compressed SWF (ZWS) is not supported"; return -1; }
    if (data[0] == 'C') {
        total = data[4] | (data[5] << 8) | (data[6] << 16) | ((u32)data[7] << 24);
        u8 *buf = total < 0x01000000 ? kmalloc(total) : NULL;
        u32 got = 0;
        if (!buf) { *err = "not enough memory to decompress"; heap_reset(mark); return -1; }
        memcpy(buf, data, 8);
        if (size < 10) { *err = "corrupt file"; heap_reset(mark); return -1; }
        inflate_raw(data + 10, size - 10, buf + 8, total - 8, &got);
        if (got < 16) { *err = "decompression failed"; heap_reset(mark); return -1; }
        total = got + 8;
        swf = buf;
    }
    rd r;
    rinit(&r, swf + 8, total - 8);
    s32 stage[4];
    rrect(&r, stage);
    u32 rate = r16(&r);
    r16(&r); /* frame count */
    float fps = (rate >> 8) + (rate & 255) / 256.0f;
    if (fps < 1) fps = 12;
    if (fps > 120) fps = 120;
    u32 frame_ms = (u32)(int)(1000.0f / fps);

    chars = kmalloc(sizeof(chr) * 65536);
    RE_cap = 262144;
    RE = kmalloc(sizeof(redge) * RE_cap);
    ACT = kmalloc(sizeof(int) * RE_cap);
    int npool = 128;
    inst *pool = kmalloc(sizeof(inst) * npool);
    if (!chars || !RE || !ACT || !pool) { *err = "not enough memory"; heap_reset(mark); return -1; }
    memset(chars, 0, sizeof(chr) * 65536);
    pool_free = NULL;
    for (int i = 0; i < npool; i++) { pool[i].next_free = pool_free; pool_free = &pool[i]; }
    bg_color = 0xFFFFFFFFu;

    inst *root = inst_new(swf + 8 + r.i, total - 8 - r.i);
    root->fresh = 0;

    /* fit the stage to the screen, keeping aspect ratio */
    float sw = (stage[1] - stage[0]) / 20.0f, sh = (stage[3] - stage[2]) / 20.0f;
    if (sw < 1) sw = 550;
    if (sh < 1) sh = 400;
    float s = scr_w / sw < scr_h / sh ? scr_w / sw : scr_h / sh;
    int vw = (int)(sw * s), vh = (int)(sh * s);
    int vx = (scr_w - vw) / 2, vy = (scr_h - vh) / 2;
    mat base = {s / 20.0f, 0, 0, s / 20.0f, vx - stage[0] * s / 20.0f, vy - stage[2] * s / 20.0f};

    video_fill(0, 0, scr_w, scr_h, 0xFF000000u);
    video_present(0, 0, scr_w, scr_h);
    for (;;) {
        u32 t0 = plat_ms();
        if (exit_key()) break;
        run_frame(root, 1);
        advance(root);
        video_fill(vx, vy, vw, vh, bg_color);
        render(root, base, CX_ID, 0);
        video_present(vx, vy, vw, vh);
        int quit = 0;
        while (plat_ms() - t0 < frame_ms) if (exit_key()) { quit = 1; break; }
        if (quit) break;
    }
    *err = fail;
    heap_reset(mark);
    return 0;
}
