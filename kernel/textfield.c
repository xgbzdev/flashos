/* FlashKernel text fields: layout, word wrap, embedded fonts, caret,
 * selection, clipboard, undo and scrolling.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Positions are in field-local twips. Line numbers are 0-based here and
 * 1-based in ActionScript (scroll, maxscroll, bottomScroll).
 */
#include "swf.h"

#define G 40.0f            /* 2 pixel gutter, as in Flash */
#define MAXL 32768

static int Ls[MAXL + 1], Le[MAXL + 1], Ln;

/* ---------------------------------------------------------------- font metrics */
typedef struct {
    font_def *fd;
    float h, scale, ascent, lineh;
} metric;

static void font_map(font_def *fd) {
    if (fd->mapped) return;
    for (int i = 0; i < 256; i++) fd->map[i] = -1;
    if (fd->code)
        for (int g = 0; g < fd->nglyph; g++)
            if (fd->code[g] < 256 && fd->map[fd->code[g]] < 0) fd->map[fd->code[g]] = (s16)g;
    fd->mapped = 1;
}

static void get_metric(inst *in, metric *mt) {
    etext_def *e = in->et;
    mt->h = e->height > 20 ? e->height : 240;
    mt->fd = NULL;
    if (e->font && chars && chars[e->font].type == CH_FONT && (e->flags & 0x0001)) {
        font_def *fd = chars[e->font].p;
        if (fd && fd->nglyph && fd->glyph && fd->code) { font_map(fd); mt->fd = fd; }
    }
    if (mt->fd) {
        font_def *fd = mt->fd;
        mt->scale = mt->h / fd->em;
        float asc = fd->ascent > 0 ? fd->ascent : fd->em * 0.9f;
        float desc = fd->descent > 0 ? fd->descent : fd->em * 0.25f;
        mt->ascent = asc * mt->scale;
        mt->lineh = (asc + desc + (fd->leading > 0 ? fd->leading : 0)) * mt->scale;
    } else {
        mt->scale = 1;
        mt->ascent = mt->h * 0.8f;
        mt->lineh = mt->h * 1.15f;
    }
    if (mt->lineh < 40) mt->lineh = 40;
}

static float adv(metric *mt, u8 c) {
    if (c == '\t') return adv(mt, ' ') * 4;
    if (!mt->fd) return mt->h * 0.5f;
    int g = mt->fd->map[c];
    if (g < 0) g = mt->fd->map['?'];
    if (g < 0) return mt->h * 0.5f;
    if (mt->fd->adv) return mt->fd->adv[g] * mt->scale;
    shape *s = mt->fd->glyph[g];
    return s ? (s->bounds[1] + 40) * mt->scale : mt->h * 0.3f;
}

/* ---------------------------------------------------------------- buffer */
const char *tf_text(inst *in) { return in->text ? in->text : ""; }

static u32 tlen(inst *in) {
    if (!in->tcap) in->tlen = in->text ? kstrlen(in->text) : 0;
    return in->tlen;
}

static int ensure(inst *in, u32 need) {
    if (in->tcap && need + 1 <= in->tcap) return 1;
    u32 n = tlen(in);
    u32 cap = in->tcap ? in->tcap * 2 : 256;
    while (cap < need + 1) cap *= 2;
    char *b = amalloc(cap);
    if (!b) return 0;
    if (n) memcpy(b, in->text, n);
    b[n] = 0;
    in->text = b;
    in->tcap = cap;
    in->tlen = n;
    return 1;
}

static void clamp_sel(inst *in) {
    int n = (int)tlen(in);
    if (in->caret < 0) in->caret = 0;
    if (in->caret > n) in->caret = n;
    if (in->sel < 0) in->sel = 0;
    if (in->sel > n) in->sel = n;
}

void tf_set_text(inst *in, const char *s, u32 n) {
    if (!ensure(in, n)) return;
    memmove(in->text, s, n);
    in->text[n] = 0;
    in->tlen = n;
    clamp_sel(in);
}

/* replace [a,b) with s */
static int replace(inst *in, int a, int b, const char *s, u32 k) {
    u32 n = tlen(in);
    if (a > b) { int t = a; a = b; b = t; }
    if (a < 0) a = 0;
    if ((u32)b > n) b = (int)n;
    etext_def *e = in->et;
    if (e->maxlen && n - (b - a) + k > (u32)e->maxlen) {
        u32 room = (u32)e->maxlen - (n - (b - a));
        if ((s32)room <= 0) return 0;
        if (k > room) k = room;
    }
    if (!ensure(in, n - (b - a) + k)) return 0;
    memmove(in->text + a + k, in->text + b, n - b + 1);
    if (k) memcpy(in->text + a, s, k);
    in->tlen = n - (b - a) + k;
    in->caret = in->sel = a + (int)k;
    return 1;
}

