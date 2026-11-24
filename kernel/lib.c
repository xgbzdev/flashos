/* FlashKernel runtime library.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 */
#include "kernel.h"

#ifndef HOSTED
void *memcpy(void *d, const void *s, u32 n) {
    void *r = d;
    __asm__ volatile("cld; rep movsb" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
    return r;
}
void *memset(void *d, int c, u32 n) {
    void *r = d;
    __asm__ volatile("cld; rep stosb" : "+D"(d), "+c"(n) : "a"(c) : "memory");
    return r;
}
void *memmove(void *d, const void *s, u32 n) {
    u8 *dd = d;
    const u8 *ss = s;
    if (dd < ss) return memcpy(d, s, n);
    while (n--) dd[n] = ss[n];
    return d;
}
#endif

u32 kstrlen(const char *s) { u32 n = 0; while (s[n]) n++; return n; }
char kupper(char c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

int kstrnicmp(const char *a, const char *b, u32 n) {
    for (u32 i = 0; i < n; i++) {
        char x = kupper(a[i]), y = kupper(b[i]);
        if (x != y) return x - y;
        if (!x) return 0;
    }
    return 0;
}
int kstricmp(const char *a, const char *b) { return kstrnicmp(a, b, 0xFFFFFFFF); }

void kstrcpy(char *d, const char *s, u32 max) {
    u32 i = 0;
    for (; s[i] && i + 1 < max; i++) d[i] = s[i];
    d[i] = 0;
}

void kutoa(u32 v, char *out) {
    char t[12];
    int n = 0;
    do { t[n++] = '0' + v % 10; v /= 10; } while (v);
    while (n) *out++ = t[--n];
    *out = 0;
}

/* Bump allocator with mark/reset (each SWF run resets to its start mark). */
static u8 *heap_base, *heap_top, *heap_end;

void heap_init(void *base, u32 size) {
    heap_base = heap_top = base;
    heap_end = heap_base + size;
}
void *kmalloc(u32 n) {
    n = (n + 15) & ~15u;
    if ((u32)(heap_end - heap_top) < n) return NULL;
    void *p = heap_top;
    heap_top += n;
    return p;
}
u32 heap_mark(void) { return (u32)(heap_top - heap_base); }
void heap_reset(u32 mark) { heap_top = heap_base + mark; }
u32 heap_free_bytes(void) { return (u32)(heap_end - heap_top); }
