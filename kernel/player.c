/* FlashKernel SWF player: characters, display tree, timeline, events.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 */
#include "swf.h"
#include "audio.h"

chr *chars;
inst *root;
int swf_ver;
int stage_w, stage_h;
int mouse_visible = 1, mouse_stage_x, mouse_stage_y;
int quit_requested;
int last_key_code, last_key_ascii;

static u32 bg_color;
static mat base;          /* stage twips -> screen pixels */
static int interactive;
static const u8 **init_done;
static int n_init_done;

/* ---------------------------------------------------------------- memory */
static u8 *a_base, *a_top, *a_end;

void *amalloc(u32 n) {
    n = (n + 7) & ~7u;
    if (!a_base) { void *p = kmalloc(n); if (p) memset(p, 0, n); return p; }
    if ((u32)(a_end - a_top) < n) return NULL;
    void *p = a_top;
    a_top += n;
    memset(p, 0, n);
    return p;
}
char *astrndup(const char *s, u32 n) {
    char *d = amalloc(n + 1);
    if (!d) return (char *)"";
    memcpy(d, s, n);
    d[n] = 0;
    return d;
}
char *astrdup(const char *s) { return astrndup(s, kstrlen(s)); }

/* ---------------------------------------------------------------- timelines */
static timeline *make_timeline(const u8 *tags, u32 len) {
    timeline *t = amalloc(sizeof *t);
    t->tags = tags;
    t->len = len;
    int frame = 1, nl = 0;
    const char *names[256];
    u16 frames[256];
    u32 pos = 0;
    while (pos + 2 <= len) {
        u32 hdr = tags[pos] | (tags[pos + 1] << 8), code = hdr >> 6, l = hdr & 63;
        pos += 2;
        if (l == 63) {
            if (pos + 4 > len) break;
            l = tags[pos] | (tags[pos + 1] << 8) | (tags[pos + 2] << 16) | ((u32)tags[pos + 3] << 24);
            pos += 4;
        }
        if (l > len - pos) l = len - pos;
        if (code == 0) break;
        if (code == 1) frame++;
        if (code == 43 && nl < 256) { names[nl] = (const char *)tags + pos; frames[nl++] = (u16)frame; }
        if (code == 12 || code == 59 || code == 7 || code == 34) interactive = 1;
        if (code == 26 && l && (tags[pos] & 0x80)) interactive = 1;
        if (code == 39 && l > 4) make_timeline(tags + pos + 4, l - 4); /* scan sprites for AS */
        pos += l;
    }
    t->nframes = (u16)(frame > 1 ? frame - 1 : 1);
    t->nlabels = nl;
    t->lname = amalloc(sizeof(char *) * (nl + 1));
    t->lframe = amalloc(sizeof(u16) * (nl + 1));
    for (int i = 0; i < nl; i++) { t->lname[i] = names[i]; t->lframe[i] = frames[i]; }
    return t;
}

int inst_label(inst *in, const char *label) {
    if (!in || !in->tl) return 0;
    for (int i = 0; i < in->tl->nlabels; i++)
        if (!kstricmp(in->tl->lname[i], label)) return in->tl->lframe[i];
    return 0;
}

/* ---------------------------------------------------------------- definitions */
static void define_button(u16 id, const u8 *b, u32 len, int v2) {
    rd r;
    rinit(&r, b, len);
    button_def *bd = amalloc(sizeof *bd);
    u32 act_off = 0, act_base = 0;
    if (v2) { r8(&r); act_base = r.i; act_off = r16(&r); }
    btnrec tmp[64];
    int n = 0;
    for (;;) {
        u32 f = r8(&r);
        if (!f || r.i >= r.n) break;
        btnrec br;
        br.states = (u8)(f & 15);
        br.id = (u16)r16(&r);
        br.depth = (s16)r16(&r);
        br.m = rmat(&r);
        br.cx = v2 ? rcx(&r, 1) : CX_ID;
        if (v2 && (f & 0x10)) { /* filter list: skip */
            u32 nfil = r8(&r);
            (void)nfil;
            break; /* filters are complex; stop reading records */
        }
        if (v2 && (f & 0x20)) r8(&r);
        if (n < 64) tmp[n++] = br;
    }
    /* sort by depth */
    for (int i = 1; i < n; i++) {
        btnrec t = tmp[i];
        int j = i;
        while (j > 0 && tmp[j - 1].depth > t.depth) { tmp[j] = tmp[j - 1]; j--; }
        tmp[j] = t;
    }
    bd->nrec = n;
    bd->rec = amalloc(sizeof(btnrec) * (n + 1));
    memcpy(bd->rec, tmp, sizeof(btnrec) * n);
    btnact acts[32];
    int na = 0;
    if (!v2) {
        acts[na].cond = 0x08;
        acts[na].code = b + r.i;
        acts[na].len = len - r.i;
        na++;
    } else if (act_off) {
        u32 p = act_base + act_off;
        while (p + 4 <= len && na < 32) {
            u32 sz = b[p] | (b[p + 1] << 8);
            acts[na].cond = (u16)(b[p + 2] | (b[p + 3] << 8));
            acts[na].code = b + p + 4;
            acts[na].len = sz ? sz - 4 : len - p - 4;
            na++;
            if (!sz) break;
            p += sz;
        }
    }
    bd->nact = na;
    bd->act = amalloc(sizeof(btnact) * (na + 1));
    memcpy(bd->act, acts, sizeof(btnact) * na);
    chars[id].type = CH_BUTTON;
    chars[id].p = bd;
}

static void define_font(u16 id, const u8 *b, u32 len, int ver) {
    font_def *fd = amalloc(sizeof *fd);
    fd->em = ver == 3 ? 20480.0f : 1024.0f;
    if (ver == 1) {
        if (len < 2) return;
        u32 first = b[0] | (b[1] << 8);
        int n = (int)(first / 2);
        fd->nglyph = n;
        fd->glyph = amalloc(sizeof(shape *) * (n + 1));
        for (int i = 0; i < n; i++) {
            u32 o = b[i * 2] | (b[i * 2 + 1] << 8);
            u32 e = i + 1 < n ? (u32)(b[i * 2 + 2] | (b[i * 2 + 3] << 8)) : len;
            if (o < len && e <= len && e > o) fd->glyph[i] = parse_shape(b + o, e - o, 1, 1, 0);
        }
    } else {
        rd r;
        rinit(&r, b, len);
        u32 flags = r8(&r);
        r8(&r);
        u32 nl = r8(&r);
        r.i += nl;
        int n = (int)r16(&r);
        int wide = flags & 8, widecodes = flags & 4;
        u32 tbl = r.i;
        fd->nglyph = n;
        fd->glyph = amalloc(sizeof(shape *) * (n + 1));
        fd->code = amalloc(sizeof(u16) * (n + 1));
        u32 offs[1025];
        if (n > 1024) n = fd->nglyph = 1024;
        for (int i = 0; i <= n; i++) offs[i] = wide ? r32(&r) : r16(&r);
        for (int i = 0; i < n; i++) {
            u32 o = tbl + offs[i], e = tbl + offs[i + 1];
            if (o < len && e <= len && e > o) fd->glyph[i] = parse_shape(b + o, e - o, 1, 1, 0);
        }
        r.i = tbl + offs[n];
        for (int i = 0; i < n; i++) fd->code[i] = (u16)(widecodes ? r16(&r) : r8(&r));
    }
    chars[id].type = CH_FONT;
    chars[id].p = fd;
}

