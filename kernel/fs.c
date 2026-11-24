/* FlashFS: read-only file archive loaded by the boot loader.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Layout: "FLFS" | u32 count | count x { char name[56]; u32 offset; u32 size } | data
 */
#include "kernel.h"

static const u8 *valid(const u8 *a) {
    return (a && a[0] == 'F' && a[1] == 'L' && a[2] == 'F' && a[3] == 'S') ? a : NULL;
}
static u32 rd32(const u8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24); }
static int count_of(const u8 *a) { return a ? (int)rd32(a + 4) : 0; }

/* Built-in files first, then the ones found on USB sticks / extra disks. */
int fs_count(void) { return count_of(valid(plat_fs())) + count_of(valid(plat_usbfs())); }

int fs_get(int i, kfile *f) {
    const u8 *a = valid(plat_fs()), *u = valid(plat_usbfs());
    int nb = count_of(a);
    int usb = 0;
    if (i >= nb) { i -= nb; a = u; usb = 1; }
    if (!a || i < 0 || i >= count_of(a)) return 0;
    const u8 *e = a + 8 + i * 64;
    f->name = (const char *)e;
    f->data = a + rd32(e + 56);
    f->size = rd32(e + 60);
    f->usb = usb;
    return 1;
}

int fs_find(const char *name, kfile *f) {
    for (int i = 0; i < fs_count(); i++)
        if (fs_get(i, f) && !kstricmp(f->name, name)) return 1;
    return 0;
}
