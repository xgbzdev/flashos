/* FlashKernel SWF renderer: shapes, gradients, bitmaps, text, cursor.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Anti-aliased scanline rasteriser (4x vertical supersampling).
 */
#include "swf.h"

const mat MAT_ID = {1, 0, 0, 1, 0, 0};
const cxf CX_ID = {1, 1, 1, 1, 0, 0, 0, 0};

/* ---------------------------------------------------------------- reading */
void rrect(rd *r, s32 *q) {
    ralign(r);
    int n = rub(r, 5);
    for (int i = 0; i < 4; i++) q[i] = rsb(r, n);
    ralign(r);
}
mat rmat(rd *r) {
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
cxf rcx(rd *r, int alpha) {
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
u32 rrgb(rd *r) { u32 R = r8(r), G = r8(r), B = r8(r); return 0xFF000000u | (R << 16) | (G << 8) | B; }
u32 rrgba(rd *r) { u32 R = r8(r), G = r8(r), B = r8(r), A = r8(r); return (A << 24) | (R << 16) | (G << 8) | B; }

/* ---------------------------------------------------------------- math */
mat mat_mul(mat p, mat q) {
    mat r;
    r.a = p.a * q.a + p.c * q.b;
    r.b = p.b * q.a + p.d * q.b;
    r.c = p.a * q.c + p.c * q.d;
    r.d = p.b * q.c + p.d * q.d;
    r.tx = p.a * q.tx + p.c * q.ty + p.tx;
    r.ty = p.b * q.tx + p.d * q.ty + p.ty;
    return r;
}
mat mat_inv(mat m) {
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
cxf cx_mul(cxf p, cxf c) {
    cxf r;
    r.rm = p.rm * c.rm; r.ra = p.rm * c.ra + p.ra;
    r.gm = p.gm * c.gm; r.ga = p.gm * c.ga + p.ga;
    r.bm = p.bm * c.bm; r.ba = p.bm * c.ba + p.ba;
    r.am = p.am * c.am; r.aa = p.am * c.aa + p.aa;
    return r;
}
static int clamp255(float v) { int i = (int)v; return i < 0 ? 0 : i > 255 ? 255 : i; }
u32 cx_apply(const cxf *c, u32 col) {
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
#define MAX_STYLES 2048
#define MAX_EDGES 65536
static fstyle sf[MAX_STYLES];
static lstyle sl[MAX_STYLES];
static edge escr[MAX_EDGES];
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

static void flatten(float x, float y, float cx, float cy, float ax, float ay, u16 f0, u16 f1, u16 ln) {
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
}

/* Build a shape object from the scratch style/edge arrays (reusing the
 * arrays of `reuse` when they are big enough). */
static u32 fpos[MAX_STYLES + 2], lpos[MAX_STYLES + 2];

static shape *finish_shape(int nonzero, const s32 *bounds, shape *reuse) {
    shape *s = reuse;
    if (!s || s->cap_e < ne || s->cap_f < nf || s->cap_l < nl) {
        int ce = reuse ? ne * 2 + 16 : ne, cf = reuse ? nf * 2 + 4 : nf, cl = reuse ? nl * 2 + 4 : nl;
        s = amalloc(sizeof *s);
        if (!s) return NULL;
        s->cap_e = ce; s->cap_f = cf; s->cap_l = cl;
        s->fills = amalloc(sizeof(fstyle) * (cf + 1));
        s->lines = amalloc(sizeof(lstyle) * (cl + 1));
        s->edges = amalloc(sizeof(edge) * (ce + 1));
        s->fofs = amalloc(4 * (cf + 2));
        s->lofs = amalloc(4 * (cl + 2));
        s->fidx = amalloc(4 * (2 * ce + 1));
        s->lidx = amalloc(4 * (ce + 1));
        if (!s->fills || !s->lines || !s->edges || !s->fofs || !s->lofs || !s->fidx || !s->lidx) return NULL;
    }
    s->nfill = nf; s->nline = nl; s->nedge = ne; s->nonzero = nonzero;
    memcpy(s->bounds, bounds, sizeof s->bounds);
    memcpy(s->fills, sf, sizeof(fstyle) * nf);
    memcpy(s->lines, sl, sizeof(lstyle) * nl);
    memcpy(s->edges, escr, sizeof(edge) * ne);
    memset(s->fofs, 0, 4 * (nf + 2));
    memset(s->lofs, 0, 4 * (nl + 2));
    for (int i = 0; i < ne; i++) {
        edge *e = &escr[i];
        if (e->f0 != e->f1) {
            if (e->f0 && e->f0 <= nf) s->fofs[e->f0 + 1]++;
            if (e->f1 && e->f1 <= nf) s->fofs[e->f1 + 1]++;
        }
        if (e->ln && e->ln <= nl) s->lofs[e->ln + 1]++;
    }
    for (int i = 1; i <= nf + 1; i++) s->fofs[i] += s->fofs[i - 1];
    for (int i = 1; i <= nl + 1; i++) s->lofs[i] += s->lofs[i - 1];
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

/* d points after the character id. For glyphs (is_glyph) there are no
 * style arrays and fill style 1 is the glyph colour. */
shape *parse_shape(const u8 *d, u32 len, int v, int is_glyph, int unused) {
    (void)unused;
    rd rr, *r = &rr;
    rinit(r, d, len);
    s32 rc[4] = {0, 0, 0, 0};
    int nonzero = 0;
    nf = nl = ne = 0;
    if (!is_glyph) {
        rrect(r, rc);
        if (v == 4) { s32 eb[4]; rrect(r, eb); nonzero = (r8(r) >> 2) & 1; }
        read_styles(r, v);
    } else {
        memset(&sf[0], 0, sizeof sf[0]);
        sf[0].color = 0xFF000000u;
        nf = 1;
    }
    int fbase = 0, lbase = 0;
    ralign(r);
    int fbits = rub(r, 4), lbits = rub(r, 4);
    float x = 0, y = 0;
    float bx0 = 1e30f, by0 = 1e30f, bx1 = -1e30f, by1 = -1e30f;
    u16 f0 = 0, f1 = 0, ln = 0;
    while (r->i < r->n) {
        if (!rub(r, 1)) {
            u32 fl = rub(r, 5);
            if (!fl) break;
            if (fl & 1) { int n = rub(r, 5); x = (float)rsb(r, n); y = (float)rsb(r, n); }
            if (fl & 2) { u32 s = rub(r, fbits); f0 = s ? (u16)(fbase + s) : 0; }
            if (fl & 4) { u32 s = rub(r, fbits); f1 = s ? (u16)(fbase + s) : 0; }
            if (fl & 8) { u32 s = rub(r, lbits); ln = s ? (u16)(lbase + s) : 0; }
            if ((fl & 16) && !is_glyph) {
                ralign(r);
                fbase = nf;
                lbase = nl;
                read_styles(r, v);
                ralign(r);
                fbits = rub(r, 4);
                lbits = rub(r, 4);
            }
        } else if (rub(r, 1)) {
            int n = rub(r, 4) + 2;
            float dx = 0, dy = 0;
            if (rub(r, 1)) { dx = (float)rsb(r, n); dy = (float)rsb(r, n); }
            else if (rub(r, 1)) dy = (float)rsb(r, n);
            else dx = (float)rsb(r, n);
            add_edge(x, y, x + dx, y + dy, f0, f1, ln);
            x += dx; y += dy;
        } else {
            int n = rub(r, 4) + 2;
            float cx = x + rsb(r, n), cy = y + rsb(r, n);
            float ax = cx + rsb(r, n), ay = cy + rsb(r, n);
            flatten(x, y, cx, cy, ax, ay, f0, f1, ln);
            x = ax; y = ay;
        }
        if (x < bx0) bx0 = x;
        if (x > bx1) bx1 = x;
        if (y < by0) by0 = y;
        if (y > by1) by1 = y;
    }
    if (is_glyph && bx1 >= bx0) { rc[0] = (s32)bx0; rc[1] = (s32)bx1; rc[2] = (s32)by0; rc[3] = (s32)by1; }
    return finish_shape(nonzero, rc, NULL);
}

/* Drawing-API shape: rebuild from the clip's dynamic edge list. */
shape *parse_dyn_shape(dyndraw *d) {
    nf = d->nf < MAX_STYLES ? d->nf : MAX_STYLES;
    nl = d->nl < MAX_STYLES ? d->nl : MAX_STYLES;
    ne = d->ne < MAX_EDGES ? d->ne : MAX_EDGES;
    memcpy(sf, d->fills, sizeof(fstyle) * nf);
    memcpy(sl, d->lines, sizeof(lstyle) * nl);
    memcpy(escr, d->edges, sizeof(edge) * ne);
    s32 b[4] = {0, 0, 0, 0};
    if (ne) {
        float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
        for (int i = 0; i < ne; i++) {
            edge *e = &escr[i];
            if (e->x0 < x0) x0 = e->x0;
            if (e->x1 < x0) x0 = e->x1;
            if (e->x0 > x1) x1 = e->x0;
            if (e->x1 > x1) x1 = e->x1;
            if (e->y0 < y0) y0 = e->y0;
            if (e->y1 < y0) y0 = e->y1;
            if (e->y0 > y1) y1 = e->y0;
            if (e->y1 > y1) y1 = e->y1;
        }
        b[0] = (s32)x0; b[1] = (s32)x1; b[2] = (s32)y0; b[3] = (s32)y1;
    }
    return finish_shape(0, b, d->built);
}

/* ---------------------------------------------------------------- bitmaps */
bitmap *parse_lossless(const u8 *d, u32 len, int v2) {
    rd rr, *r = &rr;
    rinit(r, d, len);
    int fmt = r8(r), w = r16(r), h = r16(r);
    int ct = fmt == 3 ? (int)r8(r) + 1 : 0;
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return NULL;
    int cb = v2 ? 4 : 3;
    u32 rowb = fmt == 3 ? ((u32)w + 3) & ~3u : fmt == 4 ? ((u32)w * 2 + 3) & ~3u : (u32)w * 4;
    u32 need = (u32)ct * cb + rowb * h;
    u8 *tmp = amalloc(need);
    bitmap *bm = amalloc(sizeof *bm);
    if (!tmp || !bm) return NULL;
    bm->px = amalloc((u32)w * h * 4);
    if (!bm->px) return NULL;
    bm->w = w; bm->h = h;
    u32 got = 0;
    if (r->n - r->i < 2 || !inflate_raw(r->d + r->i + 2, r->n - r->i - 2, tmp, need, &got)) return NULL;
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

typedef struct { float xt, yt, xb, yb, dxdy; int dir; } redge;
static redge *RE;
static int *ACT;
static u32 RE_cap;
static u16 cov[4096];
static float xs[8192];
static int ws[8192];

/* masks: 8-bit coverage planes; drawing is multiplied by the active one */
#define MASK_LEVELS 4
static u8 *mask_bufs[MASK_LEVELS];
static u8 *mask_write, *mask_active;
u8 *mask_begin_write(int level) {
    if (level < 0 || level >= MASK_LEVELS) return NULL;
    if (!mask_bufs[level]) mask_bufs[level] = kmalloc((u32)scr_w * scr_h);
    if (mask_bufs[level]) memset(mask_bufs[level], 0, (u32)scr_w * scr_h);
    return mask_bufs[level];
}
void mask_set_write(u8 *buf) { mask_write = buf; }
void mask_set_active(u8 *buf) { mask_active = buf; }
u8 *mask_get_active(void) { return mask_active; }
/* a solid rectangle, only where the active mask is (text field boxes) */
void fill_rect_masked(int x, int y, int w, int h, u32 col) {
    if (mask_write) return;
    if (!mask_active) { video_fill(x, y, w, h, col); return; }
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > scr_w) x1 = scr_w;
    if (y1 > scr_h) y1 = scr_h;
    u32 *bb = video_bb();
    u32 sa = col >> 24;
    for (int yy = y; yy < y1; yy++) {
        u32 *row = bb + yy * scr_w;
        const u8 *am = mask_active + yy * scr_w;
        for (int xx = x; xx < x1; xx++) {
            u32 a = am[xx] * sa / 255;
            if (!a) continue;
            u32 d = row[xx];
            if (a >= 255) { row[xx] = col | 0xFF000000u; continue; }
            u32 rb = ((col & 0xFF00FF) * a + (d & 0xFF00FF) * (255 - a)) >> 8;
            u32 g = ((col & 0x00FF00) * a + (d & 0x00FF00) * (255 - a)) >> 8;
            row[xx] = 0xFF000000u | (rb & 0xFF00FF) | (g & 0x00FF00);
        }
    }
}

void render_init(void) {
    if (RE) return;
    RE_cap = 131072;
    RE = kmalloc(sizeof(redge) * RE_cap);
    ACT = kmalloc(sizeof(int) * RE_cap);
    if (!RE || !ACT) RE_cap = 0;
}

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
    if (p->spread == 1) {
        t = t - 2.0f * kfloor(t * 0.5f);
        if (t > 1) t = 2 - t;
    } else if (p->spread == 2) {
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

static void sort_edges(redge *e, int n) {
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
        const u8 *am = mask_active ? mask_active + y * scr_w : NULL;
        if (mask_write) { /* building a mask: record coverage instead of colour */
            u8 *mw = mask_write + y * scr_w;
            for (int x = minx; x <= maxx; x++) {
                u32 c = cov[x];
                if (!c) continue;
                cov[x] = 0;
                if (c > 256) c = 256;
                u32 v = c * 255 / 256;
                if (am) v = v * am[x] / 255;
                if (v > mw[x]) mw[x] = (u8)v;
            }
            continue;
        }
        for (int x = minx; x <= maxx; x++) {
            u32 c = cov[x];
            if (!c) continue;
            cov[x] = 0;
            if (c > 256) c = 256;
            if (am) { c = c * am[x] / 255; if (!c) continue; }
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

void draw_shape(shape *s, mat m, const cxf *cx, u32 override_color) {
    if (!s || !RE_cap) return;
    for (int g = 1; g <= s->nfill; g++) {
        u32 a = s->fofs[g], b = s->fofs[g + 1];
        if (a == b) continue;
        if (override_color) {
            pnt.type = P_SOLID;
            pnt.col = cx_apply(cx, override_color);
            if (!(pnt.col >> 24)) continue;
        } else if (!setup_paint(&pnt, &s->fills[g - 1], m, cx)) continue;
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

/* Point (shape-local twips) inside any filled area? Even-odd ray cast. */
int shape_hit(shape *s, float x, float y) {
    if (!s) return 0;
    if (x < s->bounds[0] - 20 || x > s->bounds[1] + 20 || y < s->bounds[2] - 20 || y > s->bounds[3] + 20) return 0;
    int inside = 0;
    for (int i = 0; i < s->nedge; i++) {
        edge *e = &s->edges[i];
        if ((e->f0 != 0) == (e->f1 != 0)) continue;
        if ((e->y0 > y) == (e->y1 > y)) continue;
        float xi = e->x0 + (y - e->y0) * (e->x1 - e->x0) / (e->y1 - e->y0);
        if (xi > x) inside = !inside;
    }
    if (inside) return 1;
    /* thick outlines count too */
    for (int i = 0; i < s->nedge; i++) {
        edge *e = &s->edges[i];
        if (!e->ln || e->ln > s->nline) continue;
        float hw = s->lines[e->ln - 1].w * 0.5f + 20;
        float dx = e->x1 - e->x0, dy = e->y1 - e->y0, l2 = dx * dx + dy * dy;
        float t = l2 > 0 ? ((x - e->x0) * dx + (y - e->y0) * dy) / l2 : 0;
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        float px = e->x0 + t * dx - x, py = e->y0 + t * dy - y;
        if (px * px + py * py <= hw * hw) return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- bitmap text */
/* Text in the PC's own 8x16 font, scaled (pixelated look). m maps twips to
 * screen; x, y, h, clip are in twips (local to the text field). */
void draw_bitmap_text(const char *s, mat m, float x, float y, float h, u32 color, float clipw, float cliph, int align) {
    const u8 *font = plat_font();
    float sx = ksqrt(m.a * m.a + m.b * m.b), sy = ksqrt(m.c * m.c + m.d * m.d);
    float cell_h = h * sy;                  /* pixels */
    if (cell_h < 6) cell_h = 6;
    float px = cell_h / 16.0f, cw = px * 8;
    float lx0 = m.a * x + m.c * y + m.tx, ly = m.b * x + m.d * y + m.ty;
    float maxx = lx0 + clipw * sx, maxy = ly + cliph * sy;
    u32 *bb = video_bb();
    const char *line = s;
    while (*line) {
        const char *end = line;
        while (*end && *end != '\n' && *end != '\r') end++;
        float lw = (float)(end - line) * cw, lx = lx0;
        if (align == 1) lx = lx0 + (maxx - lx0 - lw);
        else if (align == 2) lx = lx0 + (maxx - lx0 - lw) / 2;
        for (const char *c = line; c < end; c++, lx += cw) {
            const u8 *g = font + (u8)*c * 16;
            for (int gy = 0; gy < 16; gy++) {
                u8 bits = g[gy];
                if (!bits) continue;
                int y0 = (int)(ly + gy * px), y1 = (int)(ly + (gy + 1) * px);
                if (y1 == y0) y1 = y0 + 1;
                for (int gx = 0; gx < 8; gx++) {
                    if (!(bits & (0x80 >> gx))) continue;
                    int x0 = (int)(lx + gx * px), x1 = (int)(lx + (gx + 1) * px);
                    if (x1 == x0) x1 = x0 + 1;
                    for (int yy = y0; yy < y1; yy++) {
                        if (yy < 0 || yy >= scr_h || yy >= maxy + 1) continue;
                        for (int xx = x0; xx < x1; xx++) {
                            if (xx < 0 || xx >= scr_w || xx > maxx + 1) continue;
                            u32 a = 256, idx = (u32)(yy * scr_w + xx);
                            if (mask_active) { a = mask_active[idx] * 256u / 255; if (!a) continue; }
                            if (mask_write) { if (a > 255) a = 255; if (a > mask_write[idx]) mask_write[idx] = (u8)a; continue; }
                            blend(&bb[idx], color, a);
                        }
                    }
                }
            }
        }
        ly += cell_h * 1.15f;
        line = *end ? end + 1 : end;
        if (*end == '\r' && *line == '\n') line++;
    }
}

/* ---------------------------------------------------------------- cursor */
/* Pixelated black cursors with a white outline, drawn at 2x straight into
 * the framebuffer so they move smoothly between frames. */
static const char *ARROW[] = {
    "W", "WW", "WBW", "WBBW", "WBBBW", "WBBBBW", "WBBBBBW", "WBBBBBBW",
    "WBBBBBBBW", "WBBBBBBBBW", "WBBBBBBWWWW", "WBBBWBBW", "WBBWWBBW", "WBW  WBBW",
    "WW   WBBW", "W     WBBW", "      WBBW", "       WW", 0};
static const char *HAND[] = {
    "    WW", "   WBBW", "   WBBW", "   WBBW", "   WBBWWW", "   WBBWBBWWW",
    "WW WBBWBBWBBWW", "WBWWBBBBBBBBBW", "WBBWBBBBBBBBBW", " WBBBBBBBBBBBW",
    "  WBBBBBBBBBBW", "  WBBBBBBBBBW", "   WBBBBBBBBW", "   WBBBBBBBW", "    WBBBBBBW",
    "    WWWWWWWW", 0};
#define CUR_SCALE 2
static int cur_x = -1000, cur_y = -1000, cur_w, cur_h;
u32 cursor_fill = 0xFF000000u, cursor_line = 0xFFFFFFFFu;   /* ByroTT sets its cursor colours */

void erase_cursor(void) {
    if (cur_x > -1000) video_present(cur_x, cur_y, cur_w, cur_h);
    cur_x = -1000;
}

void draw_cursor(int x, int y, int hand) {
    const char **img = hand ? HAND : ARROW;
    int hx = hand ? 5 : 0; /* hot spot */
    x -= hx * CUR_SCALE;
    int w = 0, h = 0;
    for (; img[h]; h++) if ((int)kstrlen(img[h]) > w) w = (int)kstrlen(img[h]);
    erase_cursor();
    for (int r = 0; r < h; r++)
        for (int c = 0; img[r][c]; c++) {
            char p = img[r][c];
            if (p == ' ') continue;
            video_fb_rect(x + c * CUR_SCALE, y + r * CUR_SCALE, CUR_SCALE, CUR_SCALE,
                          p == 'B' ? cursor_fill : cursor_line);
        }
    cur_x = x; cur_y = y; cur_w = w * CUR_SCALE; cur_h = h * CUR_SCALE;
}

/* ---------------------------------------------------------------- BitmapData */
/* m maps bitmap pixels (1 px = 20 twips) to the screen */
void draw_bitmap(const bitmap *b, mat m, const cxf *cx, int smooth) {
    if (!b || !b->px || b->w <= 0 || b->h <= 0) return;
    float W = b->w * 20.0f, H = b->h * 20.0f;
    float xs[4] = {m.tx, m.a * W + m.tx, m.c * H + m.tx, m.a * W + m.c * H + m.tx};
    float ys[4] = {m.ty, m.b * W + m.ty, m.d * H + m.ty, m.b * W + m.d * H + m.ty};
    float x0 = xs[0], x1 = xs[0], y0 = ys[0], y1 = ys[0];
    for (int i = 1; i < 4; i++) {
        if (xs[i] < x0) x0 = xs[i];
        if (xs[i] > x1) x1 = xs[i];
        if (ys[i] < y0) y0 = ys[i];
        if (ys[i] > y1) y1 = ys[i];
    }
    int ix0 = kfloor(x0), ix1 = kfloor(x1 + 0.999f), iy0 = kfloor(y0), iy1 = kfloor(y1 + 0.999f);
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 > scr_w) ix1 = scr_w;
    if (iy1 > scr_h) iy1 = scr_h;
    mat iv = mat_inv(m);
    int cxid = cx_is_id(cx);
    /* smoothing only helps when shrinking or enlarging a lot */
    float sc = ksqrt(m.a * m.a + m.b * m.b) * 20.0f;
    int bil = smooth && (sc < 0.95f || sc > 1.05f);
    u32 *bb = video_bb();
    for (int y = iy0; y < iy1; y++) {
        float fy = y + 0.5f;
        float u = (iv.a * (ix0 + 0.5f) + iv.c * fy + iv.tx) / 20.0f, v = (iv.b * (ix0 + 0.5f) + iv.d * fy + iv.ty) / 20.0f;
        float du = iv.a / 20.0f, dv = iv.b / 20.0f;
        u32 *row = bb + y * scr_w;
        const u8 *am = mask_active ? mask_active + y * scr_w : NULL;
        for (int x = ix0; x < ix1; x++, u += du, v += dv) {
            if (u < 0 || v < 0 || u >= b->w || v >= b->h) continue;
            u32 c;
            if (bil) {
                float fu = u - 0.5f, fv = v - 0.5f;
                int bx = kfloor(fu), by = kfloor(fv);
                u32 tx = (u32)((fu - bx) * 256), ty = (u32)((fv - by) * 256);
                int bx1 = bx + 1, by1 = by + 1;
                if (bx < 0) bx = 0;
                if (by < 0) by = 0;
                if (bx1 >= b->w) bx1 = b->w - 1;
                if (by1 >= b->h) by1 = b->h - 1;
                u32 p00 = b->px[by * b->w + bx], p01 = b->px[by * b->w + bx1], p10 = b->px[by1 * b->w + bx], p11 = b->px[by1 * b->w + bx1];
                c = 0;
                for (int sh = 0; sh < 32; sh += 8) {
                    u32 t0 = ((p00 >> sh & 255) * (256 - tx) + (p01 >> sh & 255) * tx) >> 8;
                    u32 t1 = ((p10 >> sh & 255) * (256 - tx) + (p11 >> sh & 255) * tx) >> 8;
                    c |= ((t0 * (256 - ty) + t1 * ty) >> 8) << sh;
                }
            } else {
                c = b->px[(int)v * b->w + (int)u];
            }
            if (!cxid) c = cx_apply(cx, c);
            u32 a = 256;
            if (am) { a = am[x] * 256u / 255; if (!a) continue; }
            if (mask_write) { u32 cv = (c >> 24) * a / 256; if (cv > mask_write[y * scr_w + x]) mask_write[y * scr_w + x] = (u8)cv; continue; }
            blend(&row[x], c, a);
        }
    }
}

/* ---------------------------------------------------------------- glyph cache */
/* Text glyphs are rasterised once per size and quarter-pixel position and
 * then blitted, instead of filling their outlines every frame. */
typedef struct { const shape *g; u32 key; s16 ox, oy; u16 w, h; u8 *a; } gce;
#define GC_SIZE 8192
static gce *gcache;
static u8 *gc_tmp;
static int gc_count;

void glyph_cache_reset(void) { gcache = NULL; gc_count = 0; }

void draw_glyph(shape *g, mat m, const cxf *cx, u32 color) {
    if (!g) return;
    if (m.b != 0 || m.c != 0 || m.a <= 0 || m.a - m.d > 1e-5f || m.d - m.a > 1e-5f || m.a * g->bounds[1] > 400) {
        draw_shape(g, m, cx, color);
        return;
    }
    u32 col = cx_apply(cx, color);
    if (!(col >> 24)) return;
    if (!gcache) {
        gcache = amalloc(sizeof(gce) * GC_SIZE);
        if (!gcache) { draw_shape(g, m, cx, color); return; }
        memset(gcache, 0, sizeof(gce) * GC_SIZE);
        gc_count = 0;
    }
    if (!gc_tmp) { gc_tmp = kmalloc((u32)scr_w * scr_h); if (!gc_tmp) { draw_shape(g, m, cx, color); return; } memset(gc_tmp, 0, (u32)scr_w * scr_h); }
    int ix = kfloor(m.tx), iy = kfloor(m.ty + 0.5f);
    int phase = (int)((m.tx - ix) * 4);
    if (phase > 3) phase = 3;
    u32 key = ((u32)(m.a * 1048576.0f) << 2) | (u32)phase;
    u32 hsh = ((u32)(unsigned long)g * 2654435761u ^ key * 40503u) & (GC_SIZE - 1);
    gce *e = NULL;
    for (int probe = 0; probe < 16; probe++) {
        gce *c = &gcache[(hsh + probe) & (GC_SIZE - 1)];
        if (c->g == g && c->key == key) { e = c; break; }
        if (!c->g) {
            if (gc_count > GC_SIZE * 3 / 4) break; /* full: draw directly */
            /* rasterise into the scratch buffer at (pad, pad) */
            float bx0 = g->bounds[0] * m.a, by0 = g->bounds[2] * m.a, bx1 = g->bounds[1] * m.a, by1 = g->bounds[3] * m.a;
            int ox = kfloor(bx0) - 2, oy = kfloor(by0) - 2;
            int w = kfloor(bx1) - ox + 4, h = kfloor(by1) - oy + 4;
            if (w <= 0 || h <= 0 || w > scr_w || h > scr_h) break;
            mat gm = {m.a, 0, 0, m.a, (float)-ox + phase * 0.25f, (float)-oy};
            u8 *saved_w = mask_write, *saved_a = mask_active;
            mask_write = gc_tmp;
            mask_active = NULL;
            draw_shape(g, gm, &CX_ID, 0xFF000000u);
            mask_write = saved_w;
            mask_active = saved_a;
            c->a = amalloc((u32)w * h);
            if (!c->a) { for (int y = 0; y < h; y++) memset(gc_tmp + y * scr_w, 0, (u32)w); break; }
            for (int y = 0; y < h; y++) { memcpy(c->a + y * w, gc_tmp + y * scr_w, (u32)w); memset(gc_tmp + y * scr_w, 0, (u32)w); }
            c->g = g; c->key = key; c->ox = (s16)ox; c->oy = (s16)oy; c->w = (u16)w; c->h = (u16)h;
            gc_count++;
            e = c;
            break;
        }
    }
    if (!e) { draw_shape(g, m, cx, color); return; }
    u32 *bb = video_bb();
    int x0 = ix + e->ox, y0 = iy + e->oy;
    for (int y = 0; y < e->h; y++) {
        int sy = y0 + y;
        if (sy < 0 || sy >= scr_h) continue;
        const u8 *src = e->a + y * e->w;
        u32 *row = bb + sy * scr_w;
        const u8 *am = mask_active ? mask_active + sy * scr_w : NULL;
        for (int x = 0; x < e->w; x++) {
            u32 a = src[x];
            if (!a) continue;
            int sx = x0 + x;
            if (sx < 0 || sx >= scr_w) continue;
            a = a * 256u / 255;
            if (am) { a = a * am[sx] / 255; if (!a) continue; }
            if (mask_write) { u32 v = a * (col >> 24) / 256; if (v > mask_write[sy * scr_w + sx]) mask_write[sy * scr_w + sx] = (u8)v; continue; }
            blend(&row[sx], col, a);
        }
    }
}
