/* FlashKernel ActionScript engine (AVM1: ActionScript 1.0 and 2.0).
 * Copyright (c) 2026 FlashOS. All rights reserved.
 */
#include "swf.h"

/* ================================================================ math */
#ifdef HOSTED
#include <math.h>
num m_floor(num x) { return floor(x); }
num m_sin(num x) { return sin(x); }
num m_cos(num x) { return cos(x); }
num m_atan2(num y, num x) { return atan2(y, x); }
num m_sqrt(num x) { return sqrt(x); }
num m_log(num x) { return log(x); }
num m_exp(num x) { return exp(x); }
num m_pow(num a, num b) { return pow(a, b); }
num m_fmod(num a, num b) { return fmod(a, b); }
#else
num m_floor(num x) {
    num r;
    u16 cw, nw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    nw = (u16)((cw & ~0x0C00) | 0x0400); /* round down */
    __asm__ volatile("fldcw %0" : : "m"(nw));
    __asm__ volatile("frndint" : "=t"(r) : "0"(x));
    __asm__ volatile("fldcw %0" : : "m"(cw));
    return r;
}
num m_sin(num x) { num r; __asm__("fsin" : "=t"(r) : "0"(x)); return r; }
num m_cos(num x) { num r; __asm__("fcos" : "=t"(r) : "0"(x)); return r; }
num m_atan2(num y, num x) { num r; __asm__("fpatan" : "=t"(r) : "0"(x), "u"(y) : "st(1)"); return r; }
num m_sqrt(num x) { num r; __asm__("fsqrt" : "=t"(r) : "0"(x)); return r; }
num m_log(num x) { num r; __asm__("fldln2; fxch; fyl2x" : "=t"(r) : "0"(x)); return r; }
num m_exp(num x) {
    num r;
    /* 2^(x*log2(e)) */
    __asm__("fldl2e\n\t"
            "fmulp\n\t"
            "fld %%st(0)\n\t"
            "frndint\n\t"
            "fsub %%st, %%st(1)\n\t"
            "fxch\n\t"
            "f2xm1\n\t"
            "fld1\n\t"
            "faddp\n\t"
            "fscale\n\t"
            "fstp %%st(1)"
            : "=t"(r) : "0"(x));
    return r;
}
num m_fmod(num a, num b) {
    num r;
    __asm__("1: fprem\n\t"
            "fnstsw %%ax\n\t"
            "sahf\n\t"
            "jp 1b\n\t"
            "fstp %%st(1)"
            : "=t"(r) : "0"(a), "u"(b) : "ax", "cc");
    return r;
}
num m_pow(num a, num b) {
    if (b == 0) return 1;
    num fb = m_floor(b);
    if (fb == b && b > -1024 && b < 1024) {
        int n = (int)(b < 0 ? -b : b);
        num r = 1, p = a;
        while (n) { if (n & 1) r *= p; p *= p; n >>= 1; }
        return b < 0 ? 1 / r : r;
    }
    if (a < 0) return m_nan();
    if (a == 0) return 0;
    return m_exp(b * m_log(a));
}
#endif
int m_isnan(num x) { return x != x; }
num m_nan(void) { volatile num z = 0; return z / z; }
num m_inf(void) { volatile num z = 0; return 1 / z; }
static int m_isinf(num x) { return !m_isnan(x) && (x - x) != 0; }

/* ================================================================ objects */
enum { O_OBJ, O_ARRAY, O_FUNC, O_CLIP, O_SUPER, O_DATE };

typedef struct prop {
    const char *name;
    val v;
    struct prop *next;
    obj *getter, *setter;
    u8 dontenum;
} prop;

typedef struct ctx ctx;
typedef val (*native_fn)(ctx *c, val th, val *a, int n);

struct obj {
    u8 kind, captured;
    prop *props;
    obj *proto;
    inst *clip;
    /* functions */
    const u8 *code;
    u32 clen;
    u8 fn2, nregs;
    u16 fflags;
    int np;
    const char **pname;
    u8 *preg;
    obj **scope;
    int nscope;
    inst *deftarget;
    val *cpool;
    int ncpool;
    native_fn native;
    /* arrays */
    val *arr;
    int alen, acap;
    /* super */
    obj *sup_proto, *sup_ctor;
    val sup_this;
    num prim;
    obj *next_free;
};

struct ctx {
    inst *target, *orig;
    obj *with[16];
    u32 with_end[16];
    int nwith;
    obj *act;
    obj **scope;
    int nscope;
    val thisv;
    val *regs;
    int nregs;
    val *cpool;
    int ncpool;
    val *args;
    int nargs;
    obj *home;
    int ret_set;
    val ret;
};

static obj *G, *obj_proto, *func_proto, *array_proto, *string_proto, *number_proto, *bool_proto,
    *clip_proto, *button_proto, *text_proto, *color_proto, *sound_proto, *date_proto, *key_obj, *mouse_obj;
static ctx *cur;
static int depth_guard;
static val globals_regs[4];
static int throwing;
static val thrown;
static u32 rnd_state = 2463534242u;

static obj *free_objs;
static prop *free_props;

static int name_eq(const char *a, const char *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (swf_ver >= 7) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
    return !kstricmp(a, b);
}

static obj *new_obj(obj *proto) {
    obj *o;
    if (free_objs) { o = free_objs; free_objs = o->next_free; memset(o, 0, sizeof *o); }
    else o = amalloc(sizeof(obj));
    if (!o) return NULL;
    o->kind = O_OBJ;
    o->proto = proto;
    return o;
}
static prop *new_prop(void) {
    prop *p;
    if (free_props) { p = free_props; free_props = p->next; memset(p, 0, sizeof *p); return p; }
    return amalloc(sizeof(prop));
}
static void recycle_act(obj *o) {
    if (!o || o->captured) return;
    prop *p = o->props;
    while (p) { prop *n = p->next; p->next = free_props; free_props = p; p = n; }
    o->props = NULL;
    o->next_free = free_objs;
    free_objs = o;
}

static prop *own_prop(obj *o, const char *name) {
    for (prop *p = o->props; p; p = p->next) if (name_eq(p->name, name)) return p;
    return NULL;
}
static prop *find_prop(obj *o, const char *name, obj **where) {
    for (int d = 0; o && d < 64; o = o->proto, d++) {
        prop *p = own_prop(o, name);
        if (p) { if (where) *where = o; return p; }
    }
    return NULL;
}
static void set_own(obj *o, const char *name, val v) {
    if (!o) return;
    prop *p = own_prop(o, name);
    if (!p) {
        p = new_prop();
        if (!p) return;
        p->name = name;
        p->next = o->props;
        o->props = p;
    }
    p->v = v;
}
static void set_hidden(obj *o, const char *name, val v) {
    set_own(o, name, v);
    prop *p = own_prop(o, name);
    if (p) p->dontenum = 1;
}

static val call_fn(obj *fn, val thisv, val *args, int n, obj *home);
static int scmp(const char *a, const char *b);

/* ================================================================ conversions */
static const char *num_to_str(num x);

static num str_to_num(const char *s, int strict) {
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (!*s) return strict ? (swf_ver >= 7 ? m_nan() : 0) : m_nan();
    int neg = 0;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    num v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        int any = 0;
        for (;; s++) {
            int d;
            if (*s >= '0' && *s <= '9') d = *s - '0';
            else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
            else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
            else break;
            v = v * 16 + d;
            any = 1;
        }
        if (!any || (strict && *s)) return m_nan();
        return neg ? -v : v;
    }
    if (!kstrnicmp(s, "Infinity", 8)) return neg ? -m_inf() : m_inf();
    int any = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s++ - '0'); any = 1; }
    if (*s == '.') {
        s++;
        num f = 0.1;
        while (*s >= '0' && *s <= '9') { v += (*s++ - '0') * f; f *= 0.1; any = 1; }
    }
    if (!any) return m_nan();
    if (*s == 'e' || *s == 'E') {
        s++;
        int en = 0, e = 0;
        if (*s == '-') { en = 1; s++; } else if (*s == '+') s++;
        while (*s >= '0' && *s <= '9') e = e * 10 + (*s++ - '0');
        num p = 1;
        while (e--) p *= 10;
        v = en ? v / p : v * p;
    }
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    if (strict && *s) return m_nan();
    return neg ? -v : v;
}

static num to_num(val v) {
    switch (v.t) {
    case V_NUM: return v.u.n;
    case V_BOOL: return v.u.b;
    case V_STR: return str_to_num(v.u.s, 1);
    case V_UNDEF: return swf_ver >= 7 ? m_nan() : 0;
    case V_NULL: return swf_ver >= 7 ? m_nan() : 0;
    case V_OBJ: {
        obj *o = v.u.o;
        if (o->kind == O_DATE) return o->prim;
        prop *p = find_prop(o, "valueOf", NULL);
        if (p && p->v.t == V_OBJ && p->v.u.o->kind == O_FUNC && !p->v.u.o->native) {
            val r = call_fn(p->v.u.o, v, NULL, 0, NULL);
            if (r.t != V_OBJ) return to_num(r);
        }
        return m_nan();
    }
    }
    return 0;
}
static s32 to_i32(val v) {
    num d = to_num(v);
    if (m_isnan(d) || m_isinf(d)) return 0;
    d = d < 0 ? -m_floor(-d) : m_floor(d);
    if (d >= -2147483648.0 && d <= 2147483647.0) return (s32)d;
    d = m_fmod(d, 4294967296.0);
    if (d < 0) d += 4294967296.0;
    if (d >= 2147483648.0) d -= 4294967296.0;
    return (s32)d;
}
static int to_bool(val v) {
    switch (v.t) {
    case V_BOOL: return v.u.b;
    case V_NUM: return !m_isnan(v.u.n) && v.u.n != 0;
    case V_STR:
        if (swf_ver >= 7) return v.u.s[0] != 0;
        { num n = str_to_num(v.u.s, 1); return !m_isnan(n) && n != 0; }
    case V_OBJ: return 1;
    }
    return 0;
}

static const char *clip_path(inst *in, int slash);

const char *as_tostring(val v) {
    switch (v.t) {
    case V_UNDEF: return swf_ver >= 7 ? "undefined" : "";
    case V_NULL: return "null";
    case V_BOOL: return v.u.b ? "true" : "false";
    case V_NUM: return num_to_str(v.u.n);
    case V_STR: return v.u.s;
    case V_OBJ: {
        obj *o = v.u.o;
        if (o->kind == O_CLIP) return o->clip ? clip_path(o->clip, 0) : "";
        if (o->kind == O_FUNC) return "[type Function]";
        prop *p = find_prop(o, "toString", NULL);
        if (p && p->v.t == V_OBJ && p->v.u.o->kind == O_FUNC) {
            val r = call_fn(p->v.u.o, v, NULL, 0, NULL);
            if (r.t == V_STR) return r.u.s;
            if (r.t != V_OBJ) return as_tostring(r);
        }
        return "[object Object]";
    }
    }
    return "";
}

static char numbuf[64];
static const char *num_to_str(num x) {
    char *b = numbuf;
    if (m_isnan(x)) return "NaN";
    if (m_isinf(x)) return x > 0 ? "Infinity" : "-Infinity";
    if (x == 0) return "0";
    if (x < 0) { *b++ = '-'; x = -x; }
    if (x == m_floor(x) && x < 1e15) {
        num hi = m_floor(x / 1e9), lo = x - hi * 1e9;
        char t[24];
        int n = 0;
        u32 l = (u32)(int)lo;
        if (hi > 0) {
            u32 h = (u32)(int)hi;
            for (int i = 0; i < 9; i++) { t[n++] = (char)('0' + l % 10); l /= 10; }
            while (h) { t[n++] = (char)('0' + h % 10); h /= 10; }
        } else {
            do { t[n++] = (char)('0' + l % 10); l /= 10; } while (l);
        }
        while (n) *b++ = t[--n];
        *b = 0;
        return astrdup(numbuf);
    }
    /* 15 significant digits */
    int e = 0;
    num y = x;
    while (y >= 10) { y /= 10; e++; }
    while (y < 1) { y *= 10; e--; }
    num m = y * 1e14;
    num mf = m_floor(m + 0.5);
    if (mf >= 1e15) { mf = m_floor(mf / 10 + 0.5); e++; }
    char dg[16];
    num hi = m_floor(mf / 1e8), lo = mf - hi * 1e8;
    u32 hh = (u32)(int)hi, ll = (u32)(int)lo;
    for (int i = 6; i >= 0; i--) { dg[i] = (char)('0' + hh % 10); hh /= 10; }
    for (int i = 14; i >= 7; i--) { dg[i] = (char)('0' + ll % 10); ll /= 10; }
    int nd = 15;
    while (nd > 1 && dg[nd - 1] == '0') nd--;
    if (e >= 15 || e < -5) {
        *b++ = dg[0];
        if (nd > 1) { *b++ = '.'; for (int i = 1; i < nd; i++) *b++ = dg[i]; }
        *b++ = 'e';
        *b++ = e < 0 ? '-' : '+';
        char t[8];
        int n = 0, ae = e < 0 ? -e : e;
        do { t[n++] = (char)('0' + ae % 10); ae /= 10; } while (ae);
        while (n) *b++ = t[--n];
    } else if (e < 0) {
        *b++ = '0'; *b++ = '.';
        for (int i = -1; i > e; i--) *b++ = '0';
        for (int i = 0; i < nd; i++) *b++ = dg[i];
    } else {
        for (int i = 0; i <= e; i++) *b++ = i < nd ? dg[i] : '0';
        if (nd > e + 1) { *b++ = '.'; for (int i = e + 1; i < nd; i++) *b++ = dg[i]; }
    }
    *b = 0;
    return astrdup(numbuf);
}

static const char *str_cat(const char *a, const char *b) {
    u32 la = kstrlen(a), lb = kstrlen(b);
    char *d = amalloc(la + lb + 1);
    if (!d) return "";
    memcpy(d, a, la);
    memcpy(d + la, b, lb + 1);
    return d;
}

/* ================================================================ clips */
obj *as_clip_obj(inst *in) {
    if (!in) return NULL;
    if (!in->o) {
        in->o = new_obj(in->kind == K_BUTTON ? button_proto : in->kind == K_ETEXT ? text_proto : clip_proto);
        if (!in->o) return NULL;
        in->o->kind = O_CLIP;
        in->o->clip = in;
        in->o->captured = 1;
    }
    return in->o;
}
void as_clip_dead(inst *in) { if (in->o) in->o->clip = NULL; }

static const char *clip_path(inst *in, int slash) {
    const char *parts[64];
    int n = 0;
    for (inst *p = in; p && p != root && n < 64; p = p->parent) parts[n++] = p->name ? p->name : "instance";
    const char *s = slash ? "" : "_level0";
    if (slash && !n) return "/";
    while (n--) s = str_cat(str_cat(s, slash ? "/" : "."), parts[n]);
    return s;
}

static inst *val_clip(val v) {
    if (v.t == V_OBJ && v.u.o->kind == O_CLIP) return v.u.o->clip;
    return NULL;
}

static val lookup_var(ctx *c, const char *name);
static val get_member(val o, val name);

