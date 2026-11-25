/* FlashKernel PC platform layer: ports, TSC timer, PS/2 keyboard + mouse,
 * CMOS clock, power.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * FlashKernel runs with interrupts disabled: devices are polled.
 */
#include "kernel.h"

#define BOOTINFO_ADDR 0x6000
#define FONT_ADDR 0x5000
#define FS_ADDR 0x02000000
#define HEAP_ADDR 0x00800000
#define HEAP_SIZE (0x01D00000 - 0x00800000) /* 0x1D00000+: installer copies */

static inline void outb(u16 p, u8 v) { __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p)); }
static inline void outw(u16 p, u16 v) { __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(p)); }
static inline u8 inb(u16 p) { u8 v; __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p)); return v; }
static inline u64 rdtsc(void) {
    u32 lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}
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

/* ---------------- PS/2 controller helpers ---------------- */
static void kbc_wait_write(void) { for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) {} }
static int kbc_wait_read(void) {
    for (int i = 0; i < 100000; i++) if (inb(0x64) & 1) return 1;
    return 0;
}
static void mouse_cmd(u8 v) {
    kbc_wait_write(); outb(0x64, 0xD4);
    kbc_wait_write(); outb(0x60, v);
    if (kbc_wait_read()) (void)inb(0x60); /* ACK */
}
static void mouse_init(void) {
    kbc_wait_write(); outb(0x64, 0xA8);           /* enable aux port */
    kbc_wait_write(); outb(0x64, 0x20);           /* read config byte */
    u8 cfg = kbc_wait_read() ? inb(0x60) : 0x47;
    cfg &= (u8)~0x20;                             /* mouse clock on */
    cfg &= (u8)~0x03;                             /* no IRQs: we poll */
    kbc_wait_write(); outb(0x64, 0x60);
    kbc_wait_write(); outb(0x60, cfg);
    mouse_cmd(0xF6);                              /* defaults */
    mouse_cmd(0xF4);                              /* streaming on */
}

/* ---------------- ACPI power-management timer (3.579545 MHz) ---------------- */
static u16 pm_port;
static u32 pm_mask, pm_last;
static u64 pm_acc;
static int clock_src; /* 0 TSC, 1 ACPI PM timer */

static int sig_eq(const u8 *p, const char *s, int n) { for (int i = 0; i < n; i++) if (p[i] != (u8)s[i]) return 0; return 1; }
static const u8 *find_rsdp(void) {
    u32 bda = 0x40E; /* BIOS data area: EBDA segment */
    __asm__("" : "+r"(bda));
    u32 ebda = (u32)(*(volatile u16 *)bda) << 4;
    if (ebda >= 0x80000 && ebda < 0xA0000)
        for (u32 a = ebda; a < ebda + 1024; a += 16) if (sig_eq((const u8 *)a, "RSD PTR ", 8)) return (const u8 *)a;
    for (u32 a = 0xE0000; a < 0x100000; a += 16) if (sig_eq((const u8 *)a, "RSD PTR ", 8)) return (const u8 *)a;
    return NULL;
}
static void acpi_timer_init(void) {
    const u8 *rsdp = find_rsdp();
    if (!rsdp) return;
    const u8 *rsdt = (const u8 *)*(const u32 *)(rsdp + 16);
    if (!rsdt || !sig_eq(rsdt, "RSDT", 4)) return;
    u32 len = *(const u32 *)(rsdt + 4);
    for (u32 off = 36; off + 4 <= len; off += 4) {
        const u8 *t = (const u8 *)*(const u32 *)(rsdt + off);
        if (!t || !sig_eq(t, "FACP", 4)) continue;
        u32 port = *(const u32 *)(t + 76);
        if (!port || port > 0xFFFF || t[91] != 4) return;
        pm_port = (u16)port;
        pm_mask = (*(const u32 *)(t + 112) & 0x100) ? 0xFFFFFFFFu : 0x00FFFFFFu; /* TMR_VAL_EXT */
        return;
    }
}
static inline u32 inl_(u16 p) { u32 v; __asm__ volatile("inl %1, %0" : "=a"(v) : "Nd"(p)); return v; }

void plat_init(void) {
    acpi_timer_init(); /* before the heaps can overwrite the ACPI tables */
    if (pm_port) {
        /* sanity check: the timer must advance */
        u32 a = inl_(pm_port) & pm_mask;
        for (int i = 0; i < 100000; i++) {}
        u32 b = inl_(pm_port) & pm_mask;
        if (a != b) { clock_src = 1; pm_last = b; }
    }
    outb(0x43, 0x34);
    outb(0x40, 0);
    outb(0x40, 0);
    u16 prev = pit_read();
    u64 t0 = rdtsc();
    u32 ticks = 0;
    while (ticks < 59659) {
        u16 cur = pit_read();
        ticks += (u16)(prev - cur);
        prev = cur;
    }
    u32 per = div64(rdtsc() - t0, 50);
    if (per > 1000) tsc_per_ms = per;
    tsc0 = rdtsc();
    mouse_init();
    while (inb(0x64) & 1) (void)inb(0x60);
}

