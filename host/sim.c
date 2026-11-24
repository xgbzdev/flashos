/* FlashOS host simulator: runs the real FlashKernel code on a normal PC
 * with a fake framebuffer, scripted keyboard and virtual clock, and writes
 * screenshots. Used to test the kernel without an emulator.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Script lines:  key <ms> <text>   (\n = Enter, \e = Esc)
 *                shot <ms> <name>
 *                end <ms>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"

void kmain(void);

#define W 1024
#define H 768
static bootinfo_t bi = {0, W * 4, W, H, 32, 0, 0};
static u8 *fbmem, *fsdata, font[4096];
static u32 now;

typedef struct { u32 t; char kind; char text[128]; int done; } ev;
static ev evs[256];
static int nev;
static char keyq[512];
static int kq_head, kq_tail;

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

static void advance(u32 ms) {
    now += ms;
    for (int i = 0; i < nev; i++) {
        ev *e = &evs[i];
        if (e->done || now < e->t) continue;
        e->done = 1;
        if (e->kind == 's') shot(e->text);
        else if (e->kind == 'e') { printf("  [%6u ms] end\n", now); exit(0); }
        else if (e->kind == 'k') {
            for (char *c = e->text; *c; c++) {
                char ch = *c;
                if (ch == '\\' && c[1] == 'n') { ch = '\n'; c++; }
                else if (ch == '\\' && c[1] == 'e') { ch = 27; c++; }
                keyq[kq_tail++ % 512] = ch;
            }
        }
    }
}

void plat_init(void) {}
bootinfo_t *plat_bootinfo(void) { return &bi; }
u8 *plat_fb(void) { return fbmem; }
const u8 *plat_font(void) { return font; }
const u8 *plat_fs(void) { return fsdata; }
void *plat_heap(u32 *size) { *size = 24u << 20; return malloc(*size); }
u32 plat_ms(void) { advance(1); return now; }
void plat_sleep(u32 ms) { advance(ms); }
static u32 last_key;
int plat_key(void) {
    advance(1);
    if (kq_head == kq_tail || now - last_key < 70) return -1; /* typing speed */
    last_key = now;
    return (u8)keyq[kq_head++ % 512];
}
void plat_reboot(void) { printf("reboot\n"); exit(0); }
void plat_poweroff(void) { printf("poweroff\n"); }
void plat_halt(void) { for (;;) advance(10); }

int main(int argc, char **argv) {
    fbmem = calloc(W * H, 4);
    FILE *f = fopen("font.bin", "rb");
    if (!f || fread(font, 1, 4096, f) != 4096) { fprintf(stderr, "font.bin missing\n"); return 1; }
    fclose(f);
    f = fopen("flashfs.bin", "rb");
    fsdata = malloc(64 << 20);
    size_t n = fread(fsdata, 1, 64 << 20, f);
    fclose(f);
    bi.fs_bytes = (u32)n;
    f = fopen(argc > 1 ? argv[1] : "script.txt", "r");
    char line[256];
    while (f && fgets(line, sizeof line, f) && nev < 256) {
        char kind[16], rest[160] = "";
        unsigned t;
        line[strcspn(line, "\r\n")] = 0;
        if (sscanf(line, "%15s %u %159[^\n]", kind, &t, rest) < 2) continue;
        evs[nev].t = t;
        evs[nev].kind = kind[0];
        strcpy(evs[nev].text, rest);
        nev++;
    }
    kmain();
    return 0;
}