static inst *path_to_clip(ctx *c, const char *p) {
    inst *cur_t = c->target;
    if (!p) return NULL;
    if (!*p) return cur_t;
    if (*p == '/') { cur_t = root; p++; }
    char seg[128];
    while (*p) {
        int n = 0;
        while (*p && *p != '/' && *p != '.' && *p != ':' && n < 127) seg[n++] = *p++;
        seg[n] = 0;
        if (*p == '.' && p[1] == '.') { /* "../" */
            if (!n) { p += 2; cur_t = cur_t ? cur_t->parent : NULL; if (*p == '/') p++; continue; }
        }
        if (*p) p++;
        if (!n) continue;
        if (!cur_t) return NULL;
        if (!kstricmp(seg, "_root") || !kstricmp(seg, "_level0")) cur_t = root;
        else if (!kstricmp(seg, "_parent") || !kstrnicmp(seg, "..", 3)) cur_t = cur_t->parent;
        else if (!kstricmp(seg, "this")) { inst *t = val_clip(c->thisv); if (t) cur_t = t; }
        else {
            inst *k = inst_child(cur_t, seg);
            if (!k) {
                val v = get_member(vobj(as_clip_obj(cur_t)), vstr(astrdup(seg)));
                k = val_clip(v);
                if (!k && cur_t == c->target) k = val_clip(lookup_var(c, astrdup(seg)));
            }
            cur_t = k;
        }
    }
    return cur_t;
}

static inst *target_of(ctx *c, val v) {
    inst *t = val_clip(v);
    if (t) return t;
    if (v.t == V_STR) return path_to_clip(c, v.u.s);
    return NULL;
}

/* ---- clip properties ---- */
static num clip_xscale(inst *in) { return m_sqrt(in->m.a * in->m.a + in->m.b * in->m.b) * 100; }
static num clip_yscale(inst *in) {
    num s = m_sqrt(in->m.c * in->m.c + in->m.d * in->m.d) * 100;
    return (in->m.a * in->m.d - in->m.b * in->m.c) < 0 ? -s : s;
}
static num clip_rot(inst *in) { return m_atan2(in->m.b, in->m.a) * 180 / 3.14159265358979; }
static void clip_compose(inst *in, num xs, num ys, num rot) {
    num r = rot * 3.14159265358979 / 180, cs = m_cos(r), sn = m_sin(r);
    in->m.a = (float)(xs / 100 * cs);
    in->m.b = (float)(xs / 100 * sn);
    in->m.c = (float)(-ys / 100 * sn);
    in->m.d = (float)(ys / 100 * cs);
}

static const char *PROPS[] = {"_x", "_y", "_xscale", "_yscale", "_currentframe", "_totalframes", "_alpha",
                              "_visible", "_width", "_height", "_rotation", "_target", "_framesloaded",
                              "_name", "_droptarget", "_url", "_highquality", "_focusrect",
                              "_soundbuftime", "_quality", "_xmouse", "_ymouse", 0};

static int prop_index(const char *name) {
    if (!name || name[0] != '_') return -1;
    for (int i = 0; PROPS[i]; i++) if (!kstricmp(PROPS[i], name)) return i;
    return -1;
}

static val clip_getp(inst *in, int idx) {
    float x0, y0, x1, y1;
    switch (idx) {
    case 0: return vnum(in->m.tx / 20.0);
    case 1: return vnum(in->m.ty / 20.0);
    case 2: return vnum(clip_xscale(in));
    case 3: return vnum(clip_yscale(in));
    case 4: return vnum(in->kind == K_SPRITE ? (in->frame < 1 ? 1 : in->frame) : 1);
    case 5: case 12: return vnum(in->tl ? in->tl->nframes : 1);
    case 6: return vnum(in->cx.am * 100.0);
    case 7: return vbool(in->visible);
    case 8: inst_bounds(in, 0, &x0, &y0, &x1, &y1); return vnum((x1 - x0) / 20.0);
    case 9: inst_bounds(in, 0, &x0, &y0, &x1, &y1); return vnum((y1 - y0) / 20.0);
    case 10: return vnum(clip_rot(in));
    case 11: return vstr(clip_path(in, 1));
    case 13: return vstr(in->name ? in->name : "");
    case 14: return vstr("");
    case 15: return vstr("file:///flashos.swf");
    case 16: return vnum(1);
    case 17: return vbool(1);
    case 18: return vnum(5);
    case 19: return vstr("HIGH");
    case 20: case 21: {
        mat iv = mat_inv(inst_world(in));
        float sx = mouse_stage_x * 20.0f, sy = mouse_stage_y * 20.0f;
        return vnum((idx == 20 ? iv.a * sx + iv.c * sy + iv.tx : iv.b * sx + iv.d * sy + iv.ty) / 20.0);
    }
    }
    return vundef();
}

static void clip_setp(inst *in, int idx, val v) {
    num n = to_num(v);
    if (idx != 7 && idx != 13 && m_isnan(n)) return;
    float x0, y0, x1, y1;
    switch (idx) {
    case 0: in->m.tx = (float)(n * 20); break;
    case 1: in->m.ty = (float)(n * 20); break;
    case 2: clip_compose(in, n, clip_yscale(in), clip_rot(in)); break;
    case 3: clip_compose(in, clip_xscale(in), n, clip_rot(in)); break;
    case 6: in->cx.am = (float)(n / 100); break;
    case 7: in->visible = (u8)to_bool(v); break;
    case 8: case 9: {
        inst_bounds(in, 0, &x0, &y0, &x1, &y1);
        num cur = (idx == 8 ? x1 - x0 : y1 - y0) / 20.0;
        if (cur <= 0) break;
        num f = n / cur;
        if (idx == 8) clip_compose(in, clip_xscale(in) * f, clip_yscale(in), clip_rot(in));
        else clip_compose(in, clip_xscale(in), clip_yscale(in) * f, clip_rot(in));
        break;
    }
    case 10: clip_compose(in, clip_xscale(in), clip_yscale(in), n); break;
    case 13: in->name = astrdup(as_tostring(v)); break;
    default: return;
    }
    in->as_moved = 1;
}

/* ================================================================ members */
static int arr_index(const char *s, int *out) {
    if (!*s) return 0;
    int v = 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return 0;
        v = v * 10 + (*s - '0');
        if (v > 10000000) return 0;
    }
    *out = v;
    return 1;
}
static void arr_set(obj *a, int i, val v) {
    if (i < 0) return;
    if (i >= a->acap) {
        int nc = a->acap ? a->acap : 8;
        while (nc <= i) nc *= 2;
        val *na = amalloc(sizeof(val) * nc);
        if (!na) return;
        if (a->alen) memcpy(na, a->arr, sizeof(val) * a->alen);
        a->arr = na;
        a->acap = nc;
    }
    for (int k = a->alen; k < i; k++) a->arr[k].t = V_UNDEF;
    a->arr[i] = v;
    if (i >= a->alen) a->alen = i + 1;
}
static obj *new_array(void) { obj *a = new_obj(array_proto); if (a) a->kind = O_ARRAY; return a; }

static val get_prop_val(obj *o, val self, const char *name) {
    obj *where;
    prop *p = find_prop(o, name, &where);
    if (!p) return vundef();
    if (p->getter) return call_fn(p->getter, self, NULL, 0, NULL);
    return p->v;
}

static val get_member(val ov, val namev) {
    const char *name = namev.t == V_STR ? namev.u.s : as_tostring(namev);
    if (ov.t == V_STR) {
        if (!kstricmp(name, "length")) {
            return vnum(kstrlen(ov.u.s));
        }
        return get_prop_val(string_proto, ov, name);
    }
    if (ov.t == V_NUM) return get_prop_val(number_proto, ov, name);
    if (ov.t == V_BOOL) return get_prop_val(bool_proto, ov, name);
    if (ov.t != V_OBJ) return vundef();
    obj *o = ov.u.o;
    if (o->kind == O_CLIP) {
        inst *in = o->clip;
        if (in) {
            int pi = prop_index(name);
            if (pi >= 0) return clip_getp(in, pi);
            if (!kstricmp(name, "_parent")) return in->parent ? vobj(as_clip_obj(in->parent)) : vundef();
            if (!kstricmp(name, "_root")) return vobj(as_clip_obj(root));
            if (!kstricmp(name, "_global")) return vobj(G);
            if (in->kind == K_ETEXT) {
                if (!kstricmp(name, "text") || !kstricmp(name, "htmlText")) return vstr(in->text ? in->text : "");
                if (!kstricmp(name, "textColor")) return vnum(in->et->color & 0xFFFFFF);
                if (!kstricmp(name, "length")) return vnum(in->text ? kstrlen(in->text) : 0);
            }
            if (!kstricmp(name, "enabled")) return vbool(in->enabled);
            if (!kstricmp(name, "useHandCursor")) return vbool(in->hand);
            prop *p = own_prop(o, name);
            if (p) return p->getter ? call_fn(p->getter, ov, NULL, 0, NULL) : p->v;
            inst *k = inst_child(in, name);
            if (k) return vobj(as_clip_obj(k));
        }
        return get_prop_val(o, ov, name);
    }
    if (o->kind == O_ARRAY) {
        int idx;
        if (!kstricmp(name, "length")) return vnum(o->alen);
        if (arr_index(name, &idx)) return idx < o->alen ? o->arr[idx] : vundef();
    }
    if (o->kind == O_SUPER) return get_prop_val(o->sup_proto, o->sup_this, name);
    if (!kstricmp(name, "__proto__")) return vobj(o->proto);
    return get_prop_val(o, ov, name);
}

static void set_member(val ov, val namev, val v) {
    if (ov.t != V_OBJ) return;
    obj *o = ov.u.o;
    const char *name = namev.t == V_STR ? namev.u.s : as_tostring(namev);
    if (o->kind == O_CLIP && o->clip) {
        inst *in = o->clip;
        int pi = prop_index(name);
        if (pi >= 0) { clip_setp(in, pi, v); return; }
        if (in->kind == K_ETEXT) {
            if (!kstricmp(name, "text") || !kstricmp(name, "htmlText")) {
                inst_set_text(in, as_tostring(v));
                if (in->et->var && *in->et->var && in->parent) {
                    obj *po = as_clip_obj(in->parent);
                    set_own(po, in->et->var, vstr(in->text));
                }
                return;
            }
            if (!kstricmp(name, "textColor")) {
                etext_def *e = amalloc(sizeof *e);
                *e = *in->et;
                e->color = 0xFF000000u | ((u32)to_i32(v) & 0xFFFFFF);
                in->et = e;
                return;
            }
        }
        if (!kstricmp(name, "enabled")) { in->enabled = (u8)to_bool(v); return; }
        if (!kstricmp(name, "useHandCursor")) { in->hand = (u8)to_bool(v); return; }
    }
    if (o->kind == O_ARRAY) {
        int idx;
        if (!kstricmp(name, "length")) {
            int n = to_i32(v);
            if (n < 0) n = 0;
            if (n < o->alen) o->alen = n;
            else if (n > o->alen) arr_set(o, n - 1, vundef());
            return;
        }
        if (arr_index(name, &idx)) { arr_set(o, idx, v); return; }
    }
    if (!kstricmp(name, "__proto__")) { o->proto = v.t == V_OBJ ? v.u.o : NULL; return; }
    /* setter on the prototype chain? */
    obj *where;
    prop *p = find_prop(o, name, &where);
    if (p && p->setter) { call_fn(p->setter, ov, &v, 1, NULL); return; }
    if (p && where == o) { p->v = v; return; }
    set_own(o, namev.t == V_STR ? name : astrdup(name), v);
}

static int has_member(obj *o, const char *name) {
    if (o->kind == O_CLIP && o->clip) {
        if (prop_index(name) >= 0) return 1;
        if (inst_child(o->clip, name)) return 1;
    }
    return find_prop(o, name, NULL) != NULL;
}

/* ================================================================ variables */
static val lookup_var(ctx *c, const char *name) {
    if (!kstricmp(name, "this")) return c->thisv;
    if (!kstricmp(name, "_root") || !kstricmp(name, "_level0")) return vobj(as_clip_obj(root));
    if (!kstricmp(name, "_global")) return vobj(G);
    if (!kstricmp(name, "_parent")) return c->target && c->target->parent ? vobj(as_clip_obj(c->target->parent)) : vundef();
    for (int i = c->nwith - 1; i >= 0; i--)
        if (has_member(c->with[i], name)) return get_member(vobj(c->with[i]), vstr(name));
    if (c->act) {
        prop *p = own_prop(c->act, name);
        if (p) return p->v;
        if (c->args && !kstricmp(name, "arguments")) {
            obj *a = new_array();
            for (int i = 0; i < c->nargs; i++) arr_set(a, i, c->args[i]);
            return vobj(a);
        }
    }
    for (int i = c->nscope - 1; i >= 0; i--) {
        prop *p = own_prop(c->scope[i], name);
        if (p) return p->v;
    }
    if (c->target && !c->target->dead) {
        obj *t = as_clip_obj(c->target);
        if (has_member(t, name)) return get_member(vobj(t), vstr(name));
    }
    prop *p = find_prop(G, name, NULL);
    if (p) return p->v;
    return vundef();
}

static val get_var(ctx *c, const char *name) {
    /* slash / colon syntax: "/a/b:var", "a:var", "../x" */
    const char *colon = 0;
    for (const char *p = name; *p; p++) if (*p == ':') colon = p;
    if (colon) {
        inst *t = path_to_clip(c, astrndup(name, (u32)(colon - name)));
        if (!t) return vundef();
        return get_member(vobj(as_clip_obj(t)), vstr(colon + 1));
    }
    if (name[0] == '/' || (name[0] == '.' && name[1] == '.')) {
        inst *t = path_to_clip(c, name);
        return t ? vobj(as_clip_obj(t)) : vundef();
    }
    /* dot syntax: "a.b.c" */
    const char *dot = 0;
    for (const char *p = name; *p; p++) if (*p == '.') { dot = p; break; }
    if (dot) {
        val v = lookup_var(c, astrndup(name, (u32)(dot - name)));
        const char *p = dot + 1;
        while (*p) {
            const char *e = p;
            while (*e && *e != '.') e++;
            v = get_member(v, vstr(astrndup(p, (u32)(e - p))));
            p = *e ? e + 1 : e;
        }
        return v;
    }
    return lookup_var(c, name);
}

static void set_var(ctx *c, const char *name, val v) {
    const char *colon = 0, *dot = 0;
    for (const char *p = name; *p; p++) { if (*p == ':') colon = p; if (*p == '.') dot = p; }
    if (colon) {
        inst *t = path_to_clip(c, astrndup(name, (u32)(colon - name)));
        if (t) set_member(vobj(as_clip_obj(t)), vstr(colon + 1), v);
        return;
    }
    if (dot) {
        val o = get_var(c, astrndup(name, (u32)(dot - name)));
        set_member(o, vstr(dot + 1), v);
        return;
    }
    for (int i = c->nwith - 1; i >= 0; i--)
        if (has_member(c->with[i], name)) { set_member(vobj(c->with[i]), vstr(name), v); return; }
    if (c->act) {
        prop *p = own_prop(c->act, name);
        if (p) { p->v = v; return; }
    }
    for (int i = c->nscope - 1; i >= 0; i--) {
        prop *p = own_prop(c->scope[i], name);
        if (p) { p->v = v; return; }
    }
    if (c->target && !c->target->dead) set_member(vobj(as_clip_obj(c->target)), vstr(name), v);
}