/* ---------------------------------------------------------------- undo */
typedef struct { char *buf; u32 cap, len; int caret, sel; } snap;
typedef struct { snap s[32]; int n, top; int kind; u32 when; } undo_t;

static void undo_push(inst *in, int kind) {
    undo_t *u = in->undo;
    if (!u) { u = amalloc(sizeof *u); if (!u) return; memset(u, 0, sizeof *u); in->undo = u; }
    u32 now = plat_ms();
    if (kind && kind == u->kind && now - u->when < 1500) { u->when = now; return; }
    u->kind = kind;
    u->when = now;
    u32 n = tlen(in);
    snap *sp = &u->s[u->top];
    if (sp->cap < n + 1) {
        u32 cap = n + 1 + 256;
        char *b = amalloc(cap);
        if (!b) return;
        sp->buf = b;
        sp->cap = cap;
    }
    memcpy(sp->buf, tf_text(in), n + 1);
    sp->len = n;
    sp->caret = in->caret;
    sp->sel = in->sel;
    u->top = (u->top + 1) % 32;
    if (u->n < 32) u->n++;
}

void tf_undo(inst *in) {
    undo_t *u = in->undo;
    if (!u || !u->n) return;
    u->top = (u->top + 31) % 32;
    u->n--;
    snap *sp = &u->s[u->top];
    tf_set_text(in, sp->buf, sp->len);
    in->caret = sp->caret;
    in->sel = sp->sel;
    clamp_sel(in);
    u->kind = 0;
    text_changed(in);
}

/* ---------------------------------------------------------------- clipboard */
static char *clip;
static u32 clip_n, clip_cap;
void clip_set(const char *s, u32 n) {
    if (n + 1 > clip_cap) {
        u32 cap = n + 1 + 1024;
        char *b = kmalloc(cap); /* survives between movies */
        if (!b) return;
        clip = b;
        clip_cap = cap;
    }
    memcpy(clip, s, n);
    clip[n] = 0;
    clip_n = n;
}
const char *clip_get(u32 *n) { if (n) *n = clip_n; return clip ? clip : ""; }

/* ---------------------------------------------------------------- layout */
static int multiline(inst *in) { return (in->et->flags & 0x2000) != 0; }
static int wordwrap(inst *in) { return (in->et->flags & 0x4000) != 0; }

static void layout(inst *in, metric *mt) {
    const u8 *t = (const u8 *)tf_text(in);
    int n = (int)tlen(in);
    float width = (float)(in->et->bounds[1] - in->et->bounds[0]) - 2 * G;
    int wrap = wordwrap(in) && width > 0;
    int ml = multiline(in);
    int i = 0;
    Ln = 0;
    for (;;) {
        Ls[Ln] = i;
        float x = 0;
        int j = i, lastsp = -1;
        while (j < n) {
            u8 c = t[j];
            if (ml && (c == '\n' || c == '\r')) break;
            float w = adv(mt, c);
            if (wrap && x + w > width && j > i) {
                if (lastsp >= i) j = lastsp + 1;
                break;
            }
            if (c == ' ' || c == '\t') lastsp = j;
            x += w;
            j++;
        }
        int end = j;
        /* trailing spaces of a wrapped line stay on it */
        Le[Ln] = end;
        Ln++;
        if (Ln >= MAXL) break;
        if (j >= n) break;
        if (ml && (t[j] == '\n' || t[j] == '\r')) {
            if (t[j] == '\r' && j + 1 < n && t[j + 1] == '\n') j++;
            j++;
            if (j >= n) { Ls[Ln] = Le[Ln] = n; Ln++; break; }
        }
        i = j;
    }
}

static int vis_lines(inst *in, metric *mt) {
    float h = (float)(in->et->bounds[3] - in->et->bounds[2]) - 2 * G;
    int v = (int)(h / mt->lineh);
    return v < 1 ? 1 : v;
}

static int line_of(int pos) {
    for (int l = Ln - 1; l > 0; l--) if (pos >= Ls[l]) return l;
    return 0;
}