static void define_text(u16 id, const u8 *b, u32 len, int v2) {
    rd r;
    rinit(&r, b, len);
    text_def *td = amalloc(sizeof *td);
    rrect(&r, td->bounds);
    td->m = rmat(&r);
    int gbits = r8(&r), abits = r8(&r);
    textrun runs[128];
    int nr = 0;
    u16 font = 0;
    u32 color = 0xFF000000u;
    float x = 0, y = 0, h = 240;
    while (r.i < r.n && nr < 128) {
        u32 f = r8(&r);
        if (!f) break;
        if (f & 8) font = (u16)r16(&r);
        if (f & 4) color = v2 ? rrgba(&r) : rrgb(&r);
        if (f & 1) x = (float)(s16)r16(&r);
        if (f & 2) y = (float)(s16)r16(&r);
        if (f & 8) h = (float)r16(&r);
        int n = r8(&r);
        textrun *t = &runs[nr++];
        t->font = font; t->color = color; t->x = x; t->y = y; t->height = h; t->n = n;
        t->glyph = amalloc(sizeof(u16) * (n + 1));
        t->adv = amalloc(sizeof(float) * (n + 1));
        for (int i = 0; i < n; i++) {
            t->glyph[i] = (u16)rub(&r, gbits);
            t->adv[i] = (float)rsb(&r, abits);
            x += t->adv[i];
        }
        ralign(&r);
    }
    td->nrun = nr;
    td->run = amalloc(sizeof(textrun) * (nr + 1));
    memcpy(td->run, runs, sizeof(textrun) * nr);
    chars[id].type = CH_TEXT;
    chars[id].p = td;
}

static void define_etext(u16 id, const u8 *b, u32 len) {
    rd r;
    rinit(&r, b, len);
    etext_def *e = amalloc(sizeof *e);
    rrect(&r, e->bounds);
    e->flags = (u16)((r8(&r) << 8));
    e->flags |= (u16)r8(&r);
    e->color = 0xFF000000u;
    e->height = 240;
    if (e->flags & 0x0100) e->font = (u16)r16(&r);          /* HasFont */
    if (e->flags & 0x0080) rstr(&r);                         /* HasFontClass */
    if (e->flags & 0x0100) e->height = (float)r16(&r);
    if (e->flags & 0x0400) e->color = rrgba(&r);             /* HasTextColor */
    if (e->flags & 0x0200) e->maxlen = (int)r16(&r);         /* HasMaxLength */
    if (e->flags & 0x0020) { e->align = (int)r8(&r); r16(&r); r16(&r); r16(&r); r16(&r); }
    e->var = rstr(&r);
    if (e->flags & 0x8000) e->init = rstr(&r);              /* HasText */
    chars[id].type = CH_ETEXT;
    chars[id].p = e;
}

/* ---------------------------------------------------------------- sound */
void *audio_alloc(u32 n) { return amalloc(n); }

static sound_def *sound_ready(u16 id) {
    if (chars[id].type != CH_SOUND) return NULL;
    sound_def *sd = chars[id].p;
    if (!sd->tried) {
        sd->tried = 1;
        u32 fr = 0;
        int rate = 0;
        sd->pcm = audio_decode(sd->format, sd->rate, sd->is16, sd->stereo, sd->count, sd->data, sd->len, &fr, &rate);
        sd->frames = fr;
        sd->prate = rate;
    }
    return sd->pcm ? sd : NULL;
}
int snd_play(u16 id, int loops, int vol, int tag) {
    sound_def *sd = sound_ready(id);
    return sd ? audio_play(sd->pcm, sd->frames, sd->prate, loops, vol, tag, id) : -1;
}
int snd_find(const char *link) {
    for (int i = 0; i < 65536; i++)
        if (chars[i].type == CH_SOUND && chars[i].export_name && !kstricmp(chars[i].export_name, link)) return i;
    return -1;
}
u32 snd_duration_ms(u16 id) {
    sound_def *sd = sound_ready(id);
    return sd && sd->prate ? (u32)((sd->frames / (u32)sd->prate) * 1000 + (sd->frames % (u32)sd->prate) * 1000 / (u32)sd->prate) : 0;
}

/* SOUNDINFO record (StartSound, button sounds) */
static void sound_info_play(u16 id, rd *r) {
    u32 f = r8(r);
    int loops = 1;
    if (f & 1) r32(r);
    if (f & 2) r32(r);
    if (f & 4) loops = (int)r16(r);
    if (f & 8) { u32 n = r8(r); r->i += n * 8; }
    if (f & 0x20) { audio_stop_sound(id); return; }          /* SyncStop */
    if ((f & 0x10) && audio_playing(id)) return;               /* SyncNoMultiple */
    snd_play(id, loops, 256, 0);
}

static int stream_fmt = -1, stream_rate, stream_16, stream_stereo;

static void define(u32 code, const u8 *b, u32 len) {
    if (len < 2) return;
    u16 id = (u16)(b[0] | (b[1] << 8));
    if (chars[id].type != CH_NONE) return;
    const u8 *d = b + 2;
    u32 dl = len - 2;
    switch (code) {
    case 2: case 22: case 32: case 83: {
        shape *s = parse_shape(d, dl, code == 2 ? 1 : code == 22 ? 2 : code == 32 ? 3 : 4, 0, 0);
        if (s) { chars[id].type = CH_SHAPE; chars[id].p = s; }
        break;
    }
    case 39:
        if (dl >= 2) {
            chars[id].type = CH_SPRITE;
            chars[id].p = make_timeline(d + 2, dl - 2);
        }
        break;
    case 20: case 36: chars[id].type = CH_BITMAP; chars[id].p = parse_lossless(d, dl, code == 36); break;
    case 6: case 21: case 35: case 90: chars[id].type = CH_BITMAP; chars[id].p = image_decode_tag((int)code, d, dl); break;
    case 7: define_button(id, d, dl, 0); break;
    case 34: define_button(id, d, dl, 1); break;
    case 10: define_font(id, d, dl, 1); break;
    case 48: define_font(id, d, dl, 2); break;
    case 75: define_font(id, d, dl, 3); break;
    case 11: define_text(id, d, dl, 0); break;
    case 33: define_text(id, d, dl, 1); break;
    case 37: define_etext(id, d, dl); break;
    case 14: { /* DefineSound */
        if (dl < 5) break;
        sound_def *sd = amalloc(sizeof *sd);
        u8 fl = d[0];
        sd->format = (u8)(fl >> 4);
        sd->rate = (u8)((fl >> 2) & 3);
        sd->is16 = (u8)((fl >> 1) & 1);
        sd->stereo = (u8)(fl & 1);
        sd->count = d[1] | (d[2] << 8) | (d[3] << 16) | ((u32)d[4] << 24);
        sd->data = d + 5;
        sd->len = dl - 5;
        chars[id].type = CH_SOUND;
        chars[id].p = sd;
        break;
    }
    }
}

/* ---------------------------------------------------------------- display tree */
static inst *inst_alloc(void) { return amalloc(sizeof(inst)); }

static void kids_insert(inst *p, inst *c) {
    if (p->n == p->cap) {
        int nc = p->cap ? p->cap * 2 : 8;
        inst **k = amalloc(sizeof(inst *) * nc);
        if (!k) return;
        if (p->n) memcpy(k, p->kids, sizeof(inst *) * p->n);
        p->kids = k;
        p->cap = nc;
    }
    int i = p->n;
    while (i > 0 && p->kids[i - 1]->depth > c->depth) { p->kids[i] = p->kids[i - 1]; i--; }
    p->kids[i] = c;
    p->n++;
    c->parent = p;
}
static void kids_remove(inst *p, inst *c) {
    for (int i = 0; i < p->n; i++)
        if (p->kids[i] == c) {
            memmove(&p->kids[i], &p->kids[i + 1], sizeof(inst *) * (p->n - i - 1));
            p->n--;
            return;
        }
}