val as_get_var_path(inst *base, const char *path) {
    ctx c;
    memset(&c, 0, sizeof c);
    c.target = c.orig = base ? base : root;
    c.thisv = vobj(as_clip_obj(c.target));
    return get_var(&c, path);
}

void as_text_sync(inst *in) {
    if (!in->et || !in->et->var || !*in->et->var) return;
    inst *base = in->parent ? in->parent : root;
    val v = as_get_var_path(base, in->et->var);
    if (v.t == V_UNDEF) {
        ctx c;
        memset(&c, 0, sizeof c);
        c.target = c.orig = base;
        set_var(&c, in->et->var, vstr(in->text ? in->text : ""));
        return;
    }
    const char *s = as_tostring(v);
    if (!in->text || scmp(s, in->text))
        inst_set_text(in, s);
}

/* ================================================================ functions */
static obj *new_fn(native_fn f) {
    obj *o = new_obj(func_proto);
    if (!o) return NULL;
    o->kind = O_FUNC;
    o->native = f;
    o->captured = 1;
    return o;
}
static void def_native(obj *on, const char *name, native_fn f) {
    obj *fn = new_fn(f);
    set_hidden(on, name, vobj(fn));
}

static val run_code(ctx *c, const u8 *code, u32 len);

static val call_fn(obj *fn, val thisv, val *args, int n, obj *home) {
    if (!fn || fn->kind != O_FUNC) return vundef();
    if (fn->native) return fn->native(cur, thisv, args, n);
    if (depth_guard > 64) return vundef();
    depth_guard++;
    ctx c;
    memset(&c, 0, sizeof c);
    c.target = fn->deftarget && !fn->deftarget->dead ? fn->deftarget : root;
    c.orig = c.target;
    c.thisv = thisv;
    if (thisv.t != V_OBJ && thisv.t != V_STR && thisv.t != V_NUM) c.thisv = vobj(as_clip_obj(c.target));
    c.scope = fn->scope;
    c.nscope = fn->nscope;
    c.cpool = fn->cpool;
    c.ncpool = fn->ncpool;
    c.args = args;
    c.nargs = n;
    c.home = home;
    c.act = new_obj(NULL);
    val regs[256];
    int nregs = fn->fn2 ? fn->nregs : 4;
    for (int i = 0; i < nregs; i++) regs[i].t = V_UNDEF;
    c.regs = regs;
    c.nregs = nregs;
    if (fn->fn2) {
        int r = 1;
        u16 f = fn->fflags;
        if (f & 0x0001) { if (r < nregs) regs[r] = c.thisv; r++; }
        if (f & 0x0004) {
            obj *a = new_array();
            for (int i = 0; i < n; i++) arr_set(a, i, args[i]);
            if (r < nregs) regs[r] = vobj(a);
            r++;
        }
        if (f & 0x0010) {
            obj *s = new_obj(NULL);
            s->kind = O_SUPER;
            s->sup_this = c.thisv;
            obj *h = home;
            if (!h && thisv.t == V_OBJ) h = thisv.u.o->proto;
            s->sup_proto = h ? h->proto : NULL;
            prop *pc = h ? find_prop(h, "__constructor__", NULL) : NULL;
            s->sup_ctor = pc && pc->v.t == V_OBJ ? pc->v.u.o : NULL;
            if (r < nregs) regs[r] = vobj(s);
            r++;
        }
        if (f & 0x0040) { if (r < nregs) regs[r] = vobj(as_clip_obj(root)); r++; }
        if (f & 0x0080) { if (r < nregs) regs[r] = c.target->parent ? vobj(as_clip_obj(c.target->parent)) : vundef(); r++; }
        if (f & 0x0100) { if (r < nregs) regs[r] = vobj(G); r++; }
        for (int i = 0; i < fn->np; i++) {
            val a = i < n ? args[i] : vundef();
            if (fn->preg[i] && fn->preg[i] < nregs) regs[fn->preg[i]] = a;
            else set_own(c.act, fn->pname[i], a);
        }
    } else {
        for (int i = 0; i < fn->np; i++) set_own(c.act, fn->pname[i], i < n ? args[i] : vundef());
    }
    ctx *saved = cur;
    cur = &c;
    val r = run_code(&c, fn->code, fn->clen);
    cur = saved;
    depth_guard--;
    recycle_act(c.act);
    return r;
}

static val construct(obj *ctor, val *args, int n) {
    if (!ctor || ctor->kind != O_FUNC) return vundef();
    prop *pp = own_prop(ctor, "prototype");
    obj *o = new_obj(pp && pp->v.t == V_OBJ ? pp->v.u.o : obj_proto);
    if (!o) return vundef();
    o->captured = 1;
    set_hidden(o, "__constructor__", vobj(ctor));
    val r = call_fn(ctor, vobj(o), args, n, pp && pp->v.t == V_OBJ ? pp->v.u.o : NULL);
    if (r.t == V_OBJ && ctor->native) return r;
    if (r.t == V_OBJ && r.u.o != o && r.u.o->kind != O_CLIP) return r;
    return vobj(o);
}

/* ================================================================ queue / entry */
typedef struct { const u8 *code; u32 len; inst *target; } qitem;
static qitem *q;
static int qn, qcap, qh;

void as_queue(const u8 *code, u32 len, inst *target) {
    if (qn == qcap) {
        int nc = qcap ? qcap * 2 : 64;
        qitem *nq = amalloc(sizeof(qitem) * nc);
        if (!nq) return;
        if (qn) memcpy(nq, q, sizeof(qitem) * qn);
        q = nq;
        qcap = nc;
    }
    q[qn].code = code;
    q[qn].len = len;
    q[qn].target = target;
    qn++;
}
void as_queue_init_class(inst *in) { (void)in; }

void as_flush_queue(void) {
    while (qh < qn) {
        qitem it = q[qh++];
        if (it.target && !it.target->dead) as_run(it.code, it.len, it.target);
    }
    qh = qn = 0;
}

void as_run(const u8 *code, u32 len, inst *target) {
    if (!target || target->dead) return;
    ctx c;
    memset(&c, 0, sizeof c);
    c.target = c.orig = target;
    c.thisv = vobj(as_clip_obj(target));
    c.regs = globals_regs;
    c.nregs = 4;
    ctx *saved = cur;
    cur = &c;
    throwing = 0;
    run_code(&c, code, len);
    throwing = 0;
    cur = saved;
}

int as_has_handler(inst *in, const char *name) {
    if (!in || !in->o) return 0;
    prop *p = find_prop(in->o, name, NULL);
    return p && p->v.t == V_OBJ && p->v.u.o->kind == O_FUNC;
}

void as_call_handler(inst *in, const char *name, val *args, int n) {
    if (!in || !in->o || in->dead) return;
    prop *p = find_prop(in->o, name, NULL);
    if (!p || p->v.t != V_OBJ || p->v.u.o->kind != O_FUNC) return;
    ctx c;
    memset(&c, 0, sizeof c);
    c.target = c.orig = in;
    c.thisv = vobj(in->o);
    ctx *saved = cur;
    cur = &c;
    call_fn(p->v.u.o, vobj(in->o), args, n, NULL);
    cur = saved;
}

void as_listeners(const char *which, const char *event) {
    obj *o = !kstricmp(which, "Key") ? key_obj : mouse_obj;
    prop *p = own_prop(o, "_listeners");
    if (!p || p->v.t != V_OBJ) return;
    obj *a = p->v.u.o;
    for (int i = 0; i < a->alen; i++) {
        val l = a->arr[i];
        val f = get_member(l, vstr(event));
        if (f.t == V_OBJ && f.u.o->kind == O_FUNC) {
            ctx c;
            memset(&c, 0, sizeof c);
            c.target = c.orig = root;
            c.thisv = l;
            ctx *saved = cur;
            cur = &c;
            call_fn(f.u.o, l, NULL, 0, NULL);
            cur = saved;
        }
    }
}

void as_clip_constructed(inst *in) {
    if (!in || in->kind != K_SPRITE) return;
    obj *k = chars[in->id].klass;
    if (!k) return;
    obj *o = as_clip_obj(in);
    prop *pp = own_prop(k, "prototype");
    if (pp && pp->v.t == V_OBJ) o->proto = pp->v.u.o;
    set_hidden(o, "__constructor__", vobj(k));
    ctx c;
    memset(&c, 0, sizeof c);
    c.target = c.orig = in;
    c.thisv = vobj(o);
    ctx *saved = cur;
    cur = &c;
    call_fn(k, vobj(o), NULL, 0, pp && pp->v.t == V_OBJ ? pp->v.u.o : NULL);
    cur = saved;
}

/* ---- intervals ---- */
typedef struct { int id, active, once; u32 ms, next; val fn, thisv; const char *method; val args[4]; int nargs; } interval;
static interval *ivs;
static int niv, ivcap, ivid;

void as_intervals(void) {
    u32 now = plat_ms();
    for (int i = 0; i < niv; i++) {
        interval *t = &ivs[i];
        if (!t->active || (s32)(now - t->next) < 0) continue;
        t->next += t->ms ? t->ms : 1;
        if ((s32)(now - t->next) > 1000) t->next = now + t->ms;
        if (t->once) t->active = 0;
        val f = t->fn;
        if (t->method) f = get_member(t->thisv, vstr(t->method));
        if (f.t == V_OBJ && f.u.o->kind == O_FUNC) {
            ctx c;
            memset(&c, 0, sizeof c);
            c.target = c.orig = root;
            c.thisv = t->thisv;
            ctx *saved = cur;
            cur = &c;
            call_fn(f.u.o, t->thisv, t->args, t->nargs, NULL);
            cur = saved;
        }
    }
}

/* ================================================================ interpreter */
#define STK 4096
static val stk[STK];
static int sp;
static inline void push(val v) { if (sp < STK) stk[sp++] = v; }
static inline val pop(void) { return sp > 0 ? stk[--sp] : vundef(); }

static int rand_int(int max) {
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 17;
    rnd_state ^= rnd_state << 5;
    return max > 0 ? (int)(rnd_state % (u32)max) : 0;
}

static val add2(val a, val b) {
    if (a.t == V_OBJ) { obj *o = a.u.o; if (o->kind == O_DATE) a = vnum(o->prim); else a = vstr(as_tostring(a)); }
    if (b.t == V_OBJ) { obj *o = b.u.o; if (o->kind == O_DATE) b = vnum(o->prim); else b = vstr(as_tostring(b)); }
    if (a.t == V_STR || b.t == V_STR) return vstr(str_cat(as_tostring(a), as_tostring(b)));
    return vnum(to_num(a) + to_num(b));
}

static int scmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (u8)*a - (u8)*b;
}

static int equals2(val a, val b) {
    if (a.t == b.t) {
        switch (a.t) {
        case V_UNDEF: case V_NULL: return 1;
        case V_NUM: return a.u.n == b.u.n;
        case V_STR: return !scmp(a.u.s, b.u.s);
        case V_BOOL: return a.u.b == b.u.b;
        case V_OBJ: return a.u.o == b.u.o || (a.u.o->kind == O_CLIP && b.u.o->kind == O_CLIP && a.u.o->clip == b.u.o->clip && a.u.o->clip);
        }
    }
    if ((a.t == V_UNDEF || a.t == V_NULL) && (b.t == V_UNDEF || b.t == V_NULL)) return 1;
    if (a.t == V_UNDEF || a.t == V_NULL || b.t == V_UNDEF || b.t == V_NULL) return 0;
    if (a.t == V_OBJ && b.t == V_OBJ) return 0;
    if (a.t == V_OBJ) {
        if (a.u.o->kind == O_CLIP && b.t == V_STR) return !scmp(as_tostring(a), b.u.s);
        a = a.u.o->kind == O_DATE ? vnum(a.u.o->prim) : vstr(as_tostring(a));
    }
    if (b.t == V_OBJ) {
        if (b.u.o->kind == O_CLIP && a.t == V_STR) return !scmp(as_tostring(b), a.u.s);
        b = b.u.o->kind == O_DATE ? vnum(b.u.o->prim) : vstr(as_tostring(b));
    }
    if (a.t == V_STR && b.t == V_STR) return !scmp(a.u.s, b.u.s);
    return to_num(a) == to_num(b);
}

static int strict_eq(val a, val b) {
    if (a.t != b.t) return 0;
    return equals2(a, b);
}

/* returns 1 if a<b, 0 if not, -1 undefined (NaN) */
static int less2(val a, val b) {
    if (a.t == V_STR && b.t == V_STR) return scmp(a.u.s, b.u.s) < 0;
    num x = to_num(a), y = to_num(b);
    if (m_isnan(x) || m_isnan(y)) return -1;
    return x < y;
}

static const char *type_of(val v) {
    switch (v.t) {
    case V_UNDEF: return "undefined";
    case V_NULL: return "null";
    case V_BOOL: return "boolean";
    case V_NUM: return "number";
    case V_STR: return "string";
    case V_OBJ:
        if (v.u.o->kind == O_CLIP) return v.u.o->clip && v.u.o->clip->kind == K_ETEXT ? "object" : "movieclip";
        if (v.u.o->kind == O_FUNC) return "function";
        return "object";
    }
    return "undefined";
}

static int instance_of(val v, val ctor) {
    if (v.t != V_OBJ || ctor.t != V_OBJ) return 0;
    prop *pp = own_prop(ctor.u.o, "prototype");
    if (!pp || pp->v.t != V_OBJ) return 0;
    for (obj *p = v.u.o->proto; p; p = p->proto) if (p == pp->v.u.o) return 1;
    return 0;
}

static void do_goto(inst *t, val f, int play, int bias) {
    if (!t) return;
    if (f.t == V_STR) {
        const char *s = f.u.s;
        const char *colon = 0;
        for (const char *p = s; *p; p++) if (*p == ':') colon = p;
        if (colon) { t = path_to_clip(cur, astrndup(s, (u32)(colon - s))); s = colon + 1; if (!t) return; }
        int fr = inst_label(t, s);
        if (!fr) { num n = str_to_num(s, 1); if (m_isnan(n)) return; fr = (int)n; }
        inst_goto(t, fr + bias, play);
    } else {
        inst_goto(t, (int)to_num(f) + bias, play);
    }
}

static obj *make_function(ctx *c, const u8 *code, u32 len, int fn2, u8 nregs, u16 flags, int np,
                          const char **pnames, u8 *pregs) {
    obj *f = new_fn(NULL);
    if (!f) return NULL;
    f->code = code;
    f->clen = len;
    f->fn2 = (u8)fn2;
    f->nregs = nregs;
    f->fflags = flags;
    f->np = np;
    f->pname = pnames;
    f->preg = pregs;
    f->deftarget = c->target;
    f->cpool = c->cpool;
    f->ncpool = c->ncpool;
    /* closure scope = outer scopes + current activation */
    int ns = c->nscope + (c->act ? 1 : 0);
    f->scope = amalloc(sizeof(obj *) * (ns + 1));
    for (int i = 0; i < c->nscope; i++) f->scope[i] = c->scope[i];
    if (c->act) { f->scope[c->nscope] = c->act; c->act->captured = 1; }
    f->nscope = ns;
    obj *pr = new_obj(obj_proto);
    pr->captured = 1;
    set_hidden(pr, "constructor", vobj(f));
    set_hidden(f, "prototype", vobj(pr));
    return f;
}

