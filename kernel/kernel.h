/* FlashKernel - the FlashOS kernel.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 */
#ifndef FLASH_KERNEL_H
#define FLASH_KERNEL_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef short s16;
typedef int s32;

#ifdef HOSTED
#include <string.h>
#else
#define NULL ((void *)0)
void *memcpy(void *d, const void *s, u32 n);
void *memset(void *d, int c, u32 n);
void *memmove(void *d, const void *s, u32 n);
int memcmp(const void *a, const void *b, u32 n);
#endif

#define KERNEL_NAME "FlashKernel"
#define KERNEL_VERSION "1.0"
#define OS_VERSION "1.0"

/* ---- lib.c ---- */
u32 kstrlen(const char *s);
int kstricmp(const char *a, const char *b);
int kstrnicmp(const char *a, const char *b, u32 n);
char kupper(char c);
void kstrcpy(char *d, const char *s, u32 max);
void kutoa(u32 v, char *out);
void *kmalloc(u32 n);
u32 heap_mark(void);
void heap_reset(u32 mark);
u32 heap_free_bytes(void);
void heap_init(void *base, u32 size);

/* ---- platform (plat_pc.c on hardware, host/sim.c when testing) ---- */
typedef struct {
    u32 fb;
    u16 pitch, w, h;
    u8 bpp, pad;
    u32 fs_bytes;
    u32 memtop;
    u32 usb_addr, usb_bytes; /* SWF files found on USB sticks / disks */
} bootinfo_t;

#define KEY_F12 0x10C
#define KEY_UP 0x101
#define KEY_DOWN 0x102
#define KEY_ESC 27
#define KEY_CTRL_Q 0x11

/* Flash key codes */
#define FK_F12 123

void plat_init(void);
bootinfo_t *plat_bootinfo(void);
const u8 *plat_font(void);
u8 *plat_fb(void);
const u8 *plat_fs(void);
const u8 *plat_usbfs(void);
void *plat_heap(u32 *size);
u32 plat_ms(void);
void plat_sleep(u32 ms);
int plat_key(void);              /* shell: next char / KEY_* (drains input) */
void plat_poll(void);             /* read keyboard + mouse controller */
int plat_kevent(int *code, int *down, int *ascii); /* Flash key events */
int plat_kdown(int code);         /* is Flash key code held */
void plat_mouse(int *x, int *y, int *buttons);
void plat_mouse_set(int x, int y);
void plat_flush_input(void);
void plat_rtc(int *h, int *m, int *s, int *day, int *mon, int *year);
void *plat_heap2(u32 *size);      /* big heap for ActionScript */
void plat_reboot(void);
void plat_poweroff(void);
void plat_halt(void);

/* ---- video.c ---- */
extern int scr_w, scr_h;
u32 *video_bb(void);
void video_init(void);
void video_present(int x, int y, int w, int h);
void video_fill(int x, int y, int w, int h, u32 c);
void video_fb_rect(int x, int y, int w, int h, u32 c);

void con_init(void);
void con_clear(void);
void con_putc(char c);
void con_puts(const char *s);
void con_defer(int on);
void con_cursor(int on);
int con_row_y(int row);
int con_rows_used(void);
int con_cols(void);

/* ---- fs.c ---- */
typedef struct { const char *name; const u8 *data; u32 size; int usb; } kfile;
int fs_count(void);
int fs_get(int i, kfile *f);
int fs_find(const char *name, kfile *f);

/* ---- ata.c (IDE disks, for INSTALL) ---- */
typedef struct { int base, ctrl, slave; u32 sectors; char model[41]; } ata_disk;
int ata_scan(ata_disk *out, int max);
int ata_write(ata_disk *d, u32 lba, const u8 *buf, u32 count);
int ata_read(ata_disk *d, u32 lba, u8 *buf, u32 count);

/* ---- inflate.c ---- */
int inflate_raw(const u8 *src, u32 srclen, u8 *dst, u32 dstlen, u32 *outlen);

/* ---- swf.c ---- */
int swf_play(const u8 *data, u32 size, const char **err);

/* math helpers */
static inline float ksqrt(float x) {
#ifdef HOSTED
    return __builtin_sqrtf(x);
#else
    float r;
    __asm__("fsqrt" : "=t"(r) : "0"(x));
    return r;
#endif
}
static inline int kfloor(float x) { int i = (int)x; return (x < (float)i) ? i - 1 : i; }
static inline float kabs(float x) { return x < 0 ? -x : x; }

#endif