static float x_of(inst *in, metric *mt, int line, int pos) {
    const u8 *t = (const u8 *)tf_text(in);
    float x = 0;
    for (int i = Ls[line]; i < pos && i < Le[line]; i++) x += adv(mt, t[i]);
    return x;
}

static float align_off(inst *in, metric *mt, int line) {
    if (!in->et->align) return 0;
    float width = (float)(in->et->bounds[1] - in->et->bounds[0]) - 2 * G;
    float w = x_of(in, mt, line, Le[line]);
    if (in->et->align == 1) return width - w;
    if (in->et->align == 2) return (width - w) / 2;
    return 0;
}

static int pos_at(inst *in, metric *mt, int line, float x) {
    const u8 *t = (const u8 *)tf_text(in);
    if (line < 0) return 0;
    if (line >= Ln) return Le[Ln - 1];
    x -= align_off(in, mt, line);
    float cx = 0;
    for (int i = Ls[line]; i < Le[line]; i++) {
        float w = adv(mt, t[i]);
        if (x < cx + w / 2) return i;
        cx += w;
    }
    return Le[line];
}

static void keep_caret_visible(inst *in) {
    metric mt;
    get_metric(in, &mt);
    layout(in, &mt);
    int l = line_of(in->caret), v = vis_lines(in, &mt);
    if (l < in->scroll) in->scroll = l;
    if (l >= in->scroll + v) in->scroll = l - v + 1;
    if (in->scroll > Ln - v) in->scroll = Ln - v;
    if (in->scroll < 0) in->scroll = 0;
}

int tf_maxscroll(inst *in) {
    metric mt;
    get_metric(in, &mt);
    layout(in, &mt);
    int m = Ln - vis_lines(in, &mt) + 1;
    return m < 1 ? 1 : m;
}
int tf_bottom(inst *in) {
    metric mt;
    get_metric(in, &mt);
    layout(in, &mt);
    int b = in->scroll + vis_lines(in, &mt);
    return b > Ln ? Ln : b;
}
void tf_scroll_to(inst *in, int line1) {
    int m = tf_maxscroll(in);
    if (line1 > m) line1 = m;
    if (line1 < 1) line1 = 1;
    in->scroll = line1 - 1;
}
int tf_text_width(inst *in) {
    metric mt;
    get_metric(in, &mt);
    layout(in, &mt);
    float w = 0;
    for (int l = 0; l < Ln; l++) { float x = x_of(in, &mt, l, Le[l]); if (x > w) w = x; }
    return (int)(w / 20);
}
int tf_text_height(inst *in) {
    metric mt;
    get_metric(in, &mt);
    layout(in, &mt);
    return (int)(Ln * mt.lineh / 20);
}

/* ---------------------------------------------------------------- render */
static u32 blink_t;

static void fill_local(mat m, float x0, float y0, float x1, float y1, u32 col) {
    int a = (int)(m.a * x0 + m.tx), b = (int)(m.d * y0 + m.ty);
    int c = (int)(m.a * x1 + m.tx), d = (int)(m.d * y1 + m.ty);
    if (c <= a) c = a + 1;
    if (d <= b) d = b + 1;
    fill_rect_masked(a, b, c - a, d - b, col);
}