bootinfo_t *plat_bootinfo(void) { return (bootinfo_t *)BOOTINFO_ADDR; }
u8 *plat_fb(void) { return (u8 *)plat_bootinfo()->fb; }
const u8 *plat_font(void) { return (const u8 *)FONT_ADDR; }
const u8 *plat_fs(void) { return (const u8 *)FS_ADDR; }
const u8 *plat_usbfs(void) { return plat_bootinfo()->usb_bytes ? (const u8 *)plat_bootinfo()->usb_addr : NULL; }
void *plat_heap(u32 *size) { *size = HEAP_SIZE; return (void *)HEAP_ADDR; }

void *plat_heap2(u32 *size) {
    u32 base = (FS_ADDR + plat_bootinfo()->fs_bytes + 0xFFFFF) & ~0xFFFFFu;
    if (plat_bootinfo()->usb_bytes) base = (plat_bootinfo()->usb_addr + plat_bootinfo()->usb_bytes + 0xFFFFF) & ~0xFFFFFu;
    u32 top = plat_bootinfo()->memtop;
    if (top < base + (8u << 20)) top = 0x04000000;     /* assume 64 MB */
    if (top > 0xC0000000u) top = 0xC0000000u;
    if (top < base + (4u << 20)) { *size = 0; return 0; }
    *size = top - base - net_reserved;
    return (void *)base;
}

u32 plat_ms(void) {
    if (clock_src == 1) {
        u32 now = inl_(pm_port) & pm_mask;
        pm_acc += (now - pm_last) & pm_mask;
        pm_last = now;
        return div64(pm_acc * 1000, 3579545);
    }
    return div64(rdtsc() - tsc0, tsc_per_ms);
}
const char *plat_clock_name(void) { return clock_src == 1 ? "ACPI PM timer" : "TSC (calibrated with the PIT)"; }
void plat_sleep(u32 ms) { u32 t = plat_ms(); while (plat_ms() - t < ms) __asm__ volatile("pause"); }

/* ---------------- keyboard (scancode set 1, US layout) ---------------- */
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
/* scancode -> Flash key code */
static const u8 flash_code[0x59] = {
    0, 27, 49, 50, 51, 52, 53, 54, 55, 56, 57, 48, 189, 187, 8, 9,
    81, 87, 69, 82, 84, 89, 85, 73, 79, 80, 219, 221, 13, 17, 65, 83,
    68, 70, 71, 72, 74, 75, 76, 186, 222, 192, 16, 220, 90, 88, 67, 86,
    66, 78, 77, 188, 190, 191, 16, 106, 18, 32, 20, 112, 113, 114, 115, 116,
    117, 118, 119, 120, 121, 144, 145, 103, 104, 105, 109, 100, 101, 102, 107, 97,
    98, 99, 96, 110, 0, 0, 0, 122, 123};

static int k_shift, k_ctrl, k_caps, k_e0;
static u8 kstate[256];

#define QN 64
static int cq[QN], cq_h, cq_t;                   /* shell chars */
static struct { u8 code, down, ascii, mods; } eq[QN];  /* Flash key events */
/* Shift/Ctrl/Alt as they were when the key event being handled happened */
static int ev_mods, ev_active;
static int eq_h, eq_t;

static int mx = 320, my = 240, mbtn, mpkt[3], mpn;

static void push_char(int c) { if (cq_t - cq_h < QN) cq[cq_t++ % QN] = c; }
static void push_ev(int code, int down, int ascii) {
    if (!code) return;
    kstate[code] = (u8)down;
    if (eq_t - eq_h < QN) {
        eq[eq_t % QN].code = (u8)code;
        eq[eq_t % QN].down = (u8)down;
        eq[eq_t % QN].ascii = (u8)ascii;
        eq[eq_t % QN].mods = (u8)(kstate[16] | (kstate[17] << 1) | (kstate[18] << 2));
        eq_t++;
    }
}

