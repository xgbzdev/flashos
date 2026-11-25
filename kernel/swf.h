/* FlashKernel SWF player - shared definitions.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 */
#ifndef FLASH_SWF_H
#define FLASH_SWF_H
#include "kernel.h"

typedef double num;
typedef struct { float a, b, c, d, tx, ty; } mat;
typedef struct { float rm, gm, bm, am, ra, ga, ba, aa; } cxf;
extern const mat MAT_ID;
extern const cxf CX_ID;

typedef struct obj obj;
typedef struct inst inst;


/* ---------------- SWF bit reader ---------------- */
typedef struct { const u8 *d; u32 n, i, bb; int bn; } rd;
static inline void rinit(rd *r, const u8 *d, u32 n) { r->d = d; r->n = n; r->i = 0; r->bn = 0; r->bb = 0; }
static inline u32 r8(rd *r) { r->bn = 0; return r->i < r->n ? r->d[r->i++] : 0; }
static inline u32 r16(rd *r) { u32 a = r8(r); return a | (r8(r) << 8); }
static inline u32 r32(rd *r) { u32 a = r16(r); return a | (r16(r) << 16); }
static inline void ralign(rd *r) { r->bn = 0; }
static inline u32 rub(rd *r, int n) {
    u32 v = 0;
    while (n-- > 0) {
        if (!r->bn) { r->bb = r->i < r->n ? r->d[r->i++] : 0; r->bn = 8; }
        r->bn--;
        v = (v << 1) | ((r->bb >> r->bn) & 1);
    }
    return v;
}
static inline s32 rsb(rd *r, int n) {
    if (n <= 0) return 0;
    u32 v = rub(r, n);
    if (n < 32 && (v & (1u << (n - 1)))) v |= ~0u << n;
    return (s32)v;
}
static inline const char *rstr(rd *r) {
    const char *s = (const char *)r->d + r->i;
    while (r->i < r->n && r->d[r->i]) r->i++;
    if (r->i < r->n) r->i++;
    return s;
}
void rrect(rd *r, s32 *q);
mat rmat(rd *r);
cxf rcx(rd *r, int alpha);
u32 rrgb(rd *r);
u32 rrgba(rd *r);

/* ---------------- ActionScript values ---------------- */
enum { V_UNDEF, V_NULL, V_BOOL, V_NUM, V_STR, V_OBJ };
typedef struct { u8 t; union { num n; int b; const char *s; obj *o; } u; } val;

/* ---------------- characters ---------------- */
typedef struct { u8 type, ng, spread; u32 color; mat m; u8 ratio[16]; u32 gcol[16]; u16 bmp; } fstyle;
typedef struct { float w; u32 color; } lstyle;
typedef struct { float x0, y0, x1, y1; u16 f0, f1, ln; } edge;
typedef struct {
    int nfill, nline, nedge, nonzero;
    fstyle *fills;
    lstyle *lines;
    edge *edges;
    u32 *fofs, *fidx, *lofs, *lidx;
    s32 bounds[4]; /* twips: xmin xmax ymin ymax */
    int cap_e, cap_f, cap_l;
} shape;

typedef struct {
    const u8 *tags;
    u32 len;
    u16 nframes;
    int nlabels;
    const char **lname;
    u16 *lframe;
} timeline;

typedef struct { int w, h; u32 *px; } bitmap;
typedef struct { bitmap *bm; u8 transparent, smooth; } bdata; /* BitmapData */

typedef struct { u8 states; u16 id; s16 depth; mat m; cxf cx; } btnrec;
typedef struct { u16 cond; const u8 *code; u32 len; } btnact;
typedef struct { int nrec, nact; btnrec *rec; btnact *act; u16 snd[4]; } button_def;

typedef struct {
    int nglyph; shape **glyph; u16 *code; float em;
    const char *name;
    float ascent, descent, leading; /* em units */
    float *adv;                     /* per glyph, em units (NULL: no layout) */
    s16 map[256];                   /* Latin-1 char -> glyph index, -1 none */
    int mapped;
} font_def;
typedef struct { u16 font; u32 color; float x, y, height; int n; u16 *glyph; float *adv; } textrun;
typedef struct { s32 bounds[4]; mat m; int nrun; textrun *run; } text_def;
typedef struct {
    s32 bounds[4];
    u16 flags, font;
    float height;
    u32 color;
    int align;
    const char *var, *init;
    int maxlen;
    u8 has_bg, has_border;
    u32 bg, bc;
} etext_def;

