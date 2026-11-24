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

/* Files downloaded with GET live at the top of RAM. */
typedef struct { char name[56]; const u8 *data; u32 size; } dlfile;
static dlfile dl[32];
static int ndl;
u32 net_reserved;

int fs_add_download(const char *name, const u8 *data, u32 len) {
    u32 size;
    u8 *base = plat_heap2(&size);
    if (!base || len + (8u << 20) > size) return 0;
    int slot = -1;
    for (int i = 0; i < ndl; i++) if (!kstricmp(dl[i].name, name)) slot = i;
    if (slot < 0) { if (ndl >= 32) return 0; slot = ndl++; }
    u8 *dst = (u8 *)(((u32)(base + size) - len) & ~15u);
    memmove(dst, data, len);
    net_reserved += (u32)(base + size - dst);
    kstrcpy(dl[slot].name, name, 56);
    dl[slot].data = dst;
    dl[slot].size = len;
    return 1;
}

/* Built-in files first, then USB sticks / extra disks, then downloads. */
int fs_count(void) { return count_of(valid(plat_fs())) + count_of(valid(plat_usbfs())) + ndl; }

int fs_get(int i, kfile *f) {
    const u8 *a = valid(plat_fs()), *u = valid(plat_usbfs());
    int nb = count_of(a), nu = count_of(u);
    int usb = 0;
    if (i >= nb + nu && i < nb + nu + ndl) {
        dlfile *d = &dl[i - nb - nu];
        f->name = d->name;
        f->data = d->data;
        f->size = d->size;
        f->usb = 2;
        return 1;
    }
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