inst *inst_at_depth(inst *p, int depth) {
    for (int i = 0; i < p->n; i++) if (p->kids[i]->depth == depth) return p->kids[i];
    return NULL;
}
inst *inst_child(inst *p, const char *name) {
    if (!p || !name) return NULL;
    for (int i = 0; i < p->n; i++)
        if (p->kids[i]->name && !kstricmp(p->kids[i]->name, name)) return p->kids[i];
    return NULL;
}
int inst_next_depth(inst *p) {
    int d = 0;
    for (int i = 0; i < p->n; i++) if (p->kids[i]->depth >= d) d = p->kids[i]->depth + 1;
    return d;
}

static void mark_dead(inst *in) {
    in->dead = 1;
    as_clip_dead(in);
    for (int i = 0; i < in->n; i++) mark_dead(in->kids[i]);
}

static void clip_event(inst *in, u32 mask, int key);

void inst_remove(inst *in) {
    if (!in || in->dead || !in->parent) return;
    clip_event(in, 0x04, 0);           /* unload */
    as_call_handler(in, "onUnload", 0, 0);
    kids_remove(in->parent, in);
    mark_dead(in);
}

static int steps_actions = 1;
static void tl_step(inst *in);

inst *inst_create(inst *parent, u16 id, int depth, const char *name, int dynamic) {
    inst *in = inst_alloc();
    if (!in) return NULL;
    in->id = id;
    in->depth = depth;
    in->m = MAT_ID;
    in->cx = CX_ID;
    in->visible = 1;
    in->enabled = 1;
    in->hand = 1;
    in->dynamic = (u8)dynamic;
    in->name = name;
    switch (chars[id].type) {
    case CH_SPRITE: in->kind = K_SPRITE; break;
    case CH_BUTTON: in->kind = K_BUTTON; break;
    case CH_TEXT: in->kind = K_TEXT; break;
    case CH_ETEXT: in->kind = K_ETEXT; break;
    default: in->kind = K_SHAPE; break;
    }
    if (id == 0 && dynamic) in->kind = K_SPRITE; /* createEmptyMovieClip */
    kids_insert(parent, in);
    if (in->kind == K_SPRITE) {
        in->tl = chars[id].type == CH_SPRITE ? chars[id].p : NULL;
        in->playing = 1;
        in->fresh = 1;
        if (in->tl) {
            int save = steps_actions;
            steps_actions = 1;
            tl_step(in);
            steps_actions = save;
        } else {
            in->frame = 1;
        }
    } else if (in->kind == K_BUTTON) {
        button_def *bd = chars[id].p;
        in->btn_kids = amalloc(sizeof(inst *) * (bd->nrec + 1));
        for (int i = 0; i < bd->nrec; i++)
            if (chars[bd->rec[i].id].type == CH_SPRITE) {
                inst *k = inst_alloc();
                k->kind = K_SPRITE; k->id = bd->rec[i].id; k->tl = chars[k->id].p;
                k->m = MAT_ID; k->cx = CX_ID; k->visible = 1; k->playing = 1; k->parent = in;
                tl_step(k);
                in->btn_kids[i] = k;
            }
    } else if (in->kind == K_ETEXT) {
        etext_def *e = chars[id].p;
        in->et = e;
        inst_set_text(in, e->init ? e->init : "");
    }
    return in;
}

void inst_set_text(inst *in, const char *s) {
    /* strip HTML tags for HTML text fields */
    if (in->et && (in->et->flags & 0x0002)) {
        u32 n = kstrlen(s);
        char *d = amalloc(n + 1);
        u32 j = 0;
        int tag = 0;
        for (u32 i = 0; i < n; i++) {
            if (s[i] == '<') {
                tag = 1;
                if (!kstrnicmp(s + i, "<br", 3) || !kstrnicmp(s + i, "</p", 3)) d[j++] = '\n';
            } else if (s[i] == '>') tag = 0;
            else if (!tag) d[j++] = s[i];
        }
        d[j] = 0;
        in->text = d;
    } else {
        in->text = astrdup(s);
    }
}

/* ---------------------------------------------------------------- placement */
static void read_clip_actions(inst *in, rd *r) {
    r16(r);
    int wide = swf_ver >= 6;
    if (wide) r32(r); else r16(r);
    clipaction tmp[32];
    int n = 0;
    while (r->i < r->n && n < 32) {
        u32 f = wide ? r32(r) : r16(r);
        if (!f) break;
        u32 size = r32(r);
        u32 start = r->i;
        clipaction *c = &tmp[n++];
        c->flags = f;
        c->key = 0;
        if (f & 0x20000) { c->key = (u8)r8(r); size--; start++; }
        c->code = r->d + start;
        c->len = size;
        r->i = start + size;
    }
    in->nca = n;
    in->ca = amalloc(sizeof(clipaction) * (n + 1));
    memcpy(in->ca, tmp, sizeof(clipaction) * n);
    interactive = 1;
}

static inst **replay_old;
static int replay_n;
static inst *replay_parent;

static void tl_place(inst *in, const u8 *b, u32 len, int ver) {
    rd r;
    rinit(&r, b, len);
    u16 id = 0;
    int depth;
    u32 fl = 0;
    mat m = MAT_ID;
    cxf cx = CX_ID;
    u16 ratio = 0, clip = 0;
    const char *name = NULL;
    if (ver == 1) {
        id = (u16)r16(&r);
        depth = (int)r16(&r) - 16384;
        m = rmat(&r);
        fl = 0x06;
        if (r.i < r.n) { cx = rcx(&r, 0); fl |= 0x08; }
    } else {
        fl = r8(&r);
        u32 fl2 = ver == 3 ? r8(&r) : 0;
        depth = (int)r16(&r) - 16384;
        if (ver == 3 && ((fl2 & 8) || ((fl2 & 16) && (fl & 2)))) rstr(&r);
        if (fl & 2) id = (u16)r16(&r);
        if (fl & 4) m = rmat(&r);
        if (fl & 8) cx = rcx(&r, 1);
        if (fl & 16) ratio = (u16)r16(&r);
        if (fl & 32) name = rstr(&r);
        if (fl & 64) clip = (u16)r16(&r);
        if (ver == 3) {
            if (fl2 & 1) { u32 nfil = r8(&r); (void)nfil; fl &= ~0x80u; } /* filters: skip clip actions */
            else if (fl2 & 2) r8(&r);
            if (fl2 & 4) r8(&r);
        }
    }
    inst *e = inst_at_depth(in, depth);
    int move = fl & 1, has_char = fl & 2;
    if (!move || !e) {
        if (!has_char) return;
        /* reuse the old object during a rewind if it matches */
        if (replay_parent == in) {
            for (int i = 0; i < replay_n; i++) {
                inst *o = replay_old[i];
                if (o && o->depth == depth && o->id == id && !o->dynamic) {
                    replay_old[i] = NULL;
                    kids_insert(in, o);
                    if (!o->as_moved) { if (fl & 4) o->m = m; if (fl & 8) o->cx = cx; }
                    return;
                }
            }
        }
        if (e) inst_remove(e);
        inst *c = inst_create(in, id, depth, name ? astrdup(name) : NULL, 0);
        if (!c) return;
        c->m = m;
        c->cx = cx;
        c->ratio = ratio;
        c->clipdepth = clip;
        if (ver >= 2 && (fl & 0x80)) read_clip_actions(c, &r);
        as_clip_constructed(c);
        clip_event(c, 0x01, 0); /* load */
        return;
    }
    if (has_char && id != e->id) {
        if (e->kind == K_SHAPE && chars[id].type == CH_SHAPE) e->id = id;
        else {
            mat om = e->m;
            cxf oc = e->cx;
            inst_remove(e);
            e = inst_create(in, id, depth, NULL, 0);
            if (!e) return;
            e->m = om;
            e->cx = oc;
        }
    }
    if (e->as_moved) return;
    if (fl & 4) e->m = m;
    if (fl & 8) e->cx = cx;
    if (fl & 16) e->ratio = ratio;
    if (fl & 64) e->clipdepth = clip;
}