void tf_render(inst *in, mat m, const cxf *cx) {
    etext_def *e = in->et;
    float bx0 = (float)e->bounds[0], bx1 = (float)e->bounds[1], by0 = (float)e->bounds[2], by1 = (float)e->bounds[3];
    if (e->has_border) fill_local(m, bx0, by0, bx1, by1, cx_apply(cx, e->bc));
    if (e->has_bg) {
        float px = 1.0f / (m.a > 0.001f ? m.a : 1);
        if (e->has_border) fill_local(m, bx0 + px, by0 + px, bx1 - px, by1 - px, cx_apply(cx, e->bg));
        else fill_local(m, bx0, by0, bx1, by1, cx_apply(cx, e->bg));
    }
    metric mt;
    get_metric(in, &mt);
    layout(in, &mt);
    int v = vis_lines(in, &mt);
    if (in->scroll > Ln - 1) in->scroll = Ln - 1;
    if (in->scroll < 0) in->scroll = 0;
    const u8 *t = (const u8 *)tf_text(in);
    int pw = (e->flags & 0x1000) != 0;
    u32 col = cx_apply(cx, e->color);
    int focused = in == text_focus;
    int sa = in->sel < in->caret ? in->sel : in->caret, sb = in->sel < in->caret ? in->caret : in->sel;
    float right = bx1 - G;
    for (int l = in->scroll; l < Ln && l < in->scroll + v; l++) {
        float ly = by0 + G + (l - in->scroll) * mt.lineh;
        float x = bx0 + G + align_off(in, &mt, l);
        /* selection */
        if (focused && sa != sb && sb > Ls[l] && sa <= Le[l]) {
            int a = sa > Ls[l] ? sa : Ls[l], b = sb < Le[l] ? sb : Le[l];
            float xa = x + x_of(in, &mt, l, a), xb = x + x_of(in, &mt, l, b);
            if (sb > Le[l]) xb += adv(&mt, ' ') / 2;
            if (xb > right) xb = right;
            if (xb > xa) fill_local(m, xa, ly, xb, ly + mt.lineh, cx_apply(cx, 0xFF9AD8FFu));
        }
        if (mt.fd) {
            for (int i = Ls[l]; i < Le[l]; i++) {
                u8 c = pw ? '*' : t[i];
                float w = adv(&mt, c);
                if (x + w > right + G) break;
                int g = mt.fd->map[c];
                if (g >= 0 && c != ' ' && c != '\t' && mt.fd->glyph[g]) {
                    mat gm = {mt.scale, 0, 0, mt.scale, x, ly + mt.ascent};
                    draw_glyph(mt.fd->glyph[g], mat_mul(m, gm), cx, e->color);
                }
                x += w;
            }
        } else {
            static char buf[512];
            int k = 0;
            for (int i = Ls[l]; i < Le[l] && k < 510; i++) buf[k++] = pw ? '*' : (t[i] == '\t' ? ' ' : (char)t[i]);
            buf[k] = 0;
            if (k) draw_bitmap_text(buf, m, x, ly, mt.h, col, right + G - x, mt.lineh, 0);
        }
    }
    /* caret */
    if (focused && ((plat_ms() - blink_t) / 530) % 2 == 0) {
        int l = line_of(in->caret);
        if (l >= in->scroll && l < in->scroll + v) {
            float x = bx0 + G + align_off(in, &mt, l) + x_of(in, &mt, l, in->caret);
            float ly = by0 + G + (l - in->scroll) * mt.lineh;
            float px = 1.0f / (m.a > 0.001f ? m.a : 1);
            if (x > right) x = right;
            fill_local(m, x, ly, x + 2 * px, ly + mt.lineh, col);
        }
    }
}

/* ---------------------------------------------------------------- editing */
static void changed(inst *in) {
    keep_caret_visible(in);
    text_changed(in);
}

void tf_select(inst *in, int a, int b) {
    in->sel = a;
    in->caret = b;
    clamp_sel(in);
    keep_caret_visible(in);
}
void tf_select_all(inst *in) { tf_select(in, 0, (int)tlen(in)); }

void tf_replace_sel(inst *in, const char *s, u32 n) {
    undo_push(in, 0);
    if (replace(in, in->sel, in->caret, s, n)) changed(in);
}

void tf_copy(inst *in, int cut) {
    int a = in->sel < in->caret ? in->sel : in->caret, b = in->sel < in->caret ? in->caret : in->sel;
    if (a == b) return;
    clip_set(tf_text(in) + a, (u32)(b - a));
    if (cut && !(in->et->flags & 0x0800)) {
        undo_push(in, 0);
        replace(in, a, b, "", 0);
        changed(in);
    }
}

void tf_paste(inst *in) {
    u32 n;
    const char *s = clip_get(&n);
    if (!n || (in->et->flags & 0x0800)) return;
    if (!multiline(in)) {
        u32 k = 0;
        while (k < n && s[k] != '\n' && s[k] != '\r') k++;
        n = k;
    }
    tf_replace_sel(in, s, n);
}

