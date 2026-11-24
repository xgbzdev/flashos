/* FlashKernel PC platform layer: ports, TSC timer, PS/2 keyboard, power.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * FlashKernel runs with interrupts disabled: devices are polled.
 */
#include "kernel.h"

#define BOOTINFO_ADDR 0x6000
#define FONT_ADDR 0x5000
#define FS_ADDR 0x02000000
#define HEAP_ADDR 0x00800000
#define HEAP_SIZE (0x02000000 - 0x00800000)

static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(p)); }
static inline u8 inb(u16 p) { u8 v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline u64 rdtsc(void) {
    u32 lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}
/* 64 / 32 -> low 32 bits of quotient, without libgcc */
static u32 div64(u64 n, u32 d) {
    u32 hi = (u32)(n >> 32), lo = (u32)n, q, r;
    hi %= d;
    __asm__("divl %4" : "=a"(q), "=d"(r) : "a"(lo), "d"(hi), "rm"(d));
    (void)r;
    return q;
}

static u64 tsc0;
static u32 tsc_per_ms = 1000000;

static u16 pit_read(void) {
    outb(0x43, 0x00);
    u8 lo = inb(0x40);
    u8 hi = inb(0x40);
    return (u16)(lo | (hi << 8));
}

void plat_init(void) {
    /* PIT channel 0: mode 2, reload 65536 -> counts down at 1.193182 MHz */
    outb(0x43, 0x34);
    outb(0x40, 0);
    outb(0x40, 0);
    u16 prev = pit_read();
    u64 t0 = rdtsc();
    u32 ticks = 0;
    while (ticks < 59659) { /* 50 ms */
        u16 cur = pit_read();
        ticks += (u16)(prev - cur);
        prev = cur;
    }
    u32 per = div64(rdtsc() - t0, 50);
    if (per > 1000) tsc_per_ms = per;
    tsc0 = rdtsc();
    while (inb(0x64) & 1) (void)inb(0x60); /* flush keyboard */
}

bootinfo_t *plat_bootinfo(void) { return (bootinfo_t *)BOOTINFO_ADDR; }
u8 *plat_fb(void) { return (u8 *)plat_bootinfo()->fb; }
const u8 *plat_font(void) { return (const u8 *)FONT_ADDR; }
const u8 *plat_fs(void) { return (const u8 *)FS_ADDR; }
void *plat_heap(u32 *size) { *size = HEAP_SIZE; return (void *)HEAP_ADDR; }

u32 plat_ms(void) { return div64(rdtsc() - tsc0, tsc_per_ms); }
void plat_sleep(u32 ms) { u32 t = plat_ms(); while (plat_ms() - t < ms) __asm__ volatile("pause"); }

/* ---- PS/2 keyboard, scancode set 1 (US layout) ---- */
static const char key_lo[0x3A] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '};
static const char key_hi[0x3A] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '};
static int k_shift, k_ctrl, k_caps, k_e0;

int plat_key(void) {
    for (;;) {
        u8 st = inb(0x64);
        if (!(st & 1)) return -1;
        u8 sc = inb(0x60);
        if (st & 0x20) continue; /* mouse byte */
        if (sc == 0xE0) { k_e0 = 1; continue; }
        int rel = sc & 0x80;
        sc &= 0x7F;
        if (k_e0) {
            k_e0 = 0;
            if (sc == 0x1D) k_ctrl = !rel;
            else if (sc == 0x1C && !rel) return '\n';
            continue;
        }
        if (sc == 0x2A || sc == 0x36) { k_shift = !rel; continue; }
        if (sc == 0x1D) { k_ctrl = !rel; continue; }
        if (rel) continue;
        if (sc == 0x3A) { k_caps = !k_caps; continue; }
        if (sc == 0x58) return KEY_F12;
        if (sc >= 0x3A) continue;
        char c = k_shift ? key_hi[sc] : key_lo[sc];
        if (!c) continue;
        int letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (letter && k_caps) c ^= 0x20;
        if (letter && k_ctrl) return c & 0x1F;
        return (u8)c;
    }
}

void plat_halt(void) { for (;;) __asm__ volatile("cli; hlt"); }

void plat_reboot(void) {
    for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) {}
    outb(0x64, 0xFE);
    struct { u16 l; u32 b; } __attribute__((packed)) zero = {0, 0};
    __asm__ volatile("lidt %0; int3" : : "m"(zero)); /* triple fault */
}

void plat_poweroff(void) {
    outw(0x604, 0x2000);  /* QEMU */
    outw(0xB004, 0x2000); /* Bochs / old QEMU */
    outw(0x4004, 0x3400); /* VirtualBox */
}