/* Run the tags of the next frame. */
static void tl_step(inst *in) {
    if (!in->tl) return;
    const u8 *t = in->tl->tags;
    u32 len = in->tl->len;
    for (;;) {
        if (in->pos + 2 > len) { in->frame++; return; }
        u32 hdr = t[in->pos] | (t[in->pos + 1] << 8), code = hdr >> 6, l = hdr & 63;
        in->pos += 2;
        if (l == 63) {
            if (in->pos + 4 > len) { in->pos = len; in->frame++; return; }
            l = t[in->pos] | (t[in->pos + 1] << 8) | (t[in->pos + 2] << 16) | ((u32)t[in->pos + 3] << 24);
            in->pos += 4;
        }
        if (l > len - in->pos) l = len - in->pos;
        const u8 *b = t + in->pos;
        in->pos += l;
        switch (code) {
        case 0: in->pos = len; in->frame++; return;
        case 1: in->frame++; return;
        case 4: tl_place(in, b, l, 1); break;
        case 26: tl_place(in, b, l, 2); break;
        case 70: tl_place(in, b, l, 3); break;
        case 5: if (l >= 4) { inst *e = inst_at_depth(in, (int)(b[2] | (b[3] << 8)) - 16384); if (e) inst_remove(e); } break;
        case 28: if (l >= 2) { inst *e = inst_at_depth(in, (int)(b[0] | (b[1] << 8)) - 16384); if (e) inst_remove(e); } break;
        case 12: if (steps_actions) as_queue(b, l, in); break;
        case 59: {
            int done = 0;
            for (int i = 0; i < n_init_done; i++) if (init_done[i] == b) done = 1;
            if (!done && l > 2) {
                if (n_init_done < 512) init_done[n_init_done++] = b;
                as_run(b + 2, l - 2, root);
            }
            break;
        }
        case 15: if (l >= 2) { rd r; rinit(&r, b + 2, l - 2); sound_info_play((u16)(b[0] | (b[1] << 8)), &r); } break;
        case 89: { int id = snd_find((const char *)b); if (id >= 0) snd_play((u16)id, 1, 256, 0); break; }
        case 17: /* DefineButtonSound */
            if (l >= 2 && chars[b[0] | (b[1] << 8)].type == CH_BUTTON) {
                button_def *bd = chars[b[0] | (b[1] << 8)].p;
                rd r;
                rinit(&r, b + 2, l - 2);
                for (int k = 0; k < 4 && r.i + 2 <= r.n; k++) {
                    bd->snd[k] = (u16)r16(&r);
                    if (bd->snd[k]) { u32 f = r8(&r); if (f & 1) r32(&r); if (f & 2) r32(&r); if (f & 4) r16(&r); if (f & 8) r.i += r8(&r) * 8u; }
                }
            }
            break;
        case 18: case 45: /* SoundStreamHead */
            if (l >= 4) {
                stream_fmt = b[1] >> 4;
                stream_rate = (b[1] >> 2) & 3;
                stream_16 = (b[1] >> 1) & 1;
                stream_stereo = b[1] & 1;
                if (stream_fmt == 2) audio_stream_mp3_reset();
            }
            break;
        case 19: /* SoundStreamBlock */
            if (!steps_actions || stream_fmt < 0) break;
            if (stream_fmt == 2 && l > 4) audio_stream_mp3(b + 4, l - 4);
            else if (stream_fmt != 2) {
                u32 fr = 0;
                int rate = 0;
                short *pcm = audio_decode(stream_fmt, stream_rate, stream_16, stream_stereo, 4096, b, l, &fr, &rate);
                if (pcm) audio_stream_push(pcm, fr, rate);
            }
            break;
        case 8: image_set_tables(b, l); break; /* JPEGTables */
        case 9: if (l >= 3) bg_color = 0xFF000000u | (b[0] << 16) | (b[1] << 8) | b[2]; break;
        case 56: { /* ExportAssets */
            rd r;
            rinit(&r, b, l);
            u32 n = r16(&r);
            for (u32 i = 0; i < n && r.i < r.n; i++) {
                u16 id = (u16)r16(&r);
                chars[id].export_name = rstr(&r);
            }
            break;
        }
        default:
            define(code, b, l);
            break;
        }
    }
}

void inst_goto(inst *in, int f, int play) {
    if (!in || in->dead || in->kind != K_SPRITE || !in->tl) return;
    int nf = in->tl->nframes;
    if (f < 1) f = 1;
    if (f > nf) f = nf;
    in->playing = (u8)play;
    if (f == in->frame) return;
    int save = steps_actions;
    if (f < in->frame) {
        inst **old = in->kids;
        int on = in->n;
        inst **saved = amalloc(sizeof(inst *) * (on + 1));
        if (on) memcpy(saved, old, sizeof(inst *) * on);
        in->kids = NULL;
        in->n = in->cap = 0;
        in->pos = 0;
        in->frame = 0;
        inst **prev_old = replay_old;
        int prev_n = replay_n;
        inst *prev_parent = replay_parent;
        replay_old = saved;
        replay_n = on;
        replay_parent = in;
        while (in->frame < f) {
            steps_actions = in->frame + 1 == f;
            tl_step(in);
            if (in->pos >= in->tl->len) break;
        }
        replay_old = prev_old;
        replay_n = prev_n;
        replay_parent = prev_parent;
        for (int i = 0; i < on; i++) {
            inst *o = saved[i];
            if (!o) continue;
            if (o->dynamic) kids_insert(in, o);
            else { o->parent = in; kids_insert(in, o); inst_remove(o); }
        }
    } else {
        while (in->frame < f) {
            steps_actions = in->frame + 1 == f;
            tl_step(in);
            if (in->pos >= in->tl->len && in->frame < f) break;
        }
    }
    steps_actions = save;
}

void inst_swap_depth(inst *in, int depth) {
    if (!in || !in->parent) return;
    inst *p = in->parent, *o = inst_at_depth(p, depth);
    kids_remove(p, in);
    if (o) { kids_remove(p, o); o->depth = in->depth; kids_insert(p, o); }
    in->depth = depth;
    kids_insert(p, in);
}

inst *inst_duplicate(inst *src, const char *name, int depth) {
    if (!src || !src->parent) return NULL;
    inst *o = inst_at_depth(src->parent, depth);
    if (o) inst_remove(o);
    inst *c = inst_create(src->parent, src->id, depth, name, 1);
    if (!c) return NULL;
    c->m = src->m;
    c->cx = src->cx;
    c->visible = src->visible;
    c->nca = src->nca;
    c->ca = src->ca;
    as_clip_constructed(c);
    clip_event(c, 0x01, 0);
    return c;
}

/* ---------------------------------------------------------------- geometry */
mat inst_world(inst *in) {
    mat m = MAT_ID;
    for (inst *p = in; p && p != root; p = p->parent) m = mat_mul(p->m, m);
    return m;
}