static val call_method(ctx *c, val o, val name, val *args, int n) {
    (void)c;
    if (name.t == V_UNDEF || (name.t == V_STR && !name.u.s[0])) {
        if (o.t == V_OBJ && o.u.o->kind == O_SUPER) {
            obj *s = o.u.o;
            return s->sup_ctor ? call_fn(s->sup_ctor, s->sup_this, args, n, s->sup_proto) : vundef();
        }
        if (o.t == V_OBJ && o.u.o->kind == O_FUNC) return call_fn(o.u.o, vundef(), args, n, NULL);
        return vundef();
    }
    const char *nm = name.t == V_STR ? name.u.s : as_tostring(name);
    if (o.t == V_OBJ && o.u.o->kind == O_SUPER) {
        obj *s = o.u.o, *where = NULL;
        prop *p = find_prop(s->sup_proto, nm, &where);
        if (p && p->v.t == V_OBJ) return call_fn(p->v.u.o, s->sup_this, args, n, where);
        return vundef();
    }
    obj *where = NULL;
    val f;
    if (o.t == V_OBJ && o.u.o->kind != O_CLIP && o.u.o->kind != O_ARRAY) {
        prop *p = find_prop(o.u.o, nm, &where);
        f = p ? (p->getter ? call_fn(p->getter, o, NULL, 0, NULL) : p->v) : vundef();
    } else {
        f = get_member(o, vstr(nm));
        if (o.t == V_OBJ) find_prop(o.u.o, nm, &where);
    }
    if (f.t == V_OBJ && f.u.o->kind == O_FUNC) return call_fn(f.u.o, o, args, n, where);
    return vundef();
}

static val run_code(ctx *c, const u8 *code, u32 len) {
    u32 pc = 0;
    int base_sp = sp;
    struct { u32 start, end, catch_start, catch_end, fin_end; int reg; const char *name; int in_catch; } tries[8];
    int ntry = 0;
    int guard = 0;
    while (pc < len) {
        if (++guard > 2000000) break; /* runaway loop protection */
        while (c->nwith && pc >= c->with_end[c->nwith - 1]) c->nwith--;
        u8 op = code[pc++];
        u32 alen = 0;
        const u8 *a = code + pc;
        if (op >= 0x80) {
            if (pc + 2 > len) break;
            alen = code[pc] | (code[pc + 1] << 8);
            pc += 2;
            a = code + pc;
            if (alen > len - pc) alen = len - pc;
            pc += alen;
        }
        val x, y, z;
        switch (op) {
        case 0x00: goto done;
        case 0x04: inst_goto(c->target, c->target ? c->target->frame + 1 : 1, 0); break;
        case 0x05: inst_goto(c->target, c->target ? c->target->frame - 1 : 1, 0); break;
        case 0x06: if (c->target) c->target->playing = 1; break;
        case 0x07: if (c->target) c->target->playing = 0; break;
        case 0x08: case 0x09: break;
        case 0x0A: y = pop(); x = pop(); push(vnum(to_num(x) + to_num(y))); break;
        case 0x0B: y = pop(); x = pop(); push(vnum(to_num(x) - to_num(y))); break;
        case 0x0C: y = pop(); x = pop(); push(vnum(to_num(x) * to_num(y))); break;
        case 0x0D: {
            y = pop(); x = pop();
            num d = to_num(y);
            if (d == 0 && swf_ver < 5) push(vstr("#ERROR#"));
            else push(vnum(to_num(x) / d));
            break;
        }
        case 0x0E: y = pop(); x = pop(); push(swf_ver >= 5 ? vbool(to_num(x) == to_num(y)) : vnum(to_num(x) == to_num(y))); break;
        case 0x0F: y = pop(); x = pop(); push(swf_ver >= 5 ? vbool(to_num(x) < to_num(y)) : vnum(to_num(x) < to_num(y))); break;
        case 0x10: y = pop(); x = pop(); push(swf_ver >= 5 ? vbool(to_bool(x) && to_bool(y)) : vnum(to_bool(x) && to_bool(y))); break;
        case 0x11: y = pop(); x = pop(); push(swf_ver >= 5 ? vbool(to_bool(x) || to_bool(y)) : vnum(to_bool(x) || to_bool(y))); break;
        case 0x12: x = pop(); push(swf_ver >= 5 ? vbool(!to_bool(x)) : vnum(!to_bool(x))); break;
        case 0x13: y = pop(); x = pop(); push(vbool(!scmp(as_tostring(x), as_tostring(y)))); break;
        case 0x14: case 0x31: x = pop(); push(vnum(kstrlen(as_tostring(x)))); break;
        case 0x15: case 0x35: {
            int cnt = to_i32(pop()), idx = to_i32(pop());
            const char *s = as_tostring(pop());
            int l = (int)kstrlen(s);
            idx--;
            if (idx < 0) idx = 0;
            if (idx > l) idx = l;
            if (cnt < 0 || idx + cnt > l) cnt = l - idx;
            push(vstr(astrndup(s + idx, (u32)cnt)));
            break;
        }
        case 0x17: pop(); break;
        case 0x18: { num n = to_num(pop()); push(vnum(m_isnan(n) ? 0 : (n < 0 ? -m_floor(-n) : m_floor(n)))); break; }
        case 0x1C: x = pop(); push(get_var(c, as_tostring(x))); break;
        case 0x1D: y = pop(); x = pop(); set_var(c, as_tostring(x), y); break;
        case 0x20: { x = pop(); inst *t = x.t == V_STR && !x.u.s[0] ? c->orig : target_of(c, x); c->target = t ? t : c->orig; break; }
        case 0x21: y = pop(); x = pop(); push(vstr(str_cat(as_tostring(x), as_tostring(y)))); break;
        case 0x22: {
            int idx = to_i32(pop());
            inst *t = target_of(c, pop());
            push(t ? clip_getp(t, idx) : vundef());
            break;
        }
        case 0x23: {
            z = pop();
            int idx = to_i32(pop());
            inst *t = target_of(c, pop());
            if (t) clip_setp(t, idx, z);
            break;
        }
        case 0x24: {
            int dp = to_i32(pop());
            const char *nm = as_tostring(pop());
            inst *src = target_of(c, pop());
            if (src) inst_duplicate(src, astrdup(nm), dp);
            break;
        }
        case 0x25: { inst *t = target_of(c, pop()); if (t && t != root) inst_remove(t); break; }
        case 0x26: pop(); break; /* trace */
        case 0x27: {
            inst *t = target_of(c, pop());
            int lock = to_bool(pop()), con = to_bool(pop());
            float r[4] = {0, 0, 0, 0};
            if (con) { r[3] = (float)to_num(pop()); r[2] = (float)to_num(pop()); r[1] = (float)to_num(pop()); r[0] = (float)to_num(pop()); }
            if (t) start_drag(t, lock, con, r[0], r[1], r[2], r[3]);
            break;
        }
        case 0x28: stop_drag(); break;
        case 0x29: y = pop(); x = pop(); push(vbool(scmp(as_tostring(x), as_tostring(y)) < 0)); break;
        case 0x2A: thrown = pop(); throwing = 1; break;
        case 0x2B: y = pop(); x = pop(); push(instance_of(y, x) ? y : vobj(NULL)); break;
        case 0x2C: { pop(); int n = to_i32(pop()); while (n-- > 0) pop(); break; }
        case 0x30: { int mx = to_i32(pop()); push(vnum(rand_int(mx))); break; }
        case 0x32: case 0x36: { const char *s = as_tostring(pop()); push(vnum((u8)s[0])); break; }
        case 0x33: case 0x37: { char b[2] = {(char)to_i32(pop()), 0}; push(vstr(astrdup(b))); break; }
        case 0x34: push(vnum(plat_ms())); break;
        case 0x3A: {
            y = pop(); x = pop();
            if (x.t == V_OBJ) {
                obj *o = x.u.o;
                const char *nm = as_tostring(y);
                for (prop **pp = &o->props; *pp; pp = &(*pp)->next)
                    if (name_eq((*pp)->name, nm)) { *pp = (*pp)->next; break; }
            }
            push(vbool(1));
            break;
        }
        case 0x3B: {
            const char *nm = as_tostring(pop());
            obj *o = c->act;
            if (o) for (prop **pp = &o->props; *pp; pp = &(*pp)->next) if (name_eq((*pp)->name, nm)) { *pp = (*pp)->next; break; }
            push(vbool(1));
            break;
        }
        case 0x3C: y = pop(); x = pop(); if (c->act) set_own(c->act, as_tostring(x), y); else set_var(c, as_tostring(x), y); break;
        case 0x41: x = pop(); if (c->act) { if (!own_prop(c->act, as_tostring(x))) set_own(c->act, as_tostring(x), vundef()); } break;
        case 0x3D: {
            const char *nm = as_tostring(pop());
            int n = to_i32(pop());
            if (n < 0) n = 0;
            if (n > 64) n = 64;
            val args[64];
            for (int i = 0; i < n; i++) args[i] = pop();
            val f = get_var(c, nm);
            val r = vundef();
            if (f.t == V_OBJ && f.u.o->kind == O_FUNC) r = call_fn(f.u.o, vundef(), args, n, NULL);
            push(r);
            break;
        }
        case 0x3E: c->ret = pop(); c->ret_set = 1; goto done;
        case 0x3F: y = pop(); x = pop(); push(vnum(m_fmod(to_num(x), to_num(y)))); break;
        case 0x40: case 0x53: {
            val nmv = pop();
            val o = op == 0x53 ? pop() : vundef();
            int n = to_i32(pop());
            if (n < 0) n = 0;
            if (n > 64) n = 64;
            val args[64];
            for (int i = 0; i < n; i++) args[i] = pop();
            val ctor;
            if (op == 0x40) ctor = get_var(c, as_tostring(nmv));
            else if (nmv.t == V_UNDEF || (nmv.t == V_STR && !nmv.u.s[0])) ctor = o;
            else ctor = get_member(o, nmv);
            push(ctor.t == V_OBJ ? construct(ctor.u.o, args, n) : vundef());
            break;
        }
        case 0x42: {
            int n = to_i32(pop());
            obj *ar = new_array();
            for (int i = 0; i < n && i < 100000; i++) arr_set(ar, i, pop());
            push(vobj(ar));
            break;
        }
        case 0x43: {
            int n = to_i32(pop());
            obj *o = new_obj(obj_proto);
            for (int i = 0; i < n && i < 100000; i++) { y = pop(); x = pop(); set_member(vobj(o), vstr(astrdup(as_tostring(x))), y); }
            push(vobj(o));
            break;
        }
        case 0x44: push(vstr(type_of(pop()))); break;
        case 0x45: { inst *t = val_clip(pop()); push(t ? vstr(clip_path(t, swf_ver < 5)) : vundef()); break; }
        case 0x46: case 0x55: {
            x = pop();
            val o = op == 0x46 ? get_var(c, as_tostring(x)) : x;
            push(vobj(NULL));
            if (o.t == V_OBJ) {
                obj *ob = o.u.o;
                if (ob->kind == O_ARRAY) for (int i = ob->alen - 1; i >= 0; i--) push(vstr(num_to_str(i)));
                if (ob->kind == O_CLIP && ob->clip)
                    for (int i = 0; i < ob->clip->n; i++) if (ob->clip->kids[i]->name) push(vstr(ob->clip->kids[i]->name));
                for (obj *p = ob; p; p = p->proto)
                    for (prop *pr = p->props; pr; pr = pr->next) if (!pr->dontenum) push(vstr(pr->name));
            }
            break;
        }
        case 0x47: y = pop(); x = pop(); push(add2(x, y)); break;
        case 0x48: { y = pop(); x = pop(); int r = less2(x, y); push(r < 0 ? vundef() : vbool(r)); break; }
        case 0x49: y = pop(); x = pop(); push(vbool(equals2(x, y))); break;
        case 0x4A: push(vnum(to_num(pop()))); break;
        case 0x4B: push(vstr(as_tostring(pop()))); break;
        case 0x4C: x = pop(); push(x); push(x); break;
        case 0x4D: y = pop(); x = pop(); push(y); push(x); break;
        case 0x4E: y = pop(); x = pop(); push(get_member(x, y)); break;
        case 0x4F: z = pop(); y = pop(); x = pop(); set_member(x, y, z); break;
        case 0x50: push(vnum(to_num(pop()) + 1)); break;
        case 0x51: push(vnum(to_num(pop()) - 1)); break;
        case 0x52: {
            val nmv = pop();
            val o = pop();
            int n = to_i32(pop());
            if (n < 0) n = 0;
            if (n > 64) n = 64;
            val args[64];
            for (int i = 0; i < n; i++) args[i] = pop();
            push(call_method(c, o, nmv, args, n));
            break;
        }
        case 0x54: y = pop(); x = pop(); push(vbool(instance_of(x, y))); break;
        case 0x60: y = pop(); x = pop(); push(vnum(to_i32(x) & to_i32(y))); break;
        case 0x61: y = pop(); x = pop(); push(vnum(to_i32(x) | to_i32(y))); break;
        case 0x62: y = pop(); x = pop(); push(vnum(to_i32(x) ^ to_i32(y))); break;
        case 0x63: y = pop(); x = pop(); push(vnum((s32)((u32)to_i32(x) << (to_i32(y) & 31)))); break;
        case 0x64: y = pop(); x = pop(); push(vnum(to_i32(x) >> (to_i32(y) & 31))); break;
        case 0x65: y = pop(); x = pop(); push(vnum((num)((u32)to_i32(x) >> (to_i32(y) & 31)))); break;
        case 0x66: y = pop(); x = pop(); push(vbool(strict_eq(x, y))); break;
        case 0x67: { y = pop(); x = pop(); int r = less2(y, x); push(r < 0 ? vundef() : vbool(r)); break; }
        case 0x68: y = pop(); x = pop(); push(vbool(scmp(as_tostring(x), as_tostring(y)) > 0)); break;
        case 0x69: {
            val sup = pop(), sub = pop();
            if (sup.t == V_OBJ && sub.t == V_OBJ) {
                prop *sp2 = own_prop(sup.u.o, "prototype");
                obj *pr = new_obj(sp2 && sp2->v.t == V_OBJ ? sp2->v.u.o : obj_proto);
                pr->captured = 1;
                set_hidden(pr, "__constructor__", sup);
                set_hidden(pr, "constructor", sub);
                set_hidden(sub.u.o, "prototype", vobj(pr));
            }
            break;
        }
        /* ---- actions with payload ---- */
        case 0x81: if (alen >= 2) inst_goto(c->target, (a[0] | (a[1] << 8)) + 1, 0); break;
        case 0x83: {
            const char *url = (const char *)a;
            if (!kstrnicmp(url, "FSCommand:quit", 14)) quit_requested = 1;
            break;
        }
        case 0x87: if (alen >= 1 && a[0] < c->nregs) c->regs[a[0]] = sp > 0 ? stk[sp - 1] : vundef(); break;
        case 0x88: {
            rd r;
            rinit(&r, a, alen);
            int n = (int)r16(&r);
            val *pool = amalloc(sizeof(val) * (n + 1));
            for (int i = 0; i < n; i++) pool[i] = vstr(rstr(&r));
            c->cpool = pool;
            c->ncpool = n;
            break;
        }
        case 0x8A: break; /* WaitForFrame: everything is loaded */
        case 0x8D: pop(); break;
        case 0x8B: {
            const char *t = (const char *)a;
            inst *tt = *t ? path_to_clip(c, t) : c->orig;
            c->target = tt ? tt : c->orig;
            break;
        }
        case 0x8C: if (c->target) { int f = inst_label(c->target, (const char *)a); if (f) inst_goto(c->target, f, 0); } break;
        case 0x8E: case 0x9B: {
            rd r;
            rinit(&r, a, alen);
            const char *fname = rstr(&r);
            int np = (int)r16(&r);
            u8 nregs = 0;
            u16 flags = 0;
            if (op == 0x8E) { nregs = (u8)r8(&r); flags = (u16)r16(&r); }
            const char **pn = amalloc(sizeof(char *) * (np + 1));
            u8 *pr = amalloc((u32)np + 1);
            for (int i = 0; i < np; i++) {
                if (op == 0x8E) pr[i] = (u8)r8(&r);
                pn[i] = rstr(&r);
            }
            u32 csize = r16(&r);
            if (csize > len - pc) csize = len - pc;
            obj *f = make_function(c, code + pc, csize, op == 0x8E, nregs, flags, np, pn, pr);
            pc += csize;
            if (*fname) {
                if (c->act) set_own(c->act, fname, vobj(f));
                else set_var(c, fname, vobj(f));
            } else {
                push(vobj(f));
            }
            break;
        }
        case 0x8F: {
            rd r;
            rinit(&r, a, alen);
            u32 fl = r8(&r);
            u32 ts = r16(&r), cs = r16(&r), fs = r16(&r);
            if (ntry < 8) {
                tries[ntry].start = pc;
                tries[ntry].end = pc + ts;
                tries[ntry].catch_start = pc + ts;
                tries[ntry].catch_end = pc + ts + cs;
                tries[ntry].fin_end = pc + ts + cs + fs;
                tries[ntry].reg = (fl & 4) ? (int)r8(&r) : -1;
                tries[ntry].name = (fl & 4) ? NULL : rstr(&r);
                tries[ntry].in_catch = (fl & 1) ? 0 : -1;
                ntry++;
            }
            break;
        }
        case 0x94: {
            x = pop();
            u32 sz = alen >= 2 ? (u32)(a[0] | (a[1] << 8)) : 0;
            if (x.t == V_OBJ && c->nwith < 16) { c->with[c->nwith] = x.u.o; c->with_end[c->nwith] = pc + sz; c->nwith++; }
            break;
        }
        case 0x96: {
            rd r;
            rinit(&r, a, alen);
            while (r.i < r.n) {
                u32 t = r8(&r);
                switch (t) {
                case 0: push(vstr(rstr(&r))); break;
                case 1: { union { u32 u; float f; } cv; cv.u = r32(&r); push(vnum(cv.f)); break; }
                case 2: push(vobj(NULL)); break;
                case 3: push(vundef()); break;
                case 4: { u32 i = r8(&r); push(i < (u32)c->nregs ? c->regs[i] : vundef()); break; }
                case 5: push(vbool(r8(&r))); break;
                case 6: {
                    union { u32 w[2]; double d; } cv;
                    cv.w[1] = r32(&r);
                    cv.w[0] = r32(&r);
                    push(vnum(cv.d));
                    break;
                }
                case 7: push(vnum((s32)r32(&r))); break;
                case 8: { u32 i = r8(&r); push(i < (u32)c->ncpool ? c->cpool[i] : vundef()); break; }
                case 9: { u32 i = r16(&r); push(i < (u32)c->ncpool ? c->cpool[i] : vundef()); break; }
                default: r.i = r.n; break;
                }
            }
            break;
        }
        case 0x99: if (alen >= 2) pc += (s16)(a[0] | (a[1] << 8)); break;
        case 0x9A: {
            y = pop(); x = pop();
            if (!kstrnicmp(as_tostring(x), "FSCommand:quit", 14)) quit_requested = 1;
            break;
        }
        case 0x9D: x = pop(); if (to_bool(x) && alen >= 2) pc += (s16)(a[0] | (a[1] << 8)); break;
        case 0x9E: pop(); break;
        case 0x9F: {
            int play = alen >= 1 && (a[0] & 1);
            int bias = alen >= 3 && (a[0] & 2) ? (a[1] | (a[2] << 8)) : 0;
            x = pop();
            do_goto(c->target, x, play, bias);
            break;
        }
        default: break;
        }
        if (throwing) {
            int handled = 0;
            for (int i = ntry - 1; i >= 0; i--) {
                if (tries[i].in_catch == 0 && pc >= tries[i].start && pc <= tries[i].end) {
                    throwing = 0;
                    tries[i].in_catch = 1;
                    if (tries[i].reg >= 0 && tries[i].reg < c->nregs) c->regs[tries[i].reg] = thrown;
                    else if (tries[i].name) { if (c->act) set_own(c->act, tries[i].name, thrown); else set_var(c, tries[i].name, thrown); }
                    pc = tries[i].catch_start;
                    sp = base_sp;
                    handled = 1;
                    break;
                }
            }
            if (!handled) break;
        }
        if (pc > len) break;
    }
done:
    if (sp < base_sp) sp = base_sp;
    if (!c->act) sp = base_sp; /* timeline code leaves nothing behind */
    else if (sp > base_sp) sp = base_sp;
    return c->ret_set ? c->ret : vundef();
}

