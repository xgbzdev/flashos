/* FlashOS documents: files that apps save, kept on the hard disk where
 * FlashOS is installed (or in memory until reboot when it is not).
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Disk layout from LBA 32768 (16 MB in, after the system files, so that
 * reinstalling FlashOS keeps the documents):
 *   sector 0        "FLASHDOCS1", u32 entries
 *   sectors 1..32   256 entries x { char name[48]; u32 lba, count, size, used }
 *   then data, allocated first-fit in whole sectors
 */
#include "kernel.h"

#define DOCS_LBA 32768u
#define NENT 256
#define DIR_SECT 32
#define DATA_LBA (DOCS_LBA + 1 + DIR_SECT)
#define SYS_COPY ((const u8 *)0x01F00000)

typedef struct { char name[48]; u32 lba, cnt, size, used; } dent;

static int state;          /* 0 not checked, 1 disk, 2 memory only */
static ata_disk disk;
static dent dir[NENT];
static const u8 *mem[NENT]; /* memory-only store */
static u8 sect[512];

static int save_dir(void) {
    return state != 1 || ata_write(&disk, DOCS_LBA + 1, (const u8 *)dir, DIR_SECT);
}

static void init(void) {
    if (state) return;
    state = 2;
    memset(dir, 0, sizeof dir);
#ifndef HOSTED
    if (SYS_COPY[510] != 0x55 || SYS_COPY[511] != 0xAA) return;
    static ata_disk d[4];
    int n = ata_scan(d, 4);
    for (int i = 0; i < n; i++) {
        /* only the disk FlashOS is installed on: same boot code as ours */
        if (d[i].sectors < DATA_LBA + 4096) continue;
        if (!ata_read(&d[i], 0, sect, 1) || memcmp(sect, SYS_COPY, 440)) continue;
        disk = d[i];
        if (!ata_read(&disk, DOCS_LBA, sect, 1)) continue;
        if (memcmp(sect, "FLASHDOCS1", 10)) {
            memset(sect, 0, 512);
            memcpy(sect, "FLASHDOCS1", 10);
            sect[12] = NENT & 255;
            sect[13] = NENT >> 8;
            state = 1;
            if (!ata_write(&disk, DOCS_LBA, sect, 1) || !save_dir()) { state = 2; continue; }
        } else {
            if (!ata_read(&disk, DOCS_LBA + 1, (u8 *)dir, DIR_SECT)) continue;
            for (int k = 0; k < NENT; k++) dir[k].name[47] = 0;
        }
        state = 1;
        return;
    }
#endif
}

int docs_on_disk(void) { init(); return state == 1; }
int docs_count(void) { init(); int c = 0; for (int i = 0; i < NENT; i++) c += dir[i].used ? 1 : 0; return c; }

int docs_get(int idx, const char **name, u32 *size) {
    init();
    for (int i = 0; i < NENT; i++) {
        if (!dir[i].used) continue;
        if (idx-- == 0) { *name = dir[i].name; *size = dir[i].size; return 1; }
    }
    return 0;
}

static int find(const char *name) {
    for (int i = 0; i < NENT; i++) if (dir[i].used && !kstricmp(dir[i].name, name)) return i;
    return -1;
}

/* contents into buf (needs size bytes, rounded up to 512 for the disk) */
u8 *docs_read(const char *name, u32 *size, void *(*alloc)(u32)) {
    init();
    int i = find(name);
    if (i < 0) return NULL;
    *size = dir[i].size;
    u8 *b = alloc(dir[i].cnt * 512 + 512);
    if (!b) return NULL;
    if (state == 2) { memcpy(b, mem[i], dir[i].size); return b; }
    for (u32 k = 0; k < dir[i].cnt; k += 64) {
        u32 c = dir[i].cnt - k < 64 ? dir[i].cnt - k : 64;
        if (!ata_read(&disk, dir[i].lba + k, b + k * 512, c)) return NULL;
    }
    return b;
}

int docs_remove(const char *name) {
    init();
    int i = find(name);
    if (i < 0) return 0;
    dir[i].used = 0;
    return save_dir();
}

/* first free run of cnt sectors */
static u32 alloc_lba(u32 cnt) {
    u32 at = DATA_LBA;
    for (;;) {
        int clash = 0;
        for (int i = 0; i < NENT; i++) {
            if (!dir[i].used) continue;
            if (at < dir[i].lba + dir[i].cnt && dir[i].lba < at + cnt) { at = dir[i].lba + dir[i].cnt; clash = 1; }
        }
        if (!clash) break;
    }
    return at + cnt <= disk.sectors ? at : 0;
}

int docs_write(const char *name, const u8 *data, u32 n) {
    init();
    if (!*name || kstrlen(name) > 47) return 0;
    int i = find(name);
    u32 cnt = (n + 511) / 512;
    if (!cnt) cnt = 1;
    if (state == 2) {
        u8 *b = kmalloc(n + 1);
        if (!b) return 0;
        memcpy(b, data, n);
        if (i < 0) { for (i = 0; i < NENT && dir[i].used; i++) {} if (i == NENT) return 0; }
        mem[i] = b;
        kstrcpy(dir[i].name, name, 48);
        dir[i].size = n;
        dir[i].cnt = cnt;
        dir[i].used = 1;
        return 1;
    }
    if (i >= 0 && dir[i].cnt < cnt) dir[i].used = 0; /* does not fit: move it */
    if (i < 0 || !dir[i].used) {
        if (i < 0) { for (i = 0; i < NENT && dir[i].used; i++) {} if (i == NENT) return 0; }
        u32 lba = alloc_lba(cnt);
        if (!lba) return 0;
        dir[i].lba = lba;
        dir[i].cnt = cnt;
    }
    u32 full = n / 512;
    for (u32 k = 0; k < full; k += 64) {
        u32 c = full - k < 64 ? full - k : 64;
        if (!ata_write(&disk, dir[i].lba + k, data + k * 512, c)) return 0;
    }
    if (n % 512 || !n) {
        memset(sect, 0, 512);
        memcpy(sect, data + full * 512, n % 512);
        if (!ata_write(&disk, dir[i].lba + full, sect, 1)) return 0;
    }
    kstrcpy(dir[i].name, name, 48);
    dir[i].size = n;
    dir[i].used = 1;
    return save_dir();
}

u32 docs_free_kb(void) {
    init();
    if (state == 2) return heap_free_bytes() / 1024;
    u32 used = 0;
    for (int i = 0; i < NENT; i++) if (dir[i].used) used += dir[i].cnt;
    u32 total = disk.sectors > DATA_LBA ? disk.sectors - DATA_LBA : 0;
    return (total - used) / 2;
}