static void grow(float *b, mat m, float x, float y) {
    float X = m.a * x + m.c * y + m.tx, Y = m.b * x + m.d * y + m.ty;
    if (X < b[0]) b[0] = X;
    if (X > b[1]) b[1] = X;
    if (Y < b[2]) b[2] = Y;
    if (Y > b[3]) b[3] = Y;
}
static void grow_rect(float *b, mat m, const s32 *r) {
    grow(b, m, (float)r[0], (float)r[2]); grow(b, m, (float)r[1], (float)r[2]);
    grow(b, m, (float)r[0], (float)r[3]); grow(b, m, (float)r[1], (float)r[3]);
}
static void content_bounds(inst *in, mat m, float *b) {
    if (in->dead) return;
    switch (in->kind) {
    case K_SHAPE: if (chars[in->id].p) grow_rect(b, m, ((shape *)chars[in->id].p)->bounds); break;
    case K_TEXT: grow_rect(b, m, ((text_def *)chars[in->id].p)->bounds); break;
    case K_ETEXT: grow_rect(b, m, in->et->bounds); break;
    case K_BUTTON: {
        button_def *bd = chars[in->id].p;
        for (int i = 0; i < bd->nrec; i++) {
            if (!(bd->rec[i].states & 1)) continue;
            inst tmp;
            memset(&tmp, 0, sizeof tmp);
            tmp.id = bd->rec[i].id;
            tmp.kind = chars[tmp.id].type == CH_SHAPE ? K_SHAPE : chars[tmp.id].type == CH_TEXT ? K_TEXT : 99;
            if (tmp.kind != 99) content_bounds(&tmp, mat_mul(m, bd->rec[i].m), b);
            else if (in->btn_kids[i]) content_bounds(in->btn_kids[i], mat_mul(m, bd->rec[i].m), b);
        }
        break;
    }
    case K_SPRITE:
        if (in->dyn && in->dyn->built) grow_rect(b, m, in->dyn->built->bounds);
        for (int i = 0; i < in->n; i++) content_bounds(in->kids[i], mat_mul(m, in->kids[i]->m), b);
        break;
    }
}
void inst_bounds(inst *in, int world, float *x0, float *y0, float *x1, float *y1) {
    float b[4] = {1e30f, -1e30f, 1e30f, -1e30f};
    mat m = world ? inst_world(in) : in->m;
    content_bounds(in, m, b);
    if (b[0] > b[1]) { b[0] = b[1] = m.tx; b[2] = b[3] = m.ty; }
    *x0 = b[0]; *x1 = b[1]; *y0 = b[2]; *y1 = b[3];
}

static int hit_rec(inst *in, mat m, float x, float y, int hit_state) {
    if (in->dead || !in->visible) return 0;
    switch (in->kind) {
    case K_SHAPE: {
        mat iv = mat_inv(m);
        return shape_hit(chars[in->id].p, iv.a * x + iv.c * y + iv.tx, iv.b * x + iv.d * y + iv.ty);
    }
    case K_TEXT: case K_ETEXT: {
        const s32 *r = in->kind == K_TEXT ? ((text_def *)chars[in->id].p)->bounds : in->et->bounds;
        mat iv = mat_inv(m);
        float lx = iv.a * x + iv.c * y + iv.tx, ly = iv.b * x + iv.d * y + iv.ty;
        return lx >= r[0] && lx <= r[1] && ly >= r[2] && ly <= r[3];
    }
    case K_BUTTON: {
        button_def *bd = chars[in->id].p;
        int any = 0;
        for (int i = 0; i < bd->nrec; i++) if (bd->rec[i].states & 8) any = 1;
        for (int i = 0; i < bd->nrec; i++) {
            if (!(bd->rec[i].states & (hit_state && any ? 8 : 1))) continue;
            mat mm = mat_mul(m, bd->rec[i].m);
            if (in->btn_kids[i]) { if (hit_rec(in->btn_kids[i], mm, x, y, 0)) return 1; continue; }
            inst tmp;
            memset(&tmp, 0, sizeof tmp);
            tmp.id = bd->rec[i].id;
            tmp.visible = 1;
            tmp.kind = chars[tmp.id].type == CH_SHAPE ? K_SHAPE : chars[tmp.id].type == CH_TEXT ? K_TEXT : 99;
            if (tmp.kind != 99 && hit_rec(&tmp, mm, x, y, 0)) return 1;
        }
        return 0;
    }
    case K_SPRITE: {
        if (in->dyn && in->dyn->built) {
            mat iv = mat_inv(m);
            if (shape_hit(in->dyn->built, iv.a * x + iv.c * y + iv.tx, iv.b * x + iv.d * y + iv.ty)) return 1;
        }
        for (int i = in->n - 1; i >= 0; i--)
            if (!in->kids[i]->clipdepth && hit_rec(in->kids[i], mat_mul(m, in->kids[i]->m), x, y, hit_state)) return 1;
        return 0;
    }
    }
    return 0;
}
int inst_hit_point(inst *in, float sx, float sy) { return hit_rec(in, inst_world(in), sx, sy, 0); }

/* ---------------------------------------------------------------- drawing API */
dyndraw *dyn_get(inst *in) {
    if (!in->dyn) {
        in->dyn = amalloc(sizeof(dyndraw));
        in->dyn->cap = 64;
        in->dyn->edges = amalloc(sizeof(edge) * 64);
        in->dyn->fills = amalloc(sizeof(fstyle) * 64);
        in->dyn->lines = amalloc(sizeof(lstyle) * 64);
    }
    return in->dyn;
}
void shape_from_dyn(dyndraw *d) {
    if (d->dirty) { d->built = parse_dyn_shape(d); d->dirty = 0; }
}

/* ---------------------------------------------------------------- rendering */
static void render_node(inst *in, mat pm, cxf pc);
static void render_kids(inst *in, mat m, cxf c);

static void render_text(text_def *td, mat m, const cxf *cx) {
    mat tm = mat_mul(m, td->m);
    for (int r = 0; r < td->nrun; r++) {
        textrun *t = &td->run[r];
        font_def *fd = chars[t->font].type == CH_FONT ? chars[t->font].p : NULL;
        float x = t->x;
        for (int i = 0; i < t->n; i++) {
            shape *g = fd && t->glyph[i] < fd->nglyph ? fd->glyph[t->glyph[i]] : NULL;
            if (g) {
                float s = t->height / fd->em;
                mat gm = {s, 0, 0, s, x, t->y};
                draw_shape(g, mat_mul(tm, gm), cx, t->color);
            } else if (fd && fd->code && t->glyph[i] < fd->nglyph) {
                char c[2] = {(char)fd->code[t->glyph[i]], 0};
                draw_bitmap_text(c, tm, x, t->y - t->height * 0.8f, t->height, cx_apply(cx, t->color), 1e6f, 1e6f, 0);
            }
            x += t->adv[i];
        }
    }
}