void tf_key(inst *in, int code, int ascii) {
    if (!in || in->dead || !in->et) return;
    clamp_sel(in);
    blink_t = plat_ms();
    int shift = plat_kdown(16), ctrl = plat_kdown(17);
    int ro = (in->et->flags & 0x0800) != 0;
    int a = in->sel < in->caret ? in->sel : in->caret, b = in->sel < in->caret ? in->caret : in->sel;
    if (ctrl) {
        switch (code) {
        case 65: tf_select_all(in); return;
        case 67: tf_copy(in, 0); return;
        case 88: tf_copy(in, 1); return;
        case 86: tf_paste(in); return;
        case 90: tf_undo(in); keep_caret_visible(in); return;
        }
        if (code != 37 && code != 39 && code != 36 && code != 35) return; /* other Ctrl shortcuts belong to the app */
    }
    metric mt;
    get_metric(in, &mt);
    layout(in, &mt);
    int n = (int)tlen(in);
    int move = -1;
    switch (code) {
    case 37: move = (!shift && a != b) ? a : (in->caret > 0 ? in->caret - 1 : 0);
        if (ctrl) { const char *t = tf_text(in); move = in->caret; while (move > 0 && t[move - 1] == ' ') move--; while (move > 0 && t[move - 1] != ' ' && t[move - 1] != '\n') move--; }
        break;
    case 39: move = (!shift && a != b) ? b : (in->caret < n ? in->caret + 1 : n);
        if (ctrl) { const char *t = tf_text(in); move = in->caret; while (move < n && t[move] != ' ' && t[move] != '\n') move++; while (move < n && t[move] == ' ') move++; }
        break;
    case 38: case 40: case 33: case 34: {
        int l = line_of(in->caret);
        float x = x_of(in, &mt, l, in->caret) + align_off(in, &mt, l);
        int v = vis_lines(in, &mt);
        int d = code == 38 ? -1 : code == 40 ? 1 : code == 33 ? -v : v;
        int nl = l + d;
        if (nl < 0) { move = 0; break; }
        if (nl >= Ln) { move = n; break; }
        move = pos_at(in, &mt, nl, x);
        if (code == 33 || code == 34) { in->scroll += d; if (in->scroll < 0) in->scroll = 0; }
        break;
    }
    case 36: move = ctrl ? 0 : Ls[line_of(in->caret)]; break;
    case 35: move = ctrl ? n : Le[line_of(in->caret)]; break;
    }
    if (move >= 0) {
        in->caret = move;
        if (!shift) in->sel = move;
        keep_caret_visible(in);
        return;
    }
    if (ro) return;
    if (code == 8 || code == 46) {
        if (a == b) {
            const char *t = tf_text(in);
            if (code == 8) { if (!a) return; a--; if (a > 0 && t[a] == '\n' && t[a - 1] == '\r') a--; }
            else { if (b >= n) return; b++; if (t[b - 1] == '\r' && b < n && t[b] == '\n') b++; }
        }
        undo_push(in, 2);
        replace(in, a, b, "", 0);
        changed(in);
        return;
    }
    char ch[4];
    u32 k = 0;
    if (code == 13) { if (!multiline(in)) return; ch[k++] = '\n'; }
    else if (code == 9) { if (!multiline(in)) return; ch[0] = ch[1] = ch[2] = ch[3] = ' '; k = 4; }
    else if (ascii >= 32 && ascii < 256 && ascii != 127) ch[k++] = (char)ascii;
    else return;
    undo_push(in, 1);
    if (replace(in, a, b, ch, k)) changed(in);
}

void tf_mouse(inst *in, float sx, float sy, int press, int shift) {
    if (!in || in->dead || !in->et) return;
    mat iv = mat_inv(inst_world(in));
    float x = iv.a * sx + iv.c * sy + iv.tx, y = iv.b * sx + iv.d * sy + iv.ty;
    metric mt;
    get_metric(in, &mt);
    layout(in, &mt);
    float top = (float)in->et->bounds[2] + G;
    int l = in->scroll + (int)kfloor((y - top) / mt.lineh);
    int v = vis_lines(in, &mt);
    if (!press) { /* dragging past the edge scrolls */
        if (l < in->scroll && in->scroll > 0) in->scroll--;
        else if (l >= in->scroll + v && in->scroll + v < Ln) in->scroll++;
    }
    if (l < 0) l = 0;
    if (l >= Ln) l = Ln - 1;
    int p = pos_at(in, &mt, l, x - in->et->bounds[0] - G);
    in->caret = p;
    if (press && !shift) in->sel = p;
    blink_t = plat_ms();
}

/* ---------------------------------------------------------------- formats */
etext_def *tf_own(inst *in) {
    etext_def *e = amalloc(sizeof *e);
    *e = *in->et;
    in->et = e;
    return e;
}

void tf_set_font(inst *in, u16 font, float h, int color, int align) {
    etext_def *e = tf_own(in);
    if (font) { e->font = font; e->flags |= 0x0100; }
    if (h > 0) e->height = h;
    if (color >= 0) e->color = 0xFF000000u | (u32)color;
    if (align >= 0) e->align = align;
}