static void kbd_byte(u8 sc) {
    if (sc == 0xE0) { k_e0 = 1; return; }
    int rel = sc & 0x80;
    sc &= 0x7F;
    if (k_e0) {
        k_e0 = 0;
        int fc = 0;
        switch (sc) {
        case 0x48: fc = 38; if (!rel) push_char(KEY_UP); break;
        case 0x50: fc = 40; if (!rel) push_char(KEY_DOWN); break;
        case 0x4B: fc = 37; break;
        case 0x4D: fc = 39; break;
        case 0x47: fc = 36; break;
        case 0x4F: fc = 35; break;
        case 0x49: fc = 33; break;
        case 0x51: fc = 34; break;
        case 0x52: fc = 45; break;
        case 0x53: fc = 46; break;
        case 0x1D: fc = 17; k_ctrl = !rel; break;
        case 0x38: fc = 18; break;
        case 0x1C: fc = 13; if (!rel) push_char('\n'); break;
        }
        push_ev(fc, !rel, fc == 13 ? 13 : 0);
        return;
    }
    if (sc == 0x2A || sc == 0x36) k_shift = !rel;
    if (sc == 0x1D) k_ctrl = !rel;
    if (sc == 0x3A && !rel) k_caps = !k_caps;
    int fc = sc < 0x59 ? flash_code[sc] : 0;
    char c = 0;
    if (sc < 0x3A) {
        c = k_shift ? key_hi[sc] : key_lo[sc];
        int letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        if (letter && k_caps) c ^= 0x20;
        if (letter && k_ctrl && !rel) { push_char(c & 0x1F); push_ev(fc, 1, c); return; }
    }
    if (c == '\n') c = 13;
    push_ev(fc, !rel, (u8)c);
    if (rel) return;
    if (sc == 0x58) push_char(KEY_F12);
    else if (c == 13) push_char('\n');
    else if (c) push_char((u8)c);
}

static void mouse_byte(u8 b) {
    if (mpn == 0 && !(b & 0x08)) return; /* resync */
    mpkt[mpn++] = b;
    if (mpn < 3) return;
    mpn = 0;
    int f = mpkt[0];
    if (f & 0xC0) return; /* overflow */
    int dx = mpkt[1] - ((f & 0x10) ? 256 : 0);
    int dy = mpkt[2] - ((f & 0x20) ? 256 : 0);
    mx += dx;
    my -= dy;
    bootinfo_t *bi = plat_bootinfo();
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;
    if (mx >= bi->w) mx = bi->w - 1;
    if (my >= bi->h) my = bi->h - 1;
    mbtn = f & 7;
}

void plat_poll(void) {
    for (int n = 0; n < 256; n++) {
        u8 st = inb(0x64);
        if (!(st & 1)) return;
        u8 b = inb(0x60);
        if (st & 0x20) mouse_byte(b);
        else kbd_byte(b);
    }
}

int plat_key(void) {
    plat_poll();
    eq_h = eq_t; /* the shell does not use key events */
    if (cq_h == cq_t) return -1;
    return cq[cq_h++ % QN];
}
int plat_kevent(int *code, int *down, int *ascii) {
    plat_poll();
    if (eq_h == eq_t) { ev_active = 0; return 0; }
    *code = eq[eq_h % QN].code;
    *down = eq[eq_h % QN].down;
    *ascii = eq[eq_h % QN].ascii;
    ev_mods = eq[eq_h % QN].mods;
    ev_active = 1;
    eq_h++;
    return 1;
}
int plat_kdown(int code) {
    if (code < 0 || code >= 256) return 0;
    if (ev_active && code >= 16 && code <= 18 && (ev_mods >> (code - 16) & 1)) return 1;
    return kstate[code];
}
void plat_mouse(int *x, int *y, int *b) { plat_poll(); *x = mx; *y = my; *b = mbtn; }
void plat_mouse_set(int x, int y) { mx = x; my = y; }
void plat_flush_input(void) {
    plat_poll();
    cq_h = cq_t;
    eq_h = eq_t;
    ev_active = 0;
    memset(kstate, 0, sizeof kstate);
}

static int cmos(int r) { outb(0x70, (u8)r); return inb(0x71); }
static int bcd(int v, int binary) { return binary ? v : (v & 15) + (v >> 4) * 10; }
void plat_rtc(int *h, int *m, int *s, int *day, int *mon, int *year) {
    for (int i = 0; i < 100000 && (cmos(0x0A) & 0x80); i++) {}
    int bin = cmos(0x0B) & 4;
    *s = bcd(cmos(0x00), bin);
    *m = bcd(cmos(0x02), bin);
    *h = bcd(cmos(0x04) & 0x7F, bin);
    *day = bcd(cmos(0x07), bin);
    *mon = bcd(cmos(0x08), bin);
    *year = 2000 + bcd(cmos(0x09), bin);
}

void plat_halt(void) { for (;;) __asm__ volatile("cli; hlt"); }

void plat_reboot(void) {
    kbc_wait_write();
    outb(0x64, 0xFE);
    struct { u16 l; u32 b; } __attribute__((packed)) zero = {0, 0};
    __asm__ volatile("lidt %0; int3" : : "m"(zero));
}

void plat_poweroff(void) {
    outw(0x604, 0x2000);
    outw(0xB004, 0x2000);
    outw(0x4004, 0x3400);
}