static void render_node(inst *in, mat pm, cxf pc) {
    if (in->dead || !in->visible || in->clipdepth) return;
    mat m = mat_mul(pm, in->m);
    cxf c = cx_mul(pc, in->cx);
    switch (in->kind) {
    case K_SHAPE: draw_shape(chars[in->id].p, m, &c, 0); break;
    case K_TEXT: render_text(chars[in->id].p, m, &c); break;
    case K_ETEXT: {
        etext_def *e = in->et;
        as_text_sync(in);
        if (e->flags & 0x0008) { /* border: white box with a black frame */
            int x0 = (int)(m.a * e->bounds[0] + m.tx), y0 = (int)(m.d * e->bounds[2] + m.ty);
            int x1 = (int)(m.a * e->bounds[1] + m.tx), y1 = (int)(m.d * e->bounds[3] + m.ty);
            if (!mask_get_active()) {
                video_fill(x0, y0, x1 - x0, y1 - y0, 0xFF000000u);
                video_fill(x0 + 1, y0 + 1, x1 - x0 - 2, y1 - y0 - 2, 0xFFFFFFFFu);
            }
        }
        const char *t = in->text ? in->text : "";
        static char shown[1024];
        u32 n = kstrlen(t);
        if (n > 1000) n = 1000;
        for (u32 i = 0; i < n; i++) shown[i] = (e->flags & 0x1000) ? '*' : t[i]; /* password */
        shown[n] = 0;
        if (in == text_focus && (plat_ms() / 500) % 2 == 0) { shown[n] = '_'; shown[n + 1] = 0; }
        if (*shown)
            draw_bitmap_text(shown, m, (float)e->bounds[0] + 40, (float)e->bounds[2] + 40, e->height,
                             cx_apply(&c, e->color), (float)(e->bounds[1] - e->bounds[0] - 40),
                             (float)(e->bounds[3] - e->bounds[2]), e->align);
        break;
    }
    case K_BUTTON: {
        button_def *bd = chars[in->id].p;
        u8 bit = in->btn_state == 2 ? 4 : in->btn_state == 1 ? 2 : 1;
        for (int i = 0; i < bd->nrec; i++) {
            btnrec *br = &bd->rec[i];
            if (!(br->states & bit)) continue;
            mat mm = mat_mul(m, br->m);
            cxf cc = cx_mul(c, br->cx);
            if (in->btn_kids[i]) { render_node(in->btn_kids[i], mm, cc); continue; }
            if (chars[br->id].type == CH_SHAPE) draw_shape(chars[br->id].p, mm, &cc, 0);
            else if (chars[br->id].type == CH_TEXT) render_text(chars[br->id].p, mm, &cc);
        }
        break;
    }
    case K_SPRITE:
        if (in->dyn) { shape_from_dyn(in->dyn); if (in->dyn->built) draw_shape(in->dyn->built, m, &c, 0); }
        render_kids(in, m, c);
        break;
    }
}

/* children in depth order; a child with clipdepth is a mask for the
 * children above it up to that depth */
static int mask_level;
static void render_kids(inst *in, mat m, cxf c) {
    u8 *outer = mask_get_active();
    int limit = -100000; /* depth up to which the current mask applies */
    u8 *cur = NULL;
    for (int i = 0; i < in->n; i++) {
        inst *k = in->kids[i];
        if (cur && k->depth > limit) { mask_set_active(outer); cur = NULL; mask_level--; }
        if (k->clipdepth) {
            if (k->dead || !k->visible) continue;
            if (cur) { mask_set_active(outer); cur = NULL; mask_level--; }
            u8 *buf = mask_begin_write(mask_level);
            if (!buf) continue; /* too many nested masks: draw unmasked */
            mask_level++;
            mask_set_write(buf);
            u16 cd = k->clipdepth;
            k->clipdepth = 0;
            render_node(k, m, c);
            k->clipdepth = cd;
            mask_set_write(NULL);
            mask_set_active(buf);
            cur = buf;
            limit = (int)cd - 16384;
            continue;
        }
        render_node(k, m, c);
    }
    if (cur) { mask_set_active(outer); mask_level--; }
}

/* ---------------------------------------------------------------- tree walk */
static inst **flat;
static int nflat, capflat;
static void collect(inst *in) {
    if (in->dead) return;
    if (nflat == capflat) {
        int nc = capflat ? capflat * 2 : 256;
        inst **f = amalloc(sizeof(inst *) * nc);
        if (!f) return;
        if (nflat) memcpy(f, flat, sizeof(inst *) * nflat);
        flat = f;
        capflat = nc;
    }
    flat[nflat++] = in;
    for (int i = 0; i < in->n; i++) collect(in->kids[i]);
    if (in->kind == K_BUTTON && in->btn_kids) {
        button_def *bd = chars[in->id].p;
        for (int i = 0; i < bd->nrec; i++) if (in->btn_kids[i]) collect(in->btn_kids[i]);
    }
}
static int walk(void) { nflat = 0; collect(root); return nflat; }

/* ---------------------------------------------------------------- events */
static void clip_event(inst *in, u32 mask, int key) {
    for (int i = 0; i < in->nca && !in->dead; i++) {
        clipaction *c = &in->ca[i];
        if (!(c->flags & mask)) continue;
        if ((mask & 0x20000) && c->key != key) continue;
        as_run(c->code, c->len, in);
    }
}

static void all_clips(u32 mask, const char *handler, int key) {
    int n = walk();
    for (int i = 0; i < n; i++) {
        inst *in = flat[i];
        if (in->dead || in->kind != K_SPRITE) continue;
        if (in->nca) clip_event(in, mask, key);
        if (handler && in->o) as_call_handler(in, handler, 0, 0);
    }
}

static const u32 BTN_MASK = 0x0400 | 0x0800 | 0x1000 | 0x2000 | 0x4000 | 0x8000 | 0x10000;
static int clip_is_button(inst *in) {
    if (in == root || !in->enabled) return 0;
    for (int i = 0; i < in->nca; i++) if (in->ca[i].flags & BTN_MASK) return 1;
    return as_has_handler(in, "onPress") || as_has_handler(in, "onRelease") ||
           as_has_handler(in, "onRollOver") || as_has_handler(in, "onRollOut") ||
           as_has_handler(in, "onReleaseOutside");
}

static inst *mouse_target(inst *in, mat m, float x, float y) {
    if (in->dead || !in->visible) return NULL;
    if (in->kind == K_BUTTON) return in->enabled && hit_rec(in, m, x, y, 1) ? in : NULL;
    if (in->kind != K_SPRITE) return NULL;
    if (clip_is_button(in)) return hit_rec(in, m, x, y, 0) ? in : NULL;
    for (int i = in->n - 1; i >= 0; i--) {
        inst *k = in->kids[i];
        if (k->clipdepth) continue;
        inst *t = mouse_target(k, mat_mul(m, k->m), x, y);
        if (t) return t;
    }
    return NULL;
}

static void fire(inst *t, u16 cond, u32 clipflag, const char *handler) {
    if (!t || t->dead) return;
    if (t->kind == K_BUTTON) {
        button_def *bd = chars[t->id].p;
        int si = cond == 0x02 ? 0 : cond == 0x01 ? 1 : cond == 0x04 ? 2 : cond == 0x08 ? 3 : -1;
        if (si >= 0 && bd->snd[si]) snd_play(bd->snd[si], 1, 256, 0);
        for (int i = 0; i < bd->nact; i++)
            if (bd->act[i].cond & cond) as_run(bd->act[i].code, bd->act[i].len, t->parent);
    } else {
        clip_event(t, clipflag, 0);
    }
    as_call_handler(t, handler, 0, 0);
}

static inst *hot, *pressed;
static int mouse_down, last_mx = -1, last_my = -1;
static inst *drag;
static int drag_lock, drag_constrain;
static float drag_ox, drag_oy, drag_r[4];

void start_drag(inst *in, int lock, int constrain, float x0, float y0, float x1, float y1) {
    drag = in;
    drag_lock = lock;
    drag_constrain = constrain;
    drag_r[0] = x0; drag_r[1] = y0; drag_r[2] = x1; drag_r[3] = y1;
    mat pw = in->parent ? inst_world(in->parent) : MAT_ID;
    mat iv = mat_inv(pw);
    float mx = mouse_stage_x * 20.0f, my = mouse_stage_y * 20.0f;
    float lx = iv.a * mx + iv.c * my + iv.tx, ly = iv.b * mx + iv.d * my + iv.ty;
    drag_ox = lock ? 0 : in->m.tx - lx;
    drag_oy = lock ? 0 : in->m.ty - ly;
}
void stop_drag(void) { drag = NULL; }