/* ================================================================ natives */
#define ARG(i) ((i) < n ? a[i] : vundef())
#define THIS_CLIP inst *cl = val_clip(th); if (!cl) return vundef();

static val nf_trace(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; return vundef(); }
static val nf_getTimer(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; return vnum(plat_ms()); }
static val nf_parseInt(ctx *c, val th, val *a, int n) {
    (void)c; (void)th;
    const char *s = as_tostring(ARG(0));
    int radix = n > 1 ? to_i32(a[1]) : 10;
    while (*s == ' ') s++;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; } else if (*s == '+') s++;
    if ((radix == 16 || n < 2) && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) { s += 2; radix = 16; }
    if (radix < 2 || radix > 36) return vnum(m_nan());
    num v = 0;
    int any = 0;
    for (;; s++) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
        else break;
        if (d >= radix) break;
        v = v * radix + d;
        any = 1;
    }
    return any ? vnum(neg ? -v : v) : vnum(m_nan());
}
static val nf_parseFloat(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vnum(str_to_num(as_tostring(ARG(0)), 0)); }
static val nf_isNaN(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vbool(m_isnan(to_num(ARG(0)))); }
static val nf_isFinite(ctx *c, val th, val *a, int n) { (void)c; (void)th; num x = to_num(ARG(0)); return vbool(!m_isnan(x) && !m_isinf(x)); }
static val nf_Number(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vnum(n ? to_num(a[0]) : 0); }
static val nf_String(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vstr(n ? as_tostring(a[0]) : ""); }
static val nf_Boolean(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vbool(n ? to_bool(a[0]) : 0); }
static val nf_escape(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vstr(as_tostring(ARG(0))); }

static val nf_setInterval(ctx *c, val th, val *a, int n) {
    (void)th;
    if (n < 2) return vundef();
    if (niv == ivcap) {
        int nc = ivcap ? ivcap * 2 : 16;
        interval *ni = amalloc(sizeof(interval) * nc);
        if (!ni) return vundef();
        if (niv) memcpy(ni, ivs, sizeof(interval) * niv);
        ivs = ni;
        ivcap = nc;
    }
    interval *t = &ivs[niv++];
    memset(t, 0, sizeof *t);
    int k;
    if (a[0].t == V_OBJ && a[0].u.o->kind == O_FUNC) { t->fn = a[0]; t->thisv = c ? c->thisv : vundef(); k = 1; }
    else { t->thisv = a[0]; t->method = astrdup(as_tostring(a[1])); k = 2; }
    num ms = to_num(ARG(k));
    t->ms = m_isnan(ms) || ms < 1 ? 1 : (u32)(int)ms;
    t->next = plat_ms() + t->ms;
    for (int i = k + 1; i < n && t->nargs < 4; i++) t->args[t->nargs++] = a[i];
    t->id = ++ivid;
    t->active = 1;
    return vnum(t->id);
}
static val nf_setTimeout(ctx *c, val th, val *a, int n) {
    val r = nf_setInterval(c, th, a, n);
    if (niv) ivs[niv - 1].once = 1;
    return r;
}
static val nf_clearInterval(ctx *c, val th, val *a, int n) {
    (void)c; (void)th;
    int id = to_i32(ARG(0));
    for (int i = 0; i < niv; i++) if (ivs[i].id == id) ivs[i].active = 0;
    return vundef();
}

/* Math */
static val nf_abs(ctx *c, val th, val *a, int n) { (void)c; (void)th; num x = to_num(ARG(0)); return vnum(x < 0 ? -x : x); }
static val nf_floor(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vnum(m_floor(to_num(ARG(0)))); }
static val nf_ceil(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vnum(-m_floor(-to_num(ARG(0)))); }
static val nf_round(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vnum(m_floor(to_num(ARG(0)) + 0.5)); }
static val nf_sqrt(ctx *c, val th, val *a, int n) { (void)c; (void)th; num x = to_num(ARG(0)); return vnum(x < 0 ? m_nan() : m_sqrt(x)); }
static val nf_sin(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vnum(m_sin(to_num(ARG(0)))); }
static val nf_cos(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vnum(m_cos(to_num(ARG(0)))); }
static val nf_tan(ctx *c, val th, val *a, int n) { (void)c; (void)th; num x = to_num(ARG(0)); return vnum(m_sin(x) / m_cos(x)); }
static val nf_atan2(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vnum(m_atan2(to_num(ARG(0)), to_num(ARG(1)))); }
static val nf_atan(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vnum(m_atan2(to_num(ARG(0)), 1)); }
static val nf_asin(ctx *c, val th, val *a, int n) { (void)c; (void)th; num x = to_num(ARG(0)); return vnum(m_atan2(x, m_sqrt(1 - x * x))); }
static val nf_acos(ctx *c, val th, val *a, int n) { (void)c; (void)th; num x = to_num(ARG(0)); return vnum(m_atan2(m_sqrt(1 - x * x), x)); }
static val nf_exp(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vnum(m_exp(to_num(ARG(0)))); }
static val nf_log(ctx *c, val th, val *a, int n) { (void)c; (void)th; num x = to_num(ARG(0)); return vnum(x <= 0 ? m_nan() : m_log(x)); }
static val nf_pow(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vnum(m_pow(to_num(ARG(0)), to_num(ARG(1)))); }
static val nf_random(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; rand_int(2); return vnum((rnd_state >> 1) / 2147483648.0); }
static val nf_min(ctx *c, val th, val *a, int n) {
    (void)c; (void)th;
    if (!n) return vnum(m_inf());
    num r = to_num(a[0]);
    for (int i = 1; i < n; i++) { num x = to_num(a[i]); if (x < r) r = x; }
    return vnum(r);
}
static val nf_max(ctx *c, val th, val *a, int n) {
    (void)c; (void)th;
    if (!n) return vnum(-m_inf());
    num r = to_num(a[0]);
    for (int i = 1; i < n; i++) { num x = to_num(a[i]); if (x > r) r = x; }
    return vnum(r);
}

/* Key / Mouse */
static val nf_isDown(ctx *c, val th, val *a, int n) { (void)c; (void)th; return vbool(plat_kdown(to_i32(ARG(0)))); }
static val nf_getCode(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; return vnum(last_key_code); }
static val nf_getAscii(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; return vnum(last_key_ascii); }
static val nf_isToggled(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; return vbool(0); }
static val nf_addListener(ctx *c, val th, val *a, int n) {
    (void)c;
    if (th.t != V_OBJ || !n) return vbool(0);
    prop *p = own_prop(th.u.o, "_listeners");
    obj *arr;
    if (!p || p->v.t != V_OBJ) { arr = new_array(); set_hidden(th.u.o, "_listeners", vobj(arr)); }
    else arr = p->v.u.o;
    for (int i = 0; i < arr->alen; i++) if (equals2(arr->arr[i], a[0])) return vbool(1);
    arr_set(arr, arr->alen, a[0]);
    return vbool(1);
}
static val nf_removeListener(ctx *c, val th, val *a, int n) {
    (void)c;
    if (th.t != V_OBJ || !n) return vbool(0);
    prop *p = own_prop(th.u.o, "_listeners");
    if (!p || p->v.t != V_OBJ) return vbool(0);
    obj *arr = p->v.u.o;
    for (int i = 0; i < arr->alen; i++)
        if (equals2(arr->arr[i], a[0])) {
            memmove(&arr->arr[i], &arr->arr[i + 1], sizeof(val) * (arr->alen - i - 1));
            arr->alen--;
            return vbool(1);
        }
    return vbool(0);
}
static val nf_mhide(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; mouse_visible = 0; return vundef(); }
static val nf_mshow(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; mouse_visible = 1; return vundef(); }

/* Object */
static val nf_Object(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return th.t == V_OBJ ? th : vobj(new_obj(obj_proto)); }
static val nf_hasOwn(ctx *c, val th, val *a, int n) {
    (void)c;
    if (th.t != V_OBJ) return vbool(0);
    return vbool(own_prop(th.u.o, as_tostring(ARG(0))) != NULL);
}
static val nf_objToString(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; return vstr("[object Object]"); }
static val nf_valueOf(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return th; }
static val nf_addProperty(ctx *c, val th, val *a, int n) {
    (void)c;
    if (th.t != V_OBJ || n < 2) return vbool(0);
    const char *nm = astrdup(as_tostring(a[0]));
    set_own(th.u.o, nm, vundef());
    prop *p = own_prop(th.u.o, nm);
    if (a[1].t == V_OBJ) p->getter = a[1].u.o;
    if (n > 2 && a[2].t == V_OBJ) p->setter = a[2].u.o;
    return vbool(1);
}
static val nf_registerClass(ctx *c, val th, val *a, int n) {
    (void)c; (void)th;
    const char *nm = as_tostring(ARG(0));
    obj *k = n > 1 && a[1].t == V_OBJ ? a[1].u.o : NULL;
    for (int i = 0; i < 65536; i++)
        if (chars[i].export_name && !kstricmp(chars[i].export_name, nm)) { chars[i].klass = k; return vbool(1); }
    return vbool(0);
}
static val nf_noop(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; return vundef(); }
static val nf_true(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; return vbool(1); }

/* Function.call / apply */
static val nf_call(ctx *c, val th, val *a, int n) {
    (void)c;
    if (th.t != V_OBJ) return vundef();
    return call_fn(th.u.o, ARG(0), n > 1 ? a + 1 : NULL, n > 1 ? n - 1 : 0, NULL);
}
static val nf_apply(ctx *c, val th, val *a, int n) {
    (void)c;
    if (th.t != V_OBJ) return vundef();
    val args[64];
    int na = 0;
    if (n > 1 && a[1].t == V_OBJ && a[1].u.o->kind == O_ARRAY)
        for (int i = 0; i < a[1].u.o->alen && na < 64; i++) args[na++] = a[1].u.o->arr[i];
    return call_fn(th.u.o, ARG(0), args, na, NULL);
}

