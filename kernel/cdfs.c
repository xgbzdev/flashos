/* FlashKernel game CDs: load .swf files from ISO 9660 discs / ISO images.
 * Copyright (c) 2026 FlashOS. All rights reserved.
 *
 * Every CD/DVD drive on the IDE controller is read (ATAPI). Joliet long
 * names are used when present. SWF files in the root folder and in folders
 * one level down are copied into memory and listed with a CD tag.
 */
#include "kernel.h"

static u8 sec[2048], dsec[2048];
static ata_disk *drv;
static int found;

static u32 le32(const u8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24); }

static int is_swf(const char *n) {
    u32 l = kstrlen(n);
    return l > 4 && !kstricmp(n + l - 4, ".swf");
}

static void get_name(const u8 *r, int joliet, char *out) {
    int nl = r[32], n = 0;
    const u8 *nm = r + 33;
    if (joliet) {
        for (int i = 0; i + 1 < nl && n < 55; i += 2) {
            u32 c = (u32)(nm[i] << 8) | nm[i + 1];
            out[n++] = c < 128 ? (char)c : '_';
        }
    } else {
        for (int i = 0; i < nl && n < 55; i++) {
            char c = (char)nm[i];
            if (c == ';') break;
            out[n++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        if (n && out[n - 1] == '.') n--; /* "NAME." for files without extension */
    }
    out[n] = 0;
}

static void load_file(const char *name, u32 lba, u32 size) {
    u32 cap;
    u8 *buf = plat_heap2(&cap); /* scratch, then moved to the top of RAM */
    if (!buf || size + (16u << 20) > cap) return;
    u32 secs = (size + 2047) / 2048;
    if (!atapi_read(drv, lba, buf, secs)) return;
    if (!((buf[0] == 'F' || buf[0] == 'C' || buf[0] == 'Z') && buf[1] == 'W' && buf[2] == 'S')) return;
    if (fs_add_file(name, buf, size, 3)) found++;
}

static void scan_dir(u32 lba, u32 size, int joliet, int depth) {
    u32 secs = (size + 2047) / 2048;
    if (secs > 64) secs = 64;
    u32 subs[16], subsz[16];
    int nsub = 0;
    for (u32 s = 0; s < secs; s++) {
        if (!atapi_read(drv, lba + s, dsec, 1)) return;
        for (u32 off = 0; off < 2048;) {
            const u8 *r = dsec + off;
            u32 len = r[0];
            if (!len) break; /* rest of this sector is padding */
            if (off + len > 2048 || len < 34) break;
            int dir = r[25] & 2;
            u32 ext = le32(r + 2), sz = le32(r + 10);
            int special = r[32] == 1 && (r[33] == 0 || r[33] == 1);
            if (!special) {
                char name[56];
                get_name(r, joliet, name);
                if (dir) { if (depth == 0 && nsub < 16) { subs[nsub] = ext; subsz[nsub] = sz; nsub++; } }
                else if (is_swf(name) && name[0] != '.' && sz) load_file(name, ext, sz);
            }
            off += len;
        }
    }
    for (int i = 0; i < nsub; i++) scan_dir(subs[i], subsz[i], joliet, depth + 1);
}

static void scan_disc(void) {
    u32 root = 0, rsize = 0;
    int joliet = 0;
    for (u32 lba = 16; lba < 32; lba++) {
        if (!atapi_read(drv, lba, sec, 1)) return;
        if (sec[1] != 'C' || sec[2] != 'D' || sec[3] != '0' || sec[4] != '0' || sec[5] != '1') return;
        if (sec[0] == 255) break;
        if (sec[0] == 1 && !root) { root = le32(sec + 156 + 2); rsize = le32(sec + 156 + 10); }
        if (sec[0] == 2 && sec[88] == '%' && sec[89] == '/' && (sec[90] == '@' || sec[90] == 'C' || sec[90] == 'E')) {
            root = le32(sec + 156 + 2);
            rsize = le32(sec + 156 + 10);
            joliet = 1;
        }
    }
    if (root) scan_dir(root, rsize, joliet, 0);
}

int cd_scan(void) {
    static ata_disk cds[4];
    found = 0;
    fs_drop_tag(3);
    int n = atapi_scan(cds, 4);
    for (int i = 0; i < n; i++) {
        drv = &cds[i];
        scan_disc();
    }
    return found;
}