static void update_drag(void) {
    if (!drag) return;
    if (drag->dead) { drag = NULL; return; }
    mat pw = drag->parent ? inst_world(drag->parent) : MAT_ID;
    mat iv = mat_inv(pw);
    float mx = mouse_stage_x * 20.0f, my = mouse_stage_y * 20.0f;
    float x = iv.a * mx + iv.c * my + iv.tx + drag_ox, y = iv.b * mx + iv.d * my + iv.ty + drag_oy;
    if (drag_constrain) {
        if (x < drag_r[0] * 20) x = drag_r[0] * 20;
        if (x > drag_r[2] * 20) x = drag_r[2] * 20;
        if (y < drag_r[1] * 20) y = drag_r[1] * 20;
        if (y > drag_r[3] * 20) y = drag_r[3] * 20;
    }
    drag->m.tx = x;
    drag->m.ty = y;
    drag->as_moved = 1;
}

/* Flash button key codes for on(keyPress "<Left>") etc. */
static int btn_key(int code, int ascii) {
    switch (code) {
    case 37: return 1; case 39: return 2; case 36: return 3; case 35: return 4;
    case 45: return 5; case 46: return 6; case 8: return 8; case 13: return 13;
    case 38: return 14; case 40: return 15; case 33: return 16; case 34: return 17;
    case 9: return 18; case 27: return 19;
    }
    return ascii;
}

static int sx_mouse, sy_mouse;

inst *text_focus;
void text_set_focus(inst *in) { text_focus = (in && in->kind == K_ETEXT && !(in->et->flags & 0x0800)) ? in : NULL; }

static void text_store(inst *in) {
    if (in->et->var && *in->et->var) {
        /* keep the bound variable in step with what was typed */
        val v = vstr(in->text);
        obj *po = as_clip_obj(in->parent ? in->parent : root);
        (void)po;
        as_set_var_path(in->parent ? in->parent : root, in->et->var, v);
    }
}

static void text_type(int code, int ascii) {
    inst *t = text_focus;
    if (!t || t->dead) { text_focus = NULL; return; }
    u32 n = t->text ? kstrlen(t->text) : 0;
    char *nt = amalloc(n + 2);
    if (!nt) return;
    if (n) memcpy(nt, t->text, n);
    if (code == 8) { if (n) n--; }
    else if (code == 13) { if (t->et->flags & 0x2000) nt[n++] = '\n'; else return; }
    else if (ascii >= 32 && ascii < 127) { if (t->et->maxlen && (int)n >= t->et->maxlen) return; nt[n++] = (char)ascii; }
    else return;
    nt[n] = 0;
    t->text = nt;
    text_store(t);
}

/* topmost editable text field under the mouse */
static inst *text_hit(inst *in, mat m, float x, float y) {
    if (in->dead || !in->visible) return NULL;
    if (in->kind == K_ETEXT) return (!(in->et->flags & 0x0800) && hit_rec(in, m, x, y, 0)) ? in : NULL;
    if (in->kind != K_SPRITE) return NULL;
    for (int i = in->n - 1; i >= 0; i--) {
        inst *r = text_hit(in->kids[i], mat_mul(m, in->kids[i]->m), x, y);
        if (r) return r;
    }
    return NULL;
}

/* ---- volume keys: F9 mute, F10 down, F11 up, with a pixel volume bar ---- */
static u32 osd_until;
static int osd_shown;
static void draw_volume_osd(void) {
    int v = audio_get_master(), muted = audio_get_muted();
    int x = scr_w - 260, y = 20;
    video_fb_rect(x - 6, y - 6, 252, 44, 0xFF000000u);
    video_fb_rect(x - 4, y - 4, 248, 40, 0xFFFF2020u);
    video_fb_rect(x - 2, y - 2, 244, 36, 0xFF000000u);
    /* speaker */
    video_fb_rect(x + 4, y + 10, 8, 12, 0xFFFF2020u);
    for (int i = 0; i < 8; i++) video_fb_rect(x + 12 + i, y + 10 - i, 1, 12 + i * 2, 0xFFFF2020u);
    if (muted) for (int i = 0; i < 12; i++) { video_fb_rect(x + 24 + i, y + 10 + i, 3, 3, 0xFFFFFFFFu); video_fb_rect(x + 35 - i, y + 10 + i, 3, 3, 0xFFFFFFFFu); }
    for (int i = 0; i < 10; i++) {
        int on = !muted && v * 10 >= (i + 1) * 256 - 128;
        video_fb_rect(x + 48 + i * 18, y + 4, 14, 24, on ? 0xFFFF2020u : 0xFF401010u);
    }
    osd_until = plat_ms() + 1500;
    osd_shown = 1;
}
static void osd_tick(void) {
    if (osd_shown && (s32)(plat_ms() - osd_until) > 0) { video_present(scr_w - 266, 14, 266, 46); osd_shown = 0; }
}
static void draw_volume_osd_keep(void) { u32 t = osd_until; draw_volume_osd(); osd_until = t; }
static int volume_key(int code) {
    if (code == 120) audio_set_muted(!audio_get_muted());
    else if (code == 121) { audio_set_muted(0); audio_set_master(audio_get_master() - 26); }
    else if (code == 122) { audio_set_muted(0); audio_set_master(audio_get_master() + 26); }
    else return 0;
    draw_volume_osd();
    return 1;
}

static void handle_input(void) {
    int code, down, ascii;
    while (plat_kevent(&code, &down, &ascii)) {
        if (code >= 120 && code <= 122) { if (down) volume_key(code); continue; }
        if (down && (code == FK_F12 || (code == 81 && plat_kdown(17)) || (code == 27 && !interactive))) {
            quit_requested = 1;
            return;
        }
        if (down && text_focus) text_type(code, ascii);
        if (down) {
            last_key_code = code;
            last_key_ascii = ascii;
            all_clips(0x40, NULL, 0);
            as_listeners("Key", "onKeyDown");
            int bk = btn_key(code, ascii);
            all_clips(0x20000, NULL, bk);
            int n = walk();
            for (int i = 0; i < n; i++) {
                inst *b = flat[i];
                if (b->dead || b->kind != K_BUTTON) continue;
                button_def *bd = chars[b->id].p;
                for (int k = 0; k < bd->nact; k++)
                    if ((bd->act[k].cond >> 9) == bk && bk) as_run(bd->act[k].code, bd->act[k].len, b->parent);
            }
        } else {
            last_key_code = code;
            all_clips(0x80, NULL, 0);
            as_listeners("Key", "onKeyUp");
        }
        as_flush_queue();
    }

    int mx, my, mb;
    plat_mouse(&mx, &my, &mb);
    sx_mouse = mx;
    sy_mouse = my;
    mat iv = mat_inv(base);
    float tx = iv.a * mx + iv.c * my + iv.tx, ty = iv.b * mx + iv.d * my + iv.ty;
    mouse_stage_x = (int)(tx / 20);
    mouse_stage_y = (int)(ty / 20);
    if (mx != last_mx || my != last_my) {
        last_mx = mx;
        last_my = my;
        update_drag();
        all_clips(0x08, "onMouseMove", 0);
        as_listeners("Mouse", "onMouseMove");
    }
    inst *t = mouse_target(root, MAT_ID, tx, ty);
    if (t != hot) {
        if (!pressed) {
            fire(hot, 0x02, 0x4000, "onRollOut");
            fire(t, 0x01, 0x2000, "onRollOver");
        } else if (hot == pressed) {
            fire(pressed, 0x10, 0x10000, "onDragOut");
        } else if (t == pressed) {
            fire(pressed, 0x20, 0x8000, "onDragOver");
        }
        hot = t;
    }
    int is_down = mb & 1;
    if (is_down && !mouse_down) {
        mouse_down = 1;
        text_set_focus(text_hit(root, MAT_ID, tx, ty));
        all_clips(0x10, "onMouseDown", 0);
        as_listeners("Mouse", "onMouseDown");
        if (hot) { pressed = hot; fire(hot, 0x04, 0x0400, "onPress"); }
    } else if (!is_down && mouse_down) {
        mouse_down = 0;
        all_clips(0x20, "onMouseUp", 0);
        as_listeners("Mouse", "onMouseUp");
        if (pressed) {
            if (pressed == hot) fire(pressed, 0x08, 0x0800, "onRelease");
            else fire(pressed, 0x40, 0x1000, "onReleaseOutside");
            pressed = NULL;
        }
    }
    /* button visual states */
    int n = walk();
    for (int i = 0; i < n; i++) {
        inst *b = flat[i];
        if (b->kind != K_BUTTON) continue;
        b->btn_state = (u8)(b == hot ? (b == pressed ? 2 : 1) : (b == pressed ? 1 : 0));
    }
    as_flush_queue();
}

