/* FlashOS host simulator: runs the real FlashKernel code on a normal PC
 * with a fake framebuffer, scripted keyboard/mouse and a virtual clock, and
 * writes screenshots. Used to test the kernel without an emulator.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Script lines:  key   <ms> <text>        typed into the shell (\n Enter, \u Up, \d Down)
 *                kd    <ms> <flashcode>   key down (Flash key code)
 *                ku    <ms> <flashcode>   key up
 *                mouse <ms> <x> <y>       move mouse (screen pixels)
 *                mdown <ms>  /  mup <ms>  left button
 *                shot  <ms> <name>
 *                end   <ms>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"

void kmain(void);

#define W 1024
#define H 768
static bootinfo_t bi = {0, W * 4, W, H, 32, 0, 0, 256u << 20};
static u8 *fbmem, *fsdata, font[4096];
static u32 now;

typedef struct { u32 t; char kind[8]; char text[128]; int done; } ev;
static ev evs[512];
static int nev;
static int cq[512], cq_h, cq_t;
static struct { int code, down, ascii; } kq[256];
static int kq_h, kq_t;
static u8 kstate[256];
static int mx = 512, my = 384, mb;

static void shot(const char *name) {
    char path[256];
    snprintf(path, sizeof path, "shot_%s.ppm", name);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", W, H);
    for (int i = 0; i < W * H; i++) {
        u8 *p = fbmem + i * 4;
        fputc(p[2], f); fputc(p[1], f); fputc(p[0], f);
    }
    fclose(f);
    printf("  [%6u ms] %s\n", now, path);
}

static void kev(int code, int down, int ascii) {
    kstate[code & 255] = (u8)down;
    kq[kq_t % 256].code = code; kq[kq_t % 256].down = down; kq[kq_t % 256].ascii = ascii;
    kq_t++;
}

static void advance(u32 ms) {
    now += ms;
    for (int i = 0; i < nev; i++) {
        ev *e = &evs[i];
        if (e->done || now < e->t) continue;
        e->done = 1;
        if (!strcmp(e->kind, "shot")) shot(e->text);
        else if (!strcmp(e->kind, "end")) {
            extern u64 host_render_ns, host_frames, host_as_ns;
            if (host_frames) printf("  render: %llu frames, %.2f ms per frame, ActionScript %.2f ms per frame\n", (unsigned long long)host_frames, host_render_ns / 1e6 / host_frames, host_as_ns / 1e6 / host_frames);
            printf("  [%6u ms] end\n", now);
            exit(0);
        }
        else if (!strcmp(e->kind, "key")) {
            for (char *c = e->text; *c; c++) {
                int ch = (u8)*c;
                if (ch == '\\' && c[1] == 'n') { ch = '\n'; c++; }
                else if (ch == '\\' && c[1] == 'u') { ch = KEY_UP; c++; }
                else if (ch == '\\' && c[1] == 'd') { ch = KEY_DOWN; c++; }
                cq[cq_t++ % 512] = ch;
            }
        } else if (!strcmp(e->kind, "type")) { /* key events for a running SWF */
            for (char *c = e->text; *c; c++) {
                int ch = (u8)*c, code;
                if (ch == '\\' && c[1] == 'n') { ch = 13; c++; }
                code = ch >= 'a' && ch <= 'z' ? ch - 32 : ch;
                kev(code, 1, ch);
                kev(code, 0, 0);
            }
        } else if (!strcmp(e->kind, "kd")) {
            int code = atoi(e->text);
            kev(code, 1, (code >= 65 && code <= 90) ? code + 32 : code);
        } else if (!strcmp(e->kind, "ku")) {
            kev(atoi(e->text), 0, 0);
        } else if (!strcmp(e->kind, "reset")) { /* restart render statistics */
            extern u64 host_render_ns, host_frames, host_as_ns;
            host_render_ns = host_frames = host_as_ns = 0;
        } else if (!strcmp(e->kind, "mouse")) {
            sscanf(e->text, "%d %d", &mx, &my);
        } else if (!strcmp(e->kind, "mdown")) mb = 1;
        else if (!strcmp(e->kind, "mup")) mb = 0;
    }
}

void plat_init(void) {}
bootinfo_t *plat_bootinfo(void) { return &bi; }
u8 *plat_fb(void) { return fbmem; }
const u8 *plat_font(void) { return font; }
const u8 *plat_fs(void) { return fsdata; }
static u8 *usbdata;
const u8 *plat_usbfs(void) { return usbdata; }
void *plat_heap(u32 *size) { *size = 24u << 20; return malloc(*size); }
static void *heap2;
void *plat_heap2(u32 *size) {
    *size = 200u << 20;
    if (!heap2) heap2 = malloc(*size);
    return heap2;
}
u32 plat_ms(void) { advance(1); return now; }
const char *plat_clock_name(void) { return "simulator"; }
void plat_sleep(u32 ms) { advance(ms); }
static u32 last_key;
int plat_key(void) {
    advance(1);
    kq_h = kq_t;
    if (cq_h == cq_t || now - last_key < 70) return -1;
    last_key = now;
    return cq[cq_h++ % 512];
}
void plat_poll(void) {}
int plat_kevent(int *code, int *down, int *ascii) {
    if (kq_h == kq_t) return 0;
    *code = kq[kq_h % 256].code; *down = kq[kq_h % 256].down; *ascii = kq[kq_h % 256].ascii;
    kq_h++;
    return 1;
}
int plat_kdown(int code) { return code >= 0 && code < 256 && kstate[code]; }
void plat_mouse(int *x, int *y, int *b) { *x = mx; *y = my; *b = mb; }
void plat_mouse_set(int x, int y) { (void)x; (void)y; }
void plat_flush_input(void) { cq_h = cq_t; kq_h = kq_t; memset(kstate, 0, sizeof kstate); }
void plat_rtc(int *h, int *m, int *s, int *day, int *mon, int *year) { *h = 17; *m = 30; *s = 0; *day = 24; *mon = 11; *year = 2026; }
void plat_reboot(void) { printf("reboot\n"); exit(0); }
void plat_poweroff(void) { printf("poweroff\n"); }
void plat_halt(void) { for (;;) advance(10); }

int main(int argc, char **argv) {
    if (getenv("SIM_INSTALLED")) bi.flags = 1; /* boot as if installed on a hard disk */
    fbmem = calloc(W * H, 4);
    FILE *f = fopen("font.bin", "rb");
    if (!f || fread(font, 1, 4096, f) != 4096) { fprintf(stderr, "font.bin missing\n"); return 1; }
    fclose(f);
    f = fopen("flashfs.bin", "rb");
    fsdata = malloc(64 << 20);
    size_t n = fread(fsdata, 1, 64 << 20, f);
    fclose(f);
    bi.fs_bytes = (u32)n;
    f = fopen("usbfs.bin", "rb");
    if (f) { usbdata = malloc(64 << 20); bi.usb_bytes = (u32)fread(usbdata, 1, 64 << 20, f); fclose(f); }
    f = fopen(argc > 1 ? argv[1] : "script.txt", "r");
    char line[256];
    while (f && fgets(line, sizeof line, f) && nev < 512) {
        char kind[8], rest[160] = "";
        unsigned t;
        line[strcspn(line, "\r\n")] = 0;
        if (sscanf(line, "%7s %u %159[^\n]", kind, &t, rest) < 2) continue;
        evs[nev].t = t;
        strcpy(evs[nev].kind, kind);
        strcpy(evs[nev].text, rest);
        nev++;
    }
    kmain();
    return 0;
}