enum { CH_NONE, CH_SHAPE, CH_SPRITE, CH_BITMAP, CH_BUTTON, CH_FONT, CH_TEXT, CH_ETEXT, CH_SOUND };
typedef struct { u8 type; void *p; const char *export_name; obj *klass; } chr;
extern chr *chars;

/* ---------------- display objects ---------------- */
enum { K_SHAPE, K_SPRITE, K_BUTTON, K_TEXT, K_ETEXT };
typedef struct { u32 flags; u8 key; const u8 *code; u32 len; } clipaction;

typedef struct {
    int nf, nl, ne, cap;
    fstyle *fills;
    lstyle *lines;
    edge *edges;
    u16 cur_fill, cur_line;
    float x, y, sx, sy;
    shape *built;
    int dirty;
} dyndraw;

struct inst {
    u8 kind, visible, playing, fresh, dynamic, enabled, hand, dead;
    u8 btn_state, as_moved;
    u16 id;
    int depth, seq;
    u16 clipdepth, ratio;
    mat m;
    cxf cx;
    const char *name;
    inst *parent;
    obj *o;
    timeline *tl;
    u32 pos;
    int frame;
    int n, cap;
    inst **kids;
    int nca;
    clipaction *ca;
    char *text;      /* edit text contents */
    etext_def *et;
    dyndraw *dyn;
    inst **btn_kids; /* sprite instances inside a button */
    /* text fields: editable buffer, caret, selection, scrolling */
    u32 tlen, tcap;
    int caret, sel, scroll;
    void *undo;
    /* attached BitmapData (attachBitmap) */
    void *bmpdata;
    /* setMask: this clip is drawn only where maskc is; a mask is not drawn */
    inst *maskc;
    u8 is_mask;
};

/* ---------------- player API used by the ActionScript engine ---------------- */
extern inst *root;
extern int swf_ver;
extern int stage_w, stage_h; /* pixels */
extern int mouse_visible, mouse_stage_x, mouse_stage_y;
extern int quit_requested;
extern int last_key_code, last_key_ascii;

void *amalloc(u32 n);
u32 a_free_bytes(void);
char *astrdup(const char *s);
char *astrndup(const char *s, u32 n);

inst *inst_create(inst *parent, u16 id, int depth, const char *name, int dynamic);
void inst_remove(inst *in);
inst *inst_child(inst *p, const char *name);
inst *inst_at_depth(inst *p, int depth);
void inst_goto(inst *in, int frame, int play);
int inst_label(inst *in, const char *label);
mat inst_world(inst *in);   /* local -> stage twips */
void inst_bounds(inst *in, int world, float *x0, float *y0, float *x1, float *y1);
int inst_hit_point(inst *in, float sx, float sy); /* stage twips */
void inst_swap_depth(inst *in, int depth);
inst *inst_duplicate(inst *src, const char *name, int depth);
int inst_next_depth(inst *p);
void inst_set_text(inst *in, const char *s);
void start_drag(inst *in, int lock, int constrain, float x0, float y0, float x1, float y1);
void stop_drag(void);
void shape_from_dyn(dyndraw *d);
dyndraw *dyn_get(inst *in);
shape *parse_dyn_shape(dyndraw *d);

/* ---------------- ActionScript engine (avm1.c) ---------------- */
void as_init(void);
obj *as_clip_obj(inst *in);
void as_clip_dead(inst *in);
void as_run(const u8 *code, u32 len, inst *target);
void as_queue(const u8 *code, u32 len, inst *target);
void as_queue_init_class(inst *in);
void as_flush_queue(void);
int as_has_handler(inst *in, const char *name);
void as_call_handler(inst *in, const char *name, val *args, int n);
void as_listeners(const char *which, const char *event); /* "Key" / "Mouse" */
void as_intervals(void);
void as_clip_constructed(inst *in);
val as_get_var_path(inst *base, const char *path);
void as_set_var_path(inst *base, const char *path, val v);
const char *as_tostring(val v);
void as_text_sync(inst *in);

static inline val vnum(num n) { val v; v.t = V_NUM; v.u.n = n; return v; }
static inline val vbool(int b) { val v; v.t = V_BOOL; v.u.b = b ? 1 : 0; return v; }
static inline val vstr(const char *s) { val v; v.t = V_STR; v.u.s = s; return v; }
static inline val vobj(obj *o) { val v; if (o) { v.t = V_OBJ; v.u.o = o; } else v.t = V_NULL; return v; }
static inline val vundef(void) { val v; v.t = V_UNDEF; return v; }