/* Array */
static obj *this_arr(val th) { return th.t == V_OBJ && th.u.o->kind == O_ARRAY ? th.u.o : NULL; }
static val nf_Array(ctx *c, val th, val *a, int n) {
    (void)c;
    obj *ar = th.t == V_OBJ && th.u.o->kind == O_OBJ ? th.u.o : new_array();
    ar->kind = O_ARRAY;
    if (n == 1 && a[0].t == V_NUM) { int k = to_i32(a[0]); if (k > 0) arr_set(ar, k - 1, vundef()); }
    else for (int i = 0; i < n; i++) arr_set(ar, i, a[i]);
    return vobj(ar);
}
static val nf_push(ctx *c, val th, val *a, int n) {
    (void)c;
    obj *ar = this_arr(th);
    if (!ar) return vundef();
    for (int i = 0; i < n; i++) arr_set(ar, ar->alen, a[i]);
    return vnum(ar->alen);
}
static val nf_pop(ctx *c, val th, val *a, int n) {
    (void)c; (void)a; (void)n;
    obj *ar = this_arr(th);
    if (!ar || !ar->alen) return vundef();
    return ar->arr[--ar->alen];
}
static val nf_shift(ctx *c, val th, val *a, int n) {
    (void)c; (void)a; (void)n;
    obj *ar = this_arr(th);
    if (!ar || !ar->alen) return vundef();
    val v = ar->arr[0];
    memmove(ar->arr, ar->arr + 1, sizeof(val) * (ar->alen - 1));
    ar->alen--;
    return v;
}
static val nf_unshift(ctx *c, val th, val *a, int n) {
    (void)c;
    obj *ar = this_arr(th);
    if (!ar) return vundef();
    for (int i = 0; i < n; i++) arr_set(ar, ar->alen, vundef());
    memmove(ar->arr + n, ar->arr, sizeof(val) * (ar->alen - n));
    for (int i = 0; i < n; i++) ar->arr[i] = a[i];
    return vnum(ar->alen);
}
static const char *join(obj *ar, const char *sep) {
    const char *s = "";
    for (int i = 0; i < ar->alen; i++) {
        if (i) s = str_cat(s, sep);
        if (ar->arr[i].t != V_UNDEF && ar->arr[i].t != V_NULL) s = str_cat(s, as_tostring(ar->arr[i]));
        else if (swf_ver >= 7 && ar->arr[i].t == V_UNDEF) s = str_cat(s, "undefined");
    }
    return s;
}
static val nf_join(ctx *c, val th, val *a, int n) {
    (void)c;
    obj *ar = this_arr(th);
    if (!ar) return vstr("");
    return vstr(join(ar, n && a[0].t != V_UNDEF ? as_tostring(a[0]) : ","));
}
static val nf_arrToString(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; obj *ar = this_arr(th); return vstr(ar ? join(ar, ",") : ""); }
static val nf_slice(ctx *c, val th, val *a, int n) {
    (void)c;
    obj *ar = this_arr(th);
    obj *r = new_array();
    if (!ar) return vobj(r);
    int s = n > 0 ? to_i32(a[0]) : 0, e = n > 1 && a[1].t != V_UNDEF ? to_i32(a[1]) : ar->alen;
    if (s < 0) s += ar->alen;
    if (e < 0) e += ar->alen;
    if (s < 0) s = 0;
    if (e > ar->alen) e = ar->alen;
    for (int i = s; i < e; i++) arr_set(r, r->alen, ar->arr[i]);
    return vobj(r);
}
static val nf_splice(ctx *c, val th, val *a, int n) {
    (void)c;
    obj *ar = this_arr(th);
    obj *r = new_array();
    if (!ar || !n) return vobj(r);
    int s = to_i32(a[0]);
    if (s < 0) s += ar->alen;
    if (s < 0) s = 0;
    if (s > ar->alen) s = ar->alen;
    int dc = n > 1 ? to_i32(a[1]) : ar->alen - s;
    if (dc < 0) dc = 0;
    if (s + dc > ar->alen) dc = ar->alen - s;
    for (int i = 0; i < dc; i++) arr_set(r, i, ar->arr[s + i]);
    int ins = n > 2 ? n - 2 : 0;
    int nl = ar->alen - dc + ins;
    val *na = amalloc(sizeof(val) * (nl + 8));
    if (!na) return vobj(r);
    memcpy(na, ar->arr, sizeof(val) * s);
    for (int i = 0; i < ins; i++) na[s + i] = a[2 + i];
    memcpy(na + s + ins, ar->arr + s + dc, sizeof(val) * (ar->alen - s - dc));
    ar->arr = na;
    ar->alen = nl;
    ar->acap = nl + 8;
    return vobj(r);
}
static val nf_concat(ctx *c, val th, val *a, int n) {
    (void)c;
    obj *ar = this_arr(th), *r = new_array();
    if (ar) for (int i = 0; i < ar->alen; i++) arr_set(r, r->alen, ar->arr[i]);
    for (int k = 0; k < n; k++) {
        obj *o = a[k].t == V_OBJ && a[k].u.o->kind == O_ARRAY ? a[k].u.o : NULL;
        if (o) for (int i = 0; i < o->alen; i++) arr_set(r, r->alen, o->arr[i]);
        else arr_set(r, r->alen, a[k]);
    }
    return vobj(r);
}
static val nf_reverse(ctx *c, val th, val *a, int n) {
    (void)c; (void)a; (void)n;
    obj *ar = this_arr(th);
    if (!ar) return th;
    for (int i = 0, j = ar->alen - 1; i < j; i++, j--) { val t = ar->arr[i]; ar->arr[i] = ar->arr[j]; ar->arr[j] = t; }
    return th;
}
static obj *sort_fn;
static int sort_flags;
static int sort_cmp(val x, val y) {
    if (sort_fn) {
        val args[2] = {x, y};
        return to_i32(call_fn(sort_fn, vundef(), args, 2, NULL));
    }
    if (sort_flags & 16) { num p = to_num(x), q = to_num(y); return p < q ? -1 : p > q; }
    const char *p = as_tostring(x), *q = as_tostring(y);
    return (sort_flags & 1) ? kstricmp(p, q) : scmp(p, q);
}
static val nf_sort(ctx *c, val th, val *a, int n) {
    (void)c;
    obj *ar = this_arr(th);
    if (!ar) return th;
    sort_fn = NULL;
    sort_flags = 0;
    for (int i = 0; i < n; i++) {
        if (a[i].t == V_OBJ && a[i].u.o->kind == O_FUNC) sort_fn = a[i].u.o;
        else if (a[i].t == V_NUM) sort_flags = to_i32(a[i]);
    }
    for (int i = 1; i < ar->alen; i++) {
        val t = ar->arr[i];
        int j = i;
        while (j > 0) {
            int r = sort_cmp(ar->arr[j - 1], t);
            if (sort_flags & 2) r = -r;
            if (r <= 0) break;
            ar->arr[j] = ar->arr[j - 1];
            j--;
        }
        ar->arr[j] = t;
    }
    return th;
}