/* ---------------------------------------------------------------- frame */
static void advance_all(void) {
    int n = walk();
    for (int i = 0; i < n; i++) {
        inst *in = flat[i];
        if (in->dead || in->kind != K_SPRITE || !in->tl) continue;
        if (in->fresh) { in->fresh = 0; continue; }
        if (!in->playing) continue;
        if (in->frame >= in->tl->nframes) {
            if (in->tl->nframes > 1) inst_goto(in, 1, 1);
        } else {
            tl_step(in);
        }
    }
    as_flush_queue();
    n = walk();
    for (int i = 0; i < n; i++) {
        inst *in = flat[i];
        if (in->dead || in->kind != K_SPRITE) continue;
        if (in->nca) clip_event(in, 0x02, 0);
        if (in->o) as_call_handler(in, "onEnterFrame", 0, 0);
    }
    as_flush_queue();
    as_intervals();
    as_flush_queue();
}

int swf_play(const u8 *data, u32 size, const char **err) {
    u32 mark = heap_mark();
    *err = NULL;
    const u8 *swf = data;
    u32 total = size;
    if (size < 8) { *err = "file too small"; return -1; }
    swf_ver = data[3];
    if (data[0] == 'Z') { *err = "LZMA-compressed SWF (ZWS) is not supported"; return -1; }

    u32 hsize;
    a_base = plat_heap2(&hsize);
    if (a_base && hsize) { a_top = a_base; a_end = a_base + hsize; }
    else { a_base = a_top = a_end = NULL; }

    if (data[0] == 'C') {
        total = data[4] | (data[5] << 8) | (data[6] << 16) | ((u32)data[7] << 24);
        u8 *buf = total < 0x08000000 ? amalloc(total) : NULL;
        u32 got = 0;
        if (!buf || size < 10) { *err = "not enough memory to decompress"; goto out; }
        memcpy(buf, data, 8);
        inflate_raw(data + 10, size - 10, buf + 8, total - 8, &got);
        if (got < 16) { *err = "decompression failed"; goto out; }
        total = got + 8;
        swf = buf;
    }
    {
        rd r;
        rinit(&r, swf + 8, total - 8);
        s32 st[4];
        rrect(&r, st);
        u32 rate = r16(&r);
        r16(&r);
        float fps = (rate >> 8) + (rate & 255) / 256.0f;
        if (fps < 1) fps = 12;
        if (fps > 120) fps = 120;
        u32 frame_ms = (u32)(int)(1000.0f / fps);

        chars = amalloc(sizeof(chr) * 65536);
        init_done = amalloc(sizeof(u8 *) * 512);
        if (!chars || !init_done) { *err = "not enough memory"; goto out; }
        n_init_done = 0;
        bg_color = 0xFFFFFFFFu;
        quit_requested = 0;
        interactive = 0;
        mouse_visible = 1;
        hot = pressed = drag = NULL;
        text_focus = NULL;
        mouse_down = 0;
        last_mx = last_my = -1;
        flat = NULL;
        nflat = capflat = 0;

        float sw = (st[1] - st[0]) / 20.0f, sh = (st[3] - st[2]) / 20.0f;
        if (sw < 1) sw = 550;
        if (sh < 1) sh = 400;
        stage_w = (int)sw;
        stage_h = (int)sh;
        float s = scr_w / sw < scr_h / sh ? scr_w / sw : scr_h / sh;
        int vw = (int)(sw * s), vh = (int)(sh * s);
        int vx = (scr_w - vw) / 2, vy = (scr_h - vh) / 2;
        mat b = {s / 20.0f, 0, 0, s / 20.0f, vx - st[0] * s / 20.0f, vy - st[2] * s / 20.0f};
        base = b;

        timeline *tl = make_timeline(swf + 8 + r.i, total - 8 - r.i);
        audio_init();
        audio_stop_all();
        stream_fmt = -1;
        as_init();
        root = inst_alloc();
        root->kind = K_SPRITE;
        root->tl = tl;
        root->m = MAT_ID;
        root->cx = CX_ID;
        root->visible = 1;
        root->enabled = 1;
        root->playing = 1;
        root->name = "_level0";
        tl_step(root);
        as_flush_queue();

        plat_flush_input();
        plat_mouse_set(scr_w / 2, scr_h / 2);
        video_fill(0, 0, scr_w, scr_h, 0xFF000000u);
        video_present(0, 0, scr_w, scr_h);
        int first = 1, skipped = 0;
        while (!quit_requested) {
            u32 t0 = plat_ms();
            handle_input();
            if (quit_requested) break;
            if (!first) advance_all();
            first = 0;
            /* streamed music falling behind (slow PC): advance the movie
             * without drawing it, like Flash's frame dropping */
            audio_pump();
            if (stream_fmt >= 0 && audio_stream_queued() < (u32)audio_rate() / 4 && skipped < 8) {
                skipped++;
                continue;
            }
            skipped = 0;
            video_fill(vx, vy, vw, vh, bg_color);
            render_node(root, base, CX_ID);
            erase_cursor();
            video_present(vx, vy, vw, vh);
            audio_pump();
            if (osd_shown) draw_volume_osd_keep();
            int show = interactive && mouse_visible;
            if (show) draw_cursor(sx_mouse, sy_mouse, hot && hot->hand);
            /* streamed music sets the pace, like Flash: if the music is about
             * to run dry, play the next frame now instead of waiting */
            int hurry = stream_fmt >= 0 && audio_stream_queued() < (u32)audio_rate() / 4;
            while (!hurry && plat_ms() - t0 < frame_ms && !quit_requested) {
                int mx, my, mb;
                plat_mouse(&mx, &my, &mb);
                if (show && (mx != sx_mouse || my != sy_mouse)) {
                    sx_mouse = mx;
                    sy_mouse = my;
                    draw_cursor(mx, my, hot && hot->hand);
                }
                if (plat_kdown(FK_F12)) quit_requested = 1;
                audio_pump();
                osd_tick();
            }
        }
        erase_cursor();
    }
out:
    audio_halt();
    plat_flush_input();
    a_base = a_top = a_end = NULL;
    heap_reset(mark);
    return 0;
}