/* math (x87) */
num m_floor(num x);
num m_sin(num x);
num m_cos(num x);
num m_atan2(num y, num x);
num m_sqrt(num x);
num m_log(num x);
num m_exp(num x);
num m_pow(num a, num b);
num m_fmod(num a, num b);
int m_isnan(num x);
num m_nan(void);
num m_inf(void);

/* renderer (render.c) */
extern int scr_w, scr_h;
void render_init(void);
shape *parse_shape(const u8 *d, u32 len, int v, int is_glyph, int glyph_fbits_from_data);
bitmap *parse_lossless(const u8 *d, u32 len, int v2);
void draw_shape(shape *s, mat m, const cxf *cx, u32 override_color);
void draw_bitmap_text(const char *s, mat m, float x, float y, float h, u32 color, float clipw, float cliph, int align);
int shape_hit(shape *s, float x, float y);
void draw_cursor(int x, int y, int hand);
void erase_cursor(void);
mat mat_mul(mat p, mat q);
mat mat_inv(mat m);
cxf cx_mul(cxf p, cxf c);
u32 cx_apply(const cxf *c, u32 col);

#endif

/* ---------------- images (image.c) ---------------- */
bitmap *image_decode_file(const u8 *d, u32 n);       /* JPEG, PNG, GIF or BMP */
u8 *bmp_encode(const bitmap *b, u32 *len);           /* 24-bit BMP */
void draw_bitmap(const bitmap *b, mat m, const cxf *cx, int smooth);
void draw_glyph(shape *g, mat m, const cxf *cx, u32 color);
void glyph_cache_reset(void);
bitmap *image_decode_tag(int code, const u8 *d, u32 n);
void image_set_tables(const u8 *d, u32 n);

/* ---------------- masks (render.c) ---------------- */
u8 *mask_begin_write(int level);   /* clears and returns the mask buffer for a level */
void mask_set_write(u8 *buf);      /* NULL: normal drawing */
void mask_set_active(u8 *buf);     /* NULL: no clipping */
u8 *mask_get_active(void);
void fill_rect_masked(int x, int y, int w, int h, u32 col);

/* ---------------- text input (player.c) ---------------- */
extern inst *text_focus;
void text_set_focus(inst *in);
void text_changed(inst *in);          /* var binding + onChanged */
u16 font_by_name(const char *name);   /* embedded font character id, 0 none */
extern chr *chars;

/* ---------------- FLV video (flv.c) ---------------- */
typedef struct flvvid flvvid;
flvvid *flv_open(const u8 *data, u32 size);
bitmap *flv_bitmap(flvvid *v);
int flv_width(flvvid *v);
int flv_height(flvvid *v);
u32 flv_duration(flvvid *v);
u32 flv_time(flvvid *v);
int flv_playing(flvvid *v);
void flv_play(flvvid *v);
void flv_pause(flvvid *v);
void flv_stop(flvvid *v);
void flv_seek(flvvid *v, u32 ms);
void flv_close(flvvid *v);
void flv_tick_all(void);
void flv_reset_all(void);

/* ---------------- text fields (textfield.c) ---------------- */
void tf_render(inst *in, mat m, const cxf *c);
void tf_key(inst *in, int code, int ascii);
void tf_mouse(inst *in, float sx, float sy, int press, int shift); /* stage twips */
void tf_set_text(inst *in, const char *s, u32 n);
const char *tf_text(inst *in);         /* NUL-terminated contents */
int tf_maxscroll(inst *in);            /* 1-based like Flash */
int tf_bottom(inst *in);
void tf_scroll_to(inst *in, int line1);
void tf_select(inst *in, int a, int b);
void tf_replace_sel(inst *in, const char *s, u32 n);
void tf_copy(inst *in, int cut);
void tf_paste(inst *in);
void tf_undo(inst *in);
void tf_select_all(inst *in);
etext_def *tf_own(inst *in);           /* private, writable format */
void tf_set_font(inst *in, u16 font, float height_twips, int color, int align);
void clip_set(const char *s, u32 n);
const char *clip_get(u32 *n);
int tf_text_width(inst *in);           /* pixels of the longest line */
int tf_text_height(inst *in);

/* ---------------- sound (player.c) ---------------- */
typedef struct {
    u8 format, rate, is16, stereo, tried;
    u32 count, len;
    const u8 *data;
    short *pcm;
    u32 frames;
    int prate;
} sound_def;
int snd_play(u16 id, int loops, int vol, int tag);
int snd_find(const char *link);
u32 snd_duration_ms(u16 id);
