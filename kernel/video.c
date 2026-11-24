/* FlashKernel video: back buffer, framebuffer presentation and text console.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 */
#include "kernel.h"

int scr_w, scr_h;
static u32 *bb;
static u8 *fb;
static int pitch, bpp;

u32 *video_bb(void) { return bb; }

void video_init(void) {
    bootinfo_t *bi = plat_bootinfo();
    scr_w = bi->w;
    scr_h = bi->h;
    pitch = bi->pitch;
    bpp = bi->bpp;
    fb = plat_fb();
    bb = kmalloc((u32)scr_w * scr_h * 4);
    memset(bb, 0, (u32)scr_w * scr_h * 4);
}

void video_present(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > scr_w) w = scr_w - x;
    if (y + h > scr_h) h = scr_h - y;
    if (w <= 0 || h <= 0) return;
    for (int r = y; r < y + h; r++) {
        u32 *src = bb + r * scr_w + x;
        u8 *dst = fb + r * pitch + x * (bpp / 8);
        if (bpp == 32) {
            memcpy(dst, src, (u32)w * 4);
        } else {
            for (int i = 0; i < w; i++) {
                u32 c = src[i];
                dst[0] = (u8)c;
                dst[1] = (u8)(c >> 8);
                dst[2] = (u8)(c >> 16);
                dst += 3;
            }
        }
    }
}

/* draw straight into the framebuffer (used for the mouse cursor) */
void video_fb_rect(int x, int y, int w, int h, u32 c) {
    for (int r = y; r < y + h; r++) {
        if (r < 0 || r >= scr_h) continue;
        for (int i = x; i < x + w; i++) {
            if (i < 0 || i >= scr_w) continue;
            u8 *d = fb + r * pitch + i * (bpp / 8);
            if (bpp == 32) *(u32 *)d = c;
            else { d[0] = (u8)c; d[1] = (u8)(c >> 8); d[2] = (u8)(c >> 16); }
        }
    }
}

void video_fill(int x, int y, int w, int h, u32 c) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > scr_w) w = scr_w - x;
    if (y + h > scr_h) h = scr_h - y;
    for (int r = y; r < y + h; r++) {
        u32 *p = bb + r * scr_w + x;
        for (int i = 0; i < w; i++) p[i] = c;
    }
}

/* ---- text console: BIOS 8x16 font drawn at 2x (16x32 cells), FlashOS red ---- */
#define CW 16
#define CH 32
#define OX 24
#define OY 14
#define FG 0xFFFF1010u
#define MAXC 128
#define MAXR 64

static char grid[MAXR][MAXC];
static int cols, rows, cx, cy, deferred, used;
static const u8 *font;

int con_cols(void) { return cols; }
int con_row_y(int row) { return OY + row * CH; }
int con_rows_used(void) { return used; }
void con_defer(int on) { deferred = on; }

static void draw_cell(int c, int r) {
    int px = OX + c * CW, py = OY + r * CH;
    video_fill(px, py, CW, CH, 0xFF000000u);
    const u8 *g = font + (u8)grid[r][c] * 16;
    for (int y = 0; y < 16; y++) {
        u8 bits = g[y];
        if (!bits) continue;
        for (int x = 0; x < 8; x++)
            if (bits & (0x80 >> x)) video_fill(px + x * 2, py + y * 2, 2, 2, FG);
    }
    if (!deferred) video_present(px, py, CW, CH);
}

void con_cursor(int on) {
    if (cx >= cols || cy >= rows) return;
    int px = OX + cx * CW, py = OY + cy * CH;
    draw_cell(cx, cy);
    if (on) video_fill(px + 1, py + CH - 6, CW - 2, 4, FG);
    if (!deferred) video_present(px, py, CW, CH);
}

void con_init(void) {
    font = plat_font();
    cols = (scr_w - OX * 2) / CW;
    rows = (scr_h - OY) / CH;
    if (cols > MAXC) cols = MAXC;
    if (rows > MAXR) rows = MAXR;
    con_clear();
}

void con_clear(void) {
    memset(grid, ' ', sizeof grid);
    cx = cy = used = 0;
    video_fill(0, 0, scr_w, scr_h, 0xFF000000u);
    if (!deferred) video_present(0, 0, scr_w, scr_h);
}

static void scroll(void) {
    memmove(grid[0], grid[1], (u32)(rows - 1) * MAXC);
    memset(grid[rows - 1], ' ', MAXC);
    cy = rows - 1;
    int d = deferred;
    deferred = 1;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) draw_cell(c, r);
    deferred = d;
    if (!deferred) video_present(0, 0, scr_w, scr_h);
}

void con_putc(char ch) {
    if (ch == '\n') {
        cx = 0;
        if (++cy >= rows) scroll();
    } else if (ch == '\r') {
        cx = 0;
    } else if (ch == '\b') {
        if (cx > 0) { cx--; grid[cy][cx] = ' '; draw_cell(cx, cy); }
    } else {
        if (cx >= cols) { cx = 0; if (++cy >= rows) scroll(); }
        grid[cy][cx] = ch;
        draw_cell(cx, cy);
        cx++;
    }
    if (cy + 1 > used) used = cy + 1;
}

void con_puts(const char *s) { while (*s) con_putc(*s++); }