/* String */
static const char *this_str(val th) { return th.t == V_STR ? th.u.s : as_tostring(th); }
static val nf_charAt(ctx *c, val th, val *a, int n) {
    (void)c;
    const char *s = this_str(th);
    int i = to_i32(ARG(0));
    if (i < 0 || i >= (int)kstrlen(s)) return vstr("");
    return vstr(astrndup(s + i, 1));
}
static val nf_charCodeAt(ctx *c, val th, val *a, int n) {
    (void)c;
    const char *s = this_str(th);
    int i = to_i32(ARG(0));
    if (i < 0 || i >= (int)kstrlen(s)) return vnum(m_nan());
    return vnum((u8)s[i]);
}
static val nf_indexOf(ctx *c, val th, val *a, int n) {
    (void)c;
    if (th.t == V_OBJ && th.u.o->kind == O_ARRAY) {
        obj *ar = th.u.o;
        for (int i = 0; i < ar->alen; i++) if (strict_eq(ar->arr[i], ARG(0))) return vnum(i);
        return vnum(-1);
    }
    const char *s = this_str(th), *f = as_tostring(ARG(0));
    int st = n > 1 ? to_i32(a[1]) : 0, ls = (int)kstrlen(s), lf = (int)kstrlen(f);
    if (st < 0) st = 0;
    for (int i = st; i + lf <= ls; i++) if (!memcmp(s + i, f, (u32)lf)) return vnum(i);
    return vnum(-1);
}
static val nf_lastIndexOf(ctx *c, val th, val *a, int n) {
    (void)c;
    const char *s = this_str(th), *f = as_tostring(ARG(0));
    int ls = (int)kstrlen(s), lf = (int)kstrlen(f);
    for (int i = ls - lf; i >= 0; i--) if (!memcmp(s + i, f, (u32)lf)) return vnum(i);
    return vnum(-1);
}
static val nf_substr(ctx *c, val th, val *a, int n) {
    (void)c;
    const char *s = this_str(th);
    int l = (int)kstrlen(s), st = to_i32(ARG(0));
    if (st < 0) st += l;
    if (st < 0) st = 0;
    if (st > l) st = l;
    int cnt = n > 1 && a[1].t != V_UNDEF ? to_i32(a[1]) : l - st;
    if (cnt < 0) cnt = 0;
    if (st + cnt > l) cnt = l - st;
    return vstr(astrndup(s + st, (u32)cnt));
}
static val nf_substring(ctx *c, val th, val *a, int n) {
    (void)c;
    const char *s = this_str(th);
    int l = (int)kstrlen(s), st = to_i32(ARG(0)), e = n > 1 && a[1].t != V_UNDEF ? to_i32(a[1]) : l;
    if (st < 0) st = 0;
    if (e < 0) e = 0;
    if (st > l) st = l;
    if (e > l) e = l;
    if (st > e) { int t = st; st = e; e = t; }
    return vstr(astrndup(s + st, (u32)(e - st)));
}
static val nf_sslice(ctx *c, val th, val *a, int n) {
    if (th.t == V_OBJ && th.u.o->kind == O_ARRAY) return nf_slice(c, th, a, n);
    const char *s = this_str(th);
    int l = (int)kstrlen(s), st = to_i32(ARG(0)), e = n > 1 && a[1].t != V_UNDEF ? to_i32(a[1]) : l;
    if (st < 0) st += l;
    if (e < 0) e += l;
    if (st < 0) st = 0;
    if (e > l) e = l;
    if (e < st) e = st;
    return vstr(astrndup(s + st, (u32)(e - st)));
}
static val nf_upper(ctx *c, val th, val *a, int n) {
    (void)c; (void)a; (void)n;
    char *d = astrdup(this_str(th));
    for (char *p = d; *p; p++) *p = kupper(*p);
    return vstr(d);
}
static val nf_lower(ctx *c, val th, val *a, int n) {
    (void)c; (void)a; (void)n;
    char *d = astrdup(this_str(th));
    for (char *p = d; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
    return vstr(d);
}
static val nf_split(ctx *c, val th, val *a, int n) {
    (void)c;
    const char *s = this_str(th);
    obj *r = new_array();
    if (!n || a[0].t == V_UNDEF) { arr_set(r, 0, vstr(s)); return vobj(r); }
    const char *sep = as_tostring(a[0]);
    int ls = (int)kstrlen(sep);
    if (!ls) {
        for (int i = 0; s[i]; i++) arr_set(r, r->alen, vstr(astrndup(s + i, 1)));
        return vobj(r);
    }
    const char *p = s;
    for (;;) {
        const char *f = p;
        while (*f && memcmp(f, sep, (u32)ls)) f++;
        arr_set(r, r->alen, vstr(astrndup(p, (u32)(f - p))));
        if (!*f) break;
        p = f + ls;
    }
    return vobj(r);
}
static val nf_strconcat(ctx *c, val th, val *a, int n) {
    if (th.t == V_OBJ && th.u.o->kind == O_ARRAY) return nf_concat(c, th, a, n);
    const char *s = this_str(th);
    for (int i = 0; i < n; i++) s = str_cat(s, as_tostring(a[i]));
    return vstr(s);
}
static val nf_fromCharCode(ctx *c, val th, val *a, int n) {
    (void)c; (void)th;
    char *d = amalloc((u32)n + 1);
    for (int i = 0; i < n; i++) d[i] = (char)to_i32(a[i]);
    return vstr(d);
}
static val nf_toString(ctx *c, val th, val *a, int n) {
    (void)c;
    if (th.t == V_NUM && n && to_i32(a[0]) != 10) {
        int radix = to_i32(a[0]);
        if (radix < 2 || radix > 36) radix = 10;
        s32 v = to_i32(th);
        char t[40];
        int k = 0, neg = v < 0;
        u32 u = neg ? (u32)-v : (u32)v;
        do { int d = (int)(u % (u32)radix); t[k++] = (char)(d < 10 ? '0' + d : 'a' + d - 10); u /= (u32)radix; } while (u);
        char *o = amalloc((u32)k + 2), *p = o;
        if (neg) *p++ = '-';
        while (k) *p++ = t[--k];
        return vstr(o);
    }
    return vstr(as_tostring(th));
}

/* MovieClip */
static val nf_play(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; THIS_CLIP cl->playing = 1; return vundef(); }
static val nf_stop(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; THIS_CLIP cl->playing = 0; return vundef(); }
static val nf_gotoAndPlay(ctx *c, val th, val *a, int n) { (void)c; THIS_CLIP do_goto(cl, ARG(0), 1, 0); return vundef(); }
static val nf_gotoAndStop(ctx *c, val th, val *a, int n) { (void)c; THIS_CLIP do_goto(cl, ARG(0), 0, 0); return vundef(); }
static val nf_nextFrame(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; THIS_CLIP inst_goto(cl, cl->frame + 1, 0); return vundef(); }
static val nf_prevFrame(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; THIS_CLIP inst_goto(cl, cl->frame - 1, 0); return vundef(); }
static val nf_removeMC(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; THIS_CLIP if (cl != root) inst_remove(cl); return vundef(); }
static val nf_getDepth(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; THIS_CLIP return vnum(cl->depth); }
static val nf_nextDepth(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; THIS_CLIP return vnum(inst_next_depth(cl)); }
static val nf_atDepth(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    inst *k = inst_at_depth(cl, to_i32(ARG(0)));
    return k ? vobj(as_clip_obj(k)) : vundef();
}
static val nf_swapDepths(ctx *c, val th, val *a, int n) {
    THIS_CLIP
    val t = ARG(0);
    if (t.t == V_NUM) inst_swap_depth(cl, to_i32(t));
    else { inst *o = target_of(c, t); if (o && o->parent == cl->parent) inst_swap_depth(cl, o->depth); }
    return vundef();
}
static void copy_init(inst *k, val init) {
    if (init.t != V_OBJ) return;
    obj *o = as_clip_obj(k);
    for (prop *p = init.u.o->props; p; p = p->next) set_member(vobj(o), vstr(p->name), p->v);
}
static val nf_attachMovie(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    const char *link = as_tostring(ARG(0));
    for (int id = 0; id < 65536; id++) {
        if (!chars[id].export_name || kstricmp(chars[id].export_name, link)) continue;
        int dp = to_i32(ARG(2));
        inst *o = inst_at_depth(cl, dp);
        if (o) inst_remove(o);
        inst *k = inst_create(cl, (u16)id, dp, astrdup(as_tostring(ARG(1))), 1);
        if (!k) return vundef();
        copy_init(k, ARG(3));
        as_clip_constructed(k);
        as_call_handler(k, "onLoad", 0, 0);
        return vobj(as_clip_obj(k));
    }
    return vundef();
}
static val nf_duplicateMC(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    inst *k = inst_duplicate(cl, astrdup(as_tostring(ARG(0))), to_i32(ARG(1)));
    if (!k) return vundef();
    copy_init(k, ARG(2));
    return vobj(as_clip_obj(k));
}
static val nf_createEmpty(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    int dp = to_i32(ARG(1));
    inst *o = inst_at_depth(cl, dp);
    if (o) inst_remove(o);
    inst *k = inst_create(cl, 0, dp, astrdup(as_tostring(ARG(0))), 1);
    return k ? vobj(as_clip_obj(k)) : vundef();
}
static val nf_createTextField(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    int dp = to_i32(ARG(1));
    inst *o = inst_at_depth(cl, dp);
    if (o) inst_remove(o);
    inst *k = inst_create(cl, 0, dp, astrdup(as_tostring(ARG(0))), 1);
    if (!k) return vundef();
    etext_def *e = amalloc(sizeof *e);
    e->bounds[0] = 0;
    e->bounds[1] = (s32)(to_num(ARG(4)) * 20);
    e->bounds[2] = 0;
    e->bounds[3] = (s32)(to_num(ARG(5)) * 20);
    e->height = 240;
    e->color = 0xFF000000u;
    e->var = "";
    k->kind = K_ETEXT;
    k->et = e;
    k->tl = NULL;
    k->text = (char *)"";
    k->m.tx = (float)(to_num(ARG(2)) * 20);
    k->m.ty = (float)(to_num(ARG(3)) * 20);
    k->o = NULL;
    return vobj(as_clip_obj(k));
}
static val nf_hitTest(ctx *c, val th, val *a, int n) {
    THIS_CLIP
    if (n >= 2) {
        float x = (float)to_num(a[0]) * 20, y = (float)to_num(a[1]) * 20;
        if (n > 2 && to_bool(a[2])) return vbool(inst_hit_point(cl, x, y));
        float x0, y0, x1, y1;
        inst_bounds(cl, 1, &x0, &y0, &x1, &y1);
        return vbool(x >= x0 && x <= x1 && y >= y0 && y <= y1);
    }
    inst *o = target_of(c, ARG(0));
    if (!o) return vbool(0);
    float a0, b0, a1, b1, c0, d0, c1, d1;
    inst_bounds(cl, 1, &a0, &b0, &a1, &b1);
    inst_bounds(o, 1, &c0, &d0, &c1, &d1);
    return vbool(a0 <= c1 && c0 <= a1 && b0 <= d1 && d0 <= b1);
}
static val nf_getBounds(ctx *c, val th, val *a, int n) {
    THIS_CLIP
    float x0, y0, x1, y1;
    inst_bounds(cl, 1, &x0, &y0, &x1, &y1);
    inst *t = n ? target_of(c, a[0]) : cl;
    mat iv = t ? mat_inv(inst_world(t)) : MAT_ID;
    float bx[4] = {x0, x1, x0, x1}, by[4] = {y0, y0, y1, y1};
    float rx0 = 1e30f, ry0 = 1e30f, rx1 = -1e30f, ry1 = -1e30f;
    for (int i = 0; i < 4; i++) {
        float X = iv.a * bx[i] + iv.c * by[i] + iv.tx, Y = iv.b * bx[i] + iv.d * by[i] + iv.ty;
        if (X < rx0) rx0 = X;
        if (X > rx1) rx1 = X;
        if (Y < ry0) ry0 = Y;
        if (Y > ry1) ry1 = Y;
    }
    obj *r = new_obj(obj_proto);
    set_own(r, "xMin", vnum(rx0 / 20.0));
    set_own(r, "xMax", vnum(rx1 / 20.0));
    set_own(r, "yMin", vnum(ry0 / 20.0));
    set_own(r, "yMax", vnum(ry1 / 20.0));
    return vobj(r);
}
static val nf_localToGlobal(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    if (!n || a[0].t != V_OBJ) return vundef();
    mat w = inst_world(cl);
    num x = to_num(get_member(a[0], vstr("x"))) * 20, y = to_num(get_member(a[0], vstr("y"))) * 20;
    set_member(a[0], vstr("x"), vnum((w.a * x + w.c * y + w.tx) / 20));
    set_member(a[0], vstr("y"), vnum((w.b * x + w.d * y + w.ty) / 20));
    return vundef();
}
static val nf_globalToLocal(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    if (!n || a[0].t != V_OBJ) return vundef();
    mat w = mat_inv(inst_world(cl));
    num x = to_num(get_member(a[0], vstr("x"))) * 20, y = to_num(get_member(a[0], vstr("y"))) * 20;
    set_member(a[0], vstr("x"), vnum((w.a * x + w.c * y + w.tx) / 20));
    set_member(a[0], vstr("y"), vnum((w.b * x + w.d * y + w.ty) / 20));
    return vundef();
}
static val nf_startDrag(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    int con = n >= 5;
    start_drag(cl, to_bool(ARG(0)), con, (float)to_num(ARG(1)), (float)to_num(ARG(2)), (float)to_num(ARG(3)), (float)to_num(ARG(4)));
    return vundef();
}
static val nf_stopDrag(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; stop_drag(); return vundef(); }
static val nf_bytes(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; return vnum(100000); }
static val nf_getSWFVersion(ctx *c, val th, val *a, int n) { (void)c; (void)th; (void)a; (void)n; return vnum(swf_ver); }

/* drawing API */
static void dyn_grow(dyndraw *d) {
    if (d->ne + 32 < d->cap && d->nf + 2 < d->cap && d->nl + 2 < d->cap) return;
    int nc = d->cap * 2;
    edge *e = amalloc(sizeof(edge) * nc);
    fstyle *f = amalloc(sizeof(fstyle) * nc);
    lstyle *l = amalloc(sizeof(lstyle) * nc);
    if (!e || !f || !l) return;
    memcpy(e, d->edges, sizeof(edge) * d->ne);
    memcpy(f, d->fills, sizeof(fstyle) * d->nf);
    memcpy(l, d->lines, sizeof(lstyle) * d->nl);
    d->edges = e; d->fills = f; d->lines = l; d->cap = nc;
}
static void dyn_edge(dyndraw *d, float x0, float y0, float x1, float y1, int fill_only) {
    dyn_grow(d);
    if (d->ne + 1 >= d->cap) return;
    edge *e = &d->edges[d->ne++];
    e->x0 = x0; e->y0 = y0; e->x1 = x1; e->y1 = y1;
    e->f0 = 0;
    e->f1 = d->cur_fill;
    e->ln = fill_only ? 0 : d->cur_line;
    d->dirty = 1;
}
static void dyn_close(dyndraw *d) {
    if (d->cur_fill && (d->x != d->sx || d->y != d->sy)) dyn_edge(d, d->x, d->y, d->sx, d->sy, 1);
}
static u32 argb(val rgb, val alpha) {
    u32 c = (u32)to_i32(rgb) & 0xFFFFFF;
    num al = alpha.t == V_UNDEF ? 100 : to_num(alpha);
    if (al < 0) al = 0;
    if (al > 100) al = 100;
    return ((u32)(int)(al * 2.55) << 24) | c;
}
static val nf_lineStyle(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    dyndraw *d = dyn_get(cl);
    if (!n || a[0].t == V_UNDEF) { d->cur_line = 0; return vundef(); }
    dyn_grow(d);
    lstyle *l = &d->lines[d->nl++];
    l->w = (float)to_num(a[0]) * 20;
    l->color = argb(ARG(1), ARG(2));
    d->cur_line = (u16)d->nl;
    return vundef();
}
static val nf_beginFill(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    dyndraw *d = dyn_get(cl);
    dyn_close(d);
    dyn_grow(d);
    fstyle *f = &d->fills[d->nf++];
    memset(f, 0, sizeof *f);
    f->m = MAT_ID;
    if (n >= 2 && a[1].t == V_OBJ && a[1].u.o->kind == O_ARRAY) { /* beginGradientFill: use first colour */
        obj *cols = a[1].u.o, *als = n > 2 && a[2].t == V_OBJ ? a[2].u.o : NULL;
        f->color = argb(cols->alen ? cols->arr[0] : vnum(0), als && als->alen ? als->arr[0] : vundef());
    } else {
        f->color = argb(ARG(0), ARG(1));
    }
    d->cur_fill = (u16)d->nf;
    d->sx = d->x;
    d->sy = d->y;
    return vundef();
}
static val nf_endFill(ctx *c, val th, val *a, int n) {
    (void)c; (void)a; (void)n;
    THIS_CLIP
    dyndraw *d = dyn_get(cl);
    dyn_close(d);
    d->cur_fill = 0;
    return vundef();
}
static val nf_moveTo(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    dyndraw *d = dyn_get(cl);
    dyn_close(d);
    d->x = d->sx = (float)to_num(ARG(0)) * 20;
    d->y = d->sy = (float)to_num(ARG(1)) * 20;
    return vundef();
}
static val nf_lineTo(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    dyndraw *d = dyn_get(cl);
    float x = (float)to_num(ARG(0)) * 20, y = (float)to_num(ARG(1)) * 20;
    dyn_edge(d, d->x, d->y, x, y, 0);
    d->x = x;
    d->y = y;
    return vundef();
}
static val nf_curveTo(ctx *c, val th, val *a, int n) {
    (void)c;
    THIS_CLIP
    dyndraw *d = dyn_get(cl);
    float cx = (float)to_num(ARG(0)) * 20, cy = (float)to_num(ARG(1)) * 20;
    float ax = (float)to_num(ARG(2)) * 20, ay = (float)to_num(ARG(3)) * 20;
    float px = d->x, py = d->y;
    for (int i = 1; i <= 12; i++) {
        float t = i / 12.0f, u = 1 - t;
        float qx = u * u * d->x + 2 * u * t * cx + t * t * ax, qy = u * u * d->y + 2 * u * t * cy + t * t * ay;
        dyn_edge(d, px, py, qx, qy, 0);
        px = qx;
        py = qy;
    }
    d->x = ax;
    d->y = ay;
    return vundef();
}
static val nf_clear(ctx *c, val th, val *a, int n) {
    (void)c; (void)a; (void)n;
    THIS_CLIP
    dyndraw *d = dyn_get(cl);
    d->ne = d->nf = d->nl = 0;
    d->cur_fill = d->cur_line = 0;
    d->dirty = 1;
    return vundef();
}

/* Color */
static val nf_Color(ctx *c, val th, val *a, int n) {
    (void)c;
    if (th.t == V_OBJ) set_hidden(th.u.o, "_target", ARG(0));
    return th;
}
static inst *color_target(val th) {
    if (th.t != V_OBJ) return NULL;
    prop *p = own_prop(th.u.o, "_target");
    return p ? target_of(cur, p->v) : NULL;
}
static val nf_setRGB(ctx *c, val th, val *a, int n) {
    (void)c;
    inst *t = color_target(th);
    if (!t) return vundef();
    u32 v = (u32)to_i32(ARG(0));
    t->cx.rm = t->cx.gm = t->cx.bm = 0;
    t->cx.ra = (float)((v >> 16) & 255);
    t->cx.ga = (float)((v >> 8) & 255);
    t->cx.ba = (float)(v & 255);
    return vundef();
}
static val nf_getRGB(ctx *c, val th, val *a, int n) {
    (void)c; (void)a; (void)n;
    inst *t = color_target(th);
    if (!t) return vnum(0);
    return vnum(((int)t->cx.ra << 16) | ((int)t->cx.ga << 8) | (int)t->cx.ba);
}
static val nf_setTransform(ctx *c, val th, val *a, int n) {
    (void)c;
    inst *t = color_target(th);
    if (!t || !n) return vundef();
    val o = a[0];
    val v;
    if ((v = get_member(o, vstr("ra"))).t != V_UNDEF) t->cx.rm = (float)(to_num(v) / 100);
    if ((v = get_member(o, vstr("ga"))).t != V_UNDEF) t->cx.gm = (float)(to_num(v) / 100);
    if ((v = get_member(o, vstr("ba"))).t != V_UNDEF) t->cx.bm = (float)(to_num(v) / 100);
    if ((v = get_member(o, vstr("aa"))).t != V_UNDEF) t->cx.am = (float)(to_num(v) / 100);
    if ((v = get_member(o, vstr("rb"))).t != V_UNDEF) t->cx.ra = (float)to_num(v);
    if ((v = get_member(o, vstr("gb"))).t != V_UNDEF) t->cx.ga = (float)to_num(v);
    if ((v = get_member(o, vstr("bb"))).t != V_UNDEF) t->cx.ba = (float)to_num(v);
    if ((v = get_member(o, vstr("ab"))).t != V_UNDEF) t->cx.aa = (float)to_num(v);
    return vundef();
}
static val nf_getTransform(ctx *c, val th, val *a, int n) {
    (void)c; (void)a; (void)n;
    inst *t = color_target(th);
    obj *o = new_obj(obj_proto);
    if (!t) return vobj(o);
    set_own(o, "ra", vnum(t->cx.rm * 100)); set_own(o, "rb", vnum(t->cx.ra));
    set_own(o, "ga", vnum(t->cx.gm * 100)); set_own(o, "gb", vnum(t->cx.ga));
    set_own(o, "ba", vnum(t->cx.bm * 100)); set_own(o, "bb", vnum(t->cx.ba));
    set_own(o, "aa", vnum(t->cx.am * 100)); set_own(o, "ab", vnum(t->cx.aa));
    return vobj(o);
}

/* Date */
static int days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    int yoe = y - era * 400;
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static val nf_Date(ctx *c, val th, val *a, int n) {
    (void)c; (void)a; (void)n;
    obj *o = th.t == V_OBJ ? th.u.o : new_obj(date_proto);
    int h, mi, s, d, mo, y;
    plat_rtc(&h, &mi, &s, &d, &mo, &y);
    if (mo < 1 || mo > 12) mo = 1;
    if (d < 1) d = 1;
    o->kind = O_DATE;
    o->prim = ((num)days_from_civil(y, mo, d) * 86400.0 + h * 3600 + mi * 60 + s) * 1000.0 + (plat_ms() % 1000);
    return vobj(o);
}
static num date_field(val th, int which) {
    if (th.t != V_OBJ) return 0;
    num t = th.u.o->prim;
    num days = m_floor(t / 86400000.0);
    num ms = t - days * 86400000.0;
    int z = (int)days + 719468;
    int era = (z >= 0 ? z : z - 146096) / 146097;
    int doe = z - era * 146097;
    int yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    int d = doy - (153 * mp + 2) / 5 + 1, m = mp + (mp < 10 ? 3 : -9);
    if (m <= 2) y++;
    switch (which) {
    case 0: return y;
    case 1: return m - 1;
    case 2: return d;
    case 3: return m_floor(ms / 3600000.0);
    case 4: return m_floor(m_fmod(ms, 3600000.0) / 60000.0);
    case 5: return m_floor(m_fmod(ms, 60000.0) / 1000.0);
    case 6: return m_fmod(ms, 1000.0);
    case 7: { int w = (int)m_fmod(days + 4, 7); return w < 0 ? w + 7 : w; }
    }
    return 0;
}
static val nf_getTime(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return vnum(th.t == V_OBJ ? th.u.o->prim : 0); }
static val nf_getFullYear(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return vnum(date_field(th, 0)); }
static val nf_getYear(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return vnum(date_field(th, 0) - 1900); }
static val nf_getMonth(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return vnum(date_field(th, 1)); }
static val nf_getDate(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return vnum(date_field(th, 2)); }
static val nf_getHours(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return vnum(date_field(th, 3)); }
static val nf_getMinutes(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return vnum(date_field(th, 4)); }
static val nf_getSeconds(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return vnum(date_field(th, 5)); }
static val nf_getMs(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return vnum(date_field(th, 6)); }
static val nf_getDay(ctx *c, val th, val *a, int n) { (void)c; (void)a; (void)n; return vnum(date_field(th, 7)); }

/* ================================================================ setup */
static obj *mk_class(const char *name, native_fn ctor, obj **proto_out) {
    obj *f = new_fn(ctor);
    obj *p = new_obj(obj_proto);
    p->captured = 1;
    set_hidden(p, "constructor", vobj(f));
    set_hidden(f, "prototype", vobj(p));
    set_hidden(G, name, vobj(f));
    if (proto_out) *proto_out = p;
    return f;
}
static void num_const(obj *o, const char *name, num v) { set_hidden(o, name, vnum(v)); }

void as_init(void) {
    free_objs = NULL;
    free_props = NULL;
    q = NULL;
    qn = qcap = qh = 0;
    ivs = NULL;
    niv = ivcap = ivid = 0;
    sp = 0;
    depth_guard = 0;
    throwing = 0;
    cur = NULL;
    for (int i = 0; i < 4; i++) globals_regs[i].t = V_UNDEF;
    rnd_state ^= plat_ms() * 2654435761u + 1;

    obj_proto = new_obj(NULL);
    obj_proto->captured = 1;
    func_proto = new_obj(obj_proto);
    func_proto->captured = 1;
    G = new_obj(obj_proto);
    G->captured = 1;

    obj *O = mk_class("Object", nf_Object, NULL);
    set_hidden(O, "prototype", vobj(obj_proto));
    set_hidden(obj_proto, "constructor", vobj(O));
    def_native(obj_proto, "hasOwnProperty", nf_hasOwn);
    def_native(obj_proto, "toString", nf_objToString);
    def_native(obj_proto, "valueOf", nf_valueOf);
    def_native(obj_proto, "addProperty", nf_addProperty);
    def_native(obj_proto, "watch", nf_true);
    def_native(obj_proto, "unwatch", nf_true);
    def_native(obj_proto, "isPropertyEnumerable", nf_true);
    def_native(O, "registerClass", nf_registerClass);
    obj *F = mk_class("Function", nf_noop, NULL);
    set_hidden(F, "prototype", vobj(func_proto));
    def_native(func_proto, "call", nf_call);
    def_native(func_proto, "apply", nf_apply);

    mk_class("Array", nf_Array, &array_proto);
    def_native(array_proto, "push", nf_push);
    def_native(array_proto, "pop", nf_pop);
    def_native(array_proto, "shift", nf_shift);
    def_native(array_proto, "unshift", nf_unshift);
    def_native(array_proto, "join", nf_join);
    def_native(array_proto, "toString", nf_arrToString);
    def_native(array_proto, "slice", nf_slice);
    def_native(array_proto, "splice", nf_splice);
    def_native(array_proto, "concat", nf_concat);
    def_native(array_proto, "reverse", nf_reverse);
    def_native(array_proto, "sort", nf_sort);
    {
        prop *p = own_prop(G, "Array");
        obj *A = p->v.u.o;
        num_const(A, "CASEINSENSITIVE", 1);
        num_const(A, "DESCENDING", 2);
        num_const(A, "UNIQUESORT", 4);
        num_const(A, "RETURNINDEXEDARRAY", 8);
        num_const(A, "NUMERIC", 16);
    }

    obj *S = mk_class("String", nf_String, &string_proto);
    def_native(S, "fromCharCode", nf_fromCharCode);
    def_native(string_proto, "charAt", nf_charAt);
    def_native(string_proto, "charCodeAt", nf_charCodeAt);
    def_native(string_proto, "indexOf", nf_indexOf);
    def_native(string_proto, "lastIndexOf", nf_lastIndexOf);
    def_native(string_proto, "substr", nf_substr);
    def_native(string_proto, "substring", nf_substring);
    def_native(string_proto, "slice", nf_sslice);
    def_native(string_proto, "split", nf_split);
    def_native(string_proto, "toUpperCase", nf_upper);
    def_native(string_proto, "toLowerCase", nf_lower);
    def_native(string_proto, "concat", nf_strconcat);
    def_native(string_proto, "toString", nf_toString);
    def_native(array_proto, "indexOf", nf_indexOf);

    obj *N = mk_class("Number", nf_Number, &number_proto);
    def_native(number_proto, "toString", nf_toString);
    num_const(N, "MAX_VALUE", 1.79769313486231e308);
    num_const(N, "MIN_VALUE", 4.94065645841247e-324);
    num_const(N, "NaN", m_nan());
    num_const(N, "POSITIVE_INFINITY", m_inf());
    num_const(N, "NEGATIVE_INFINITY", -m_inf());
    mk_class("Boolean", nf_Boolean, &bool_proto);
    def_native(bool_proto, "toString", nf_toString);

    obj *M = new_obj(obj_proto);
    M->captured = 1;
    set_hidden(G, "Math", vobj(M));
    def_native(M, "abs", nf_abs); def_native(M, "floor", nf_floor); def_native(M, "ceil", nf_ceil);
    def_native(M, "round", nf_round); def_native(M, "sqrt", nf_sqrt); def_native(M, "sin", nf_sin);
    def_native(M, "cos", nf_cos); def_native(M, "tan", nf_tan); def_native(M, "atan2", nf_atan2);
    def_native(M, "atan", nf_atan); def_native(M, "asin", nf_asin); def_native(M, "acos", nf_acos);
    def_native(M, "exp", nf_exp); def_native(M, "log", nf_log); def_native(M, "pow", nf_pow);
    def_native(M, "random", nf_random); def_native(M, "min", nf_min); def_native(M, "max", nf_max);
    num_const(M, "PI", 3.141592653589793); num_const(M, "E", 2.718281828459045);
    num_const(M, "SQRT2", 1.4142135623730951); num_const(M, "SQRT1_2", 0.7071067811865476);
    num_const(M, "LN2", 0.6931471805599453); num_const(M, "LN10", 2.302585092994046);
    num_const(M, "LOG2E", 1.4426950408889634); num_const(M, "LOG10E", 0.4342944819032518);

    key_obj = new_obj(obj_proto);
    key_obj->captured = 1;
    set_hidden(G, "Key", vobj(key_obj));
    def_native(key_obj, "isDown", nf_isDown);
    def_native(key_obj, "getCode", nf_getCode);
    def_native(key_obj, "getAscii", nf_getAscii);
    def_native(key_obj, "isToggled", nf_isToggled);
    def_native(key_obj, "addListener", nf_addListener);
    def_native(key_obj, "removeListener", nf_removeListener);
    static const struct { const char *n; int v; } keys[] = {
        {"BACKSPACE", 8}, {"CAPSLOCK", 20}, {"CONTROL", 17}, {"DELETEKEY", 46}, {"DOWN", 40}, {"END", 35},
        {"ENTER", 13}, {"ESCAPE", 27}, {"HOME", 36}, {"INSERT", 45}, {"LEFT", 37}, {"PGDN", 34},
        {"PGUP", 33}, {"RIGHT", 39}, {"SHIFT", 16}, {"SPACE", 32}, {"TAB", 9}, {"UP", 38}, {"ALT", 18}};
    for (u32 i = 0; i < sizeof keys / sizeof keys[0]; i++) num_const(key_obj, keys[i].n, keys[i].v);

    mouse_obj = new_obj(obj_proto);
    mouse_obj->captured = 1;
    set_hidden(G, "Mouse", vobj(mouse_obj));
    def_native(mouse_obj, "hide", nf_mhide);
    def_native(mouse_obj, "show", nf_mshow);
    def_native(mouse_obj, "addListener", nf_addListener);
    def_native(mouse_obj, "removeListener", nf_removeListener);

    obj *stage = new_obj(obj_proto);
    stage->captured = 1;
    set_hidden(G, "Stage", vobj(stage));
    num_const(stage, "width", stage_w);
    num_const(stage, "height", stage_h);
    set_hidden(stage, "scaleMode", vstr("showAll"));
    set_hidden(stage, "align", vstr(""));
    def_native(stage, "addListener", nf_addListener);
    def_native(stage, "removeListener", nf_removeListener);

    obj *sys = new_obj(obj_proto);
    sys->captured = 1;
    set_hidden(G, "System", vobj(sys));
    obj *caps = new_obj(obj_proto);
    set_hidden(sys, "capabilities", vobj(caps));
    set_hidden(caps, "os", vstr("FlashOS"));
    set_hidden(caps, "version", vstr("FOS 8,0,0,0"));
    set_hidden(caps, "screenResolutionX", vnum(scr_w));
    set_hidden(caps, "screenResolutionY", vnum(scr_h));
    obj *sec = new_obj(obj_proto);
    set_hidden(sys, "security", vobj(sec));
    def_native(sec, "allowDomain", nf_noop);
    set_hidden(sys, "useCodepage", vbool(1));

    mk_class("MovieClip", nf_noop, &clip_proto);
    def_native(clip_proto, "play", nf_play);
    def_native(clip_proto, "stop", nf_stop);
    def_native(clip_proto, "gotoAndPlay", nf_gotoAndPlay);
    def_native(clip_proto, "gotoAndStop", nf_gotoAndStop);
    def_native(clip_proto, "nextFrame", nf_nextFrame);
    def_native(clip_proto, "prevFrame", nf_prevFrame);
    def_native(clip_proto, "removeMovieClip", nf_removeMC);
    def_native(clip_proto, "getDepth", nf_getDepth);
    def_native(clip_proto, "getNextHighestDepth", nf_nextDepth);
    def_native(clip_proto, "getInstanceAtDepth", nf_atDepth);
    def_native(clip_proto, "swapDepths", nf_swapDepths);
    def_native(clip_proto, "attachMovie", nf_attachMovie);
    def_native(clip_proto, "duplicateMovieClip", nf_duplicateMC);
    def_native(clip_proto, "createEmptyMovieClip", nf_createEmpty);
    def_native(clip_proto, "createTextField", nf_createTextField);
    def_native(clip_proto, "hitTest", nf_hitTest);
    def_native(clip_proto, "getBounds", nf_getBounds);
    def_native(clip_proto, "getRect", nf_getBounds);
    def_native(clip_proto, "localToGlobal", nf_localToGlobal);
    def_native(clip_proto, "globalToLocal", nf_globalToLocal);
    def_native(clip_proto, "startDrag", nf_startDrag);
    def_native(clip_proto, "stopDrag", nf_stopDrag);
    def_native(clip_proto, "getBytesLoaded", nf_bytes);
    def_native(clip_proto, "getBytesTotal", nf_bytes);
    def_native(clip_proto, "getSWFVersion", nf_getSWFVersion);
    def_native(clip_proto, "loadMovie", nf_noop);
    def_native(clip_proto, "loadVariables", nf_noop);
    def_native(clip_proto, "unloadMovie", nf_noop);
    def_native(clip_proto, "getURL", nf_noop);
    def_native(clip_proto, "setMask", nf_noop);
    def_native(clip_proto, "lineStyle", nf_lineStyle);
    def_native(clip_proto, "beginFill", nf_beginFill);
    def_native(clip_proto, "beginGradientFill", nf_beginFill);
    def_native(clip_proto, "endFill", nf_endFill);
    def_native(clip_proto, "moveTo", nf_moveTo);
    def_native(clip_proto, "lineTo", nf_lineTo);
    def_native(clip_proto, "curveTo", nf_curveTo);
    def_native(clip_proto, "clear", nf_clear);
    mk_class("Button", nf_noop, &button_proto);
    mk_class("TextField", nf_noop, &text_proto);
    def_native(text_proto, "setTextFormat", nf_noop);
    def_native(text_proto, "setNewTextFormat", nf_noop);
    def_native(text_proto, "removeTextField", nf_removeMC);
    def_native(text_proto, "getDepth", nf_getDepth);
    def_native(text_proto, "swapDepths", nf_swapDepths);
    mk_class("TextFormat", nf_Object, NULL);

    mk_class("Color", nf_Color, &color_proto);
    def_native(color_proto, "setRGB", nf_setRGB);
    def_native(color_proto, "getRGB", nf_getRGB);
    def_native(color_proto, "setTransform", nf_setTransform);
    def_native(color_proto, "getTransform", nf_getTransform);
    mk_class("Sound", nf_Object, &sound_proto);
    def_native(sound_proto, "attachSound", nf_noop);
    def_native(sound_proto, "start", nf_noop);
    def_native(sound_proto, "stop", nf_noop);
    def_native(sound_proto, "setVolume", nf_noop);
    def_native(sound_proto, "getVolume", nf_noop);
    def_native(sound_proto, "setPan", nf_noop);
    def_native(sound_proto, "loadSound", nf_noop);
    mk_class("Date", nf_Date, &date_proto);
    def_native(date_proto, "getTime", nf_getTime);
    def_native(date_proto, "valueOf", nf_getTime);
    def_native(date_proto, "getFullYear", nf_getFullYear);
    def_native(date_proto, "getYear", nf_getYear);
    def_native(date_proto, "getMonth", nf_getMonth);
    def_native(date_proto, "getDate", nf_getDate);
    def_native(date_proto, "getDay", nf_getDay);
    def_native(date_proto, "getHours", nf_getHours);
    def_native(date_proto, "getMinutes", nf_getMinutes);
    def_native(date_proto, "getSeconds", nf_getSeconds);
    def_native(date_proto, "getMilliseconds", nf_getMs);

    def_native(G, "trace", nf_trace);
    def_native(G, "getTimer", nf_getTimer);
    def_native(G, "parseInt", nf_parseInt);
    def_native(G, "parseFloat", nf_parseFloat);
    def_native(G, "isNaN", nf_isNaN);
    def_native(G, "isFinite", nf_isFinite);
    def_native(G, "escape", nf_escape);
    def_native(G, "unescape", nf_escape);
    def_native(G, "setInterval", nf_setInterval);
    def_native(G, "clearInterval", nf_clearInterval);
    def_native(G, "setTimeout", nf_setTimeout);
    def_native(G, "clearTimeout", nf_clearInterval);
    def_native(G, "fscommand", nf_noop);
    def_native(G, "updateAfterEvent", nf_noop);
    def_native(G, "ASSetPropFlags", nf_noop);
    num_const(G, "NaN", m_nan());
    num_const(G, "Infinity", m_inf());
}
